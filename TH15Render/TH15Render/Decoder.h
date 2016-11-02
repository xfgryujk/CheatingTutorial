#pragma once
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
};
#include <thread>
#include <memory>
#include <functional>


class CDecoder
{
public:
	CDecoder(LPCSTR fileName);
	virtual ~CDecoder();

	virtual void Run();
	virtual void Pause();
	virtual void Stop();

	virtual void GetVideoSize(SIZE& size);

	// 设置需要呈现时的回调函数
	virtual void SetOnPresent(std::function<void(BYTE*)>&& onPresent);

protected:
	AVFormatContext* m_formatCtx = NULL;
	int m_videoStreamIndex = -1;
	AVCodecContext* m_codecCtx = NULL;
	AVCodec* m_codec = NULL;
	SwsContext* m_swsCtx = NULL;

	enum DecodeState{ DECODE_RUN, DECODE_PAUSE, DECODE_STOP };
	DecodeState m_decodeState = DECODE_STOP;
	std::unique_ptr<std::thread> m_decodeThread;
	std::unique_ptr<BYTE[]> m_imgData;

	SIZE m_videoSize;
	// 需要呈现时被调用
	std::function<void(BYTE*)> m_onPresent;

	void DecodeThread();
};
