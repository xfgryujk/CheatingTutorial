// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"
#include <d3dx9.h>


#pragma pack(push)
#pragma pack(1)
#ifndef _WIN64
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
#else
struct JmpCode
{
private:
	BYTE jmp[6];
	uintptr_t address;

public:
	JmpCode(uintptr_t srcAddr, uintptr_t dstAddr)
	{
		static const BYTE JMP[] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
		memcpy(jmp, JMP, sizeof(jmp));
		setAddress(srcAddr, dstAddr);
	}

	void setAddress(uintptr_t srcAddr, uintptr_t dstAddr)
	{
		address = dstAddr;
	}
};
#endif
#pragma pack(pop)

void hook(void* originalFunction, void* hookFunction, BYTE* oldCode)
{
	JmpCode code((uintptr_t)originalFunction, (uintptr_t)hookFunction);
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


void* endSceneAddr = NULL;
BYTE endSceneOldCode[sizeof(JmpCode)];

ID3DXFont* g_font = NULL;


HRESULT STDMETHODCALLTYPE MyEndScene(IDirect3DDevice9* thiz)
{
	if (g_font == NULL)
	{
		// 初始化
		D3DXFONT_DESC d3dFont = {};
		d3dFont.Height = 25;
		d3dFont.Width = 12;
		d3dFont.Weight = 500;
		d3dFont.Italic = FALSE;
		d3dFont.CharSet = DEFAULT_CHARSET;
		wcscpy_s(d3dFont.FaceName, L"Times New Roman");
		D3DXCreateFontIndirect(thiz, &d3dFont, &g_font);
	}

	static RECT rect = { 0, 0, 200, 200 };
	g_font->DrawText(NULL, _T("Hello World"), -1, &rect, DT_TOP | DT_LEFT, D3DCOLOR_XRGB(255, 0, 0));

	unhook(endSceneAddr, endSceneOldCode);
	HRESULT hr = thiz->EndScene();
	hook(endSceneAddr, MyEndScene, endSceneOldCode);
	return hr;
}

DWORD WINAPI initHookThread(LPVOID dllMainThread)
{
	// 等待DllMain（LoadLibrary线程）结束
	WaitForSingleObject(dllMainThread, INFINITE);
	CloseHandle(dllMainThread);

	// 创建一个窗口用于初始化D3D

	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(wc);
	wc.style = CS_OWNDC;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpfnWndProc = DefWindowProc;
	wc.lpszClassName = _T("DummyWindow");
	if (RegisterClassEx(&wc) == 0)
	{
		MessageBox(NULL, _T("注册窗口类失败"), _T(""), MB_OK);
		return 0;
	}

	HWND hwnd = CreateWindowEx(0, wc.lpszClassName, _T(""), WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, NULL, NULL, wc.hInstance, NULL);
	if (hwnd == NULL)
	{
		MessageBox(NULL, _T("创建窗口失败"), _T(""), MB_OK);
		return 0;
	}

	// 初始化D3D

	IDirect3D9* d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
	if (d3d9 == NULL)
	{
		MessageBox(NULL, _T("创建D3D失败"), _T(""), MB_OK);
		DestroyWindow(hwnd);
		return 0;
	}

	D3DPRESENT_PARAMETERS pp = {};
	pp.Windowed = TRUE;
	pp.SwapEffect = D3DSWAPEFFECT_COPY;

	IDirect3DDevice9* device;
	if (FAILED(d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, 
		D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device)))
	{
		MessageBox(NULL, _T("创建设备失败"), _T(""), MB_OK);
		d3d9->Release();
		DestroyWindow(hwnd);
		return 0;
	}

	// hook EndScene
	endSceneAddr = (*(void***)device)[42]; // EndScene是IDirect3DDevice9第43个函数
	hook(endSceneAddr, MyEndScene, endSceneOldCode);

	// 释放
	d3d9->Release();
	device->Release();
	DestroyWindow(hwnd);
	return 0;
}


BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
	)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		// 取当前线程句柄
		HANDLE curThread;
		if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &curThread, SYNCHRONIZE, FALSE, 0))
			return FALSE;
		// DllMain中不能使用COM组件，所以要在另一个线程初始化
		CloseHandle(CreateThread(NULL, 0, initHookThread, curThread, 0, NULL));
		break;

	case DLL_PROCESS_DETACH:
		if (endSceneAddr != NULL)
			unhook(endSceneAddr, endSceneOldCode);
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}

