/*
 *  Simple Virtual framebuffer
 *
 *  Copyright (c) 2007 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

/*
 * We implement basic drawing primitives for framebuffers of
 * 8, 16, 24 and 32 bits per pixel. No alpha. The formats
 * are RGB332, RGB565, RGB888, ?RGB888
 *
 * (todo: stride for nested framebuffers)
 */

/* This is LITTLE ENDIAN only!! (iam confused) */

#include "vfb.h"

/*
 * aligned fast memset
 *  1) prefer memset on 32-bit words, aligned at 32-bit
 *  2) for big memsets, use prefetch
 * not easy to time, very sensitive. so it doesn't really matter...
 */

static inline void *memset_u32 (uint *ptr, uint c, uint n)
{
#ifdef __i386__
	uint d0, d1;
	__asm__ __volatile__ (
		"rep\n\t"
		"stosl"
		: "=&c" (d0), "=&D" (d1)
		: "a" (c), "1" (ptr), "0" (n)
		: "memory"
	);
	return ptr + n;
#else
	while (n >= 4) {
		*ptr++ = c;
		*ptr++ = c;
		*ptr++ = c;
		*ptr++ = c;
		n -= 4;
	}
	while (n--)
		*ptr++ = c;
	return ptr;
#endif
}

static void *memset_u32_big (uint *ptr, uint c, uint n) __attribute__ ((noinline));

static void *memset_u32_big (uint *ptr, uint c, uint n)
{
	/* prefetch the next block while writting on the current block.
	   over here, the value 2048 seems optimal but YMMV */
#define NN 2048
	while (n > NN) {
		__builtin_prefetch (ptr + NN, 1, 0);
		while (n >= 4) {
			*ptr++ = c;
			*ptr++ = c;
			*ptr++ = c;
			*ptr++ = c;
			n -= 4;
		}
	}
	return memset_u32 (ptr, c, n);
}

static void memset_u16 (ushort *ptr, ushort c, uint n)
{
	if ((int)ptr % 4) {
		if (!n) return;
		*ptr++ = c;
		if (!--n) return;
	}
	uint c2 = c << 16 | c;
	ptr = memset_u32 ((uint*) ptr, c2, n >> 1);
	if (n & 1)
		*ptr = c;
}

static void memset_u16_big (ushort *ptr, ushort c, uint n)
{
	if ((int)ptr % 4) {
		if (!n) return;
		*ptr++ = c;
		if (!--n) return;
	}
	uint c2 = c << 16 | c;
	ptr = memset_u32_big ((uint*) ptr, c2, n >> 1);
	if (n & 1)
		*ptr = c;
}

static void memset_u8 (uchar *ptr, uchar c, uint n)
{
	if (!n) return;
	while ((int)ptr % 4) {
		*ptr++ = c;
		if (!--n) return;
	}
	uint c2 = c << 24 | c << 16 | c << 8 | c;
	ptr = memset_u32 ((uint*) ptr, c2, n >> 2);
	switch (n & 3) {
		case 3: *ptr++ = c;
		case 2: *ptr++ = c;
		case 1: *ptr = c;
	}
}

static void memset_u8_big (uchar *ptr, uchar c, uint n)
{
	if (!n) return;
	while ((int)ptr % 4) {
		*ptr++ = c;
		if (!--n) return;
	}
	uint c2 = c << 24 | c << 16 | c << 8 | c;
	ptr = memset_u32_big ((uint*) ptr, c2, n >> 2);
	switch (n & 3) {
		case 3: *ptr++ = c;
		case 2: *ptr++ = c;
		case 1: *ptr = c;
	}
}

