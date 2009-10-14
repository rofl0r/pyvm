/*
 *  Linux audio device
 *
 *  Copyright (C) 2007 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/soundcard.h>
#include <linux/kd.h>	// KIOCSOUND -- console beep ioctl
#include <math.h>

static int fd = -1;

/* The audio fd is used in BLOCKING mode for writting.  This means (as far as we
   can tell), that a write() consumes the whole buffer, iow, this can be used
   for synchronization.
*/
int open_audio (int reading)
{
	fd = open ("/dev/dsp", reading ? O_RDONLY : O_WRONLY);
	if (fd < 0)
		return -1;
	if (!reading)
		if (fcntl (fd, F_SETFL, fcntl (fd, F_GETFL) & ~O_NONBLOCK) < 0) {
			close (fd);
			return -2;
		}
	return fd;
}

void close_audio ()
{
	close (fd);
}

/*
 Atm, the only supported sample format is:
	signed 16bit host endian
 This is what ffmpeg gives. Adapt when other fmts are needed
*/
int setup_dsp (int rate, int channels)
{
	int value = AFMT_S16_NE;
	if (ioctl (fd, SNDCTL_DSP_SETFMT, &value) < 0)
		return -1;
	value = channels;
	if (ioctl (fd, SNDCTL_DSP_CHANNELS, &value))
		return -2;
	value = rate;
	if (ioctl (fd, SNDCTL_DSP_SPEED, &value))
		return -3;
	if (value != rate)
		return -4;

	// return audio buffer size. we should check
	// for ioctl GETOSPACE, etc
	audio_buf_info zz;
	if (!ioctl (fd, SNDCTL_DSP_GETOSPACE, &zz))
		return zz.bytes;

	return 8192;
}

static int is_supported (int fd, int c)
{
	int devmask = 0;

	ioctl (fd, MIXER_READ (SOUND_MIXER_DEVMASK), &devmask);
	return devmask & (1 << c);
}

/* level between 0..100
   setting the sound in linux is a friggin mess, 
   what's going on:
	MIXER_VOLUME controls the volume we hear
	MIXER_PCM controls the volume of music we play
	 (in some cases MIXER_PCM is not supported by MIXER_ALTPCM is)
	MIXER_MIC controls the volume from the mic to the speakers
	MIXER_IGAIN controls the recording volume
   MIXER_VOLUME shadows MIXER_PCM and MIXER_MIC, while it seems to have no
   effect on MIXER_IGAIN.  This seems to be the situation for oss...

   XXXX: oss is completely broken for newer High Definition Audio devices!
	 we have to implement alsa mixer eventually
 */

int volume (int level, int dev)
{
	int fd = open ("/dev/mixer", O_RDONLY);
	int v1;
	int d;

	if (fd == -1) return 0;

	switch (dev) {
		case 0: d = SOUND_MIXER_VOLUME; break;
		case 1: {
			int have_pcm = is_supported (fd, SOUND_MIXER_PCM);
			int have_altpcm = is_supported (fd, SOUND_MIXER_ALTPCM);
			d = SOUND_MIXER_PCM;
			if (have_pcm) {
				if (have_altpcm)
				fprintf (stderr, "Have both PCM and ALTPCM! will set PCM only!\n");
			} else if (have_altpcm)
				d = SOUND_MIXER_ALTPCM;
			else fprintf (stderr, "Neither PCM nor ALTPCM seem to be supported by"
				" this soundcard!\n");
		}
		break;
		case 2: d = SOUND_MIXER_MIC; break;
		case 3: d = SOUND_MIXER_IGAIN; break;
	}

	if (level != -1) {
		level |= level << 8;
		ioctl (fd, MIXER_WRITE (d), &level);
	}
	ioctl (fd, MIXER_READ (d), &v1);

	close (fd);
	return v1 & 255;
}

// convert stereo to mono. In and Out can be the same buffer
void stereo2mono (short int *src, short int *dest, int nsamp)
{
	int s, d;

	for (s = d = 0; d < nsamp; s += 2, d++)
		dest [d] = src [s] / 2 + src [s + 1] / 2;
}

void stereo62mono (short int *src, short int *dest, int nsamp)
{
	int s, d, ss;

	for (s = d = 0; d < nsamp; s += 6, d++) {
		ss = src [s] + src [s+1] + src [s+2] + src [s+3] + src [s+4] + src [s+5];
		dest [d] = ss / 6;
	}
}

/* beeping utilities */
#ifdef KIOCSOUND
int beep (int fd, int hz)
{
	return ioctl (fd, KIOCSOUND, hz ? 1193180 / hz : 0);
}
#endif

void make_harmonic (short int vals[], int nvals, int hz, int volume)
{
	int i;
	for (i = 0; i < nvals; i++)
		vals [i] = volume * sin ((M_PI * 2 * i * hz) / 11025);
}

const char ABI [] =
"i open_audio	i	\n"
"- close_audio	-	\n"
"i setup_dsp	ii	\n"
"i volume	ii	\n"
"- stereo2mono	ssi	\n"
"- stereo62mono	ssi	\n"
#ifdef KIOCSOUND
"i beep		ii	\n"
#endif
"- make_harmonic siii	\n"
;
