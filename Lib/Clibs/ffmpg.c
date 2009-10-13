/*
 *  Wrapper for libavcodec/libavformat
 *
 *  Copyright (C) 2007-2009 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

void init (int log_level)
{
	av_register_all ();
	av_log_set_level (log_level);
	if (avcodec_version () != LIBAVCODEC_VERSION_INT)
		fprintf (stderr, "libav: version of header files and version of library differ!!\n");
}

const int version = LIBAVCODEC_VERSION_INT;
const int max_audio_frame_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;

typedef struct
{
	AVFormatContext *fmtCtx;
	AVCodecContext *vCodecCtx, *aCodecCtx;
	AVCodec *vCodec, *aCodec;
	AVFrame *vFrame;
	struct SwsContext *sws;

	int ivideo, iaudio;

	int width, height;
	int outbpp;
	int outw, outh;
	double fps;

	double duration;

	double audio_time_base;
	double video_time_base;

	AVPacket pkt;
	int bytesleft;
	unsigned char *rawdata;

	int wantaudio;
	int moreau;
} VideoStream;

const int sizeof_av = sizeof (VideoStream);

int open_av (VideoStream *v, char *filename)
{
	AVCodecContext *vCodecCtx = 0, *aCodecCtx = 0;
	AVCodec *vCodec = 0, *aCodec = 0;
	AVFrame *vFrame;
	int i, err;

	v->wantaudio = 0;
	v->fmtCtx = 0;
	v->vCodecCtx = 0;
	v->aCodecCtx = 0;
	v->vFrame = 0;
	v->sws = 0;
	v->moreau = 0;

	if ((err = av_open_input_file (&v->fmtCtx, filename, 0, 0, 0)))
		return 1;
	if (av_find_stream_info (v->fmtCtx) < 0)
		return 2;

	/* this is correct if the stream doesn't lie. A more robust way to figure out
	   would be something else... */
	if (v->fmtCtx->duration == AV_NOPTS_VALUE)
		v->duration = -1.0;
	else v->duration = v->fmtCtx->duration / 1000000.0;

	v->ivideo = -1;
	v->iaudio = -1;
	v->vCodec = 0;
	v->width = v->height = 0;
	for (i = 0; i < v->fmtCtx->nb_streams; i++)
		if (v->fmtCtx->streams [i]->codec->codec_type == CODEC_TYPE_VIDEO) {
			if (v->ivideo >= 0) continue;
			vCodecCtx = v->fmtCtx->streams [i]->codec;
			vCodec = avcodec_find_decoder (vCodecCtx->codec_id);
			if (avcodec_open (vCodecCtx, vCodec) < 0)
				return 3;
			v->ivideo = i;
			v->width = vCodecCtx->width;
			v->height = vCodecCtx->height;
			v->fps = 1.0 / av_q2d (v->fmtCtx->streams [i]->r_frame_rate);
			v->video_time_base = av_q2d (v->fmtCtx->streams [i]->time_base);
		} else if (v->fmtCtx->streams [i]->codec->codec_type == CODEC_TYPE_AUDIO) {
			if (v->iaudio >= 0) continue;
			aCodecCtx = v->fmtCtx->streams [i]->codec;
			aCodec = avcodec_find_decoder (aCodecCtx->codec_id);
			if (avcodec_open (aCodecCtx, aCodec) < 0)
				return 3;
			v->iaudio = i;
			v->audio_time_base = av_q2d (v->fmtCtx->streams [i]->time_base);
		} //else fprintf (stderr, "OTHER stream\n");

	vFrame = avcodec_alloc_frame ();

	v->vCodecCtx = vCodecCtx;
	v->vCodec = vCodec;
	v->aCodecCtx = aCodecCtx;
	v->aCodec = aCodec;
	v->vFrame = vFrame;
	v->bytesleft = 0;
	v->pkt.data = 0;

	if (!v->vCodecCtx && v->aCodecCtx)
		v->wantaudio = 1;

	return 0;
}

void close_av (VideoStream *v)
{
	if (v->sws) sws_freeContext (v->sws);
	if (v->vFrame) av_free (v->vFrame);
	if (v->vCodecCtx) avcodec_close (v->vCodecCtx);
	if (v->aCodecCtx) avcodec_close (v->aCodecCtx);
	if (v->fmtCtx) av_close_input_file (v->fmtCtx);
	v->fmtCtx = 0;
	v->vCodecCtx = 0;
	v->aCodecCtx = 0;
	v->vFrame = 0;
	v->sws = 0;
}

/*
 The function next_frame(), decodes the next frame and
 converts it to the desired bitdepth format.
 This function prepares the swscaler for this conversion.
 `bpp` values 1,2,3,4 correspond to the common screen formats
	RGB8, RGB565, RGB (can be used for pyvm rawimage), RGB32
       The value '5' is special for the format YUV420P which is
	used in the case the gfx backend supports xvideo acceleration.
*/
int setbpp (VideoStream *v, int bpp, int scrwidth, int scrheight)
{
	int fmt;

	v->outw = scrwidth  ? scrwidth  : v->width;
	v->outh = scrheight ? scrheight : v->height;
	v->outbpp = bpp;

	switch (bpp) {
		case 3: fmt = PIX_FMT_RGB24; break;
		case 4: fmt = PIX_FMT_RGB32; break;
		case 2: fmt = PIX_FMT_RGB565; break;
		case 1: fmt = PIX_FMT_RGB8; break;
		case 5: fmt = PIX_FMT_YUV420P; break;
		default: return -1;
	}
	v->sws = sws_getContext (v->width, v->height, v->vCodecCtx->pix_fmt,
				 v->outw,  v->outh, fmt,
				 SWS_BICUBIC, 0, 0, 0);
	return 0;
}

