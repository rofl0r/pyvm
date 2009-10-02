/*
 *  Audio CD Reader
 *
 *  Copyright (C) 2008 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

/* This code is for linux OS.  Patches for BSDs welcome */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>
#include <unistd.h>
#include <errno.h>

/* This works by opening the /dev/cdrom file which is most cases
   is _not_ readable by simple users.  Most distributions also include
   the cdfss module which allows mounting the audio CD as a filesystem,
   but since mainline kernel does not include this module and we can't
   be sure to find it, we go for the device.
*/

#define MAXENT 100

static int read_toc (int fd, int offsets[])
{
	int ntr;
	struct cdrom_tochdr hdr;

	/* how can we test for an audio cd? These two enough? */ 
	if (ioctl (fd, CDROMREADTOCHDR, &hdr) < 0)
		return -3;
	if (hdr.cdth_trk0 == hdr.cdth_trk1)
		return -1;

	struct cdrom_tocentry ent;
	for (ntr = hdr.cdth_trk0-1; ntr <= hdr.cdth_trk1 && ntr < MAXENT; ntr++) {
		ent.cdte_track = ntr < hdr.cdth_trk1 ? ntr + 1 : CDROM_LEADOUT;
		ent.cdte_format = CDROM_MSF;
		if (ioctl (fd, CDROMREADTOCENTRY, &ent) < 0)
			return -4;
		offsets [ntr] = ent.cdte_addr.msf.minute * 60 * 75
				+ ent.cdte_addr.msf.second * 75
				+ ent.cdte_addr.msf.frame - CD_MSF_OFFSET;
	}

	offsets [ntr] = -1;
	return 0;
}

int cdls (int fd, int ee[])
{
	return read_toc (fd, ee);
}

int opencd (char *dev)
{
	int fd = open (dev, O_RDONLY|O_NONBLOCK, 0);
	if (fd == -1) return errno == EACCES ? -1 : -2;
	return fd;
}

int eject (char *dev)
{
	int fd = open (dev, O_RDONLY|O_NONBLOCK, 0);
	if (fd == -1) return errno == EACCES ? -1 : -2;
	int ret = ioctl (fd, CDROMEJECT);
	close (fd);
	return ret < 0 ? -3 : 0;
}

void closecd (int fd)
{
	close (fd);
}

int read_frames (int fd, int offset, int n, char buffer[])
{
	struct cdrom_read_audio ra;

	ra.addr.lba = offset;
	ra.addr_format = CDROM_LBA;
	ra.nframes = n;
	ra.buf = (void*) buffer;
	return ioctl (fd, CDROMREADAUDIO, &ra);
}

const int frame_size = 2352;

const char ABI [] =
"i opencd	s	\n"
"i cdls		ip32	\n"
"- closecd	i	\n"
"i frame_size		\n"
"i read_frames	iiis	\n"
"i eject	s	\n"
;
