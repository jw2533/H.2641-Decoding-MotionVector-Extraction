#include "stdafx.h"
#include <stdio.h>
#include <SDL2/SDL_video.h>
#include <iostream>

using namespace std;

#define __STDC_CONSTANT_MACROS
#ifdef _WIN32
//Windows
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "SDL2/SDL.h"
#include "libavutil/motion_vector.h" //motion_vector.h

}
;
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <SDL2/SDL.h>
#include <libavutil/imgutils.h>
#ifdef __cplusplus
};
#endif
#endif

//Output YUV420P data as a file 
#define OUTPUT_YUV420P 0

void print_vector(FILE* fp1, int frame, int code, int x, int y, int dx, int dy) {
	fprintf(fp1, "%d %d %d %d %d %d\n", frame, code, x, y, dx, dy);
}

void SaveFrame(AVFrame* pFrame, int width, int height, int iFrame) {
	FILE* pFile;
	char szFilename[32];
	int y;

	// Open file  
	sprintf(szFilename, "o%d.ppm", iFrame);
	pFile = fopen(szFilename, "wb");
	if (pFile == NULL)
		return;

	// Write header  
	fprintf(pFile, "P6\n%d %d\n255\n", width, height);

	// Write pixel data  
	for (y = 0; y < height; y++)
		fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);

	// Close file  
	fclose(pFile);
	printf("PPM saved\n");
}

static void pgm_save(unsigned char* buf, int wrap, int xsize, int ysize,
                     char* filename) {
	FILE* f;
	int i;

	f = fopen(filename, "w");
	fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
	for (i = 0; i < ysize; i++)
		fwrite(buf + i * wrap, 1, xsize, f);
	fclose(f);
}

AVMotionVector* decode_frame(AVFrame* pFrame) {
	return NULL;
}

void display_AVMotionVector(const AVMotionVector* mv) {
	printf("mvvv-------------------\n");
	cout << "Source: " << mv->source << "\t";
	cout << "Width: " << mv->w << "\t";
	cout << "Height: " << mv->h << "\t";
	cout << "src_x: " << mv->src_x << "src_y: " << mv->src_y << "\t";
	cout << "dst_x: " << mv->dst_x << "dst_y: " << mv->dst_y << "\t";
	cout << "flags: " << mv->flags << "\t";
	cout << "motion_x: " << mv->motion_x << "motion_y: " << mv->motion_y << "\t";
	cout << "motion_scale: " << mv->motion_scale << "\t";
}

int main(int argc, char* argv[]) {
	AVFormatContext* pFormatCtx;
	int i, videoindex;
	AVCodecContext* pCodecCtx;
	AVCodec* pCodec;
	AVFrame *pFrame, *pFrameYUV;
	unsigned char* out_buffer;
	AVPacket* packet;
	int y_size;
	int ret, got_picture;
	struct SwsContext* img_convert_ctx;
	AVDictionary* opts = NULL;

	char filepath[] = "tc10.264";

	//	char filepath[] = "slamtv10.264";
	//SDL---------------------------
	int screen_w = 0, screen_h = 0;
	SDL_Window* screen;
	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture;
	SDL_Rect sdlRect;

	FILE* fp_yuv;

	av_register_all();
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();

	if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0) {
		printf("Couldn't open input stream.\n");
		return -1;
	}
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		printf("Couldn't find stream information.\n");
		return -1;
	}
	videoindex = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoindex = i;
			break;
		}
	if (videoindex == -1) {
		printf("Didn't find a video stream.\n");
		return -1;
	}

	pCodecCtx = pFormatCtx->streams[videoindex]->codec;
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		printf("Codec not found.\n");
		return -1;
	}
	av_dict_set(&opts, "flags2", "+export_mvs", 0);
	if (avcodec_open2(pCodecCtx, pCodec, &opts) < 0) {
		printf("Could not open codec.\n");
		return -1;
	}

	pFrame = av_frame_alloc();
	pFrameYUV = av_frame_alloc();
	out_buffer = (unsigned char *) av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1));
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer,
	                    AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);

	packet = (AVPacket *) av_malloc(sizeof(AVPacket));
	//Output Info-----------------------------
	printf("--------------- File Information ----------------\n");
	av_dump_format(pFormatCtx, 0, filepath, 0);
	printf("-------------------------------------------------\n");
	
	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
	                                 pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

#if OUTPUT_YUV420P 
	fp_yuv = fopen("output.yuv", "wb+");
