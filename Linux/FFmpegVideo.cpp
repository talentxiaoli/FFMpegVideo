#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "swscale.lib")

static inline double r2d(AVRational r) {
	return r.den == 0 ? 0 : (double)r.num / (double)r.den;
}

const char* path = "test.avi";
int main()
{
	std::cout << AV_TIME_BASE << std::endl;
	// 初始化封装库
	av_register_all();
	// 初始化网络库(可以打开rtsp rtmp http协议的流媒体)
	avformat_network_init();
	// 注册编解码器
	avcodec_register_all();
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
	std::cout << "open input " << path << " success" << std::endl;
	int total = ic->duration / (AV_TIME_BASE / 1000); //ms
	std::cout << "totalMs = " << total << std::endl << std::endl;
	int videoStream = 0;
	int audioStream = 1;
	for (int i = 0; i < ic->nb_streams; i++) {
		AVStream* stream = ic->streams[i];
		if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			audioStream = i;
			std::cout << "音频信息" << std::endl;
			std::cout << "sample_rate = " << stream->codecpar->sample_rate << std::endl;
			std::cout << "sample_format = " << stream->codecpar->format << std::endl;
			std::cout << "audio_codec_id = " << stream->codecpar->codec_id << std::endl;
			// 一帧数据？？ (单通道)一定量的样本数
			std::cout << "audio_frame_size = " << stream->codecpar->frame_size << std::endl;
			std::cout << "channels = " << stream->codecpar->channels << std::endl;
			std::cout << std::endl;
		}
		else if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			std::cout << "视频信息" << std::endl;
			std::cout << "width = " << stream->codecpar->width << std::endl; //不一定可靠，最好从avframe
			std::cout << "height = " << stream->codecpar->height << std::endl;
			std::cout << "video_codec_id = " << stream->codecpar->codec_id << std::endl;
			std::cout << "video_frame_size = " << stream->codecpar->frame_size << std::endl;
			// 帧率 fps 分数转换
			std::cout << "video_fps = " << r2d(stream->avg_frame_rate) << std::endl;
			std::cout << std::endl;
		}
	}

	// 找到音视频解码器
	AVCodec* vcodec = avcodec_find_decoder(ic->streams[videoStream]->codec->codec_id);
	if (!vcodec) {
		std::cout << "can not find the vcodec id" << std::endl;
		return -1;
	}
	AVCodec* acodec = avcodec_find_decoder(ic->streams[audioStream]->codec->codec_id);
	if (!acodec) {
		std::cout << "can not find the acodec id" << std::endl;
	}
	// 创建音视频解码器上下文
	AVCodecContext* vc = avcodec_alloc_context3(vcodec);
	AVCodecContext* ac = avcodec_alloc_context3(acodec);
	// 配置音视频解码器上下文参数
	avcodec_parameters_to_context(vc, ic->streams[videoStream]->codecpar);
	avcodec_parameters_to_context(ac, ic->streams[audioStream]->codecpar);
	vc->thread_count = 8;
	ac->thread_count = 8;

	// 打开音视频解码器上下文
	int vret = avcodec_open2(vc, 0, 0);
	if (vret != 0) {
		char buf[1024] = { 0 };
		av_strerror(vret, buf, sizeof(buf) - 1);
		std::cout << "avcodec_open2 open video  failed! :" << buf << std::endl;
		return -1;
	}
	std::cout << "video avcodec_open2 success" << std::endl;
	int aret = avcodec_open2(ac, 0, 0);
	if (aret != 0) {
		char buf[1024] = { 0 };
		av_strerror(aret, buf, sizeof(buf) - 1);
		std::cout << "avcodec_open2 open audio failed! :" << buf << std::endl;
		return -1;
	}
	std::cout << "audio avcodec_open2 success" << std::endl;

	// malloc AVPacket并初始化
	AVPacket* pkt = av_packet_alloc();
	// malloc AVFrame并初始化
	AVFrame* frame = av_frame_alloc();
	// 像素格式和尺寸转换上下文
	SwsContext* vctx = NULL;
	unsigned char* rgb = NULL;
	for (;;) {
		int ret = av_read_frame(ic, pkt);
		if (ret != 0) {
			// 循环播放
			int ms = 0; // 3s位置
			long long pos = (double)ms / (double)1000 * pkt->pts * (r2d(ic->streams[pkt->stream_index]->time_base) * 1000);
			av_seek_frame(ic, videoStream, pos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
			break;
		}

		AVCodecContext* cc = NULL;
		if (pkt->stream_index == videoStream) {
			std::cout << "图像" << std::endl;
			cc = vc;
		}
		if (pkt->stream_index == audioStream) {
			std::cout << "音频" << std::endl;
			cc = ac;
		}

		// 显示的时间
		std::cout << "pkt->pts = " << pkt->pts << std::endl; 
		// 转换为毫秒, 方便做同步
		std::cout << "pkt->pts ms = " << pkt->pts * (r2d(ic->streams[pkt->stream_index]->time_base) * 1000) << std::endl;
		// 解码时间
		std::cout << "pkt-dts = " << pkt->dts << std::endl;

		// 解码 发送packet到解码线程 传NULL后，调用多次receive可取出缓冲区中的帧
		int ret2 = avcodec_send_packet(cc, pkt);
		av_packet_unref(pkt);

		if (ret2 != 0) {
			char buf[1024] = { 0 };
			av_strerror(ret2, buf, sizeof(buf) - 1);
			std::cout << "avcodec_send_packet falied :" << buf << std::endl;
			continue;
		}

		for (;;) {
			// 从线程中获取解码接口，一次send可能对应多次receive
			ret2 = avcodec_receive_frame(cc, frame);
			if (ret2 != 0) {
				break;
			}
			std::cout << "recv frame format = " << frame->format
				<< " " << "recv frame width = " << frame->width
				<< " " << "recv frame height =" << frame->height
				<< " " << "recv frame linesize[0] = " << frame->linesize[0] << std::endl;
			if (cc == vc) {
				vctx = sws_getCachedContext(
					vctx, //传NULL会创建
					frame->width,
					frame->height,
					(AVPixelFormat)frame->format,
					frame->width,
					frame->height,
					AV_PIX_FMT_RGBA,
					SWS_BILINEAR,
					0, 0, 0
				);
				std::cout << "像素格式尺寸转换上下文创建或者获取成功" << std::endl;
				if (vctx) {
					if (!rgb) {
						rgb = new uint8_t[frame->width * frame->height * 4];
					}
					uint8_t* data[2] = { 0 };
					data[0] = rgb;
					int lines[2] = { 0 };
					lines[0] = frame->width * 4;
					ret2 = sws_scale(
						vctx,
						frame->data,
						frame->linesize,
						0,
						frame->height,  //输入高度
						data,
						lines
					);
					std::cout << "sws_scale = " << ret2 << std::endl;
				}
				else {
					std::cout << "sws_getCachedContext create failed" << std::endl;
				}
			}
		}
	}
	av_frame_free(&frame);
	av_packet_free(&pkt);

	
	// 关闭文件
	if (ic != NULL) {
		avformat_close_input(&ic);
		ic = NULL;
	}

	return 0;
}

