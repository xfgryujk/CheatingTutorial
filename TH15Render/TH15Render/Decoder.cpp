#include "stdafx.h"
#include "Decoder.h"


CDecoder::CDecoder(LPCSTR fileName)
{
	int ret;

	// 容器
	ret = avformat_open_input(&m_formatCtx, fileName, NULL, NULL);
	ret = avformat_find_stream_info(m_formatCtx, NULL);
	for (UINT i = 0; i < m_formatCtx->nb_streams; i++)
	{
		if (m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			m_videoStreamIndex = i;
			break;
		}
	}

	// 解码器
	AVCodecParameters* codecpar = m_formatCtx->streams[m_videoStreamIndex]->codecpar;
	m_codec = avcodec_find_decoder(codecpar->codec_id);
	m_codecCtx = avcodec_alloc_context3(m_codec);
	ret = avcodec_open2(m_codecCtx, m_codec, NULL);

	m_videoSize.cx = codecpar->width;
	m_videoSize.cy = codecpar->height;

	// 颜色转换器
	m_swsCtx = sws_getContext(m_videoSize.cx, m_videoSize.cy, (AVPixelFormat)codecpar->format, m_videoSize.cx,
		m_videoSize.cy, AV_PIX_FMT_BGRA, SWS_BICUBIC, NULL, NULL, NULL);

	m_imgData.reset(new BYTE[m_videoSize.cx * m_videoSize.cy * 4]);
}

CDecoder::~CDecoder()
{
	// 停止解码线程
	if (m_decodeThread != nullptr)
	{
		TerminateThread(m_decodeThread->native_handle(), 0); // 去TM死锁
		m_decodeThread->detach();
		m_decodeThread = nullptr;
	}

	sws_freeContext(m_swsCtx);
	avcodec_free_context(&m_codecCtx);
	avformat_close_input(&m_formatCtx);
}

void CDecoder::Run()
{
	if (m_decodeState == DECODE_RUN)
		return;

	// 启动解码线程
	if (m_decodeThread != nullptr)
		m_decodeThread->join();
	m_decodeState = DECODE_RUN;
	m_decodeThread.reset(new std::thread(&CDecoder::DecodeThread, this));
}

void CDecoder::Pause()
{
	if (m_decodeState != DECODE_RUN)
		return;

	// 停止解码线程
	m_decodeState = DECODE_PAUSE;
	if (m_decodeThread != nullptr)
	{
		m_decodeThread->join();
		m_decodeThread = nullptr;
	}
}

void CDecoder::Stop()
{
	if (m_decodeState == DECODE_STOP)
		return;

	// 停止解码线程
	m_decodeState = DECODE_STOP;
	if (m_decodeThread != nullptr)
	{
		m_decodeThread->join();
		m_decodeThread = nullptr;
	}

	av_seek_frame(m_formatCtx, m_videoStreamIndex, 0, 0);
}

void CDecoder::GetVideoSize(SIZE& size)
{
	size = m_videoSize;
}

void CDecoder::SetOnPresent(std::function<void(BYTE*)>&& onPresent)
{
	m_onPresent = std::move(onPresent);
}

void CDecoder::DecodeThread()
{
	AVFrame *frame = av_frame_alloc();
	AVPacket packet;

	while (m_decodeState == DECODE_RUN && av_read_frame(m_formatCtx, &packet) >= 0)
	{
		if (packet.stream_index != m_videoStreamIndex)
			continue;

		// 解码
		avcodec_send_packet(m_codecCtx, &packet);
		if (avcodec_receive_frame(m_codecCtx, frame) != 0)
			continue;

		// 转换颜色格式
		BYTE* pImgData = m_imgData.get();
		int stride = m_codecCtx->width * 4;
		sws_scale(m_swsCtx, frame->data, frame->linesize, 0, m_codecCtx->height, &pImgData, &stride);

		// 呈现
		if (!m_onPresent._Empty())
			m_onPresent(m_imgData.get());

		// 粗略的视频同步
		Sleep(DWORD(((float)m_codecCtx->time_base.num / (float)m_codecCtx->time_base.den) * 1000));
	}
	if (m_decodeState == DECODE_RUN)
		m_decodeState = DECODE_STOP;

	av_packet_unref(&packet);
	av_frame_free(&frame);
}
