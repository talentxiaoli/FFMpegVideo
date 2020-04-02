#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>

const char* filename = NULL;

AVFormatContext* fmt_ctx = NULL;
AVCodecContext* video_dec_ctx = NULL; 
AVCodecContext* audio_dec_ctx = NULL;
AVStream* video_stream = NULL;
AVStream* audio_stream = NULL;
AVPacket* pkt;
AVFrame* frame = NULL;
uint8_t* video_dst_data[4] = {NULL};
int video_dst_linesize[4];
int video_dst_bufsize;
int video_stream_idx = -1;
int audio_stream_idx = -1;

static inline double r2d(AVRational r) {
	return r.den == 0 ? 0 : (double)r.num / (double)r.den;
}

int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type);

int decode_packet(int *got_frame, int cached);

int main(int argc, char* argv[]) {

    int ret = -1;

    av_log_set_level(AV_LOG_INFO);
    if (argc < 2) {
        av_log(NULL, AV_LOG_ERROR, "arguments can't be less than 2");
        return -1;
    }

    filename = argv[1];

	// 参数设置
	AVDictionary* options = NULL;
	// 设置rtsp流以tcp协议打开
	av_dict_set(&options, "rtsp_transport", "tcp", 0);
	// 网路延时时间
	av_dict_set(&options, "max_delay", "500", 0);
    /* open input file, and allocate format context */
    ret = avformat_open_input(&fmt_ctx, filename, NULL, &options);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "could not open source file\n");
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "open input file %s success\n", filename);

    /* retrieve stream information */
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not find stream information\n");
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "find stream info success\n");

    av_dump_format(fmt_ctx, 0, filename, 0);

    /* open the video codec context */
    ret = open_codec_context(&video_stream_idx, &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "open video codec context failed\n");
        return -1;
    }
    video_stream = fmt_ctx->streams[video_stream_idx];

    ret = av_image_alloc(video_dst_data, video_dst_linesize,
                            video_dec_ctx->width, video_dec_ctx->height, video_dec_ctx->pix_fmt, 1);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate raw video buffer\n");
        // goto end;
    }
    video_dst_bufsize = ret;

    /* open the video codec context */
    ret = open_codec_context(&audio_stream_idx, &audio_dec_ctx, fmt_ctx, AVMEDIA_TYPE_AUDIO);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "open video codec context failed\n");
        // goto end;
        return -1;
    }
    audio_stream = fmt_ctx->streams[audio_stream_idx];

    if (!audio_stream && !video_stream) {
        av_log(NULL, AV_LOG_ERROR, "Could not find audio or video stream in the input, aborting\n");
        // goto end;
        return -1;
    }

    /* malloc avpacket */
	pkt = av_packet_alloc();
    /* malloc avframe */
    frame = av_frame_alloc();

    for(;;) {
		int ret = av_read_frame(fmt_ctx, pkt);
		if (ret != 0) {
			// 循环播放
			int ms = 0; // 0s位置
			long long pos = (double)ms / (double)1000 * pkt->pts * (r2d(fmt_ctx->streams[pkt->stream_index]->time_base) * 1000);
			av_seek_frame(fmt_ctx, video_stream, pos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
			break;
		}
    }

    avformat_close_input(&fmt_ctx);

    return  0;
}


int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type) {
    int ret = -1;
    int stream_index;
    AVStream* stream;
    AVCodec*  dec = NULL;
    AVDictionary* opts = NULL;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not find %s stream in input file '%s'\n", av_get_media_type_string(type), filename);
        return -1;
    }
    stream_index = ret;
    stream = fmt_ctx->streams[stream_index];
        /* find decoder for the stream */
    dec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!dec) {
        av_log(NULL, AV_LOG_ERROR, "Failed to find %s codec\n", av_get_media_type_string(type));
        return -1;
    }

    /* Allocate a codec context for the decoder */
    *dec_ctx = avcodec_alloc_context3(dec);
    if (!*dec_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate the %s codec context\n", av_get_media_type_string(type));            
        return -1;
    }

    /* Copy codec parameters from input stream to output codec context */
    ret = avcodec_parameters_to_context(*dec_ctx, stream->codecpar);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to copy %s codec parameters to decoder context\n", av_get_media_type_string(type));   
        return -1;
    }

    /* Init the decoders, with or without reference counting */
    // av_dict_set(&opts, "refcounted_frames", refcount ? "1" : "0", 0);
    ret = avcodec_open2(*dec_ctx, dec, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to open %s codec\n", av_get_media_type_string(type)); 
        return ret;
    }
    *stream_idx = stream_index;

    return 0;
}

