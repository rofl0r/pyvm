/*
 *  Extended Virtual framebuffer routines
 *
 *  Copyright (c) 2007 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

/* These routines are more useful for pygame and similar stuff */

#include "vfb.h"

/* same thing with a transparent colorkey, useful for pygame.  */

void fb_put_image_ckey (fb *f, const uchar * __restrict__ data, int x, int y, int sx, int sy, int w,
			int h, int sw, int sh, RRGGBB col)
{
	int i, j;
	uint d1;
	uchar *dst;

	if (sx < 0) {
		x -= sx;
		sx = 0;
	}

	if (sy < 0) {
		y -= sy;
		sy = 0;
	}

	if (w <= 0 || h <= 0) return;

	if (x < 0) {
		sx -= x;
		sw += x;
		x = 0;
	}
	if (y < 0) {
		sy -= y;
		sh += y;
		y = 0;
	}

	if (x > f->w || y > f->h)
		return;

	if (x + sw > f->w)
		sw = f->w - x;
	if (y + sh > f->h)
		sh = f->h - y;

	if (sx > w || sy > h)
		return;
	if (sx + sw > w)
		sw = w - sx;
	if (sy + sh > h)
		sh = h - sy;
	if (sh <= 0 || sw <= 0)
		return;

	d1 = f->w;
	dst = f->fb8 + (y * f->w + x) * f->depth;
	data += (sy * w + sx) * f->depth;

#define PUT_IMAGE(type) {\
	type KEY = pack_ ## type (col);\
	type *DST = (type*)dst;\
	type *SRC = (type*)data;\
	for (i = 0; i < sh; i++, DST += d1, SRC += w)\
		for (j = 0; j < sw; j++)\
			if (!cmp_ ## type (SRC [j], KEY))\
				DST [j] = SRC [j];\
	}

	switch (f->depth) {
	case 1: PUT_IMAGE(uchar)	break;
	case 2: PUT_IMAGE(ushort)	break;
	case 3: PUT_IMAGE (u24)		break;
	case 4: PUT_IMAGE(uint)		break;
	}
}

void flip (const uchar *in, uint w, uint h, uint bpp, uchar * __restrict__ outdata,
	   int xaxis, int yaxis)
{
	uint x, y;
	uint pitch = w * bpp;

	if (!xaxis) {
		if (!yaxis) {
			memcopy (outdata, in, w * h * bpp);
			return;
		}

		uchar *out = outdata + (h - 1) * pitch;
		for (y = 0; y < h; y++, out -= pitch, in += pitch)
			memcopy (out, in, pitch);
		return;
	}

	uchar *out;
	int dout;

	if (!yaxis) {
		out = outdata + pitch - bpp;
		dout = 2 * w;
	} else {
		out = outdata + pitch * h - bpp;
		dout = 0;
	}

#define FLIP(type) {\
	type *out2 = (type*) out, *in2 = (type*) in;\
	for (y = 0; y < h; y++) {\
		for (x = 0; x < w; x++)\
			*out2-- = *in2++;\
		out2 += dout;\
	}}

	switch (bpp) {
	case 1: FLIP (uchar)	break;
	case 2: FLIP (ushort)	break;
	case 3: FLIP (u24)	break;
	case 4: FLIP (uint)	break;
	}
}

/*
   From pygame/src/scale2x.c which credits:

   This implements the AdvanceMAME Scale2x feature found on this page,
   http://advancemame.sourceforge.net/scale2x.html
*/

#define MAX(a,b)    (((a) > (b)) ? (a) : (b))
#define MIN(a,b)    (((a) < (b)) ? (a) : (b))

#define READINT24(x)      ((x)[0]<<16 | (x)[1]<<8 | (x)[2]) 
#define WRITEINT24(x, i)  {(x)[0]=i>>16; (x)[1]=(i>>8)&0xff; x[2]=i&0xff; }

