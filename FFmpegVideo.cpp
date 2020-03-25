#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")

static inline double r2d(AVRational r) {
	return r.den == 0 ? 0 : (double)r.num / (double)r.den;
}

const char* path = "43811.mp4";
int main()
{
	std::cout << AV_TIME_BASE << std::endl;
	// 初始化封装库
	av_register_all();
	// 初始化网络库(可以打开rtsp rtmp http协议的流媒体)
	avformat_network_init();
	// 参数设置
	AVDictionary* options = NULL;
	// 设置rtsp流以tcp协议打开
	av_dict_set(&options, "rtsp_transport", "tcp", 0);
	// 网路延时时间
	av_dict_set(&options, "max_delay", "500", 0);
	// 解封装上下文
	AVFormatContext* ic = NULL;
	int ret = avformat_open_input(&ic,
		path,
		0,  // 0表示自动选择解封器
		&options //设置参数
	);
	if (ret != 0) {
		char buf[1024] = { 0 };
		av_strerror(ret, buf, sizeof(buf) - 1);
		std::cout << "open " << path << " failed :" << buf << std::endl;
		return -1;
	}
	std::cout << "open " << path << " success" << std::endl;
	int total = ic->duration / (AV_TIME_BASE / 1000); //ms
	std::cout << "totalMs = " << total << std::endl;
	int videoStream = 0;
	int audioStream = 1;
	for (int i = 0; i < ic->nb_streams; i++) {
		AVStream* stream = ic->streams[i];
		if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			audioStream = i;
			std::cout << "音频信息" << std::endl;
			std::cout << "sample_rate = " << stream->codecpar->sample_rate << std::endl;
			std::cout << "sample_format = " << stream->codecpar->format << std::endl;
			std::cout << "channels = " << stream->codecpar->channels << std::endl;
			std::cout << "audio_codec_id = " << stream->codecpar->codec_id << std::endl;
			// 一帧数据？？ (单通道)一定量的样本数
			std::cout << "frame_size = " << stream->codecpar->frame_size << std::endl;
		}
		else if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			std::cout << "视频信息" << std::endl;
			std::cout << "width = " << stream->codecpar->width << std::endl; //不一定可靠，最好从avframe
			std::cout << "height = " << stream->codecpar->height << std::endl;
			std::cout << "video_codec_id = " << stream->codecpar->codec_id << std::endl;
			// 帧率 fps 分数转换
			std::cout << "video_fps = " << r2d(stream->avg_frame_rate) << std::endl;
		}
	}
	// malloc AVPacket并初始化
	AVPacket* pkt = av_packet_alloc();
	for (;;) {
		int ret = av_read_frame(ic, pkt);
		if (ret != 0) {
			// 循环播放
			int ms = 3000; // 3s位置
			long long pos = (double)ms / (double)1000 * pkt->pts * (r2d(ic->streams[pkt->stream_index]->time_base) * 1000);
			av_seek_frame(ic, videoStream, pos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
			break;
		}
		std::cout << pkt->size << std::endl;
		// 显示的时间
		std::cout << "pkt->pts = " << pkt->pts << std::endl; 
		// 转换为毫秒, 方便做同步
		std::cout << "pkt->pts ms = " << pkt->pts * (r2d(ic->streams[pkt->stream_index]->time_base) * 1000) << std::endl;
		// 解码时间
		std::cout << "pkt-dts = " << pkt->dts << std::endl;

		if (pkt->stream_index == videoStream) {
			std::cout << "图像" << std::endl;
		}
		if (pkt->stream_index == audioStream) {
			std::cout << "音频" << std::endl;
		}
		// 释放，引用计数减1 为0释放空间
		av_packet_unref(pkt);
	}
	av_packet_free(&pkt);

	

	// 打印视频流详细信息
	if (ic != NULL) {
		avformat_close_input(&ic);
		ic = NULL;
	}
	return 0;
}
