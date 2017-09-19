// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"
#include <d3d9.h>
#include "Decoder.h"
#include <mutex>


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

BOOL hookVTable(void* pInterface, int index, void* hookFunction, void** oldAddress)
{
	void** address = &(*(void***)pInterface)[index];
	if (address == NULL)
		return FALSE;

	// 保存原函数地址
	if (oldAddress != NULL)
		*oldAddress = *address;

	// 修改虚函数表中地址为hookFunction
	DWORD oldProtect, oldProtect2;
	VirtualProtect(address, sizeof(DWORD), PAGE_READWRITE, &oldProtect);
	*address = hookFunction;
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
IDirect3DDevice9*& g_device = *(IDirect3DDevice9**)0x4E77D8;

// 其实调用约定是__thiscall，不过用__fastcall也可以使第一个参数在ecx寄存器
int(__fastcall* const RenderPlayer)(void*) = (int(__fastcall*)(void*))0x4872F0;
BYTE renderPlayerOldCode[sizeof(JmpCode)];
// 自从调用RenderPlayer这个函数后渲染了几次
int g_renderCount = 999;

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

// 视频解码器
CDecoder* g_decoder = NULL;
SIZE g_videoSize;
SIZE g_scaledSize;
// 视频纹理
IDirect3DTexture9* g_texture = NULL;
BYTE* g_frameBuffer = NULL;
bool g_textureNeedUpdate = FALSE;
std::mutex g_frameBufferLock;


int __fastcall MyRenderPlayer(void* thiz)
{
	g_renderCount = 0;
	unhook(RenderPlayer, renderPlayerOldCode);
	int result = ((int(__fastcall*)(void*))0x4872F0)(thiz);
	hook(RenderPlayer, MyRenderPlayer, renderPlayerOldCode);
	return result;
}

HRESULT STDMETHODCALLTYPE MyDrawPrimitiveUP(IDirect3DDevice9* thiz, D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	if (g_texture == NULL)
	{
		// 初始化
		g_device->CreateTexture(g_videoSize.cx, g_videoSize.cy, 1, D3DUSAGE_DYNAMIC, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &g_texture, NULL);
	}

	// 判断是不是在渲染自机
	if (++g_renderCount == 1)
	{
		// 自己的渲染

		// 更新纹理
		if (g_textureNeedUpdate)
		{
			D3DLOCKED_RECT rect;
			g_texture->LockRect(0, &rect, NULL, 0);
			g_frameBufferLock.lock();
			for (int y = 0; y < g_videoSize.cy; y++)
				memcpy((BYTE*)rect.pBits + y * rect.Pitch, g_frameBuffer + y * g_videoSize.cx * 4, g_videoSize.cx * 4);
			g_textureNeedUpdate = false;
			g_frameBufferLock.unlock();
			g_texture->UnlockRect(0);
		}

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

// 把解码出来的RGB数据拷贝到g_frameBuffer
void OnPresent(BYTE* data)
{
	g_frameBufferLock.lock();
	memcpy(g_frameBuffer, data, g_videoSize.cx * g_videoSize.cy * 4);
	g_textureNeedUpdate = true;
	g_frameBufferLock.unlock();
}

DWORD WINAPI initThread(LPVOID)
{
	// 初始化FFmpeg解码器
	av_register_all();

	// 创建解码器
	g_decoder = new CDecoder("E:\\Bad Apple.avi");
	g_decoder->SetOnPresent(std::function<void(BYTE*)>(OnPresent));

	// 创建纹理数据缓冲
	g_decoder->GetVideoSize(g_videoSize);
	g_frameBuffer = new BYTE[g_videoSize.cx * g_videoSize.cy * 4];

	float scale1 = 100.0f / g_videoSize.cx;
	float scale2 = 100.0f / g_videoSize.cy;
	float scale = scale1 < scale2 ? scale1 : scale2;
	g_scaledSize.cx = LONG(g_videoSize.cx * scale1);
	g_scaledSize.cy = LONG(g_videoSize.cy * scale1);

	// 开始播放
	g_decoder->Run();

	// hook
	hookVTable(g_device, 83, MyDrawPrimitiveUP, (void**)&RealDrawPrimitiveUP);
	hook(RenderPlayer, MyRenderPlayer, renderPlayerOldCode);

	return 0;
}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		// 在另一个线程初始化，避免死锁
		CloseHandle(CreateThread(NULL, 0, initThread, NULL, 0, NULL));
		break;

	case DLL_PROCESS_DETACH:
		// 恢复hook
		unhook(RenderPlayer, renderPlayerOldCode);
		if (g_device != NULL && RealDrawPrimitiveUP != NULL)
			unhookVTable(g_device, 83, RealDrawPrimitiveUP);

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