static void memset_u24 (u24 *ptr, u24 c, uint n)
{
	if (!n) return;
	while ((int)ptr % 4) {
		*ptr++ = c;
		if (!--n) return;
	}

	uint *ptr2 = (uint*)ptr;
	uint c21 = c.r | c.g << 8 | c.b << 16 | c.r << 24;
	uint c22 = c.g | c.b << 8 | c.r << 16 | c.g << 24;
	uint c23 = c.b | c.r << 8 | c.g << 16 | c.b << 24;
	uint n2 = n >> 2;
	while (n2--) {
		*ptr2++ = c21;
		*ptr2++ = c22;
		*ptr2++ = c23;
	}
	n = n & 3;
	ptr = (u24*)ptr2;
	while (n--) *ptr++ = c;
}

#define memset_u24_big memset_u24

/*
 * fb primitives
 */

static void fb_draw_vline (fb *f, int x, int y1, int y2, RRGGBB col);

const int sizeof_fb = sizeof (fb);

void fb_init (fb *f, uint w, uint h, int d, uchar *fb)
{
	f->w = w;
	f->h = h;
	f->depth = d;
	f->fb = fb;
}

void fb_fill_rect (fb *f, int x, int y, int w, int h, RRGGBB col)
{
	int i;

	if (x < 0) {
		w += x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		y = 0;
	}
	if (x + w > SCRX)
		w = SCRX - x;
	if (y + h > SCRY)
		h = SCRY - y;

	if (w <= 0 || h <= 0 || w > SCRX || h > SCRY)
		return;

	if (w == 1) {
		fb_draw_vline (f, x, y, y+h, col);
		return;
	}

	if (w == SCRX) {
		switch (f->depth) {
		case 1: {
			uchar c = pack8 (col);
			uchar *buf = f->fb8;
			buf += y * SCRX;
			memset_u8_big (buf, c, w * h);
		} ncase 2: {
			ushort c = pack16 (col);
			ushort *buf = f->fb16;
			buf += y * SCRX;
			memset_u16_big (buf, c, w * h);
		} ncase 3: {
			u24 c = pack24 (col);
			u24 *buf = f->fb24;
			buf += y * SCRX;
			memset_u24_big (buf, c, w * h);
		} ndefault: {
			uint c = pack32 (col);
			uint *buf = f->fb32;
			buf += y * SCRX;
			memset_u32_big (buf, c, w * h);
		}}
		return;
	}

	unsigned int STRIDE = SCRX;

	switch (f->depth) {
	case 1: {
		uchar c = pack8 (col);
		uchar *buf = f->fb8;
		buf += x + y * STRIDE;
		for (i = 0; i < h; i++, buf += STRIDE)
			memset_u8 (buf, c, w);
	} ncase 2: {
		ushort c = pack16 (col);
		ushort *buf = f->fb16;
		buf += x + y * STRIDE;
		for (i = 0; i < h; i++, buf += STRIDE)
			memset_u16 (buf, c, w);
	} ncase 3: {
		u24 c = pack24 (col);
		u24 *buf = f->fb24;
		buf += x + y * STRIDE;
		for (i = 0; i < h; i++, buf += STRIDE)
			memset_u24 (buf, c, w);
	} ndefault: {
		uint c = pack32 (col);
		uint *buf = f->fb32;
		buf += x + y * STRIDE;
		for (i = 0; i < h; i++, buf += STRIDE)
			memset_u32 (buf, c, w);
	}}
}

void fb_put_pixel (fb *f, uint x, uint y, RRGGBB col)
{
	if (x > SCRX || y > SCRY)
		return;

	switch (f->depth) {
	case 1: {
		uchar ccol = pack8 (col);
		f->fb8 [x + y * SCRX] = ccol;
	} ncase 2: {
		ushort ccol = pack16 (col);
		f->fb16 [x + y * SCRX] = ccol;
	} ncase 3: {
		u24 ccol = pack24 (col);
		f->fb24 [x + y * SCRX] = ccol;
	} ndefault: {
		uint ccol = pack32 (col);
		f->fb32 [x + y * SCRX] = ccol;
	}}
}