void scale2x (const uchar *srcpix, uchar *dstpix, uint width, uint height, uint bpp)
{
	int looph, loopw;

	const int srcpitch = width * bpp;
	const int dstpitch = 2 * srcpitch;

#define LOOP(T) \
	{\
		typedef T type;\
	    	type E0, E1, E2, E3, B, D, E, F, H;\
		for (looph = 0; looph < height; ++looph) {\
			for (loopw = 0; loopw < width; ++loopw) {

#define READVALS(type) \
	B = *(type*)(srcpix + (MAX(0,looph-1)*srcpitch) + (sizeof (type) *loopw));\
	D = *(type*)(srcpix + (looph*srcpitch) + (sizeof(type)*MAX(0,loopw-1)));\
	E = *(type*)(srcpix + (looph*srcpitch) + (sizeof(type)*loopw));\
	F = *(type*)(srcpix + (looph*srcpitch) + (sizeof(type)*MIN(width-1,loopw+1)));\
	H = *(type*)(srcpix + (MIN(height-1,looph+1)*srcpitch) + (sizeof(type)*loopw));

#define SELECT \
	E0 = D == B && B != F && D != H ? D : E;\
        E1 = B == F && B != D && F != H ? F : E;\
	E2 = D == H && D != B && H != F ? D : E;\
	E3 = H == F && D != H && B != F ? F : E;

#define WRITEVALS(type) \
	*(uint*)(dstpix + looph*2*dstpitch + loopw*2*sizeof(type)) = E0;\
	*(uint*)(dstpix + looph*2*dstpitch + (loopw*2+1)*sizeof(type)) = E1;\
	*(uint*)(dstpix + (looph*2+1)*dstpitch + loopw*2*sizeof(type)) = E2;\
	*(uint*)(dstpix + (looph*2+1)*dstpitch + (loopw*2+1)*sizeof(type)) = E3;

#define ENDLOOP }}}

	switch (bpp) {
	case 1:
		LOOP (uchar)
			READVALS(uchar)
			SELECT
			WRITEVALS(uchar)
		ENDLOOP
	ncase 2:
		LOOP (ushort)
			READVALS(ushort)
			SELECT
			WRITEVALS(ushort)
		ENDLOOP
	ncase 3: 
		LOOP (int)
		    	B = READINT24(srcpix + (MAX(0,looph-1)*srcpitch) + (3*loopw));
		    	D = READINT24(srcpix + (looph*srcpitch) + (3*MAX(0,loopw-1)));
		    	E = READINT24(srcpix + (looph*srcpitch) + (3*loopw));
		    	F = READINT24(srcpix + (looph*srcpitch) + (3*MIN(width-1,loopw+1)));
		    	H = READINT24(srcpix + (MIN(height-1,looph+1)*srcpitch) + (3*loopw));
			SELECT
			WRITEINT24((dstpix + looph*2*dstpitch + loopw*2*3), E0);
			WRITEINT24((dstpix + looph*2*dstpitch + (loopw*2+1)*3), E1);
			WRITEINT24((dstpix + (looph*2+1)*dstpitch + loopw*2*3), E2);
			WRITEINT24((dstpix + (looph*2+1)*dstpitch + (loopw*2+1)*3), E3);
		ENDLOOP
	ndefault:
		LOOP (uint)
			READVALS(uint)
			SELECT
			WRITEVALS(uint)
		ENDLOOP
	}
}

/* Rotation, from pygame/src/transform.c */

void rotate (const void *src, int sw, int sh, void *dst, int dw, int dh, int bpp,
	     RRGGBB bgcolor, double sangle, double cangle)
{
	int x, y, dx, dy;
	int cy = dh / 2;
	int xd = (sw - dw) << 15;
	int yd = (sh - dh) << 15;
	int isin = (int) (sangle * 65536);
	int icos = (int) (cangle * 65536);
	int ax = (dw << 15) - (int) (cangle * ((dw - 1) << 15));
	int ay = (dh << 15) - (int) (sangle * ((dw - 1) << 15));
	int xmaxval = (sw << 16) - 1;
	int ymaxval = (sh << 16) - 1;

#define ROT(type) {\
	type *sp = (type*) src;\
	type *dp = (type*) dst;\
	type bg = pack_ ## type (bgcolor);\
	for (y = 0; y < dh; y++) {\
		dx = (ax + (isin * (cy - y))) + xd;\
		dy = (ay - (icos * (cy - y))) + yd;\
		for (x = 0; x < dw; x++) {\
			*dp++ = dx < 0 || dy < 0 || dx > xmaxval || dy > ymaxval\
				? bg : sp [(dy >> 16) * sw + (dx >> 16)];\
			dx += icos;\
			dy += isin;\
		}\
	}}

	switch (bpp) {
	case 1: ROT (uchar);	break;
	case 2: ROT (ushort);	break;
	case 3: ROT (u24);	break;
	case 4: ROT (uint);	break;
	}
}

/* recolor, change a color to another */

void recolor (uchar *dst, int wxh, int bpp, RRGGBB col1, RRGGBB col2)
{
	int i;

#define RECOLOR(type) {\
	type C1 = pack_ ## type (col1);\
	type C2 = pack_ ## type (col2);\
	type *DATA = (type*)dst;\
	for (i = 0; i < wxh; i++)\
		if (cmp_ ## type (DATA [i], C1))\
			DATA [i] = C2;\
	}

	switch (bpp) {
	case 1: RECOLOR(uchar)	break;
	case 2: RECOLOR(ushort)	break;
	case 3: RECOLOR(u24)	break;
	case 4: RECOLOR(uint)	break;
	}
}

const char ABI [] =
//"- fb_put_image_ckey	ssiiiiiiiii	\n"
"- flip			siiisii		\n"
"- scale2x		ssiii		\n"
"- rotate		siisiiiidd	\n"
"- recolor		siiii		\n"
;
