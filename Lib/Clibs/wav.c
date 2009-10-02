/*
 *  Simple WAV routines
 *
 *  Copyright (c) 2008 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

#ifdef PYVM_MMX
#include "mmx.h"
#endif

#ifdef PYVM_MMX
// for fun...
static void u8tos16mmx (const unsigned char *s, short *d, int n)
{
	const unsigned int sub [2] = { 0x80008000, 0x80008000 };
	const unsigned char *end = s + n;

	movq_m2r (*sub, mm2);
	while (s < end) {
		movd_m2r (*s, mm0);
		pxor_r2r (mm1, mm1);
		punpcklbw_r2r (mm0, mm1);
		psubw_r2r (mm2, mm1);
		movq_r2m (mm1, *d);
		s += 4;
		d += 4;
	}
	emms ();
}
#endif

void u8tos16 (const unsigned char *s, short *d, int n)
{
#ifdef PYVM_MMX
	u8tos16mmx (s, d, n & ~3);
	d += n & ~3;
	s += n & ~3;
	n &= 3;
#endif
	while (n--)
		*d++ = ((int)*s++ - 128) << 8;
}

const char ABI [] =
"- u8tos16	ssi\n"
;