RRGGBB fb_get_pixel (fb *f, uint x, uint y)
{
	uchar r, g, b;
	if (x > SCRX || y > SCRY)
		return -1;

	x = x + SCRX * y;
	switch (f->depth) {
	case 1: {
		return f->fb8 [x];
//		r = f->fb8 [x] & 0xe0;
//		g = (f->fb8 [x] << 3) & 0xe0;
//		b = f->fb8 [x] << 6;
	} ncase 2: {
		r = (f->fb16 [x] >> 8) & 0xf8;
		g = (f->fb16 [x] >> 3) & 0xfc;
		b = (f->fb16 [x] << 3) & 0xf8;
	} ncase 3: {
		r = f->fb24 [x].r;
		g = f->fb24 [x].g;
		b = f->fb24 [x].b;
	} ndefault: {
		uchar *c = (uchar*) &f->fb32 [x];
		r = c [2];
		g = c [1];
		b = c [0];
	}}

	return (r << 16) | (g << 8) | b;
}

static void fb_draw_vline (fb *f, int x, int y1, int y2, RRGGBB col)
{
	int i;

	if (y1 > y2) {
		i = y1;
		y1 = y2;
		y2 = i;
	}
	if (x < 0 || x >= f->w || y1 >= f->h || y2 < 0)
		return;
	if (y1 < 0) y1 = 0;
	if (y2 > f->h) y2 = f->h;

	unsigned int STRIDE = SCRX;
#undef DRAW_LINE
#define DRAW_LINE \
	screen += x + y1 * SCRX;\
	for (i = y1; i < y2; i++, screen += STRIDE)\
		*screen = ccol;

	switch (f->depth) {
	case 1: {
		uchar ccol = pack8 (col);
		uchar *screen = f->fb8;
		DRAW_LINE
	} ncase 2: {
		ushort ccol = pack16 (col);
		ushort *screen = f->fb16;
		DRAW_LINE
	} ncase 3: {
		u24 ccol = pack24 (col);
		u24 *screen = f->fb24;
		DRAW_LINE
	} ndefault: {
		uint ccol = pack32 (col);
		uint *screen = f->fb32;
		DRAW_LINE
	}}
}

static void fb_draw_hline (fb *f, int y, int x1, int x2, uint col)
{
	int i;

	if (x1 > x2) {
		i = x1;
		x1 = x2;
		x2 = i;
	}
	if (y < 0 || y >= f->h || x1 >= f->w || x2 < 0)
		return;
	if (x1 < 0) x1 = 0;
	if (x2 >= f->w) x2 = f->w - 1;

	switch (f->depth) {
	case 1: {
		uchar ccol = pack8 (col);
		uchar *screen = f->fb8;
		memset_u8 (screen + x1 + y * SCRX, ccol, x2 - x1);
	} ncase 2: {
		ushort ccol = pack16 (col);
		ushort *screen = f->fb16;
		memset_u16 (screen + x1 + y * SCRX, ccol, x2 - x1);
	} ncase 3: {
		u24 ccol = pack24 (col);
		u24 *screen = f->fb24;
		memset_u24 (screen + x1 + y * SCRX, ccol, x2 - x1);
	} ndefault: {
		uint ccol = pack32 (col);
		uint *screen = f->fb32;
		memset_u32 (screen + x1 + y * SCRX, ccol, x2 - x1);
	}}
}

#define CLIP_LEFT_EDGE   0x1
#define CLIP_RIGHT_EDGE  0x2
#define CLIP_BOTTOM_EDGE 0x4
#define CLIP_TOP_EDGE    0x8
#define CLIPX (f->w-1)
#define CLIPY (f->h-1)

static int clipEncode (fb *f, int x, int y)
{
	int code = 0;

	if (x < 0) code = CLIP_LEFT_EDGE;
	else if (x > CLIPX) code = CLIP_RIGHT_EDGE;
	if (y < 0) code |= CLIP_TOP_EDGE;
	else if (y > CLIPY) code |= CLIP_BOTTOM_EDGE;

	return code;
}

