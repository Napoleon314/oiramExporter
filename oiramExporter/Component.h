#ifndef _Component_hpp__
#define _Component_hpp__

#include <windows.h>
#include <string>
#include <map>

namespace oiram
{
	class Serializer;
	class RenderSystem;
}

class Component
{
public:
	~Component();

	static Component& getSingleton();

	// max�ļ������仯, ����������Ϣ
	void					OnMaxFileModified();
	// ���������仯, ��Ҫ���µ���
	void					OnSceneModified();

	// ���öԻ���
	bool					OptionDialog();
	// ����
	bool					Export(bool exportSelected);
	// ������
	void					Rename(const MSTR& name);

	oiram::RenderSystem*	CreateRenderSystem();
	void					DestroyRenderSystem(oiram::RenderSystem* rendersystem);
	const std::string&		GetComponentName()const { return mComponentName; }
	const std::string&		GetComponentDirectory()const { return mComponentDirectory; }

private:
	Component();

	friend INT_PTR CALLBACK OptionDialogFunction(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	void					LoadConfig();
	void					SaveConfig();

	bool					LoadImpl();
	void					UnloadImpl();

	bool					LoadComponent();
	void					UnloadComponent();

	float					GetUnitValue(int unitType);
	float					ConvertToMeter(int metricDisp, int unitType);
	float					UnitsConversion();

public:
	static const MCHAR		msComponentDirectory[];

private:
	HMODULE					mComponentModule;
	std::string				mComponentName;
	std::string				mComponentDirectory;

	oiram::Serializer*		mSerializer;

	typedef oiram::RenderSystem*(*oiramCreateRenderSystem)();
	oiramCreateRenderSystem	mCreateRenderSystem;

	HMODULE					mOiramImplModule;
	void*					mOiramExporter;

	bool					mSceneModified;
	MSTR					mRename;
};

#endif
