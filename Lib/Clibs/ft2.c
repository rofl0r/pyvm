/*
 *  Wrapper for the freetype library
 * 
 *  Copyright (c) 2007, Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

/******************************************************************************
	This wrapps the FreeType library and provides and alternative API
	which is more friendly to the pyvm DLL loader.

	The library returns grayscale glyph bitmaps as per the "rglyph.h"
	API structure.
******************************************************************************/

#include <stdio.h>
#include <math.h>
#include <ft2build.h>
#include FT_FREETYPE_H

static FT_Library library;

#define FT_CEIL(X) (((X+63)&-64)/64)
#define FT_FLOOR(X) ((X&-64)/64)

static inline int max (int a, int b)	{ return a > b ? a : b; }
static inline int min (int a, int b)	{ return a < b ? a : b; }

int init ()
{
	return FT_Init_FreeType (&library);
}

typedef struct {
	FT_Face face;
	int height;
	int ascent;
	int width;
	int fixed;
	int baseline;
	unsigned char *gamma;
} FTF_Font;

const int SIZEOF_FONTOBJ = sizeof (FTF_Font);

int load (FT_Face face, double ptsize, int retbuf[], void *fobj, void *gamma_addr)
{
	FTF_Font *F = (FTF_Font*) fobj;

	if (ptsize) {
		int err = FT_Set_Char_Size (face, 0, ptsize * 64, 0, 0);
		if (err) return -err;
	}

	/* global metrics */
	FT_Fixed scale;
	scale = face->size->metrics.y_scale;
	int ascent = FT_CEIL (FT_MulFix (face->bbox.yMax, scale));
	int descent = FT_CEIL (FT_MulFix (face->bbox.yMin, scale));
	int height = ascent - descent;
	int width = FT_CEIL (FT_MulFix (face->bbox.xMax, scale));
	int lineskip = FT_CEIL (FT_MulFix (face->height, scale));
	int fixed = FT_IS_FIXED_WIDTH (face);
//fprintf (stderr, "To open font [%s] isfixed =%i\n", fnm, fixed);

	/* return bunch of values */
	retbuf [0] = (int) face;
	retbuf [1] = ascent;
	retbuf [2] = descent;
	retbuf [3] = height;
	retbuf [4] = lineskip;
	retbuf [5] = width;
	retbuf [6] = fixed;

	F->face     = face;
	F->height   = height;
	F->ascent   = ascent;
	F->width    = width;
	F->fixed    = fixed;
	F->baseline = descent;
	F->gamma    = gamma_addr;

	return 0;
}

void all_glyphs (FTF_Font *F, unsigned int *a)
{
	FT_Face face = F->face;
	int gi;
	int cc;

	cc = FT_Get_First_Char (face, &gi);
	while (gi) {
		*a++ = cc;		// unicode charcode
		*a++ = gi;		// glyph index
		cc = FT_Get_Next_Char (face, cc, &gi);
	}
}

int count_glyphs (FTF_Font *F)
{
	FT_Face face = F->face;
	int gi;
	int cc;
	int s = 0;

	cc = FT_Get_First_Char (face, &gi);
	while (gi) {
		++s;
		cc = FT_Get_Next_Char (face, cc, &gi);
	}

	return s;
}

void adjustw (FTF_Font *F, int w)
{
	F->width = w;
}

void *open_font (char *fnm, char *data, int size)
{
	FT_Face face;
	int err;

	err = fnm [0] ? FT_New_Face (library, fnm, 0, &face)
		: FT_New_Memory_Face (library, data, size, 0, &face);
	if (err) return 0;

	return face;
}

void unload (FT_Face F)
{
	if (F) FT_Done_Face (F);
}

/* part II -- driven by PDF but can be generally useful. */

#include "rglyph.h"

int id_from_name (FTF_Font *F, char *name)
{
	return FT_Get_Name_Index (F->face, name);
}

int id_from_idx (FTF_Font *F, int c)
{
	return FT_Get_Char_Index (F->face, c);
}

/* glyphs are rendered by "GlyphID" which is not the ord() of
 * the character.  It is the value returned by FT_Get_Char_Index
 * and FT_Get_Name_Index.  `sp` is boolean which is true if the
 * glyph is the space character.  This is completely irrelevant
 * and you can safely pass 0.  It is used by the rasterizer in
 * the case we supply a non-zero wordSpacing value, which is
 * one of the possible ways PDF can draw text.  Other applications
 * that do not use wordSpacing do not need to mark the space
 * glyph.
 */
int make_glyph (FTF_Font *F, double ptsize, int gid, struct rglyph *p, int sp, int ispace)
{
	FT_Set_Char_Size (F->face, 0, ptsize * 64, 0, 0);

	/* Well, apparently, freetype without autohinting looks much
	 * better for this kind of task (type1 fonts in PDF). xpdf does
	 * the same, so no wondering why it looks real good.
	 */
	if (FT_Load_Glyph (F->face, gid,
			   FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP))
		return 0;
	if (FT_Render_Glyph (F->face->glyph, 0))
		return 0;

	FT_GlyphSlot glyph = F->face->glyph;
	int w = p->width = min (glyph->bitmap.width, 6 * F->width);
	int r = p->rows = min (glyph->bitmap.rows, 3 * F->height);
	p->px = glyph->bitmap_left;
	p->py = - glyph->bitmap_top;
	//p->py = -F->baseline - glyph->bitmap_top;
	p->advance = glyph->advance.x;
	if (ispace) p->advance = 64 * ispace;
	p->pitch = glyph->bitmap.pitch;
	p->sp = sp;
	memcpy (p->bitmap, glyph->bitmap.buffer, w * r);

	if (F->gamma) {
		int i, j;
		for (i = 0; i < p->rows; i++)
			for (j = 0; j < p->width; j++)
				p->bitmap [j + i * p->width] =
					F->gamma [(unsigned char)p->bitmap [j + i * p->width]];
	}

	return w * r + sizeof *p - 4;
}

int glyph_advance (FTF_Font *F, double ptsize, int gid)
{
	FT_Set_Char_Size (F->face, 0, ptsize * 64, 0, 0);
	if (FT_Load_Glyph (F->face, gid,
			   FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP))
		return 0;
	return F->face->glyph->advance.x;
}

const char ABI [] =
"i init			-	\n"
"i SIZEOF_FONTOBJ		\n"
"i load			idp32sv	\n"
"i open_font		ssi	\n"
"- unload		i	\n"
"- all_glyphs		sp32	\n"
"i count_glyphs		s	\n"
"- adjustw		si	\n"
"i id_from_name		ss	\n"
"i id_from_idx		si	\n"
"i make_glyph		sdisii	\n"
"i glyph_advance	sdi	\n"
;
