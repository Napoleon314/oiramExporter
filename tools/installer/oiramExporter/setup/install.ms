fn oiram_install plugin_name =
(
	pluginPath = GetDir #maxroot + "plugins\\" + plugin_name
	-- ������·���Ѵ��ڵĻ�, �����������
	if doesFileExist pluginPath do
	(
		pluginBackupPath = pluginPath + timeStamp() as string
		renameFile pluginPath pluginBackupPath
	)
	
	-- �õ�plugin.ini��·��
	pluginIniFile = (GetDir #maxData)
	
	-- plugin.ini��Max2010��ʼ����ΪPlugin.UserSettings.ini
	if ((maxVersion())[1]) > 11000 then
		pluginIniFile += "Plugin.UserSettings.ini"
	else
		pluginIniFile += "plugin.ini"

	-- �����·�����ӵ�plugin.ini��
	setINISetting pluginIniFile "Directories" plugin_name pluginPath
	
	-- ����װ·������Ϊִ��·��
	renameFile (GetDir #maxroot + "plugins\\module") pluginPath

	-- ����3dsmax
	ShellLaunch (GetDir #maxroot + "3dsmax.exe") ""
	quitMAX #noPrompt
)

oiram_install "oiramExporter"