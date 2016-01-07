// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"


#pragma pack(push)
#pragma pack(1)
struct JmpCode
{
private:
	const BYTE jmp;
	DWORD address;

public:
	JmpCode(DWORD srcAddr, DWORD dstAddr)
		: jmp(0xE9)
	{
		setAddress(srcAddr, dstAddr);
	}

	void setAddress(DWORD srcAddr, DWORD dstAddr)
	{
		address = dstAddr - srcAddr - sizeof(JmpCode);
	}
};
#pragma pack(pop)

void hook(void* originalFunction, void* hookFunction, BYTE* oldCode)
{
	JmpCode code((DWORD)originalFunction, (DWORD)hookFunction);
	DWORD oldProtect, oldProtect2;
	VirtualProtect(originalFunction, sizeof(code), PAGE_EXECUTE_READWRITE, &oldProtect);
	memcpy(oldCode, originalFunction, sizeof(code));
	memcpy(originalFunction, &code, sizeof(code));
	VirtualProtect(originalFunction, sizeof(code), oldProtect, &oldProtect2);
}

void unhook(void* originalFunction, BYTE* oldCode)
{
	DWORD oldProtect, oldProtect2;
	VirtualProtect(originalFunction, sizeof(JmpCode), PAGE_EXECUTE_READWRITE, &oldProtect);
	memcpy(originalFunction, oldCode, sizeof(JmpCode));
	VirtualProtect(originalFunction, sizeof(JmpCode), oldProtect, &oldProtect2);
}


BYTE terminateProcessOldCode[sizeof(JmpCode)];

BOOL WINAPI MyTerminateProcess(HANDLE hProcess, UINT uExitCode)
{
	return FALSE; // 禁止结束进程
}


// DLL被加载、卸载时调用
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	TCHAR processPath[MAX_PATH];
	TCHAR msg[MAX_PATH + 20];

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		GetModuleFileName(GetModuleHandle(NULL), processPath, MAX_PATH);
		_tcscpy_s(msg, _T("注入了进程 "));
		_tcscat_s(msg, processPath);
		MessageBox(NULL, msg, _T(""), MB_OK);

		hook(GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "TerminateProcess"), MyTerminateProcess, terminateProcessOldCode);
		break;

	case DLL_PROCESS_DETACH:
		MessageBox(NULL, _T("DLL卸载中"), _T(""), MB_OK);

		// 卸载时记得unhook
		unhook(GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "TerminateProcess"), terminateProcessOldCode);
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}

