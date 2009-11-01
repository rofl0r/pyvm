/*
 *  Font bitmap generation
 *
 *  Copyright (c) 2007, 2008 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

typedef unsigned int uint;

/* The glyph is painted on a framebuffer `fb` at offset x0, y0 and a
   megapixel is MPPxMPP pixels. Create a bitmap by calculating the
   average for each megapixel
*/
void MakeGlyph (const unsigned char *fb, uint fw, uint x0, uint y0, uint rows, uint cols,
		uint mpp, unsigned char *bitmap)
{
	uint dw = fw - mpp * cols;
	uint r, c, ii, jj;
	uint sums [cols];

	fb += fw * y0 + x0;
	for (r = 0; r < rows; r++) {
		for (c = 0; c < cols; c++)
			sums [c] = 0;

		for (ii = 0; ii < mpp; ii++) {
			for (c = 0; c < cols; c++)
				for (jj = 0; jj < mpp; jj++)
					sums [c] += *fb++;
			fb += dw;
		}

		for (c = 0; c < cols; c++)
			*bitmap++ = sums [c] / (mpp * mpp);
	}
}

/* Shift the glyph -MPP..MPP in the x-axis and then in the y-axis
   looking for the bitmap with the highest sum of squares (auto-hinting)
*/
static inline uint sqr (uint x)
{
	return x*x;
}

static uint squaresum (const unsigned char *bitmap, uint len)
{
	uint sum = 0;
	uint i;

	for (i = 0; i < len; i++)
		sum += sqr (255 - bitmap [i]);

	return sum;
}

uint MakeGlyph2 (const unsigned char *fb, uint fw, uint x0, uint y0, uint rows, uint cols,
		uint mpp, unsigned char *bitmap)
{
	MakeGlyph (fb, fw, x0, y0, rows, cols, mpp, bitmap);
	return squaresum (bitmap, rows*cols);
}

void MakeGlyphAH (const unsigned char *fb, uint fw, int x0, int y0, uint rows, uint cols,
		  uint mpp, unsigned char *bitmap)
{
	unsigned char bp [rows*cols];
	int i, j, xb = 0;
	uint vmax, v;

	vmax = 0;
	j = mpp / 2 - 2;
	for (i = -j; i < j; i++) {
		v = MakeGlyph2 (fb, fw, x0 + i, y0, rows, cols, mpp, bp);
		if (v > vmax) {
			xb = i;
			__builtin_memcpy (bitmap, bp, rows*cols);
			vmax = v;
		}
	}
	for (i = -3; i < 4; i++) if (i) {
		v = MakeGlyph2 (fb, fw, x0 + xb, y0 + i, rows, cols, mpp, bp);
		if (v > vmax) {
			__builtin_memcpy (bitmap, bp, rows*cols);
			vmax = v;
		}
	}
}

void MakeGlyphAH2 (const unsigned char *fb, uint fw, int x0, int y0, uint rows, uint cols,
		   uint mpp, unsigned char *bitmap)
{
	MakeGlyphAH (fb, fw, x0, y0, rows, cols, mpp, bitmap);
}

#if 0
/* Same thing auto-hinted but 50% faster */
int G (const unsigned char *fb, uint fw, int x0, int y0, uint rows, uint cols,
		uint mpp, unsigned char *bitmap)
{
	uint dm = mpp / 2 - 3;
	uint nxp = mpp * cols + 2 * dm;
	unsigned int bars [rows][nxp];
	unsigned int pixels [rows][cols];
	uint x, y, m, xx;

	/* compute bars */
	for (y = 0; y < rows; y++) {
		for (x = 0; x < nxp; x++)
			bars [y][x] = 0;
		for (m = 0; m < mpp; m++, fb += dw)
			for (x = 0; x < nxp; x++)
				bars [y][x] += *fb++;
		for (xx = x = 0; x < cols; x++, xx += mpp) {
			pixels [y][x] = 0;
			for (m = 0; m < mpp; m++)
				pixels [y][x] += bars [y][xx+m];
	}

	for (m = 1; m < dm; m++)
}
#endif

int MakeGlyph3 (const unsigned char *fb, uint fw, int x0, int y0, uint rows, uint cols,
		uint mpp, unsigned char *bitmap)
{

	uint dm = mpp / 2 - 3;
	uint nxp = mpp * cols + 2 * dm;
	uint rxc = rows * cols;
	uint dw = fw - nxp;
	unsigned short int sa [nxp * rows], *sp;
	uint c, r, ii, jj;
	int xb = 0;
	const unsigned char *fb0 = fb;
	fb += fw * y0 + x0 - dm;

	for (ii = 0, jj = nxp * rows; ii < jj; ii++)
		sa [ii] = 0;

	/* for the shifts in the x-axis, we precalculate micro-columns */
	for (r = 0; r < rows; r++)
		for (ii = 0; ii < mpp; ii++) {
			sp = sa + r * nxp;
			for (c = 0; c < nxp; c++)
				*sp++ += *fb++;
			fb += dw;
		}

	unsigned char bp [rxc];
	uint vmax, v, t;
	vmax = 0;

	for (ii = 0; ii <= 2 * dm; ii++) {
		for (jj = 0; jj < rxc; jj++)
			bp [jj] = 0;
		for (r = 0; r < rows; r++) {
			sp = &sa [r * nxp + ii];
			for (c = 0; c < cols; c++) {
				t = 0;
				for (jj = 0; jj < mpp; jj++)
					t += *sp++;
				bp [r * cols + c] = t / (mpp * mpp);
			}
		}
		v = squaresum (bp, rxc);
		if (v > vmax) {
			xb = ii;
			__builtin_memcpy (bitmap, bp, rxc);
			vmax = v;
		}
	}

	// micro shift for best effect in x-axis
	x0 = x0 + xb - dm;

	/* y-axis */
	int i;
	for (i = -3; i <= 4; i++) if (i) {
		v = MakeGlyph2 (fb0, fw, x0, y0+i, rows, cols, mpp, bp);
		if (v > vmax) {
			__builtin_memcpy (bitmap, bp, rxc);
			vmax = v;
		}
	}

#if 0
	for (jj = 0; jj < rxc; jj++)
		bitmap [jj] = bitmap [jj] < 100 ? 0 : bitmap [jj];
#endif
	return xb - dm;
}

void prepBitmap (unsigned char bitmap[], int rxc, const unsigned char gamma[])
{
	int jj;

	for (jj = 0; jj < rxc; jj++)
		bitmap [jj] = 255 - bitmap [jj];

	if (gamma)
		for (jj = 0; jj < rxc; jj++)
			bitmap [jj] = gamma [bitmap [jj]];
}

const char ABI [] = 
"- MakeGlyph      siiiiiis		\n"
"- MakeGlyphAH    siiiiiis		\n"
"- MakeGlyphAH2!  siiiiiis		\n"
"i MakeGlyph2     siiiiiis		\n"
"i MakeGlyph3     siiiiiis		\n"
"- prepBitmap	  siv			\n"
;
