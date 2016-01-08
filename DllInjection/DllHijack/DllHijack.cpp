// DllHijack.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "DllHijact.h"


Direct3DCreate9Type RealDirect3DCreate9 = NULL;


// 把MyDirect3DCreate9导出为Direct3DCreate9，用__declspec(dllexport)的话实际上导出的是_MyDirect3DCreate9@4
#ifndef _WIN64
#pragma comment(linker, "/EXPORT:Direct3DCreate9=_MyDirect3DCreate9@4")
#else
#pragma comment(linker, "/EXPORT:Direct3DCreate9=MyDirect3DCreate9")
#endif
extern "C" void* WINAPI MyDirect3DCreate9(UINT SDKVersion)
{
	MessageBox(NULL, _T("调用了Direct3DCreate9"), _T(""), MB_OK);
	return RealDirect3DCreate9(SDKVersion);
}
