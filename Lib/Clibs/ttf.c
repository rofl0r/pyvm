/*
 *  utilities for loading ttf files
 * 
 *  Copyright (c) 2008, 2009 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

typedef unsigned char u8;
typedef unsigned short int u16;
typedef short int s16;

/* ints in ttf are big-endian */

#ifdef LE
static inline unsigned int r32 (unsigned int x)
{
#ifdef __i386__
	__asm__ ("bswap %0" : "+r"(x));
#else
	x = ((x << 8) & 0xFF00FF00) | ((x >> 8) & 0x00FF00FF);
	x = (x >> 16) | (x << 16);
#endif
	return x;
}

static inline u16 r16u (u16 x)
{
#ifdef __i386__
	__asm__("rorw $8, %0" : "+r"(x));
#else
	x = (x >> 8) | (x << 8);
#endif
	return x;
}
#else
static inline unsigned int r32 (unsigned int x)
{
	return x;
}
static inline u16 r16u (u16 x)
{
	return x;
}
#endif

static inline s16 r16s (u16 x)
{
	return r16u (x);
}

void parse_hmetrics (int ng, int numhm, const char *p, int offset, int *out)
{
	int i, aa;
	p += offset;
	const u16 *pp = (u16*)p;

	for (i = 0; i < numhm; i++) {
		*out++ = r16u (*pp++);
		*out++ = r16s (*pp++);
	}

	aa = out [-2];

	for (; i < ng; i++) {
		*out++ = aa;
		*out++ = r16s (*pp++);
	}
}

void read_loc_1 (const char *sdata, int offset, int out[], int n)
{
	sdata += offset;
	const int *in = (int*) sdata;

	while (n--)
		*out++ = r32 (*in++);
}

void read_loc_0 (const char *sdata, int offset, int out[], int n)
{
	sdata += offset;
	const u16 *in = (u16*) sdata;

	while (n--)
		*out++ = 2 * r16u (*in++);
}

int parse_cmap_fmt4 (const char *sdata, int offset, int *out, int nout)
{
	int *out0 = out;
	int *maxout = out + nout;
	int length, lang, segc, i;
	const u16 *endCode, *startCode, *gia;
	const s16 *idDelta, *idRangeOffset;
	const u16 *data;

	sdata += offset;
	data = (u16*)sdata;

	length = r16u (*data++);
	lang = r16u (*data++);
	segc = r16u (*data++) / 2;

	data += 3;

	endCode = data;
	data += segc + 1;
	startCode = data;
	data += segc;
	idDelta = (s16*)data;
	data += segc;
	idRangeOffset = (s16*)data;
	data += segc;
	gia = data;

	int start, end, charcode, delta;
	for (i = 0; i < segc; i++) {
		offset = r16s (idRangeOffset [i]);
		start  = r16u (startCode [i]);
		end    = r16u (endCode [i]);
		delta  = r16s (idDelta [i]);
		if (!offset)
			for (charcode = start; charcode <= end && out < maxout; charcode++) {
				*out++ = charcode;
				*out++ = (charcode + delta) % 65536;
			}
		else
			for (charcode = start; charcode <= end && out < maxout; charcode++) {
				*out++ = charcode;
				*out++ = r16u (gia [offset / 2 - segc + (charcode - start) + i]);
			}
	}

	return out - out0;
}

void glyf_ends (const char *sdata, int offset, int *out, int n)
{
	sdata += offset;
	const u16 *in = (u16*) sdata;

	while (n--)
		*out++ = r16u (*in++);
}

int glyf_flags (const u8 *data, int offset, int *out, int n)
{
	int i = 0, x, j, k;
	data += offset;
	const u8 *d0 = data;

	while (i <= n) {
		x = out [i++] = *data++;
		if (x & 8) {
			k = *data++;
			for (j = 0; j < k && i <= n; j++)
				out [i++] = x;
		}
	}

	return data - d0;
}

int glyf_points (const u8 *data, int offset, int flags[], int xp[], int yp[], int n)
{
	int i, f, v, vp;
	data += offset;
	const u8 *d0 = data;
	union {
		u16 v;
		char d [2];
	} u;

	vp = 0;
	for (i = 0; i <= n; i++) {
		f = flags [i];
		if (f & 2) {
			v = *data++;
			if (!(f & 16))
				v = -v;
		} else if (!(f & 16)) {
			u.d [0] = *data++;
			u.d [1] = *data++;
			v = r16s (u.v);
		} else v = 0;
		vp += v;
		xp [i] = vp;
	}

	vp = 0;
	for (i = 0; i <= n; i++) {
		f = flags [i];
		if (f & 4) {
			v = *data++;
			if (!(f & 32))
				v = -v;
		} else if (!(f & 32)) {
			u.d [0] = *data++;
			u.d [1] = *data++;
			v = r16s (u.v);
		} else v = 0;
		vp += v;
		yp [i] = vp;
	}

	// leave the only bit relevant for the contours
	for (i = 0; i <= n; i++)
		flags [i] &= 1;

	return data - d0;
}

const char ABI [] =				
"- parse_hmetrics	iisip32		\n"
"- read_loc_1		sip32i		\n"
"- read_loc_0		sip32i		\n"
"i parse_cmap_fmt4	sip32i		\n"
"- glyf_ends		sip32i		\n"
"i glyf_flags		sip32i		\n"
"i glyf_points		sip32p32p32i	\n"
;
