#include "stdafx.h"
#include "ViewExtended.h"
#include "res\resource.h"
#include "requisites.h"
#include "Component.h"
#include "RenderSystem.h"

ViewExtended::
ViewExtended()
:	mWnd(0), mInstance(0),
	mRenderThreadRunning(false), mRenderSystem(nullptr), mSceneUpdated(true), mEnableTracing(false)
{
}


ViewExtended::
~ViewExtended()
{
}


void ViewExtended::
RefreshScene()
{
}


void ViewExtended::
TraceActiveViewport()
{
	if (mRenderSystem.load())
	{
		Interface* maxInterface = GetCOREInterface();
		ViewExp* maxViewport = maxInterface->GetActiveViewport();
		if (maxViewport)
		{
			// ȡ��GraphicsWindow
			GraphicsWindow* maxGraphicsWindow = maxViewport->getGW();
			assert(maxGraphicsWindow);
			// ͸��/����ͶӰ
			bool perspectiveView = maxViewport->IsPerspView() == TRUE;
			// �ӿڿ��
			float	width = static_cast<float>(maxGraphicsWindow->getWinSizeX()), 
					height = static_cast<float>(maxGraphicsWindow->getWinSizeY());
			// ����ͶӰ��Ҫ����GetVPWorldWidthȡ��camera��zoom��Ϣ
			float scaleFactor = maxViewport->GetVPWorldWidth(Point3(0,0,0)) / 360.0f;
			if (!perspectiveView)
			{
				width *= scaleFactor;
				height *= scaleFactor;
			}
			// width fov -> height fov
			float xfov = maxViewport->GetFOV();
			float aspect = static_cast<float>(width) / height;
			float yfov = 2.0f * atan(tan(xfov / 2.0f) / aspect);
			// ȡ��viewTM��nearClip/farClip
			float mat[4][4] = {0};
			Matrix3 viewTM;
			int persp = 0;
			float nearClip = 10.0f, farClip = 1000.0f;
			maxGraphicsWindow->getCameraMatrix(mat, &viewTM, &persp, &nearClip, &farClip);
			maxInterface->ReleaseViewport(maxViewport);

			// max -> ogl
			const float halfPI = 1.570796326794895f;
			Quat xRot = QFromAngAxis(halfPI, Point3(1,0,0));
			RotateMatrix(viewTM, xRot);
			// quaternion����������ϵ, ����wҪȡ��
			Quat viewOrientation(viewTM);
			viewOrientation.w = -viewOrientation.w;
			// translation
			const Point3& viewPosition = viewTM.GetTrans();

			mRenderSystem.load()->setFrustum(perspectiveView, yfov, nearClip, farClip,
				static_cast<int>(width), static_cast<int>(height),
				oiram::vec3(viewPosition.x, viewPosition.y, viewPosition.z), 
				oiram::vec4(viewOrientation.x, viewOrientation.y, viewOrientation.z, viewOrientation.w));
			mRenderSystem.load()->notifyRedraw();
		}
	}
}


void ViewExtended::
RenderScene()
{
	// ��������
	//ExportScene();
	// ��ʼ��
	Component& component = Component::getSingleton();
	oiram::RenderSystem* renderSystem = component.CreateRenderSystem();
	if (renderSystem)
	{
		if (renderSystem->create(GetHwnd(), component.GetComponentDirectory().c_str()))
		{
			mRenderSystem = renderSystem;
			mSceneUpdated = true;
			// ��ѭ��
			while (mRenderThreadRunning)
			{
				/*
				if (mSceneUpdated)
				{
					// ���ܵĻ��������볡��
					std::string sceneFileName = config.exportPath + config.maxName + ".scene";
					if (!renderSystem->loadScene(sceneFileName.c_str()))
					{
						// ���о�����ģ��
						std::string modelFileName = config.exportPath + config.maxName;
						renderSystem->loadModel(modelFileName.c_str());
					}

					mSceneUpdated = false;
				}

				// tracing mode
				if (mEnableTracing)
					TraceActiveViewport();
				*/
				if (!renderSystem->renderOneFrame())
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}

		component.DestroyRenderSystem(renderSystem);
	}
}


void ViewExtended::
ExportScene()
{
	if (Component::getSingleton().Export(false))
		mSceneUpdated = true;
}


HWND ViewExtended::
CreateViewWindow(HWND hParent, int x, int y, int w, int h)
{
#ifdef _WIN64
	mInstance = (HINSTANCE)GetWindowLongPtr(hParent, GWLP_HINSTANCE);
#else
	mInstance = (HINSTANCE)GetWindowLong(hParent, GWL_HINSTANCE);
#endif

	LRESULT WINAPI ViewExtendedDefWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, ViewExtendedDefWindowProc, 0, 0, 
		mInstance, NULL, LoadCursor(NULL, IDC_ARROW), NULL, NULL, _T("oiramViewExtended"), NULL	};

	::RegisterClassEx(&wc);
	mWnd = ::CreateWindow(_T("oiramViewExtended"), _T("oiramViewExtended"), WS_VISIBLE | WS_CHILD, x, y, w, h, 
		hParent, NULL, mInstance, (void*)this);

	// ��̨��Ⱦ�߳�
	mRenderThreadRunning = true;
	mRenderThread = std::thread(&ViewExtended::RenderScene, std::ref(*this));

	return mWnd;
}