int decode_packet(int *got_frame, int cached) {
    int ret = 0;
    int decoded = pkt->size;
    *got_frame = 0;

    if (pkt->stream_index == video_stream_idx) {
        /* decode video frame */
        ret = avcodec_decode_video2(video_dec_ctx, frame, got_frame, &pkt);
        if (ret < 0) {
            fprintf(stderr, "Error decoding video frame (%s)\n", av_err2str(ret));
            return ret;
        }

        if (*got_frame) {

            if (frame->width != width || frame->height != height ||
                frame->format != pix_fmt) {
                /* To handle this change, one could call av_image_alloc again and
                 * decode the following frames into another rawvideo file. */
                fprintf(stderr, "Error: Width, height and pixel format have to be "
                        "constant in a rawvideo file, but the width, height or "
                        "pixel format of the input video changed:\n"
                        "old: width = %d, height = %d, format = %s\n"
                        "new: width = %d, height = %d, format = %s\n",
                        width, height, av_get_pix_fmt_name(pix_fmt),
                        frame->width, frame->height,
                        av_get_pix_fmt_name(frame->format));
                return -1;
            }

            printf("video_frame%s n:%d coded_n:%d\n",
                   cached ? "(cached)" : "",
                   video_frame_count++, frame->coded_picture_number);

            /* copy decoded frame to destination buffer:
             * this is required since rawvideo expects non aligned data */
            av_image_copy(video_dst_data, video_dst_linesize,
                          (const uint8_t **)(frame->data), frame->linesize,
                          pix_fmt, width, height);

            /* write to rawvideo file */
            fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
        }
    } else if (pkt.stream_index == audio_stream_idx) {
        /* decode audio frame */
        ret = avcodec_decode_audio4(audio_dec_ctx, frame, got_frame, &pkt);
        if (ret < 0) {
            fprintf(stderr, "Error decoding audio frame (%s)\n", av_err2str(ret));
            return ret;
        }
        /* Some audio decoders decode only part of the packet, and have to be
         * called again with the remainder of the packet data.
         * Sample: fate-suite/lossless-audio/luckynight-partial.shn
         * Also, some decoders might over-read the packet. */
        decoded = FFMIN(ret, pkt.size);

        if (*got_frame) {
            size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample(frame->format);
            printf("audio_frame%s n:%d nb_samples:%d pts:%s\n",
                   cached ? "(cached)" : "",
                   audio_frame_count++, frame->nb_samples,
                   av_ts2timestr(frame->pts, &audio_dec_ctx->time_base));

            /* Write the raw audio data samples of the first plane. This works
             * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
             * most audio decoders output planar audio, which uses a separate
             * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
             * In other words, this code will write only the first audio channel
             * in these cases.
             * You should use libswresample or libavfilter to convert the frame
             * to packed data. */
            fwrite(frame->extended_data[0], 1, unpadded_linesize, audio_dst_file);
        }
    }

    /* If we use frame reference counting, we own the data and need
     * to de-reference it when we don't use it anymore */
    if (*got_frame && refcount)
        av_frame_unref(frame);

    return decoded;
}














/********************** backup code *****************************/
    // video_dst_file = fopen(video_dst_filename, "wb");
    // if (!video_dst_file) {
    //     fprintf(stderr, "Could not open destination file %s\n", video_dst_filename);
    //     ret = 1;
    //     goto end;
    // }
    /* allocate image where the decoded image will be put */
    // av_log(NULL, AV_LOG_INFO, "video width = %d\n", video_dec_ctx->width); //sometimes it's unbeliveable
    // av_log(NULL, AV_LOG_INFO, "video height = %d\n", video_dec_ctx->height);
    // av_log(NULL, AV_LOG_INFO, "video pixformat = %d\n", video_dec_ctx->pix_fmt); //0 AV_PIX_FMT_YUV420P

    // audio_dst_file = fopen(audio_dst_filename, "wb");
    // if (!audio_dst_file) {
    //     fprintf(stderr, "Could not open destination file %s\n", audio_dst_filename);
    //     ret = 1;
    //     goto end;
    // }
/****************************************************************/