static int clip_line (fb *f, int *px1, int *py1, int *px2, int *py2)
{
#define MX (y2 - y1) / (x2 - x1)
#define MY (x2 - x1) / (y2 - y1)

	int oth = 1;
	int x1, y1, x2, y2, x, y;
	int code;
other:
	x1 = *px1, y1 = *py1, x2 = *px2, y2 = *py2;
	x = x1, y = y1;
	if ((code = clipEncode (f, x1, y1))) {
		if (code & CLIP_LEFT_EDGE) {
			y = y1 + ((0 - x1) * MX);
			if (y >= 0 && y <= CLIPY) {
				x = 0;
				goto ok;
			}
		} else if (code & CLIP_RIGHT_EDGE) {
			y = y1 + ((CLIPX - x1) * MX);
			if (y >= 0 && y <= CLIPY) {
				x = CLIPX;
				goto ok;
			}
		}
		if (code & CLIP_TOP_EDGE) {
			x = x1 + ((0 - y1) * MY);
			if (x >= 0 && x <= CLIPX) {
				y = 0;
				goto ok;
			}
		} else if (code & CLIP_BOTTOM_EDGE) {
			x = x1 + ((CLIPY - y1) * MY);
			if (x >= 0 && x <= CLIPX) {
				y = CLIPY;
				goto ok;
			}
		}
		return 0;
	ok:
		*px1 = x;
		*py1 = y;
	}
	if (oth) {
		int *t;
		oth = 0;
		t = px1; px1 = px2; px2 = t;
		t = py1; py1 = py2; py2 = t;
		goto other;
	}
	return 1;
}

void fb_draw_line (fb *f, int x1, int y1, int x2, int y2, RRGGBB col)
{
	if (x1 == x2) {
		fb_draw_vline (f, x1, y1, y2, col);
		return;
	}

	if (y1 == y2) {
		fb_draw_hline (f, y1, x1, x2, col);
		return;
	}

	int x, y, dx, dy, sx, sy;
	int pixx, pixy;
	if (!clip_line (f, &x1, &y1, &x2, &y2))
		return;

	dx = x2 - x1;
	dy = y2 - y1;

	pixx = sx = dx > 0 ? 1 : -1;
	pixy = SCRX * (sy = dy > 0 ? 1 : -1);
	dx = sx * dx + 1;
	dy = sy * dy + 1;
	if (dx < dy) {
		int s = pixx;
		pixx = pixy;
		pixy = s;
		s = dx;
		dx = dy;
		dy = s;
	}

#undef DRAW_LINE
#define DRAW_LINE \
	screen += x1 + SCRX * y1;\
	for (x = y = 0; x < dx; x++, screen += pixx) {\
		screen [0] = ccol;\
		y += dy;\
		if (y >= dx) {\
			y -= dx;\
			screen += pixy;\
		}\
	}

	switch (f->depth) {
	case 1: {
		uchar ccol = pack8 (col);
		uchar *screen = f->fb8;
		DRAW_LINE
	} ncase 2: {
		ushort ccol = pack16 (col);
		ushort *screen = f->fb16;
		DRAW_LINE
	} ncase 3: {
		u24 ccol = pack24 (col);
		u24 *screen = f->fb24;
		DRAW_LINE
	} ndefault: {
		uint ccol = pack32 (col);
		uint *screen = f->fb32;
		DRAW_LINE
	}}
}

