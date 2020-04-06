#include <stdio.h>
#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#include <sys/shm.h>
#include <unistd.h>

typedef struct {
    int flag;
    int frameIndex;
    char data[1024 * 1024 * 50];
}shared_data;

#define  AVLOGI(...)  av_log(NULL, AV_LOG_INFO, __VA_ARGS__);
#define  AVLOGD(...)  av_log(NULL, AV_LOG_DEBUG, __VA_ARGS__);
#define  AVLOGE(...)  av_log(NULL, AV_LOG_ERROR, __VA_ARGS__);

const char* filepath = NULL;
const int cycles = 100;

int video_stream_index = -1;
int audio_stream_index = -1;

static inline double r2d(AVRational r) {
	return r.den == 0 ? 0 : (double)r.num / (double)r.den;
}

int main(int argc, char* argv[]) {

    av_log_set_flags(AV_LOG_INFO);

	// int shmid = shmget(1234, sizeof(shared_data), 0666 | IPC_CREAT);
	// if (shmid == -1) {
	// 	AVLOGE("获取共享内存标识失败\n");
	// 	return -1;
	// }
	// AVLOGI("获取共享标识成功:shmid = %d\n", shmid);
	// void* shm = shmat(shmid, NULL, 0);
	// if (shm == (void*)-1) {
	// 	AVLOGE("映射共享内存地址失败\n");
	// 	return -1;
	// }
	// AVLOGI("映射共享内存地址成功:%p\n", shm);
	// shared_data* shdata = (shared_data*)shm;

	// sleep(3);

	int ret = -1;
    if (argc < 2) {
        AVLOGE("at least, args > 2\n");
        return -1;
    }
    filepath = argv[1];
	// 参数设置
	AVDictionary* options = NULL;
	// 设置rtsp流以tcp协议打开
	av_dict_set(&options, "rtsp_transport", "tcp", 0);
	// 网路延时时间
	av_dict_set(&options, "max_delay", "500", 0);
	// 解封装上下文
	AVFormatContext* fmt_ctx = NULL;
	ret = avformat_open_input(&fmt_ctx, filepath, 0, &options);
	if (ret != 0) {
		char buf[1024] = { 0 };
		av_strerror(ret, buf, sizeof(buf) - 1);
        AVLOGE("open input file format failed:%s\n", buf);
		return -1;
	}
    AVLOGI("open input file format success\n\n");

    // 针对MPEG1和MPEGE2等没有Header的文件格式做处理
	ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
		char buf[1024] = { 0 };
		av_strerror(ret, buf, sizeof(buf) - 1);
        AVLOGE("find stream info failed:%s\n", buf);
        return -1;
    }
	AVLOGI("find stream info success\n\n");

	//寻找音视频流 (一般不考虑字幕啥的)
	ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (ret == AVERROR_STREAM_NOT_FOUND) {
		AVLOGE("find video stream index failed!\n");
		return -1;
	}
	video_stream_index = ret;
	AVStream* video_stream = fmt_ctx->streams[video_stream_index];
	AVLOGI("***********视频信息**********\n");
	AVLOGI("width = %d\n", video_stream->codecpar->width);
	AVLOGI("height = %d\n", video_stream->codecpar->height);
	AVLOGI("codec_id = %d\n", video_stream->codecpar->codec_id);
	AVLOGI("pixel_format = %d\n", video_stream->codecpar->format);
	ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if (ret == AVERROR_STREAM_NOT_FOUND) {
		AVLOGE("find audio streamindex failed \n");
		return -1;
	}
	audio_stream_index = ret;
	AVStream* audio_stream = fmt_ctx->streams[audio_stream_index];
	AVLOGI("***********音频信息**********\n");
	AVLOGI("sample_rate = %d\n", audio_stream->codecpar->sample_rate);
	AVLOGI("chanels = %d\n", audio_stream->codecpar->channels);
	AVLOGI("codec_id = %d\n", audio_stream->codecpar->codec_id);
	AVLOGI("sample_format = %d\n", video_stream->codecpar->format);
	AVLOGI("\n");


	// 创建和打开视频编解码器
	AVCodec* vcodec = avcodec_find_decoder(fmt_ctx->streams[video_stream_index]->codec->codec_id);
	if (!vcodec) {
		char buf[1024] = { 0 };
		av_strerror(ret, buf, sizeof(buf) - 1);
		AVLOGE("find the video codec failed:%s\n", buf);
		return -1;
	}
	AVCodecContext* video_dec_ctx = avcodec_alloc_context3(vcodec);
	avcodec_parameters_to_context(video_dec_ctx, fmt_ctx->streams[video_stream_index]->codecpar);
	int vret = avcodec_open2(video_dec_ctx, 0, 0);
	if (vret != 0) {
		char buf[1024] = { 0 };
		av_strerror(vret, buf, sizeof(buf) - 1);
		AVLOGE("open video failed:%s\n", buf);
		return -1;
	}
	AVLOGI("open video success\n");
	AVCodec* acodec = avcodec_find_decoder(fmt_ctx->streams[audio_stream_index]->codec->codec_id);
	if (!acodec) {
		char buf[1024] = { 0 };
		av_strerror(ret, buf, sizeof(buf) - 1);
		AVLOGE("find the audio codec failed:%s\n", buf);
		return -1;
	}
	AVCodecContext* audio_dec_ctx = avcodec_alloc_context3(acodec);
	avcodec_parameters_to_context(audio_dec_ctx, fmt_ctx->streams[audio_stream_index]->codecpar);
	int aret = avcodec_open2(audio_dec_ctx, 0, 0);
	if (aret != 0) {
		char buf[1024] = { 0 };
		av_strerror(aret, buf, sizeof(buf) - 1);
		AVLOGE("open audio failed:%s\n", buf);
		return -1;
	}
	AVLOGI("open audio success\n\n");

	FILE* file = fopen("/home/lihai/Desktop/xiaoli666/test/test.yuv", "wb");
	int index = 1;

	// malloc AVPacket并初始化
	AVPacket* pkt = av_packet_alloc();
	// malloc AVFrame并初始化
	AVFrame* frame = av_frame_alloc();

	for (;;) {
		int ret = av_read_frame(fmt_ctx, pkt);
		if (ret != 0) {
			// 循环播放
			break;
			int ms = 0; // 0s位置
			long long pos = (double)ms / (double)1000 * pkt->pts * (r2d(fmt_ctx->streams[pkt->stream_index]->time_base) * 1000);
			av_seek_frame(fmt_ctx, video_stream_index, pos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
			break;
		}

		AVCodecContext* cc = NULL;
		if (pkt->stream_index == video_stream_index) {
			AVLOGI("视频帧\n");
			cc = video_dec_ctx;
		}
		if (pkt->stream_index == audio_stream_index) {
			AVLOGI("音频帧\n");
			cc = audio_dec_ctx;
		}

		// 显示的时间
		AVLOGI("pkt->pts = %ld\n", pkt->pts);
		AVLOGI("pkt->pts(ms) = %lf\n", (double)pkt->pts * (r2d(fmt_ctx->streams[pkt->stream_index]->time_base) * 1000));

		// 解码时间
		AVLOGI("pkt->dts = %ld\n", pkt->dts);
		AVLOGI("pkt->dts(ms) = %lf\n", (double)pkt->pts * (r2d(fmt_ctx->streams[pkt->stream_index]->time_base) * 1000));

		// 解码 发送packet到解码线程 传NULL后，调用多次receive可取出缓冲区中的帧
		ret = avcodec_send_packet(cc, pkt);
		av_packet_unref(pkt);
		if (ret != 0) {
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			AVLOGE("avcodec_send_packet falied:%s\n", buf);
			continue;
		}
		for (;;) {
			// 从线程中获取解码接口，一次send可能对应多次receive
			ret = avcodec_receive_frame(cc, frame);
			if (ret != 0) {
				break;
			}
			if(cc == video_dec_ctx) {
				AVLOGI("frame width = %d\n", frame->width);
				AVLOGI("frame height = %d\n", frame->height);
				AVLOGI("frame pixformat = %d\n", frame->format);
				AVLOGI("frame linesize = %d\n", frame->linesize[0]);

				char* buf = (char*)malloc(frame->linesize[0] * frame->height * 3 / 2);

				int offset = frame->linesize[0] * frame->height;
				memcpy(buf, frame->data[0], offset);
				memcpy(buf + offset, frame->data[1], offset / 4);
				memcpy(buf + offset + offset / 4, frame->data[2], offset / 4);

				fwrite(buf, offset * 3 /2 , 1, file);

				// printf("index = %d\n",index);

				// if (shdata->flag == 0) {
				// 	shdata->frameIndex = index;
				// 	memcpy(shdata->data, buf, offset * 3 /2);
				// 	index++;
				// 	shdata->flag = 1;
				// }

				index++;

				free(buf);

			} else if(cc == audio_dec_ctx) {

			}
		}
	}

	fclose(file);

	av_frame_free(&frame);
	av_packet_free(&pkt);
	avcodec_free_context(&video_dec_ctx);
	avcodec_free_context(&audio_dec_ctx);
	avformat_close_input(&fmt_ctx);
	fmt_ctx = NULL;

	return 0;
}


/*




		for (;;) {
			
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
	*/




	// vc->thread_count = 8; // 设置有风险
	// ac->thread_count = 8;
		// 转换为毫秒, 方便做同步
		// std::cout << "pkt->pts ms = " << pkt->pts * (r2d(ic->streams[pkt->stream_index]->time_base) * 1000) << std::endl;