void ViewExtended::
DestroyViewWindow(HWND hWnd)
{
	// ����״̬
	mRenderSystem = nullptr;
	mEnableTracing = false;

	// �ȴ��߳̽���
	mRenderThreadRunning = false;
	mRenderThread.join();

	::DestroyWindow(mWnd);
}


LRESULT WINAPI ViewExtendedDefWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static ViewExtended* viewExtended = nullptr;
	static HMENU hMenu;
	static HMENU hMenuPopup;

	switch (uMsg)
	{
	case WM_CREATE:
		hMenu = ::LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MENU_POPUP));
		hMenuPopup = ::GetSubMenu(hMenu, 0);
		break;

	case WM_DESTROY:
		::DestroyMenu(hMenuPopup);
		::DestroyMenu(hMenu);
		break;

	case WM_NCCREATE:
		viewExtended = reinterpret_cast<ViewExtended*>(((LPCREATESTRUCT(lParam))->lpCreateParams));
		break;

	case WM_LBUTTONDOWN:
		// ����ͬ����ǰ��ӿ�״̬
		viewExtended->TraceActiveViewport();
		break;

	case WM_RBUTTONUP:
		{
			// �����˵�
			int count = ::GetMenuItemCount(hMenuPopup);
			POINT pt = { LOWORD(lParam), HIWORD(lParam) };
			::ClientToScreen(hWnd, &pt);
			::TrackPopupMenu(hMenuPopup, 0, pt.x, pt.y, 0, hWnd, NULL);
		}
		break;

	case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
			case IDM_TRACING:
				{
					// ����/�ر�׷��ģʽ
					UINT state = ::GetMenuState(hMenuPopup, IDM_TRACING, MF_BYCOMMAND);
					bool isChecked = (state & MF_CHECKED) == MF_CHECKED;
					::CheckMenuItem(hMenuPopup, IDM_TRACING, MF_BYCOMMAND | (isChecked ? MF_UNCHECKED : MF_CHECKED));
					viewExtended->SetEnableTracing(!isChecked);
				}
				break;

			case IDM_REFRESH:
				{
					// ˢ�³���
					viewExtended->RefreshScene();
				}
				break;

			case IDM_VIEWS:
				{
					// ����max��view�˵�
					POINT pt = {0};
					::GetCursorPos(&pt);
					::ScreenToClient(hWnd, &pt);
					GetCOREInterface()->PutUpViewMenu(hWnd, pt);
				}
				break;
			}
		}
		break;

	case WM_SIZE:
		{
			// ��Ӧ���ڳߴ�仯
			oiram::RenderSystem* renderSystem = viewExtended->GetRenderSystem();
			if (renderSystem)
				renderSystem->resize();
		}
		break;

	case WM_ERASEBKGND:
		return TRUE;

	case WM_PAINT:
		{
			oiram::RenderSystem* renderSystem = viewExtended->GetRenderSystem();
			if (renderSystem)
				renderSystem->notifyRedraw();
		}
		break;

	default:
		break;
	}

	return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}
