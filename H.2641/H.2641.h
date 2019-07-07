#ifndef INT64_C   
#define INT64_C(c) (c ## LL)   
#define UINT64_C(c) (c ## ULL)   
#endif   

#ifdef __cplusplus   
extern "C" {
#endif   
	/*Include ffmpeg header file*/
#include <libavformat/avformat.h>   
#include <libavcodec/avcodec.h>   
#include <libswscale/swscale.h>   

#include <libavutil/imgutils.h>    
#include <libavutil/opt.h>       
#include <libavutil/mathematics.h>     
#include <libavutil/samplefmt.h>  

#ifdef __cplusplus   
}
#endif