/*
 *  PNG utilities
 *
 *  Copyright (c) 2007, 2008 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

/* used in colortype/depth 3/4. Palettized image 4 bits per sample */
void expand4bits (const unsigned char *in, unsigned char *out, unsigned int width, unsigned int height)
{
	unsigned int j, i;
	int oddwidth = width % 2;
	unsigned char c;

	width /= 2;
	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			c = *in++;
			*out++ = c >> 4;
			*out++ = c & 15;
		}
		if (oddwidth)
			*out++ = *in++ >> 4;
	}
}

/* used in colortype/depth 3/2. Palettized image 2 bits per sample */
void expand2bits (const unsigned char *in, unsigned char *out, unsigned int width, unsigned int height)
{
	unsigned int j, i;
	unsigned int oddwidth = width % 4;
	unsigned char c;

	width /= 4;
	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			c = *in++;
			*out++ = c >> 6;
			*out++ = (c >> 4) & 3;
			*out++ = (c >> 2) & 3;
			*out++ = c & 3;
		}
		if (oddwidth) {
			c = *in++;
			*out++ = c >> 6;
			if (oddwidth > 1) {
				*out++ = (c >> 4) & 3;
				if (oddwidth == 3)
					*out++ = (c >> 2) & 3;
			}
		}
	}
}

/* used in colortype/depth 3/1. Palettized image 1 bits per sample */
void expand1bit (unsigned char *in, unsigned char *out, unsigned int width, unsigned int height)
{
	unsigned int j, i;
	unsigned int oddwidth = width % 8;
	unsigned char c;

	width /= 8;
	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			c = *in++;
			*out++ = c >> 7;
			*out++ = (c >> 6) & 1;
			*out++ = (c >> 5) & 1;
			*out++ = (c >> 4) & 1;
			*out++ = (c >> 3) & 1;
			*out++ = (c >> 2) & 1;
			*out++ = (c >> 1) & 1;
			*out++ = c & 1;
		}
		if (oddwidth) {
			c = *in++;
			for (j = 0; j < oddwidth; j++) {
				*out++ = c >> 7;
				c <<= 1;
			}
		}
	}
}

static inline unsigned int abs (int a)
{
	return a < 0 ? -a : a;
}

static unsigned char paeth (unsigned char a, unsigned char b, unsigned char c)
{
	int p = a + b - c;
	int pa = abs (p - a);
	int pb = abs (p - b);
	int pc = abs (p - c);
	if (pa <= pb && pa <= pc) return a;
	return pb <= pc ? b : c;
}

static unsigned char avg (unsigned int a, unsigned int b)
{
	return (a + b) / 2;
}

// decode filters.  `data` and `out` can be the same buffer.
int defilter (unsigned char *data, unsigned int width, unsigned int nline, unsigned char *out, int bpp)
{
	unsigned int i;
	unsigned char *rp, *lp, *pp, *cp, *in;

	/* special case first line */
	if (*data == 4) {
		++data;
			in = data;
			rp = lp = out;
			for (i = 0; i < bpp; i++)
				*rp++ = *in++;
			while (i++ < width)
				*rp++ = *in++ + paeth (*lp++, 0, 0);
		data += width;
		out += width;
		--nline;
	} else if (*data == 3) {
		++data;
			in = data;
			rp = lp = out;
			pp = out + width * (nline - 1);

			for (i = 0; i < bpp; i++)
				*rp++ = *in++ + avg (*pp++, 0);
			while (i++ < width)
				*rp++ = *in++ + avg (*pp++, *lp++);
		data += width;
		out += width;
		--nline;
	} else if (*data == 2) {
		++data;
			in = data;
			rp = out;
			lp = out + width * (nline - 1);
			for (i = 0; i < width; i++)
				*rp++ = *in++ + *lp++;

		data += width;
		out += width;
		--nline;
	}

	while (nline--) {
		switch (*data++) {
		case 0: memmove (out, data, width); break;
		case 1: {
			in = data;
			rp = lp = out;
			for (i = 0; i < bpp; i++)
				*rp++ = *in++;
			while (i++ < width)
				*rp++ = *in++ + *lp++;
			break;
		}
		case 2: {
			in = data;
			rp = out;
			lp = out - width;
			for (i = 0; i < width; i++)
				*rp++ = *in++ + *lp++;
			break;
		}
		case 3: {
			in = data;
			rp = lp = out;
			pp = out - width;

			for (i = 0; i < bpp; i++)
				*rp++ = *in++ + avg (*pp++, 0);
			while (i++ < width)
				*rp++ = *in++ + avg (*pp++, *lp++);
			break;
		}
		case 4: {
			in = data;
			rp = lp = out;
			pp = cp = out - width;

			for (i = 0; i < bpp; i++)
				*rp++ = *in++ + *pp++;
			while (i++ < width)
				*rp++ = *in++ + paeth (*lp++, *pp++, *cp++);
			break;
		}
		default: return data [-1];
		}
		data += width;
		out += width;
	}
	return 0;
}

typedef struct { unsigned char r, g, b; } rgb;

void depalette (const unsigned char in[], const rgb palette[], unsigned int inlen, rgb out[])
{
	unsigned int i;
	for (i = 0; i < inlen; i++)
		out [i] = palette [in [i]];
}

