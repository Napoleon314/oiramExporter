#include "stdafx.h"
#include "LogManager.h"
#include <windows.h>
#include <stdarg.h>
#include <assert.h>

LogManager& LogManager::
getSingleton()
{
	static LogManager msSingleton;
	return msSingleton;
}


LogManager::
~LogManager()
{
}


void LogManager::
initialize(const std::string& logFileName, HWND hDlg)
{
	mLogFileName = logFileName;
	// ����ļ�
	FILE* fp = fopen(mLogFileName.c_str(), "wt");
	if (fp)
		fclose(fp);
	
	mUsedTime = GetTickCount();
	mDialog = hDlg;
}


void LogManager::
uninitialize()
{
	// ��ʾ��ɲ���ӡʹ�õ�ʱ��
	mUsedTime = GetTickCount() - mUsedTime;
	logMessage(false, "Done, time used: %.2f seconds.", mUsedTime * 0.001f);

	// ����������ʾ��
	MessageBeep(MB_ICONASTERISK);
}


void LogManager::
logMessage(bool critical, const char* format, ...)
{
	assert(format);

	char str[512] = {0};
	va_list ap;
	va_start(ap, format);
	vsprintf(str, format, ap);
	va_end(ap);

	FILE* fp = fopen(mLogFileName.c_str(), "a");
	if (fp)
	{
		fwrite(str, strlen(str), 1, fp);
		fputc('\n', fp);
		fclose(fp);
	}

	SendMessage(mDialog, WM_USER + 1, critical ? TRUE : FALSE, LPARAM(str));
}


void LogManager::
setProgress(int progress)
{
	SendMessage(mDialog, WM_USER + 2, progress, 0);
}
