// MsgHook.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <tlhelp32.h>


// 提升进程特权，否则某些操作会失败
BOOL EnablePrivilege(BOOL enable)
{
	// 得到令牌句柄
	HANDLE hToken = NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY | TOKEN_READ, &hToken))
		return FALSE;

	// 得到特权值
	LUID luid;
	if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid))
		return FALSE;

	// 提升令牌句柄权限
	TOKEN_PRIVILEGES tp = {};
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;
	if (!AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL))
		return FALSE;

	// 关闭令牌句柄
	CloseHandle(hToken);

	return TRUE;
}

DWORD GetProcessThreadID(DWORD pid)
{
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, pid);
	THREADENTRY32 threadentry;
	threadentry.dwSize = sizeof(threadentry);

	BOOL hasNext = Thread32First(snapshot, &threadentry);
	DWORD threadID = 0;
	do
	{
		if (threadentry.th32OwnerProcessID == pid)
		{
			threadID = threadentry.th32ThreadID;
			break;
		}
		hasNext = Thread32Next(snapshot, &threadentry);
	} while (hasNext);

	CloseHandle(snapshot);
	return threadID;
}

// 注入DLL，返回钩子句柄，DLL必须导出CallWndProc钩子过程，pid = 0则安装全局钩子
HHOOK InjectDll(DWORD pid, LPCTSTR dllPath)
{
	// 载入DLL
	HMODULE module = LoadLibrary(dllPath);
	if (module == NULL)
	{
		printf("载入DLL失败，错误代码：%u\n", GetLastError());
		return NULL;
	}
	// 取钩子过程地址
	HOOKPROC proc = (HOOKPROC)GetProcAddress(module, "CallWndProc");
	if (proc == NULL)
	{
		printf("取钩子过程地址失败，错误代码：%u\n", GetLastError());
		return NULL;
	}

	// 取线程ID
	DWORD threadID = 0;
	if (pid != 0)
	{
		threadID = GetProcessThreadID(pid);
		if (threadID == 0)
		{
			printf("取线程ID失败\n");
			return NULL;
		}
	}

	// 安装钩子
	HHOOK hook = SetWindowsHookEx(WH_GETMESSAGE, proc, module, threadID);

	// 释放
	FreeLibrary(module);

	return hook;
}

int _tmain(int argc, _TCHAR* argv[])
{
	// 提升权限（其实不提升也可以，以管理员身份运行就行）
	EnablePrivilege(TRUE);

	// 取PID
	//DWORD pid = 0; // 全局钩子，少玩全局钩子不然会出问题...
	HWND hwnd = FindWindow(NULL, _T("任务管理器"));
	if (hwnd == NULL)
	{
		printf("寻找窗口失败，错误代码：%u\n", GetLastError());
		return 1;
	}
	DWORD pid;
	GetWindowThreadProcessId(hwnd, &pid);

	// 注入DLL
	// 要将MsgHookDll.dll放在本程序当前目录下
	HHOOK hook = InjectDll(pid, _T("MsgHookDll.dll"));
	if (hook == NULL)
		return 2;

	// 暂停
	printf("按回车卸载DLL\n");
	getchar();

	// 卸载DLL
	UnhookWindowsHookEx(hook);

	return 0;
}

