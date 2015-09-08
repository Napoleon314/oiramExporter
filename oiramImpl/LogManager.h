#ifndef _LogManager_hpp__
#define _LogManager_hpp__

#include <string>
#include <Windows.h>

// ��־��Ϣ
class LogManager
{
public:
	static LogManager& getSingleton();

	~LogManager();
	
	void initialize(const std::string& logFileName, HWND hDlg);
	void uninitialize();
	
	void logMessage(bool critical, const char* format, ...);
	void setProgress(int progress);

private:
	LogManager() {}
	
private:
	HWND			mDialog;		// �Ի�����
	std::string		mLogFileName;	// �ļ���
	unsigned long	mUsedTime;		// ���ĵ�ʱ��
};

#endif