int has_audio (VideoStream *v)
{
	return !!v->aCodecCtx;
}

int has_video (VideoStream *v)
{
	return !!v->vCodecCtx;
}

void setwantaudio (VideoStream *v)
{
	v->wantaudio = 1;
}

int asample_rate (VideoStream *v)
{
	return v->aCodecCtx->sample_rate;
}

int achannels (VideoStream *v)
{
	return v->aCodecCtx->channels;
}

int vwidth (VideoStream *v)
{
	return v->width;
}

int vheight (VideoStream *v)
{
	return v->height;
}

double vdelay (VideoStream *v)
{
	return v->fps;
}

double duration (VideoStream *v)
{
	return v->duration;
}

/*
 This function is called if we have both audio and video.  Return values:
	-2: no more frames
	-1: a video frame has been written in `dest`
	>0: audio data of `n` bytes have been written on audio_dest[]

 (Note: This function is declared "blocking", which means that pyvm will
  release the GIL while in it.  Not sure if needed.  For multi-core we
  could separate audio decoding in another function, in other thread.
  For further parallelization we can do the swscaling in another thread
  as well.)
*/
int next_frame (VideoStream *v, unsigned char *dest, short int *audio_dest, double pts[])
{
	int dsize, done;
	int rval;

	if (v->moreau)
		goto more_audio;

	while (1) {
		while (v->bytesleft > 0) {
			dsize = avcodec_decode_video (v->vCodecCtx, v->vFrame,
						 &done, v->rawdata, v->bytesleft);
			v->bytesleft -= dsize;
			v->rawdata += dsize;
			if (done) {
				/* XXXX: study vFrame->repeat_pict */
				if (v->vFrame->repeat_pict)
					fprintf (stderr, "repeated frame!\n");
				AVPicture p;
				if (!dest) return 1;
				p.data [0] = dest;
				p.data [1] = 0; p.data [2] = 0; p.data [3] = 0;
				p.linesize [0] = v->outw * v->outbpp;
				p.linesize [1] = 0; p.linesize [2] = 0; p.linesize [3] = 0;
				sws_scale (v->sws, v->vFrame->data, v->vFrame->linesize,
					   0, v->height, p.data, p.linesize);
				return -1;
			}
		}

		if (v->pkt.data)
			av_free_packet (&v->pkt);

		while (1) {
			if (av_read_frame (v->fmtCtx, &v->pkt) < 0)
				return -2;

			if (v->pkt.stream_index == v->ivideo)
				break;
			if (v->pkt.stream_index == v->iaudio && v->wantaudio) {
			more_audio:;
				int bs = AVCODEC_MAX_AUDIO_FRAME_SIZE;
				int ma = v->moreau;
//fprintf (stderr, "audio-dts=%f\n", v->pkt.pts * v->audio_time_base);
				pts [0] = v->pkt.pts * v->audio_time_base;
				if (audio_dest)
					rval = avcodec_decode_audio2 (v->aCodecCtx, audio_dest, &bs,
								v->pkt.data + ma, v->pkt.size - ma);
				else rval = v->pkt.size - ma;
				if (rval >= 0 && rval < v->pkt.size - ma)
					v->moreau += rval;
				else {
					v->moreau = 0;
					av_free_packet (&v->pkt);
				}
				if (bs)
					return bs;
			}
			else av_free_packet (&v->pkt);
		}

//fprintf (stderr, "video-dts=%f\n", v->pkt.dts * v->video_time_base);
		pts [0] = v->pkt.dts == AV_NOPTS_VALUE ? -1 : v->pkt.dts * v->video_time_base;
		v->bytesleft = v->pkt.size;
		v->rawdata = v->pkt.data;
	}
}

int ffseek (VideoStream *v, double frac)
{
	if (v->moreau)
		av_free_packet (&v->pkt);
	long long pos = v->fmtCtx->start_time + frac * v->fmtCtx->duration;
	int ret = av_seek_frame (v->fmtCtx, -1, pos, 0);
	if (ret >= 0) {
		if (v->vCodecCtx) avcodec_flush_buffers (v->vCodecCtx);
		if (v->aCodecCtx) avcodec_flush_buffers (v->aCodecCtx);
	}
	v->moreau = 0;
	return ret;
	//return av_seek_frame (v->fmtCtx, -1, pos, AVSEEK_FLAG_BACKWARD);
}

/*
*/

const char ABI [] =
"- init			i	\n"
"i max_audio_frame_size		\n"
"i version			\n"
"i sizeof_av			\n"
"i open_av!		ss	\n"
"- close_av		s	\n"
"i setbpp		siii	\n"
"i has_video		s	\n"
"i has_audio		s	\n"
"- setwantaudio		s	\n"
"i asample_rate		s	\n"
"i achannels		s	\n"
"i vwidth		s	\n"
"i vheight		s	\n"
"d vdelay		s	\n"
"d duration		s	\n"
"i next_frame!		svvpd	\n"
"i ffseek		sd	\n"
;
