/*
 *  BMP Image routines
 *
 *  Copyright (c) 2008 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

typedef struct { unsigned char r, g, b; } rgb;
typedef struct { unsigned char b, g, r; } bgr;
typedef unsigned int uint;

void bgr2rgb_pal (bgr *pal, uint ncolors)
{
	unsigned char t;

	while (ncolors--) {
		t = pal->r;
		pal->r = pal->b;
		pal++->b = t;
	}
}

int pal8_to_rgb (const rgb *palette, const unsigned char *in, int in_offset, rgb *out,
		  uint width, uint height)
{
	unsigned int pad = (3 * width) % 4;
	if (pad) pad = 4 - pad;

	const unsigned char *in0 = in + in_offset;
	unsigned int x, y;

	out += width * (height - 1);
	in += in_offset;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++)
			*out++ = palette [*in++];
		in += pad;
		out -= 2 * width;
	}

	return in - in0;
}

int pal4_to_rgb (const rgb *palette, const unsigned char *in, int in_offset, rgb *out,
		  uint width, uint height)
{
	int odd = width % 2;
	unsigned int pad = (width / 2 + odd) % 4;
	if (pad) pad = 4 - pad;

	const unsigned char *in0 = in + in_offset;
	unsigned int x, y;
	uint w = width / 2;

	out += width * (height - 1);
	in += in_offset;
	for (y = 0; y < height; y++) {
		for (x = 0; x < w; x++) {
			unsigned char p = *in++;
			*out++ = palette [p >> 4];
			*out++ = palette [p & 15];
		}
		if (odd) {
			unsigned char p = *in++;
			*out++ = palette [p >> 4];
		}
		in += pad;
		out -= 2 * width;
	}

	return in - in0;
}

int pal1_to_rgb (const rgb *palette, const unsigned char *in, int in_offset, rgb *out,
		  uint width, uint height)
{
	int odd = width % 8;
	unsigned int pad = (width / 2 + !!odd) % 4;
	if (pad) pad = 4 - pad;

	const unsigned char *in0 = in + in_offset;
	unsigned int x, y;
	uint w = width / 8;

	out += width * (height - 1);
	in += in_offset;
	for (y = 0; y < height; y++) {
		for (x = 0; x < w; x++) {
			unsigned char p = *in++;
			*out++ = palette [p >> 7];
			*out++ = palette [(p >> 6) & 1];
			*out++ = palette [(p >> 5) & 1];
			*out++ = palette [(p >> 4) & 1];
			*out++ = palette [(p >> 3) & 1];
			*out++ = palette [(p >> 2) & 1];
			*out++ = palette [(p >> 1) & 1];
			*out++ = palette [p & 1];
		}
		if (odd) {
			unsigned char p = *in++, i;
			for (i = 0; i < odd; i++) {
				*out++ = palette [p >> 7];
				p <<= 1;
			}
		}
		in += pad;
		out -= 2 * width;
	}

	return in - in0;
}

int rlepal8_to_rgb (const rgb *palette, const unsigned char *in, int in_offset, rgb *out,
		    uint width, uint height)
{
	rgb *outlim = out + width * height;
	unsigned char ch, ci;
	rgb r;
	const unsigned char *in0 = in + in_offset;

	in += in_offset;
	out += width * (height - 1);

	while (height > 0)
		if ((ch = *in++)) {
			r = palette [*in++];
			for (ci = 0; ci < ch && out < outlim; ci++)
				*out++ = r;
		} else switch (ch = *in++) {
			case 0:
				--height;
				out -= 2 * width;
				break;
			case 1: goto out;
			case 2: return -1;
			default:
				for (ci = 0; ci < ch && out < outlim; ci++)
					*out++ = palette [*in++];
				if (ci & 1)
					++in;

		}

    out:
	return in - in0;
}

int bgr_to_rgb (const unsigned char *in, int in_offset, rgb *out, uint width, uint height)
{
	unsigned int pad = (3 * width) % 4;
	if (pad) pad = 4 - pad;

	const unsigned char *in0 = in + in_offset;
	unsigned int x, y;
	bgr b;

	out += width * (height - 1);
	in += in_offset;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			b = *(bgr*)in;
			out->r = b.r;
			out->g = b.g;
			out++->b = b.b;
			in += 3;
		}
		in += pad;
		out -= 2 * width;
	}

	return in - in0;
}

int bgr16_to_rgb (const unsigned char *in, int in_offset, rgb *out, uint width, uint height)
{
	unsigned int pad = (2 * width) % 4;
	if (pad) pad = 4 - pad;

	const unsigned char *in0 = in + in_offset;
	unsigned int x, y;

	out += width * (height - 1);
	in += in_offset;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			/* Seems like 5-5-5 */
			unsigned short int b = *(unsigned short int*)in;
			out->r = ((b >> 10) & 0x1f) << 3;
			out->g = ((b >> 5) & 0x1f) << 3;
			out++->b = (b & 0x1f) << 3;
			in += 2;
		}
		in += pad;
		out -= 2 * width;
	}

	return in - in0;
}

int bgr32_to_rgb (const unsigned char *in, int in_offset, rgb *out, uint width, uint height)
{
	unsigned int x, y;
	bgr b;
	const unsigned char *in0 = in + in_offset;

	out += width * (height - 1);
	in += in_offset;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			b = *(bgr*)in;
			out->r = b.r;
			out->g = b.g;
			out++->b = b.b;
			in += 4;
		}
		out -= 2 * width;
	}

	return in - in0;
}

/* ICO images (favicon.ico) have a section for the transparency bits,
   one bit per pixel, upside-down, each scanline aligned at 4 bytes.
   Convert this section to an alpha table  */

int alphatab (const unsigned char *c, int w, int h, unsigned char *out)
{
	const unsigned char *c0 = c;
	int x, y, b;

	x = w / 8 + (w % 8 != 0);
	int pad = x % 4 ? 5 - x % 4 : 1;

	out += w * (h - 1);
	for (y = 0; y < h; y++, c += pad, out -= 2*w)
		for (b = x = 0; x < w; x++) {
			if (b == 8) {
				++c;
				b = 0;
			}
			*out++ = (*c & (1 << (7-b++))) ? 255 : 0;
		}
	return c - c0;
}

const char ABI [] =
"- bgr2rgb_pal		si	\n"
"i pal8_to_rgb		ssisii	\n"
"i pal4_to_rgb		ssisii	\n"
"i pal1_to_rgb		ssisii	\n"
"i rlepal8_to_rgb	ssisii	\n"
"i bgr_to_rgb		sisii	\n"
"i bgr32_to_rgb		sisii	\n"
"i bgr16_to_rgb		sisii	\n"
"i alphatab		siis	\n"
;