void fb_to_rgb (fb *f, u24 out[])
{
	int i, n;
	n = f->w * f->h;

	switch (f->depth) {
	case 1: {
		uchar *buf = f->fb8;
		for (i = 0; i < n; i++) {
			out [i].r = out [i].g = out [i].b = *buf++;
/*
			b = *buf++;
#if 1
			out [i].r = b & 0xc0;
			out [i].g = (b << 3) & 0xe0;
			out [i].b = b << 6;
#else
			int c = b & 0xc0;
			out [i].r = c | (c >> 2) | (c >> 4) | (c >> 6);
			c = (b << 3) & 0xe0;
			out [i].g = c | (c >> 3) | (c >> 6);
			c = b << 6;
			out [i].b = c | (c >> 2) | (c >> 4) | (c >> 6);
#endif
*/
		}
	} ncase 2: {
		ushort *buf = f->fb16, b;
		for (i = 0; i < n; i++) {
			b = *buf++;
			out [i].r = (b >> 8) & 0xf8;
			out [i].g = (b >> 3) & 0xfc;
			out [i].b = (b << 3) & 0xf8;
		}
	} ncase 3: {
		u24 *buf = f->fb24;
		memcopy (out, buf, n * sizeof *buf);
	} ndefault: {
		uint *buf = f->fb32;
		for (i = 0; i < n; i++) {
			uchar *cc = (uchar*)buf++;
			out [i].r = cc [2];
			out [i].g = cc [1];
			out [i].b = cc [0];
		}
	}}
}

/* bitBLT, aka "blit" aka "graphics memcpy". Without transparency or blending.
   This is one of the most heavily used functions in the low level graphics
*/
static inline void blit (uchar *dest, uint dstride, const uchar *src, uint sstride, uint w, uint h)
{
	if (dstride == w && sstride == w)
		memcopy (dest, src, w * h);
	else while (h--) {
		memcopy (dest, src, w);
		dest += dstride;
		src += sstride;
	}
}

/* data is a surface of width `w` and height `h`.
   Blit a subimage at `sx`, `sx`, `sw`, `sh`
   to `x`, `y`.
   Formats compatible
*/
void fb_put_image (fb *f, const uchar * __restrict__ data, int x, int y, int sx, int sy, int w, int h, int sw, int sh)
{
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

	blit (dst, d1 * f->depth, data, w * f->depth, sw * f->depth, sh);
}

/* with alpha table (that is either 100% or 0%) */
void fb_put_image_alpha01 (fb *f, const uchar * __restrict__ data, int x, int y, int sx, int sy, int w, int h, int sw, int sh, const uchar alpha[])
{
	int i, j;
	uint d1, d2;
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
	alpha += sy*w+sx;

#define PUT_IMAGE(T) {\
	typedef T *type;\
	type DST = (type)dst;\
	type SRC = (type)data;\
	const uchar *ALPHA = alpha;\
	d2 = sw * sizeof *DST;\
	for (i = 0; i < sh; i++, DST += d1, SRC += w, ALPHA += w)\
		for (j = 0; j < sw; j++)\
			if (ALPHA [j] != 255)\
				DST [j] = SRC [j];\
	}

	switch (f->depth) {
		case 1:   PUT_IMAGE (uchar)
		ncase 2:  PUT_IMAGE (ushort)
		ncase 3:  PUT_IMAGE (u24)
		ndefault: PUT_IMAGE (uint)
	}
#undef PUT_IMAGE
}

/* with alpha table blending */
void fb_put_image_alpha (fb *f, const uchar * __restrict__ data, int x, int y, int sx, int sy, int w, int h, int sw, int sh, const uchar alpha[])
{
	int i, j;
	uint d1, d2;
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
	alpha += sy*w+sx;

#define PUT_IMAGE(T) {\
	typedef T *type;\
	type DST = (type)dst;\
	type SRC = (type)data;\
	const uchar *ALPHA = alpha;\
	d2 = sw * sizeof *DST;\
	for (i = 0; i < sh; i++, DST += d1, SRC += w, ALPHA += w)\
		for (j = 0; j < sw; j++)\
			if (!ALPHA [j])\
				DST [j] = SRC [j];\
			else if (ALPHA [j] != 255) {\
				int dR, dG, dB;\
				int sR, sB, sG;\
				int r, g, b;\
				int a = 255 - ALPHA [j];\
				get_rgb_ ## T (DST + j, &dR, &dG, &dB);\
				get_rgb_ ## T (SRC + j, &sR, &sG, &sB);\
				r = (((sR - dR) * a) >> 8) + dR;\
				g = (((sG - dG) * a) >> 8) + dG;\
				b = (((sB - dB) * a) >> 8) + dB;\
				put_rgb_ ## T (DST + j, r, g, b);\
			}\
	}

	switch (f->depth) {
		ncase 2:  PUT_IMAGE (ushort)
		ncase 3:  PUT_IMAGE (u24)
		ndefault: PUT_IMAGE (uint)
	}
