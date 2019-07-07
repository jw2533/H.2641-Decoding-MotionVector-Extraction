#include "stdafx.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#define STREAM_DURATION   10.0
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUVJ422P /* default pix_fmt */
#define SCALE_FLAGS SWS_BILINEAR //SWS_BICUBIC

// a wrapper around a single output AVStream
typedef struct OutputStream {
	AVStream *st;
	AVCodecContext *enc;
	/* pts of the next frame that will be generated */
	int64_t next_pts;
	AVFrame *frame;
	AVFrame *tmp_frame;
	struct SwsContext *sws_ctx;
	struct SwrContext *swr_ctx;
} OutputStream;


static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt) {
	AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

	printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
		   av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
		   av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
		   av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
		   pkt->stream_index);
}

static void close_stream(AVFormatContext *oc, OutputStream *ost) {
	avcodec_free_context(&ost->enc);
	av_frame_free(&ost->frame);
	av_frame_free(&ost->tmp_frame);
	sws_freeContext(ost->sws_ctx);
	swr_free(&ost->swr_ctx);
}


static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height) {
	AVFrame *picture;
	int ret;

	picture = av_frame_alloc();
	if (!picture)
		return NULL;

	picture->format = pix_fmt;
	picture->width = width;
	picture->height = height;

	/* allocate the buffers for the frame data */
	ret = av_frame_get_buffer(picture, 32);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate frame data.\n");
		exit(1);
	}

	return picture;
}


static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt) {
	/* rescale output packet timestamp values from codec to stream timebase */
	av_packet_rescale_ts(pkt, *time_base, st->time_base);
	pkt->stream_index = st->index;

	/* Write the compressed frame to the media file. */
	log_packet(fmt_ctx, pkt);
	return av_interleaved_write_frame(fmt_ctx, pkt);
}
static AVFrame *get_video_frame(OutputStream *ost) {
	AVCodecContext *c = ost->enc;

	/* check if we want to generate more frames */
	if (av_compare_ts(ost->next_pts, c->time_base, 10, (AVRational) { 1, 1 }) >= 0)
		return NULL;

	if (!ost->sws_ctx) {
		ost->sws_ctx = sws_getContext(c->width, c->height,
									  AV_PIX_FMT_YUV420P,
									  c->width, c->height,
									  c->pix_fmt,
									  SCALE_FLAGS, NULL, NULL, NULL);
		if (!ost->sws_ctx) {
			fprintf(stderr,
					"Could not initialize the conversion context\n");
			exit(1);
		}
	}
	sws_scale(ost->sws_ctx,
		(const uint8_t * const *) ost->tmp_frame->data,
			  ost->tmp_frame->linesize,
			  0, c->height,
			  ost->frame->data,
			  ost->frame->linesize);
	ost->frame->pts = ost->next_pts++;
	return ost->frame;
}


/*
* encode one video frame and send it to the muxer
* return 1 when encoding is finished, 0 otherwise
*/
static int write_video_frame(AVFormatContext *oc, OutputStream *ost) {
	int ret;
	AVCodecContext *c;
	AVFrame *frame;
	int got_packet = 0;
	AVPacket pkt = { 0 };

	c = ost->enc;
	frame = get_video_frame(ost);
	av_init_packet(&pkt);

	/* encode the image */
	ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
	if (ret < 0) {
		fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
		exit(1);
	}

	if (got_packet) {
		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
	}
	else {
		ret = 0;
	}

	if (ret < 0) {
		fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
		exit(1);
	}

	return (frame || got_packet) ? 0 : 1;
}


static void open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg) {
	int ret;
	AVCodecContext *c = ost->enc;
	AVDictionary *opt = NULL;

	av_dict_copy(&opt, opt_arg, 0);

	/* open the codec */
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
		exit(1);
	}

	/* allocate and init a re-usable frame */
	ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
	if (!ost->frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}

	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(ost->st->codecpar, c);
	if (ret < 0) {
		fprintf(stderr, "Could not copy the stream parameters\n");
		exit(1);
	}
}


