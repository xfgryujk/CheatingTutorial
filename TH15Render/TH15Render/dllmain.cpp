// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"
#include <d3d9.h>
#include "Decoder.h"
#include <mutex>


BOOL hookVTable(void* pInterface, int index, void* hookFunction, void* oldAddress)
{
	DWORD* address = &(*(DWORD**)pInterface)[index];
	if (address == NULL)
		return FALSE;

	// 保存原函数地址
	if (oldAddress != NULL)
		*(DWORD*)oldAddress = *address;

	// 修改虚函数表中地址为hookFunction
	DWORD oldProtect, oldProtect2;
	VirtualProtect(address, sizeof(DWORD), PAGE_READWRITE, &oldProtect);
	*address = (DWORD)hookFunction;
	VirtualProtect(address, sizeof(DWORD), oldProtect, &oldProtect2);

	return TRUE;
}

BOOL unhookVTable(void* pInterface, int index, void* oldAddress)
{
	// 修改回原函数地址
	return hookVTable(pInterface, index, oldAddress, NULL);
}


typedef HRESULT(STDMETHODCALLTYPE* DrawPrimitiveUPType)(IDirect3DDevice9* thiz, D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride);
DrawPrimitiveUPType RealDrawPrimitiveUP = NULL;

// 通过调试得到的TH15中device指针地址
IDirect3DDevice9** g_ppDevice = (IDirect3DDevice9**)0x4E77D8;
IDirect3DDevice9* g_pDevice = *g_ppDevice;

// 东方用的顶点结构
struct THVertex
{
	FLOAT    x, y, z;
	D3DCOLOR specular, diffuse;
	FLOAT    tu, tv;

	void Set(FLOAT x_, FLOAT y_, FLOAT tu_, FLOAT tv_)
	{
		x = x_;
		y = y_;
		tu = tu_;
		tv = tv_;
	}
};

// 主窗口
HWND g_mainWnd = NULL;
WNDPROC g_realWndProc = NULL;
const UINT WM_DLL_INIT = WM_APP;
const UINT WM_UPDATE_TEXTURE = WM_APP + 1;

// 视频解码器
CDecoder* g_decoder = NULL;
SIZE g_videoSize;
SIZE g_scaledSize;
// 视频纹理
IDirect3DTexture9* g_texture = NULL;
BYTE* g_frameBuffer = NULL;
std::mutex g_frameBufferLock;


