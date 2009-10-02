/*
 *  Image utilities
 *
 *  Copyright (c) 2007, Stelios Xanthakis
 *  Uses MMX routines from libswscale, written by Nick Kurshev
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

#include "vfb.h"

#ifdef PYVM_MMX
#include "mmx.h"
#include "rgb24tobgr16.h"

static void rgb24tobgr16_mmx (const uint8_t *src, uint8_t *dst, long src_size)
{
	const uint8_t *s = src;
	const uint8_t *end;
	const uint8_t *mm_end;
	uint16_t *d = (uint16_t *)dst;

	end = s + src_size;
	mm_end = end - 15;
	rgb24tobgr16_mmx_init ();

	while (s < mm_end) {
		rgb24tobgr16_mmx_quantum (s, d);
		d += 4;
		s += 12;
	}

	emms ();
}

static int do_mmx = 0;

void enable_mmx ()
{
	do_mmx = 1;
}
#endif

/*
 * convert a 24-bit rgb buffer to a 32/16/8 bit buffer.
 *
 * depth	  	bits
 * ---------------------------------------------------
 * 8		grayscale
 * 16	  	rrrrrggg gggbbbbb
 * 32		aaaaaaaa rrrrrrrr gggggggg bbbbbbbb
 */
void rgb2depth (int depth, int size, unsigned char * __restrict__ in, unsigned char * __restrict__ out)
{
	unsigned int r, g, b;
	if (depth == 1)
		while (size--) {
			r = *in++;
			g = *in++;
			b = *in++;
			*out++ = (r & 0xc0) | ((g & 0xe0) >> 3) | (b >> 5);
		}
	else if (depth == 2) {
#ifdef PYVM_MMX
		/* "emms" too costly for very small image buffers */
		if (size > 256 && do_mmx) {
			rgb24tobgr16_mmx (in, out, size * 3);
			in += 3 * (size - 4);
			out += 2 * (size - 4);
			size = 4;
		}
#endif
		unsigned short *s = (unsigned short*) out;
		while (size--) {
			r = *in++;
			g = *in++;
			b = *in++;
			*s++ = ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
		}
	} else if (depth == 4)
		while (size--) {
			out [0] = in [2];
			out [1] = in [1];
			out [2] = in [0];
			out [3] = 0;
			out += 4;
			in += 3;
		}
}

/*
 * Grayscale to rgb
 */
void gray2rgb (const unsigned char *in, unsigned char *out, int len)
{
	while (len--) {
		*out++ = *in;
		*out++ = *in;
		*out++ = *in++;
	}
}

/* convert a ppm with maxval 65535 to 255 */
void ppmdown (int size, char *in, char *out)
{
	while (size--) {
		*out++ = *in++;
		in++;
	}
}

typedef struct { unsigned char r, g, b; } rgb;

/*
 * Image stretch using sampling (nearest pixel). The quality is low and the result may
 * seem ugly, but it's fast, can expand and is used in the web images.
 */

void xresize (const void *src, uint sw, uint sh, void *dest, uint dw, uint dh, int bpp)
{
	uint x, y;
	uint yy1, xx, yyp = 0;
	uint ddx = (sw << 16) / dw;

#define DOIT(type) {\
	const type *in = src;\
	type *out = dest;\
	for (y = 0; y < dh; y++) {\
		yy1 = (sh * y / dh) * sw;\
		if (y && yyp == yy1) {\
			for (x = 0; x < dw; x++, out++)\
				*out = out [-dw];\
		} else {\
			for (xx = x = 0; x < dw; x++, xx += ddx)\
				*out++ = in [(xx >> 16) + yy1];\
		}\
		yyp = yy1;\
	}\
}

	switch (bpp) {
	case 1:	DOIT (uchar); break;
	case 2:	DOIT (ushort); break;
	case 3:	DOIT (u24); break;
	case 4:	DOIT (uint); break;
	}

#undef DOIT
}

/*
 * High quality resize.
 * We use the filter sin(x)/x on four values (TAPS) according to their
 * distance from the central point. Normalized.
 * This is good for reducing an image no more than x2. For more, we
 * should run a faster average resize to reduce the image in half.
 */

extern double sin (double);
extern double fabs (double);

#define TAPS 3
#define PREC 11
typedef unsigned short int coint;

static void resample_line (const rgb inbuf[], int inlen, rgb outbuf[], int outlen, coint coeffs[])
{
	int ox, x;
	int i;
	unsigned int pr, pg, pb, c;
	const rgb *pp;
	unsigned int dinxi = (inlen << PREC) / outlen;
	unsigned int dxi = 0;

	for (ox = 0; ox < outlen; ox++) {
		dxi += dinxi;
		x = (dxi >> PREC) - 1;
		pr = pg = pb = 0;
		for (i = 0; i < TAPS; i++) {
			c = coeffs [ox * TAPS + i];
			pp = &inbuf [x + i];
			pr += pp->r * c;
			pg += pp->g * c;
			pb += pp->b * c;
		}
		outbuf [ox].r = pr >> PREC;
		outbuf [ox].g = pg >> PREC;
		outbuf [ox].b = pb >> PREC;
	}
}

static inline double filter (double x)
{
#if 0
	return x ? sin (x) / x : 1.0;
#else
	return x ? (sin (x) * sin (x/3) / (x*x/3)) : 1.0;
#endif
}