/* Add an output stream. */
static void add_stream(OutputStream *ost, AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id) {
	AVCodecContext *c;

	/* find the encoder */
	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec)) {
		fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
		exit(1);
	}

	ost->st = avformat_new_stream(oc, NULL);
	if (!ost->st) {
		fprintf(stderr, "Could not allocate stream\n");
		exit(1);
	}
	ost->st->id = oc->nb_streams - 1;
	c = avcodec_alloc_context3(*codec);
	if (!c) {
		fprintf(stderr, "Could not alloc an encoding context\n");
		exit(1);
	}
	ost->enc = c;

	switch ((*codec)->type) {
		case AVMEDIA_TYPE_VIDEO:
			c->codec_id = codec_id;
			c->bit_rate = 400000;
			c->width = 640;
			c->height = 480;
			ost->st->time_base = (AVRational) { 1, STREAM_FRAME_RATE };
			c->time_base = ost->st->time_base;
			c->gop_size = 12; /* emit one intra frame every twelve frames at most */
			c->pix_fmt = STREAM_PIX_FMT;
			break;

		default:
			break;
	}
}



int main(int argc, char *argv[]) {
	int ret = 0;
	int have_video = 0;
	int encode_video = 0;
	int stream_index = 0;
	int got_frame = 0;
	char *src_filename = argv[1];
	char *filename = argv[2];

	AVStream *st;
	OutputStream video_st = { 0 };
	AVOutputFormat *out_fmt;
	AVFormatContext *oc;
	AVFormatContext *ic;
	AVCodec *video_codec;
	AVCodec *dec;
	AVCodecContext *dec_ctx = NULL;
	AVDictionary *opt = NULL;
	AVFrame *frame = NULL;
	AVPacket pkt;

	av_register_all();

	/***********input**************/
	/* open input file, and allocate format context */
	if (avformat_open_input(&ic, src_filename, NULL, NULL) < 0) {
		fprintf(stderr, "Could not open source file %s\n", src_filename);
		exit(1);
	}

	/* retrieve stream information */
	if (avformat_find_stream_info(ic, NULL) < 0) {
		fprintf(stderr, "Could not find stream information\n");
		exit(1);
	}

	ret = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not find %s stream in input file '%s'\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO), src_filename);
		return ret;
	}
	else {
		stream_index = ret;
		st = ic->streams[stream_index];

		/* find decoder for the stream */
		dec_ctx = st->codec;
		dec = avcodec_find_decoder(dec_ctx->codec_id);
		if (!dec) {
			fprintf(stderr, "Failed to find %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
			return AVERROR(EINVAL);
		}

		if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
			fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
			return ret;
		}
	}

	/* dump input information to stderr */
	av_dump_format(ic, 0, src_filename, 0);

	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate frame\n");
		ret = AVERROR(ENOMEM);
		return ret;
	}

	/* initialize packet, set data to NULL, let the demuxer fill it */
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	/***********output*************/
	/* allocate the output media context */
	avformat_alloc_output_context2(&oc, NULL, NULL, filename);
	if (!oc) {
		printf("Could not deduce output format from file extension: using MPEG.\n");
		avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
	}
	if (!oc)
		return 1;

	out_fmt = oc->oformat;
	/* Add the audio and video streams using the default format codecs
	* and initialize the codecs. */
	if (out_fmt->video_codec != AV_CODEC_ID_NONE) {
		add_stream(&video_st, oc, &video_codec, out_fmt->video_codec);
		have_video = 1;
		encode_video = 1;
	}
	open_video(oc, video_codec, &video_st, opt);
	av_dump_format(oc, 0, filename, 1);

	/* open the output file, if needed */
	if (!(out_fmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			fprintf(stderr, "Could not open '%s': %s\n", filename, av_err2str(ret));
			return 1;
		}
	}

	/* Write the stream header, if any. */
	ret = avformat_write_header(oc, &opt);
	if (ret < 0) {
		fprintf(stderr, "Error occurred when opening output file: %s\n", av_err2str(ret));
		return 1;
	}

	av_seek_frame(ic, -1, 90000000, AVSEEK_FLAG_BACKWARD);
	while (av_read_frame(ic, &pkt) >= 0) {
		AVPacket orig_pkt = pkt;
		if (pkt.size > 0) {
			if (pkt.stream_index == stream_index) {
				ret = avcodec_decode_video2(dec_ctx, frame, &got_frame, &pkt);
				if (ret < 0) {
					fprintf(stderr, "Error decoding video frame (%s)\n", av_err2str(ret));
					return ret;
				}

				if (got_frame) {
					video_st.tmp_frame = frame;
					encode_video = write_video_frame(oc, &video_st);
				}
			}
		}
		av_packet_unref(&orig_pkt);
	}

	av_write_trailer(oc);
	if (have_video)
		close_stream(oc, &video_st);
	if (!(out_fmt->flags & AVFMT_NOFILE))
		avio_closep(&oc->pb);
	avformat_free_context(oc);

	return ret;
}