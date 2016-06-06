
#define USEFILTER 0

#include <stdio.h>
#include <conio.h>
#include <windows.h>
#include "stdafx.h"
#define snprintf _snprintf
extern "C"
{
#include "libavutil/opt.h"
#include "libavutil/time.h"
#include "libavutil/mathematics.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/audio_fifo.h"
#if USEFILTER
#include "libavfilter/avfiltergraph.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#endif
};


int main(int argc, char* argv[])
{

	char captureName[80] = { 0 };
	av_register_all();
	avdevice_register_all();
	avformat_network_init();
	AVFormatContext *pCtx = avformat_alloc_context();
	AVFormatContext *ofmt_ctx = NULL;
	AVDeviceInfoList *device_info = NULL;
	AVDictionary *options = NULL;
	char *udpAddr = "udp://127.0.0.1:6666";
	AVCodec *pEnc = NULL;
	AVCodecID pEncID = AV_CODEC_ID_H265;
	AVCodecContext *pEncCodecCtx = NULL;


	av_dict_set(&options, "list_devices", "true", 0);
	AVInputFormat *iformat = av_find_input_format("dshow");
	avformat_open_input(&pCtx, "video=dummy", iformat, &options);

	//set the input video and audio device directly and open it
	if (avformat_open_input(&pCtx, "video=Integrated Webcam", iformat, NULL) != 0){
		printf("Fail to open the camera device\n");
		return 1;
	}

	if (avformat_open_input(&pCtx, "audio=Headset Microphone (USB Audio D", iformat, NULL) != 0){
		printf("Fail to open audio device\n");
		return 1;

	}

	//check the stream
	if (avformat_find_stream_info(pCtx, NULL) < 0){
		printf("Could not find the video stream information\n");
		return 1;
	}

	int videoindex = -1;
	int audioindex = -1;
	for (int i = 0; i < pCtx->nb_streams; i++){
		if (pCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
			videoindex = i;
		}

	}

	for (int i = 0; i < pCtx->nb_streams; i++){
		if (pCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
			audioindex = i;
		}

	}
	if (videoindex == -1 || audioindex == -1){
		printf("Fail to find the video or audio stream\n");
		return 1;

	}
	//open the video codec
	if (avcodec_open2(pCtx->streams[videoindex]->codec, avcodec_find_decoder(pCtx->streams[videoindex]->codec->codec_id), NULL) < 0){
		printf("Fail to open the video codec\n");
		return 1;

	}

	if (avcodec_open2(pCtx->streams[audioindex]->codec, avcodec_find_decoder(pCtx->streams[audioindex]->codec->codec_id), NULL) < 0){
		printf("Fail to open the audio codec\n");
		return 1;

	}

	//output init
	if (avformat_alloc_output_context2(&ofmt_ctx, NULL, "mp4", udpAddr) < 0){
		printf("Fail to allocate output context\n");
		return 1;
	}

	//find H265 encoder, intel hevc encoder
	pEnc = avcodec_find_encoder(pEncID);
	if (pEnc == NULL){
		printf("Fail to find the encoder ID\n");
		return 1;
	}

	//create the AVCodecContext and set the encoder paramerters
	pEncCodecCtx = avcodec_alloc_context3(pEnc);
	if (pEncCodecCtx == NULL){
		printf("Fail to allocate encoder context\n");
		return 1;
	}
	//encoder parameter
	pEncCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
	pEncCodecCtx->width = pCtx->streams[videoindex]->codec->width;
	pEncCodecCtx->height = pCtx->streams[videoindex]->codec->height;
	pEncCodecCtx->time_base.num = 1;  //frame rate
	pEncCodecCtx->time_base.den = 25;
	pEncCodecCtx->bit_rate = 250000; //250kb
	pEncCodecCtx->gop_size = 60;   //GOP size
	pEncCodecCtx->qmin = 5;   //QP set
	pEncCodecCtx->qmax = 51;
	av_dict_set(&param, "preset", "fast", 0);
	av_dict_set(&param, "tune", "zerolatency", 0);
	if (avcodec_open2(pEncCodecCtx, pEnc, &param) < 0){
		printf("Fail to open the encoder\n");
		return 1;
	}





}