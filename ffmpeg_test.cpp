
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
	AVFormatContext *ifmt_ctx = avformat_alloc_context();
	AVFormatContext *ofmt_ctx = NULL;
	AVDeviceInfoList *device_info = NULL;
	AVDictionary *options = NULL;
	char *udpAddr = "udp://127.0.0.1:6666";
	AVCodec *pEnc = NULL;
	AVCodec *pAEnc = NULL;
	AVCodecID pEncID = AV_CODEC_ID_H265;
	AVCodecContext *pEncCodecCtx = NULL;
	AVCodecContext *pEncACodecCtx = NULL;
	AVStream *video_st = NULL;
	AVStream *audio_st = NULL;


	av_dict_set(&options, "list_devices", "true", 0);
	AVInputFormat *iformat = av_find_input_format("dshow");
	avformat_open_input(&ifmt_ctx, "video=dummy", iformat, &options);

	//set the input video and audio device directly and open it
	if (avformat_open_input(&ifmt_ctx, "video=Integrated Webcam", iformat, NULL) != 0){
		printf("Fail to open the camera device\n");
		return 1;
	}

	if (avformat_open_input(&ifmt_ctx, "audio=Headset Microphone (USB Audio D", iformat, NULL) != 0){
		printf("Fail to open audio device\n");
		return 1;

	}

	//check the stream
	if (avformat_find_stream_info(ifmt_ctx, NULL) < 0){
		printf("Could not find the video stream information\n");
		return 1;
	}

	int videoindex = -1;
	int audioindex = -1;
	for (int i = 0; i < ifmt_ctx->nb_streams; i++){
		if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
			videoindex = i;
		}

	}

	for (int i = 0; i < ifmt_ctx->nb_streams; i++){
		if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
			audioindex = i;
		}

	}
	if (videoindex == -1 || audioindex == -1){
		printf("Fail to find the video or audio stream\n");
		return 1;

	}
	//open the video codec
	if (avcodec_open2(ifmt_ctx->streams[videoindex]->codec, avcodec_find_decoder(ifmt_ctx->streams[videoindex]->codec->codec_id), NULL) < 0){
		printf("Fail to open the video codec\n");
		return 1;

	}

	if (avcodec_open2(ifmt_ctx->streams[audioindex]->codec, avcodec_find_decoder(ifmt_ctx->streams[audioindex]->codec->codec_id), NULL) < 0){
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
	pEncCodecCtx->width = ifmt_ctx->streams[videoindex]->codec->width;
	pEncCodecCtx->height = ifmt_ctx->streams[videoindex]->codec->height;
	pEncCodecCtx->time_base.num = 1;  //frame rate
	pEncCodecCtx->time_base.den = 25;
	pEncCodecCtx->bit_rate = 250000; //250kb
	pEncCodecCtx->gop_size = 60;   //GOP size
	pEncCodecCtx->qmin = 5;   //QP set
	pEncCodecCtx->qmax = 51;
	AVDictionary *param = 0;
	av_dict_set(&param, "preset", "fast", 0);
	av_dict_set(&param, "tune", "zerolatency", 0);
	if (avcodec_open2(pEncCodecCtx, pEnc, &param) < 0){
		printf("Fail to open the encoder\n");
		return 1;
	}

	//create the AVStream for video
	video_st = avformat_new_stream(ofmt_ctx, pEnc);
	if (video_st == NULL){
		printf("Fail to create the video stream\n");
		return 1;
	}
	video_st->time_base.num = 1;
	video_st->time_base.den = 25;
	video_st->codec = pEncCodecCtx;

	//audio
	pAEnc = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (pAEnc == NULL){
		printf("Fail to create the audio context\n");
		return 1;
	}
	pEncACodecCtx = avcodec_alloc_context3(pAEnc);
	pEncACodecCtx->channels = 2;
	pEncACodecCtx->bit_rate = 32000; //32kb
	pEncACodecCtx->time_base.num = 1;
	pEncACodecCtx->time_base.den = ifmt_ctx->streams[audioindex]->codec->sample_rate;
	pEncACodecCtx->sample_rate = ifmt_ctx->streams[audioindex]->codec->sample_rate;
	pEncACodecCtx->sample_fmt = pAEnc->sample_fmts[0];
	if (avcodec_open2(pEncACodecCtx, pAEnc, NULL) < 0){
		printf("Fail to open the audio encoder\n");
		return 1;
	}

	//create the AVStream for video
	audio_st = avformat_new_stream(ofmt_ctx, pAEnc);
	if(audio_st == NULL){
		printf("Fail to create the audio stream\n");
		return 1;
	}
	audio_st->time_base.num = 1;
	audio_st->time_base.den = ifmt_ctx->streams[audioindex]->codec->sample_rate;
	audio_st->codec = pEncACodecCtx;

	//open output
	if (avio_open(&ofmt_ctx->pb, udpAddr, AVIO_FLAG_READ_WRITE) < 0){
		printf("Fail to open output file\n");
		return 1;

	}

	//show dump infor
	av_dump_format(ofmt_ctx, 0, udpAddr, 1);

	//write file header
	avformat_write_header(ofmt_ctx, NULL);

	//prepare packet
	AVPacket *dec_pkt;
	dec_pkt = (AVPacket *)av_malloc(sizeof(AVPacket));

	//allocate input frame buffer 
	AVFrame *pFrameYUV;
	pFrameYUV = av_frame_alloc();
	uint8_t *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, pEncCodecCtx->width, pEncCodecCtx->height));
	avpicture_fill((AVPicture *)pFrameYUV, out_buffer, AV_PIX_FMT_YUV420P, pEncCodecCtx->width, pEncCodecCtx->height);
	
	//audio buffer
	AVAudioFifo *fifo = NULL;
	fifo = av_audio_fifo_alloc(pEncACodecCtx->sample_fmt, pEncACodecCtx->channels, 1);

	uint8_t **converted_input_samples = NULL;
	if (!(converted_input_samples = (uint8_t**)calloc(pEncACodecCtx->channels,
		sizeof(**converted_input_samples)))) {
		printf("Could not allocate converted input sample pointers\n");
		return AVERROR(ENOMEM);
	}


	//start decoder and encoder
	int aud_next_pts = 0;
	int vid_next_pts = 0;
	AVRational time_base_q = { 1, AV_TIME_BASE };
	int64_t start_time = av_gettime();
	int ret = 0;
	AVFrame *pframe = NULL;
	int dec_got_frame, enc_got_frame;
	int dec_got_frame_a, enc_got_frame_a;
	struct SwsContext *img_convert_ctx;
	struct SwrContext *aud_convert_ctx;
	while (true){
		//compare PTS and audio pts >= video_pts, write video
		if (av_compare_ts(vid_next_pts, time_base_q, aud_next_pts, time_base_q) <= 0){
			//video data from camera, it is raw video
			if (ret = av_read_frame(ifmt_ctx, dec_pkt) >= 0){

				pframe = av_frame_alloc();

				ret = avcodec_decode_video2(ifmt_ctx->streams[dec_pkt->stream_index]->codec, pframe,
					&dec_got_frame, dec_pkt);
				if (ret < 0){
					av_frame_free(&pframe);
					break;
				}
				if (dec_got_frame){
					sws_scale(img_convert_ctx, (const uint8_t* const*)pframe->data, pframe->linesize, 0, ifmt_ctx->streams[videoindex]->codec->height, pFrameYUV->data, pFrameYUV->linesize);
					pFrameYUV->width = pframe->width;
					pFrameYUV->height = pframe->height;
					pFrameYUV->format = AV_PIX_FMT_YUV420P;

				}


			}

		}


	}

}