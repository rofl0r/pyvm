/*
 *  bitmap glyph rendering
 * 
 *  Copyright (c) 2009 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

/* this mode of rendering is basically used by the xterm application */

#include "vfb.h"

typedef struct {
	fb *f;
	int cw, ch;
	int gw, gh;
	int stride;
} finfo;

const int sizeof_finfo = sizeof (finfo);

void init_finfo (finfo *fi, fb *f, int gw, int gh)
{
	fi->f = f;
	fi->gw = gw;
	fi->gh = gh;
	fi->cw = f->w / gw;
	fi->ch = f->h / gh;
	fi->stride = f->w;
}

void fill_glyph16 (finfo *f, int x, int y, const char *g, RRGGBB fg, RRGGBB bg)
{
	if (x > f->cw || y > f->ch)
		return;

	int xx, yy;
	int gw = f->gw, gh = f->gh;

	ushort *p = f->f->fb16 + x * gw + y * gh * f->stride;
	ushort fgc = pack16 (fg);
	ushort bgc = pack16 (bg);

	for (yy = 0; yy < gh; yy++) {
		ushort *pp = p + yy * f->stride;
		for (xx = 0; xx < gw; xx++)
			*pp++ = *g++ ? fgc : bgc;
	}
}

void fill_glyph24 (finfo *f, int x, int y, const char *g, RRGGBB fg, RRGGBB bg)
{
	if (x > f->cw || y > f->ch)
		return;

	int xx, yy;
	int gw = f->gw, gh = f->gh;

	u24 *p = f->f->fb24 + x * gw + y * gh * f->stride;
	u24 fgc = pack24 (fg);
	u24 bgc = pack24 (bg);

	for (yy = 0; yy < gh; yy++) {
		u24 *pp = p + yy * f->stride;
		for (xx = 0; xx < gw; xx++)
			*pp++ = *g++ ? fgc : bgc;
	}
}

void fill_glyph32 (finfo *f, int x, int y, const char *g, RRGGBB fg, RRGGBB bg)
{
	if (x > f->cw || y > f->ch)
		return;

	int xx, yy;
	int gw = f->gw, gh = f->gh;

	uint *p = f->f->fb32 + x * gw + y * gh * f->stride;
	uint fgc = pack32 (fg);
	uint bgc = pack32 (bg);

	for (yy = 0; yy < gh; yy++) {
		uint *pp = p + yy * f->stride;
		for (xx = 0; xx < gw; xx++)
			*pp++ = *g++ ? fgc : bgc;
	}
}

const char ABI [] =
"i sizeof_finfo		\n"
"- init_finfo   svii	\n"
"- fill_glyph16 siisii	\n"
"- fill_glyph24 siisii	\n"
"- fill_glyph32 siisii	\n"
;

const char __DEPENDS__ [] = "vfb.h";
