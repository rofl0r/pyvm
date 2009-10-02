/*
 *  Image Difference
 *
 *  Copyright (C) 2008 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

// given two images, find their signal difference (noise)
// that's useful for the jpeg decoder to verify that
// we are close to libjpeg

typedef struct { unsigned char r, g, b; } rgb;

static inline unsigned int sqr (int a)
{
	return a*a;
}

void imagediff (rgb *i1, rgb *i2, unsigned int npixel, double out[])
{
	double dr, dg, db, tr, tg, tb;

	dr = dg = db = tr = tg = tb = 0;

	while (npixel--) {
		dr += sqr (i1->r - (int) i2->r);
		dg += sqr (i1->g - (int) i2->g);
		db += sqr (i1->b - (int) i2->b);
		tr += sqr (i1->r);
		tg += sqr (i1->g);
		tb += sqr (i1->b);
		++i1, ++i2;
	}

	// the caller will sqrt() these
	out [0] = dr;
	out [1] = dg;
	out [2] = db;
	out [3] = tr;
	out [4] = tg;
	out [5] = tb;
}

const char ABI [] = "- imagediff ssipd";
