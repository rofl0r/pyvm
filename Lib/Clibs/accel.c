/*
 *  Graphics hardware acceleration for linuxfb.
 *  (This will be obsolete when the kernel exposes its acceleration
 *  functions to userspace through a very simple ioctl.  until then
 *  we duplicate the kernel code in userland)
 *
 *  SiS card
 *  Copyright (C) 2007 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

/*
 We care about fillrect and blit.  Later we may do YUV/scale.

 It seems that all cards do support these simple functions.
 The way to command the graphics card is: mmap some area (the MMIO area)
 and then read/write to it as volatile memory.

 In order to hack in your card you'll need.

 - The sources of your kernel (a recent one).
 - DirectFB (or SDL).

 1. Print the value of f.accel (while in linuxfb.c/init)
 2. Find the entry in linux/fb.h (for example FB_ACCEL_SIS_GLAMOUR_2)
 3. Search DirectFB/gfxdrivers or SDL/src/video/fbcon for your card.
    Also look into linux/drivers/video for the card.  verify.
 4. Follow the pattern of the sis card and add the functions

 This can be dangerous hacking that may lock the computer until it works!
*/

typedef unsigned int uint;

struct accel_funcs
{
	void (*fillrect) (uint, uint, uint, uint, uint);
	void (*imageblit) (void*, uint, uint, uint, uint, uint, uint, uint, uint);
};

//////////////////////////////// SiS cards ////////////////////////////////

#ifdef FB_ACCEL_SIS_GLAMOUR_2
static void sis_wl (uint offset, uint value)
{
	*(volatile uint*)(D.mmio + offset) = value;
}
static uint sis_rl (uint offset)
{
	return *(volatile uint*)(D.mmio + offset);
}

/* registers */
#define SIS_PAT_FGCOLOR		0x821C
#define SIS_DST_Y		0x820C
#define SIS_RECT_WIDTH		0x8218
#define SIS_CMD_READY		0x823C
#define SIS_FIRE_TRIGGER	0x8240
#define SIS_DST_PITCH		0x8214
#define SIS_DST_ADDR		0x8210
#define SIS_CMD_QUEUE_STATUS	0x85CC

#define SIS_SRC_ADDR 0x8200
#define SIS_SRC_PITCH 0x8204
#define SIS_SRC_Y 0x8208

/* flags */
#define SIS_CMD_PATFG	0x00000000
#define SIS_CMD_BITBLT	0x00000000
#define SIS_CMD_BPP8	0x00000000
#define SIS_CMD_BPP16	0x00010000
#define SIS_CMD_BPP32	0x00020000

#define SIS_CMD_SRC_SYSTEM 0x00000010

#define SIS_ROP_COPY 0xCC
#define SIS_ROP_COPY_PAT 0xF0

static void sis_idle ()
{
	while (!(sis_rl (SIS_CMD_QUEUE_STATUS) & 0x80000000));
}

static void sis_cmd (uint flags)
{
	sis_wl (SIS_CMD_READY, flags);
	sis_wl (SIS_FIRE_TRIGGER, 0);
}

static inline int BPP ()
{
	return D.bpp == 1 ? SIS_CMD_BPP8 : D.bpp == 2 ? SIS_CMD_BPP16 : SIS_CMD_BPP32;
}

static void sis_fillrect (uint x, uint y, uint w, uint h, uint c)
{
	sis_wl (SIS_PAT_FGCOLOR, c);
	/* should increase if y >= 2048 */
	sis_wl (SIS_DST_ADDR, 0);
	sis_wl (SIS_DST_PITCH, (0xffff << 16) | (D.xres * D.bpp));
	sis_wl (SIS_DST_Y, (x << 16) | y);
	sis_wl (SIS_RECT_WIDTH, (h << 16) | w);

	sis_cmd (SIS_CMD_PATFG			| // for the PAT_FGCOLOR
		 SIS_CMD_BITBLT 		| // to fill
		 (SIS_ROP_COPY_PAT << 8) 	| // ROP_COPY_PAT (dst==src==pat)
		 BPP ()
	);
	sis_idle ();
}

// DOESN'T WORK. LOCKS COMPUTER
/*
 Neither X11, nor the kernel use hw accelerated imageblit.
 They only implement screen-to-screen copy. (probably directFB too)

 I don't know why.  Sis flags has a value SIS_SRC_SYSTEM which
 probably means that the source is in system memory, so we
 guess that the card can do system-to-video blitting.

 The code below was an attempt to do this.  But it locks the computer.
 Maybe also the system memory should be aligned at 16 bytes.  Maybe it
 should be physical memory?  SiS cards generally provide no specs for
 OSS drivers, so they are not good for video hacking :(
*/
/* copy (sub)image from system memory to the screen.  Image formats compatible.
   Parameters must be prepared to not overflow the screen.
	x, y	: where to put the image
	w, h	: size of image
	sx, sy	: source-x, source-y offsets in the image
	sw, sh	: source image dimensions
	data	: image data
   Normally sx==sy==0 and sw==w and sh==h, and the entire image is blitted.
   ToDo:
   - maybe we can use AGP?
   - can it do format conversion?
*/
#if 0
static void sis_imageblit (void *data, uint x, uint y, uint sx, uint sy, uint w, uint h, uint sw, uint sh)
{
	sis_wl (SIS_DST_ADDR, 0);
	sis_wl (SIS_DST_PITCH, (0xffff << 16) | (D.xres * D.bpp));
	sis_wl (SIS_DST_Y, (x << 16) | y);
	sis_wl (SIS_RECT_WIDTH, (h << 16) | w);

	sis_wl (SIS_SRC_ADDR, (uint) data);
	sis_wl (SIS_SRC_PITCH, (sh << 16) | (sw * D.bpp));
	sis_wl (SIS_SRC_Y, (sx << 16) | sy);

	sis_cmd (SIS_CMD_SRC_SYSTEM | SIS_CMD_BITBLT | (SIS_ROP_COPY << 8) | BPP ());
	sis_idle ();
}
#endif

static struct accel_funcs
SIS_GLAMOUR_2 = {
	sis_fillrect,
	0
};

#undef SIS_PAT_FGCOLOR
#undef SIS_DST_Y
#undef SIS_RECT_WIDTH
#undef SIS_CMD_READY
#undef SIS_FIRE_TRIGGER	
#undef SIS_DST_PITCH
#undef SIS_DST_ADDR
#undef SIS_CMD_QUEUE_STATUS
#undef SIS_CMD_PATFG
#undef SIS_CMD_BITBLT
#undef SIS_CMD_BPP8
#undef SIS_CMD_BPP16
#undef SIS_CMD_BPP32
#endif
///////////////////////////////////////////////////////////////////////////

static struct accel_funcs *can_accel (unsigned int type)
{
	switch (type) {
#ifdef FB_ACCEL_SIS_GLAMOUR_2
		case FB_ACCEL_SIS_GLAMOUR_2:
			return &SIS_GLAMOUR_2;
#endif
	}
	return 0;
}
