// RemoteThread.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"


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

// 注入DLL，返回模块句柄（64位程序只能返回低32位）
DWORD InjectDll(HANDLE process, LPCTSTR dllPath)
{
	DWORD dllPathSize = ((DWORD)_tcslen(dllPath) + 1) * sizeof(TCHAR);

	// 申请内存用来存放DLL路径
	void* remoteMemory = VirtualAllocEx(process, NULL, dllPathSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (remoteMemory == NULL)
	{
		printf("申请内存失败，错误代码：%u\n", GetLastError());
		return 0;
	}

	// 写入DLL路径
	if (!WriteProcessMemory(process, remoteMemory, dllPath, dllPathSize, NULL))
	{
		printf("写入内存失败，错误代码：%u\n", GetLastError());
		return 0;
	}

	// 创建远线程调用LoadLibrary
	HANDLE remoteThread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibrary, remoteMemory, 0, NULL);
	if (remoteThread == NULL)
	{
		printf("创建远线程失败，错误代码：%u\n", GetLastError());
		return 0;
	}
	// 等待远线程结束
	WaitForSingleObject(remoteThread, INFINITE);
	// 取DLL在目标进程的句柄
	DWORD remoteModule;
	GetExitCodeThread(remoteThread, &remoteModule);

	// 释放
	CloseHandle(remoteThread);
	VirtualFreeEx(process, remoteMemory, dllPathSize, MEM_DECOMMIT);

	return remoteModule;
}

// 卸载DLL，只能用于32位程序（因为CreateRemoteThread不能传64位地址），对64位程序只能注入机器码然后远线程执行了吧...
BOOL FreeRemoteDll(HANDLE process, DWORD remoteModule)
{
	// 创建远线程调用FreeLibrary
	HANDLE remoteThread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)FreeLibrary, (LPVOID)remoteModule, 0, NULL);
	if (remoteThread == NULL)
	{
		printf("创建远线程失败，错误代码：%u\n", GetLastError());
		return FALSE;
	}
	// 等待远线程结束
	WaitForSingleObject(remoteThread, INFINITE);
	// 取返回值
	BOOL result;
	GetExitCodeThread(remoteThread, (LPDWORD)&result);

	// 释放
	CloseHandle(remoteThread);
	return result;
}

int _tmain(int argc, _TCHAR* argv[])
{
	// 提升权限
	EnablePrivilege(TRUE);

	// 打开进程
	HWND hwnd = FindWindow(NULL, _T("ARPTest"));
	DWORD pid;
	GetWindowThreadProcessId(hwnd, &pid);
	HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (process == NULL)
	{
		printf("打开进程失败，错误代码：%u\n", GetLastError());
		return 1;
	}
	
	// 要将RemoteThreadDll.dll放在本程序当前目录下
	TCHAR dllPath[MAX_PATH]; // 要用绝对路径
	GetCurrentDirectory(_countof(dllPath), dllPath);
	_tcscat_s(dllPath, _T("\\RemoteThreadDll.dll"));


	// 注入DLL
	DWORD remoteModule = InjectDll(process, dllPath);
	if (remoteModule == 0)
	{
		CloseHandle(process);
		return 2;
	}
	printf("模块句柄：0x%X\n", remoteModule);

	// 暂停
	printf("按回车卸载DLL\n");
	getchar();

	// 卸载DLL
	if (!FreeRemoteDll(process, remoteModule))
	{
		CloseHandle(process);
		return 3;
	}


	// 关闭进程
	CloseHandle(process);

	return 0;
}

