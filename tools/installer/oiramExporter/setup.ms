oiramMenuID = genClassID returnValue:true;

macroScript Export
category: "oiramExport"
tooltip: "oiramExport"
(
	on execute do
	(
		try
		(
			if oiram.optionDialog() do
			(
				oiram.export(false)
			)
		)
		catch()
	)
)

-- ��װ�Ի���
rollout oiram_setup_dialog ""
(
	HyperLink hy "www.flagon.cn" pos:[10,5] address:"http://www.flagon.cn" color:(color 255 255 255) hovercolor:(color 0 255 0)
	button btInstall "Install" pos:[oiram_setup_dialog.width - 160, oiram_setup_dialog.height - 35] width:70 height:25
	button btExit "Exit" pos:[oiram_setup_dialog.width - 80, oiram_setup_dialog.height - 35] width:70 height:25
	
	local mPluginName = "oiramExporter"
	local mWorkDirectory;
	
	on oiram_setup_dialog open do
	(
		-- ���ݲ���Ƿ�װ, �����ǰ�װ����ж��
		if (execute mPluginName) != undefined do
			btInstall.text = "UnInstall"
			
		mWorkDirectory = getSourceFileName()
		findPos = findString mWorkDirectory "\\setup.ms"
		mWorkDirectory = substring mWorkDirectory 1 findPos
	)
	
	on btInstall pressed do
	(
		destroydialog oiram_setup_dialog

		if (execute mPluginName) != undefined then
		(
			--UnInstall--
			-- �õ�plugin.ini��·��
			pluginIniFile = (GetDir #maxData)
			
			-- plugin.ini��Max2010��ʼ����ΪPlugin.UserSettings.ini
			if ((maxVersion())[1]) > 11000 then
				pluginIniFile += "Plugin.UserSettings.ini"
			else
				pluginIniFile += "plugin.ini"

			-- �����·����plugin.ini��ȥ��
			delIniSetting pluginIniFile "Directories" mPluginName

			-- ɾ������������ű�
			deleteFile (GetDir #startupScripts + "\\oiramExporter.ms")
			
			-- �Ƴ��˵�
			try (menuMan.unRegisterMenu (menuMan.findMenu "oiramExport") ) catch();
			menuMan.updateMenuBar()
			
			-- ����3dsmax
			ShellLaunch (GetDir #maxroot + "3dsmax.exe") ""
			quitMAX #noPrompt
		)
		else
		(
			--Install--
			-- ע��˵�
			if menuMan.registerMenuContext oiramMenuID[1] do
			(
				local mainMenuBar = menuMan.getMainMenuBar()
				local subMenu = menuMan.createMenu "oiramExport"
				local exportItem = menuMan.createActionItem "Export" "oiramExport"
				subMenu.addItem exportItem -1
				local subMenuItem = menuMan.createSubMenuItem "Export" subMenu
				local subMenuIndex = mainMenuBar.numItems() - 1
				mainMenuBar.addItem subMenuItem subMenuIndex
				menuMan.updateMenuBar()
			)
			
			-- ȷ�ϰ�װ, ��Ƕ��ִ��setup.zip(��ʵ�Ǹ�mzp)
			zipPath = mWorkDirectory + "setup.zip"
			extractDir = ""
			msZip.fileInPackage zipPath &extractDir
		)
	)

	on btExit pressed do
	(
		destroydialog oiram_setup_dialog
	)
)


-- ������װ�Ի���
fn oiram_setup_main =
(
	-- ����ͬ��Ŀ¼�µ�bannerͼƬ
	sourcePath = getSourceFileName()
	fromPos = findString sourcePath "setup.ms"
	bannerPath = replace sourcePath fromPos "setup.ms".count "banner.png"
	banner_bitmap = openBitMap bannerPath
	CreateDialog oiram_setup_dialog bitmap:banner_bitmap width:banner_bitmap.width height:banner_bitmap.height modal:true style:#()
)

oiram_setup_main()