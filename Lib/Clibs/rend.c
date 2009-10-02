/*
 *  Text Renderer
 * 
 *  Copyright (c) 2007, 2008 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

#include <stdio.h>
#include "rgb.h"
#include "rglyph.h"

static int d2fixed (double d)
{
	return d * 64.0;
}

static inline int fixed2int (int i)
{
	return (i + 32) >> 6;
}

/*
 * The core blending could be done in MMX
 */

double render (struct rglyph *pp[], int np, u24 *fb,
	       int width, int height, double fx0, double fy0,
	       double charSpace, double wordSpace)
{
	/* The heart of PDF text rendering */
	/* the fx0 and fy0 points to the "pen position",
		 that is the bottom left corner.  */
	int i, r, c, pixel, dw, x=0;
	unsigned int px, py;
	u24 *dest;
	struct rglyph *p;
	int cS = d2fixed (charSpace);
	int wS = d2fixed (wordSpace);
	int x0 = d2fixed (fx0);
	int y0 = fy0;

	for (i = 0; i < np; i++) {
		p = pp [i];
		px = fixed2int (x0 + x) + p->px;
		py = y0 + p->py;
		dw = p->pitch;
		if (px + p->width > width)
			break;
		for (r = 0; r < p->rows && r + py < height; r++) {
			if (r + py >= height)
				break;
			for (c = 0; c < p->width; c++) {
				pixel = p->bitmap [c + r * dw];	// (cse)
				if (pixel) {
					pixel = 255 - pixel;
					dest = &fb [(r + py) * width + c + px];
					dest->r = dest->g = dest->b = pixel;
				}
			}
		}
		x += p->advance + (p->sp ? wS : cS);
	}

	return x / 64.0;
}

// framebuffer renderers

#define PIXEL u24
#define CON(x) x ## u24
#define rend_name render_col
#define rend_mono_name render_mono_col
#include "rend_template.c"

#define PIXEL ushort
#define CON(x) x ## ushort
#define rend_name render_col16
#define rend_mono_name render_mono_col16
#include "rend_template.c"

#define PIXEL uint
#define CON(x) x ## uint
#define rend_name render_col32
#define rend_mono_name render_mono_col32
#include "rend_template.c"

/* ---------------------- text width & paragraph breaking utilities ---------------------- */

double text_width (const unsigned int widths[], const unsigned char *s, unsigned int len)
{
	unsigned int sum = 0;

	while (len--)
		sum += widths [*s++];

	return sum / 64.0;
}

/* find the breaking point in a string so that the first part of the string is
   less than the requested width for the given font widths.

   soft break breaks in spaces
   hard break breaks anywhere
*/

static unsigned int soft_break_offset (const unsigned int widths[], const unsigned char *s,
				unsigned int len, unsigned int width, unsigned int offset)
{
	unsigned int w = 0, sw = 0;
	unsigned int i, si;

	width *= 64;

	for (i = si = offset; i < len; i++) {
		if (s [i] == ' ') {
			si = i;
			sw = w;
		}
		w += widths [s [i]];
		if (w > width) 
			return si;
	}

	return len;
}

unsigned int soft_break (const unsigned int widths[], const unsigned char *s, unsigned int len,
			 unsigned int width)
{
	return soft_break_offset (widths, s, len, width, 0);
}

unsigned int hard_break (const unsigned int widths[], const unsigned char *s, unsigned int len,
	    /* hotel */	 unsigned int width)
{
	unsigned int w = 0, i;
	width *= 64;
	for (i = 0; i < len; i++)
		if ((w += widths [s [i]]) > width)
			return i;
	return len;
}

/* same thing for utf-8 strings.  The id-to-width structure is a pyvm "iidict" object.
   we use the function "__iidict_lookup__" that is exported by the runtime in order
   to lookup keys in this structure
*/
#include "utfi.c"

typedef int (*ucs_lookup_t) (void*, unsigned int, unsigned int*);
static ucs_lookup_t ucs_lookup;

void init_ucs_lookup (ucs_lookup_t t)
{
	ucs_lookup = t;
}

static inline unsigned int utf8_lookup_width (void *iidict, unsigned int uc)
{
	unsigned int ret;
	if (!ucs_lookup (iidict, uc, &ret))
		return 0;
	return ret;
}

unsigned int usoft_break (const unsigned int widths[], void *iidict, const unsigned char *s, unsigned int len, unsigned int width)
{
	unsigned int w = 0, sw = 0;
	unsigned int si, uc;
	const unsigned char *end = s + len;
	const unsigned char *start = s;

	width *= 64;

	si = 0;
	while (s < end) {
		if (*s < 0x80) {
			uc = *s++;
			if (uc == 32) {
				si = s - start - 1;
				sw = w;
			}
			w += widths [uc];
		} else {
			uc = utf8_getchar (&s);
			w += utf8_lookup_width (iidict, uc);
		}

		if (w > width) 
			return si;
	}

	return len;
}

unsigned int uhard_break (const unsigned int widths[], void *iidict, const unsigned char *s, unsigned int len, unsigned int width)
{
	const unsigned char *start = s;
	const unsigned char *end = s + len;
	unsigned int w = 0;
	width *= 64;
	while (s < end) {
		if (*s < 0x80) w += widths [*s++];
		else w += utf8_lookup_width (iidict, utf8_getchar (&s));
		if (w > width)
			return s - start - 1;
	}
	return len;
}

double utf_text_width (const unsigned int widths[], void *iidict, const unsigned char *s, unsigned int len)
{
	unsigned int w = 0;
	const unsigned char *end = s + len;

	while (s < end)
		if (*s < 0x80)
			w += widths [*s++];
		else w += utf8_lookup_width (iidict, utf8_getchar (&s));

	return w / 64.0;
}

const char ABI [] =
"d render		psiviidddd	\n"
"d render_col		psivip32ddddi	\n"
"d render_mono_col	psivip32ddddi	\n"
"d render_col16		psivip32ddddi	\n"
"d render_mono_col16	psivip32ddddi	\n"
"d render_col32		psivip32ddddi	\n"
"d render_mono_col32	psivip32ddddi	\n"
"d text_width		p32si		\n"
"i soft_break		p32sii		\n"
"i hard_break		p32sii		\n"
"- init_ucs_lookup	i		\n"
"i usoft_break		p32*sii		\n"
"i uhard_break		p32*sii		\n"
"d utf_text_width	p32*si		\n"
;

const char __DEPENDS__ [] = "rend_template.c, rgb.h, rglyph.h, utfi.c";
