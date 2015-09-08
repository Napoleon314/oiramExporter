#include "stdafx.h"
#include "Optimizer.h"
#include "LogManager.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

Optimizer::
Optimizer()
:	mEpsilon(0.0f)
{
	if (mD3D = Direct3DCreate9(D3D_SDK_VERSION))
	{
		D3DPRESENT_PARAMETERS d3dpp;
		ZeroMemory(&d3dpp, sizeof(d3dpp));
		d3dpp.Windowed = TRUE;
		d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;

		// ��Ҫ�����ɹ���������D3DCREATE_FPU_PRESERVE, �����Ӱ��3dmax��Ⱦ
		HRESULT hr = mD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, GetCOREInterface()->GetMAXHWnd(),
			D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &d3dpp, &mD3DDevice);

		if (FAILED(hr))
			LogManager::getSingleton().logMessage(false, "Create direct3d device failed, HRESULT = %ld.", hr);
	}
	else
		LogManager::getSingleton().logMessage(false, "Direct3DCreate9 failed...");
}


Optimizer::
~Optimizer()
{
	if (mD3DDevice)
		mD3DDevice->Release();
	if (mD3D)
		mD3D->Release();
}


Optimizer& Optimizer::
getSingleton()
{
	static Optimizer msSingleton;
	return msSingleton;
}


void Optimizer::
setEpsilon(float epsilon)
{
	mEpsilon = epsilon;
}