#undef PUT_IMAGE
}

void fb_put_image_ckey (fb *f, const uchar * __restrict__ data, int x, int y, int sx, int sy, int w, int h, int sw, int sh, RRGGBB colorkey)
{
	int i, j;
	uint d1, d2;
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

#define PUT_IMAGE(T) {\
	typedef T *type;\
	type DST = (type)dst;\
	type SRC = (type)data;\
	T KEY = pack_ ## T (colorkey);\
	d2 = sw * sizeof *DST;\
	for (i = 0; i < sh; i++, DST += d1, SRC += w)\
		for (j = 0; j < sw; j++)\
			if (!cmp_ ## T (SRC [j], KEY))\
				DST [j] = SRC [j];\
	}

	switch (f->depth) {
		case 1:   PUT_IMAGE (uchar)
		ncase 2:  PUT_IMAGE (ushort)
		ncase 3:  PUT_IMAGE (u24)
		ndefault: PUT_IMAGE (uint)
	}
#undef PUT_IMAGE
}

int fb_get_image (fb *f, uchar * __restrict__ dest, int x, int y, int w, int h)
{
	if (x > f->w || y > f->h)
		return 1;
	if (x + w > f-> w || y + h > f->h)
		return 1;

	// (xxx: use blit())
	const uchar *src = f->fb8 + (x + y * f->w) * f->depth;

	if (w == f->w) {
		memcopy (dest, src, w * h * f->depth);
		return 0;
	}

	while (h--) {
		memcopy (dest, src, w * f->depth);
		dest += w * f->depth;
		src += f->w * f->depth;
	}

	return 0;
}

#define memmove __builtin_memmove

void fb_vscroll (fb *f, int dy)
{
	int stride = f->w * f->depth;

	if (dy > 0) {
		if (dy < f->h)
			memmove (f->fb8, f->fb8 + stride * dy, stride * (f->h - dy));
	} else if (-dy < f->h)
		 memmove (f->fb8 - stride * dy, f->fb8, stride * (f->h + dy));
}

int fb_vmove (fb *f, int destline, int srcline, int nlines)
{
	int stride = f->w * f->depth;

	if (nlines < 0 || destline < 0 || srcline < 0
	|| destline + nlines > SCRY || srcline + nlines > SCRY)
		return 1;

	memmove (f->fb8 + destline * stride, f->fb8 + srcline * stride, nlines * stride);
	return 0;
}

void fb_copy (fb *f, char *dest)
{
	memcopy (dest, f->fb, f->w * f->h * f->depth);
}

const char ABI [] =
"i sizeof_fb				\n"
"- fb_init		siiiv		\n"
"- fb_fill_rect		siiiii		\n"
"- fb_put_pixel		siii		\n"
"i fb_get_pixel		sii		\n"
"- fb_draw_line		siiiii		\n"
"- fb_to_rgb		ss		\n"
"- fb_put_image		sviiiiiiii	\n"
"- fb_put_image_ckey	sviiiiiiiii	\n"
"- fb_put_image_alpha01	sviiiiiiiis	\n"
"- fb_put_image_alpha	sviiiiiiiis	\n"
"i fb_get_image		ssiiii		\n"
"- fb_vscroll		si		\n"
"- fb_vmove		siii		\n"
"- fb_copy		ss		\n"
;

const char __DEPENDS__ [] = "rgb.h, vfb.h";
