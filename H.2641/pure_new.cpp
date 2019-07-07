#include "stdafx.h"
#include <iostream>
#include <stdio.h>

using namespace std;


extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/motion_vector.h" //motion_vector.
}

static AVFormatContext* fmt_ctx = NULL;
static AVCodecContext* video_dec_ctx = NULL;
static AVStream* video_stream = NULL;
static const char* src_filename = NULL;

static int video_stream_idx = -1;
static AVFrame* frame = NULL;
static AVPacket pkt;
static int video_frame_count = 0;

#define av_err2str(errnum) av_make_error_string((char[AV_ERROR_MAX_STRING_SIZE]){0},AV_ERROR_MAX_STRING_SIZE,errnum)


static int decode_packet(int* got_frame, int i) {
	int decoded = pkt.size;

	*got_frame = 0;

	if (pkt.stream_index == video_stream_idx) {
		int ret = avcodec_decode_video2(video_dec_ctx, frame, got_frame, &pkt);
		if (ret < 0) {
			cout << "Error decoding video frame" << frame->pkt_pos << endl;
			return ret;
		}
		if (*got_frame) {
			int i;
			AVFrameSideData* sd;

			video_frame_count++;
			sd = av_frame_get_side_data(frame, AV_FRAME_DATA_MOTION_VECTORS);
			if (sd) {
				const AVMotionVector* mvs = (const AVMotionVector *) sd->data;
				for (i = 0; i < sd->size / sizeof(*mvs); i++) {
					const AVMotionVector* mv = &mvs[i];
					printf("%d,%2d,%2d,%2d,%4d,%4d,%4d,%4d,0x%s\n",
						   video_frame_count, mv->source,
						   mv->w, mv->h, mv->src_x, mv->src_y,
						   mv->dst_x, mv->dst_y, mv->flags);
				}
			}
		}
	}
	return decoded;
}

static int open_codec_context(int* stream_idx,AVFormatContext* fmt_ctx,enum AVMediaType type) {
	int ret;
	AVStream* st;
	AVCodecContext* dec_ctx = NULL;
	AVDictionary* opts = NULL;
	AVCodec* dec = NULL;

	ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
	if (ret < 0) {
		cout << "Could not find stream\n";
	}
	else {
		*stream_idx = ret;
		st = fmt_ctx->streams[*stream_idx];

		dec_ctx = st->codec;
		dec = avcodec_find_decoder(dec_ctx->codec_id);
		if (!dec) {
			cout << "Failed to find codec\n";
		}

		av_dict_set(&opts, "flags2", "+export_mvs", 0);
		if ((ret = avcodec_open2(dec_ctx, dec, &opts)) < 0) {
			cout << "Failed to open decodec\n";
			return ret;
		}
	}

}

int main() {
	int ret = 0, got_frame;

	src_filename = "tc10.264";

	av_register_all();
	if(avformat_open_input(&fmt_ctx,src_filename,NULL,NULL)<0) {
		cout << "Error open\n";
		return 1;
	}
	if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		cout << "Could not find stream information\n";
		return 1;
	}

	if(open_codec_context(&video_stream_idx,fmt_ctx,AVMEDIA_TYPE_VIDEO)>=0) {
		video_stream = fmt_ctx->streams[video_stream_idx];
		video_dec_ctx = video_stream->codec;
	}

	av_dump_format(fmt_ctx, 0, src_filename, 0);
	if (!video_stream) {
		cout << "Could not find video stream in the input, aborting\n";
		ret = 1;
	}
	frame = av_frame_alloc();
	if (!frame) {
		cout << "Could not allocate frame\n";
		ret = 0;
		goto end;
	}
	printf("framenum,source,blockw,blockh,srcx,srcy,dstx,dsty,flags\n");

	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;
	while (av_read_frame(fmt_ctx, &pkt) >= 0) {
		AVPacket orig_pkt = pkt;
		do {
			ret = decode_packet(&got_frame, 0);
			if (ret < 0) {
				break;
			}
			pkt.data += ret;
			pkt.size -= ret;
		} while (pkt.size>0);
		av_packet_unref(&orig_pkt);
	}
	pkt.data = NULL;
	pkt.size = 0;
	do {
		decode_packet(&got_frame, 1);
	} while (got_frame);
end:
	avcodec_close(video_dec_ctx);
	avformat_close_input(&fmt_ctx);
	av_frame_free(&frame);
	system("pause");
	return ret < 0;






}