#endif  

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return -1;
	}

	screen_w = pCodecCtx->width;
	screen_h = pCodecCtx->height;
	//SDL 2.0 Support for multiple windows
	screen = SDL_CreateWindow("Simplest ffmpeg player's Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
	                          screen_w, screen_h,
	                          SDL_WINDOW_OPENGL);

	if (!screen) {
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		return -1;
	}

	sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
	//IYUV: Y + U + V  (3 planes)
	//YV12: Y + V + U  (3 planes)
	sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);

	sdlRect.x = 0;
	sdlRect.y = 0;
	sdlRect.w = screen_w;
	sdlRect.h = screen_h;

	//SDL End----------------------
	while (av_read_frame(pFormatCtx, packet) >= 0) {
		if (packet->stream_index == videoindex) {
			ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet); //TODO:avcodec_decode_video2

			if (ret < 0) {
				printf("Decode Error.\n");
				return -1;
			}
			//printf("%d %d\n", pFrame->pict_type, pFrame->pkt_pos);
			//1 I
			//2 P
			if (got_picture) {
				//printf("GotPicture-%d-%d\n", pFrame->pkt_pos, pFrame->pict_type);
				sws_scale(img_convert_ctx,
				          (const unsigned char* const*) pFrame->data,
				          pFrame->linesize,
				          0,
				          pCodecCtx->height,
				          pFrameYUV->data,
				          pFrameYUV->linesize);
				//printf("type:%d-#(%d)\n", pFrameYUV->pict_type, pFrameYUV->pkt_pos);
				//SaveFrame(pFrameYUV, pCodecCtx->width, pCodecCtx->height, 2544524);
				//saveBMP(pCodecCtx,pFrame, img_convert_ctx);
				/*
				if (pFrame->pkt_pos == 2674518) {
					pgm_save(pFrame->data[0], pFrame->linesize[0], pFrame->width, pFrame->height, "p.ppm");
				}
				if (pFrame->pkt_pos == 2640345) {
					pgm_save(pFrame->data[0], pFrame->linesize[0], pFrame->width, pFrame->height, "I.ppm");
				}
		*/
		AVFrameSideData* sd;
		sd = av_frame_get_side_data(pFrame, AV_FRAME_DATA_MOTION_VECTORS);
		if (sd) {
			//TODO:motion vectors operator
			const AVMotionVector* mvs = (const AVMotionVector*) sd->data;
			//printf("--------********--------\nframe: %s type: %s\n", pFrame->pkt_pos, pFrame->pict_type);
			cout << "--------------------****\nframe: " << pFrame->pkt_pos << "type: %s\n" << pFrame->pict_type << endl;
			for (int i = 0; i < sd->size / sizeof(*mvs); i++) {
				const AVMotionVector* mv = &mvs[i];
				//const int direction = mv->source > 0;
				printf("mv: %s", i);
				display_AVMotionVector(mv);
			}
		}



#if OUTPUT_YUV420P
				y_size = pCodecCtx->width*pCodecCtx->height;
				fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);    //Y 
				fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
				fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V
#endif
				//SDL---------------------------
#if 0
				SDL_UpdateTexture(sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]);
#else
				/*
				SDL_UpdateYUVTexture(sdlTexture, &sdlRect,
				                     pFrameYUV->data[0], pFrameYUV->linesize[0],
				                     pFrameYUV->data[1], pFrameYUV->linesize[1],
				                     pFrameYUV->data[2], pFrameYUV->linesize[2]);
									 */
				SDL_UpdateYUVTexture(sdlTexture, &sdlRect,
				                     pFrameYUV->data[0], pFrameYUV->linesize[0],
				                     pFrameYUV->data[1], pFrameYUV->linesize[1],
				                     pFrameYUV->data[2], pFrameYUV->linesize[2]);
#endif	

				SDL_RenderClear(sdlRenderer);
				SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &sdlRect);
				SDL_RenderPresent(sdlRenderer);
				//SDL End-----------------------
				//Delay 40ms
				SDL_Delay(40);
				//printf("#%dDone----------------\n", pFrame->pkt_pos);
				got_picture = 0;
			}
		}
			av_packet_unref(packet);
	//	av_packet_unref(packet);
		//av_free_packet(packet);
	}
	//flush decoder
	//FIX: Flush Frames remained in Codec
	while (1) {
		ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);

		//printf("ret%d\n", ret);
		//printf("%d\n", pFrame->pict_type);
		if (ret < 0)
			break;
		if (!got_picture)
			break;
		//sws_scale(img_convert_ctx, (const unsigned char* const*) pFrame->data, pFrame->linesize, 0, pCodecCtx->height,pFrameYUV->data, pFrameYUV->linesize);
#if OUTPUT_YUV420P
		int y_size = pCodecCtx->width*pCodecCtx->height;
		fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);    //Y 
		fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
		fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V
#endif
		//SDL---------------------------
		SDL_UpdateTexture(sdlTexture, &sdlRect, pFrame->data[0], pFrame->linesize[0]);
		SDL_RenderClear(sdlRenderer);
		SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &sdlRect);
		SDL_RenderPresent(sdlRenderer);
		//SDL End-----------------------
		//Delay 40ms
		SDL_Delay(40);
	}

	sws_freeContext(img_convert_ctx);

#if OUTPUT_YUV420P 
	fclose(fp_yuv);
#endif 

	SDL_Quit();

	av_frame_free(&pFrameYUV);
	av_frame_free(&pFrame);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);

	system("pause");
	return 0;
}