static void calc_coeffs (coint coeff[], int inlen, int outlen)
{
	double factor = outlen / (float)inlen;
	double dinx = 1.0 / factor;
	double dx = 0, c;
	int ox, x;
	double coeffs [TAPS];
	double sum = 0;
	int i;

	/*
	* suppose input line 0..800 and output line 0..500
	* the 32th point of the output is the point 51.2 in
	* the input.  Given 4 TAPS, well use the values from
	* the input points (50, 51, 52, 53).  The distances
	* from 51.2 are: (1.2, 0.2, 0.8, 1.8).
	* sin(x)/x of the distances is: 0.776, 0.993, 0.896, 0.541
	* and normalized so that their sum adds up to 1:
	*	0.24, 0.31, 0.28, 0.17
	* and these are the coefficients for point 32.
	*/
	for (ox = 0; ox < outlen; ox++) {
		dx += dinx;
		x = (int) dx - 1;
		sum = 0;
		for (i = 0; i < TAPS; i++) {
			c = fabs (dx - (x + i));
			sum += coeffs [i] = filter (c);
		}
		for (i = 0; i < TAPS; i++)
			coeff [ox*TAPS + i] = ((1<<PREC) * coeffs [i] / sum);
	}
}

static void resample1 (int height, const rgb inbuf[], int inw, rgb outbuf[], int outw, coint coeffs[])
{
	int i;

	calc_coeffs (coeffs, inw, outw);
	for (i = 0; i < height; i++)
		resample_line (inbuf + inw * i, inw, outbuf + outw * i, outw, coeffs);
}

static void resample2 (int width, const rgb inbuf[], int inh, rgb outbuf[], int outh, coint coeffs[])
{
	int i, j;
	rgb invline [inh];
	rgb outvline [outh];

	calc_coeffs (coeffs, inh, outh);
	for (i = 0; i < width; i++) {
		for (j = 0; j < inh; j++)
			invline [j] = inbuf [i + j * width];
		resample_line (invline, inh, outvline, outh, coeffs);
		for (j = 0; j < outh; j++)
			outbuf [i + j * width] = outvline [j];
	}
}

/* hresize requires two buffers allocated by the caller.
   buf1 should be:  "inheight * outwidth * 3"  bytes
   buf2 should be:  "2 * ntaps() * max (outwidth, outheight)"  bytes
*/

void hresize (const rgb in[], int inw, int inh, rgb out[], int outw, int outh, rgb *buf1, coint *buf2)
{
	resample1 (inh, in, inw, buf1, outw, buf2);
	resample2 (outw, buf1, inh, out, outh, buf2);
}

void hresize_bg (const rgb in[], int inw, int inh, rgb out[], int outw, int outh, rgb *buf1, coint *buf2)
{
	return hresize (in, inw, inh, out, outw, outh, buf1, buf2);
}

int ntaps ()
{
	return TAPS;
}

/* rgb iterator */

typedef struct
{
	unsigned char *s;
	unsigned int len, i;
} rgbiters;

void init_rgbiter  (unsigned char *s, unsigned int l, rgbiters *r)
{
	r->s = s;
	r->len = l;
	r->i = 0;
}

unsigned int rgbiter_next (rgbiters *r)
{
	if (r->i >= r->len) return -1;
	unsigned int v = r->s [r->i] | (r->s [r->i + 1] << 8) | (r->s [r->i + 2] << 16);
	r->i += 3;
	return v;
}

/* change a color to another color in an RGB image.
   This can be used to convert the "colorkey" to the "background color"
   to achieve some kind of transparency
*/

void color_change (u24 img[], u24 nimg[], uint w, uint h, RRGGBB oldc, RRGGBB newc)
{
	u24 co = pack24 (oldc);
	u24 cn = pack24 (newc);
	uint i;

	w *= h;
	for (i = 0; i < w; i++)
		nimg [i] = cmp_u24 (co, img [i]) ? cn : img [i];
}

void repeatx (const void *src, uint w, uint h, void *dest, uint n, int bpp)
{
	uint x, y, i;

#define DOIT(type) {\
	const type *in = src;\
	type *out = dest;\
	for (y = 0; y < h; y++, in += w)\
		for (i = 0; i < n; i++, in -= w)\
			for (x = 0; x < w; x++)\
				*out++ = *in++;\
}
	switch (bpp) {
	case 1:	DOIT (uchar); break;
	case 2:	DOIT (ushort); break;
	case 3:	DOIT (u24); break;
	case 4:	DOIT (uint); break;
	}

#undef DOIT
}

void repeaty (const void *src, uint w, uint h, void *dest, uint n, int bpp)
{
	uint i, sz = w * h * bpp;

	for (i = 0; i < n; i++)
		memcopy (dest + i * sz, src, sz);
}

/* xor all the colors with 255. used to draw the cursor in xterm, for one */

void imageneg (uchar *img, uint w, uint h, uint bpp)
{
	// for 32bpp images this also negates the "dead byte" which atm
	// is unused but might mean "ALPHA" in the future
	uint n = w * h * bpp;

	while (n--)
		*img++ ^= 255;
}

//
const char ABI [] =
#ifdef PYVM_MMX
"- enable_mmx	-		\n"
#endif
"- ppmdown	iss		\n"
"- rgb2depth	iiss		\n"
"- gray2rgb	ssi		\n"
"- hresize	siisiiss	\n"
"- hresize_bg!	siisiiss	\n"
"i ntaps	-		\n"
"- init_rgbiter	sis		\n"
"i rgbiter_next	s		\n"
"- color_change	ssiiii		\n"
"- xresize	siisiii		\n"
"- repeatx	siisii		\n"
"- repeaty	siisii		\n"
"- imageneg	siii		\n"
;
