// MsgHookDll.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"


extern "C" __declspec(dllexport) // 导出这个函数
LRESULT CALLBACK CallWndProc(int code, WPARAM wParam, LPARAM lParam)
{
	return CallNextHookEx(NULL, code, wParam, lParam);
}