HRESULT STDMETHODCALLTYPE MyDrawPrimitiveUP(DWORD esp, IDirect3DDevice9* thiz, D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	// 判断是不是在渲染自机
	if (*(DWORD*)(esp + 0x9C) == 14			// 灵梦
		|| *(DWORD*)(esp + 0x88) == 14		// 其他角色按住shift
		)									// 其他情况我就不知道怎么判断了...
	{
		// 自己的渲染

		// 设置纹理坐标
		static THVertex vertex[6];
		memcpy(vertex, pVertexStreamZeroData, sizeof(vertex));
		float x = (vertex[0].x + vertex[5].x) / 2, y = (vertex[0].y + vertex[5].y) / 2; // 中点
		vertex[0].Set(x - g_scaledSize.cx / 2, y - g_scaledSize.cy / 2, 0, 0); // 左上
		vertex[1].Set(x + g_scaledSize.cx / 2, y - g_scaledSize.cy / 2, 1, 0); // 右上
		vertex[2].Set(x - g_scaledSize.cx / 2, y + g_scaledSize.cy / 2, 0, 1); // 左下
		vertex[3].Set(x + g_scaledSize.cx / 2, y - g_scaledSize.cy / 2, 1, 0); // 右上
		vertex[4].Set(x - g_scaledSize.cx / 2, y + g_scaledSize.cy / 2, 0, 1); // 左下
		vertex[5].Set(x + g_scaledSize.cx / 2, y + g_scaledSize.cy / 2, 1, 1); // 右下

		// 设置纹理
		IDirect3DBaseTexture9* oldTexture = NULL;
		thiz->GetTexture(0, &oldTexture);
		thiz->SetTexture(0, g_texture);

		// 渲染
		HRESULT hr = RealDrawPrimitiveUP(thiz, PrimitiveType, PrimitiveCount, vertex, VertexStreamZeroStride);

		// 恢复
		thiz->SetTexture(0, oldTexture);
		return hr;
	}
	return RealDrawPrimitiveUP(thiz, PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
}

__declspec(naked) // 不让编译器自动加代码破坏栈
HRESULT STDMETHODCALLTYPE MyDrawPrimitiveUPWrapper(IDirect3DDevice9* thiz, D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	__asm
	{
		pop ecx  // 返回地址出栈
		push esp // 此时[esp] = thiz
		push ecx // 恢复返回地址
		jmp MyDrawPrimitiveUP
	}
}


// 把解码出来的RGB数据拷贝到g_frameBuffer
void OnPresent(BYTE* data)
{
	g_frameBufferLock.lock();
	memcpy(g_frameBuffer, data, g_videoSize.cx * g_videoSize.cy * 4);
	g_frameBufferLock.unlock();
	PostMessage(g_mainWnd, WM_UPDATE_TEXTURE, 0, 0);
}

LRESULT CALLBACK MyWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (Msg == WM_DLL_INIT) // 在主线程的初始化
	{
		// 初始化FFmpeg解码器
		av_register_all();

		// 创建解码器
		g_decoder = new CDecoder("E:\\Bad Apple.avi");
		g_decoder->SetOnPresent(std::function<void(BYTE*)>(OnPresent));

		// 创建纹理
		g_decoder->GetVideoSize(g_videoSize);
		g_frameBuffer = new BYTE[g_videoSize.cx * g_videoSize.cy * 4];
		D3DDISPLAYMODE dm;
		g_pDevice->GetDisplayMode(NULL, &dm);
		g_pDevice->CreateTexture(g_videoSize.cx, g_videoSize.cy, 1, D3DUSAGE_DYNAMIC, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &g_texture, NULL);

		float scale1 = 100.0f / g_videoSize.cx;
		float scale2 = 100.0f / g_videoSize.cy;
		float scale = scale1 < scale2 ? scale1 : scale2;
		g_scaledSize.cx = LONG(g_videoSize.cx * scale1);
		g_scaledSize.cy = LONG(g_videoSize.cy * scale1);

		// 开始播放
		g_decoder->Run();

		// hook D3D渲染
		hookVTable(g_pDevice, 83, MyDrawPrimitiveUPWrapper, &RealDrawPrimitiveUP);
		return 0;
	}
	else if (Msg == WM_UPDATE_TEXTURE) // 更新纹理
	{
		// D3D9不是线程安全的，这里利用了处理消息时不会渲染
		D3DLOCKED_RECT rect;
		g_texture->LockRect(0, &rect, NULL, 0);
		g_frameBufferLock.lock();
		for (int y = 0; y < g_videoSize.cy; y++)
			memcpy((BYTE*)rect.pBits + y * rect.Pitch, g_frameBuffer + y * g_videoSize.cx * 4, g_videoSize.cx * 4);
		g_frameBufferLock.unlock();
		g_texture->UnlockRect(0);
		return 0;
	}

	return CallWindowProc(g_realWndProc, hWnd, Msg, wParam, lParam);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		// 子类化窗口，拦截窗口消息
		g_mainWnd = FindWindow(_T("BASE"), _T("搶曽嵁庫揱丂乣 Legacy of Lunatic Kingdom. ver 1.00b")); // 日文编码就是这样...
		g_realWndProc = (WNDPROC)SetWindowLongPtr(g_mainWnd, GWLP_WNDPROC, (ULONG_PTR)MyWndProc);

		// 接下来的初始化在主线程完成
		PostMessage(g_mainWnd, WM_DLL_INIT, 0, 0);
		break;

	case DLL_PROCESS_DETACH:
		// 恢复D3D hook
		if (*g_ppDevice != NULL && RealDrawPrimitiveUP != NULL)
			unhookVTable(g_pDevice, 83, RealDrawPrimitiveUP);

		// 恢复窗口过程
		if (IsWindow(g_mainWnd))
			SetWindowLongPtr(g_mainWnd, GWLP_WNDPROC, (ULONG_PTR)g_realWndProc);

		// 释放
		if (g_decoder != NULL)
			delete g_decoder;
		if (g_texture != NULL)
			g_texture->Release();
		if (g_frameBuffer != NULL)
			delete g_frameBuffer;
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}