void depalettea (const unsigned char in[], const unsigned char palette[], unsigned int inlen,
		 unsigned char out[])
{
	unsigned int i;
	for (i = 0; i < inlen; i++)
		out [i] = 255 - palette [in [i]];
}

void rrggbb2rgb (const unsigned char in[], unsigned int inlen, rgb out[])
{
	unsigned int i, j;
	for (i = j = 0; i < inlen; i += 6, j++) {
		out [j].r = in [i];
		out [j].g = in [i+2];
		out [j].b = in [i+4];
	}
}

void rrggbbaa2rgb (const unsigned char in[], unsigned int inlen, rgb out[], unsigned char alpha[])
{
	unsigned int i, j;
	for (i = j = 0; i < inlen; i += 8, j++) {
		out [j].r = in [i];
		out [j].g = in [i+2];
		out [j].b = in [i+4];
		// in png ALPHA means opacity
		alpha [j] = 255 - in [i+6];
	}
}

void rgba2rgb (unsigned char in[], unsigned int inlen, rgb out[], unsigned char alpha[])
{
	unsigned int i, j;
	for (i = j = 0; i < inlen; i += 4, j++) {
		out [j].r = in [i];
		out [j].g = in [i+1];
		out [j].b = in [i+2];
		// in png ALPHA means opacity
		alpha [j] = 255 - in [i+3];
	}
}

void gray2rgb (const unsigned char in[], unsigned int inlen, rgb *out, int depth)
{
	const unsigned char b2 [] = { 0, 85, 170, 255 };
	unsigned int i;
	switch (depth) {
	case 8: for (i = 0; i < inlen; i++) 
			out [i].r = out [i].g = out [i].b = in [i];
		break;
	case 4: for (i = 0; i < inlen; i++) 
			out [i].r = out [i].g = out [i].b = (in [i] << 4) | in [i];
		break;
	case 2: for (i = 0; i < inlen; i++)
			out [i].r = out [i].g = out [i].b = b2 [in [i]];
		break;
	case 1: for (i = 0; i < inlen; i++) 
			out [i].r = out [i].g = out [i].b = in [i] ? 255 : 0;
	}
}

void grayRNS (const rgb img[], const unsigned char trns[], unsigned int rgblen, unsigned char alpha[])
{
	// atm, trns is two bytes but our image is 1 byte per pixel. so compare the low
	// bits against the gray
	unsigned int i;
	for (i = 0; i < rgblen; i++)
		alpha [i] = img [i].b == trns [1] ? 255 : 0;
}

void graya2rgb (const unsigned char in[], unsigned int inlen, rgb *out, unsigned char alpha[])
{
	unsigned int i, j;
	for (i = j = 0; i < inlen; i += 2, j++)  {
		out [j].r = out [j].g = out [j].b = in [i];
		// in png ALPHA means opacity
		alpha [j] = 255 - in [i+1];
	}
}

void enfilter (char *in, int w, int h, char *out)
{
	// 'None' filter for all scanlines
	while (h--) {
		*out++ = 0;
		memcpy (out, in, w);
		out += w;
		in += w;
	}
}

/*
 * De-interlace.  From 7 distinct subimages put pixels in the final image with the
 * Adam7 method.  rfc section 2.6
 */

const static struct {
	unsigned int x0, y0, dx, dy;
} passinc [] = {
	{ 0, 0, 8, 8 },
	{ 4, 0, 8, 8 },
	{ 0, 4, 4, 8 },
	{ 2, 0, 4, 4 },
	{ 0, 2, 2, 4 },
	{ 1, 0, 2, 2 },
	{ 0, 1, 1, 2 }
};

void deinterlace (int w[], int h[], const rgb *subs[], int outw, int outh, rgb *out)
{
	int p, x, y;
	int xi, yi;
	const rgb *si;

	for (p = 0; p < 7; p++) {
		si = subs [p];
		yi = passinc [p].y0;
		for (y = 0; y < h [p]; y++, yi += passinc [p].dy) {
			xi = passinc [p].x0;
			for (x = 0; x < w [p]; x++, xi += passinc [p].dx)
				out [xi + yi * outw] = *si++;
		}
	}
}

void deinterlacea (int w[], int h[], const char *subs[], int outw, int outh, char *out)
{
	int p, x, y;
	int xi, yi;
	const char *si;

	for (p = 0; p < 7; p++) {
		si = subs [p];
		yi = passinc [p].y0;
		for (y = 0; y < h [p]; y++, yi += passinc [p].dy) {
			xi = passinc [p].x0;
			for (x = 0; x < w [p]; x++, xi += passinc [p].dx)
				out [xi + yi * outw] = *si++;
		}
	}
}

const char ABI [] =
"- expand4bits	ssii		\n"
"- expand2bits	ssii		\n"
"- expand1bit	ssii		\n"
"i defilter	siisi		\n"
"- depalette	ssis		\n"
"- depalettea	ssis		\n"
"- rrggbb2rgb	sis		\n"
"- rrggbbaa2rgb	siss		\n"
"- rgba2rgb	siss		\n"
"- gray2rgb	sisi		\n"
"- grayRNS	ssis		\n"
"- graya2rgb	siss		\n"
"- enfilter	siis		\n"
"- deinterlace	p32p32psiis	\n"
"- deinterlacea	p32p32psiis	\n"
;
