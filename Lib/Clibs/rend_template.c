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

/* this is the basic template code for the text renderer. It is specialized
   with #include to establish renderers for 16/24/32 bpp buffers
*/
#define get_rgb CON(get_rgb_)
#define put_rgb CON(put_rgb_)
#define render_pixel CON(render_pixel_)
#define ppack CON(pack_)

static inline void render_pixel (PIXEL *dest, int pixel, int rf, int gf, int bf)
{
	int rb, gb, bb;
	get_rgb (dest, &rb, &gb, &bb);
	put_rgb (dest,
		rb + ((rf - rb) * pixel >> 8),
		gb + ((gf - gb) * pixel >> 8),
		bb + ((bf - bb) * pixel >> 8)
	);
}

/* Params:
	`pp`		: array of glyph structures from rglyph.h
	`np`		: number of glyphs
	`fb`		: start of the framebuffer memory
	`width`		: framebuffer width (pitch)
	`clip`		: clip rectangle. Should be at least [0, 0, width, height]
	`fx0`		: start x pen-point
	`fy0`		: start y pen-point
	`charSpace`	: extra character spacing (in pixels) (default=0)
	`wordSpace`	: extra word spacing (in pixels) (default=0)
	`fg`		: foreground color

 The renderer blends the text on to the background. In other words for a specific
 background, we have to fill the framebuffer with that color and then call this
 renderer. An alternative framebuffer that renders with a specific bg color without
 reading the source pixels has been proved problematic because due to bearing a
 glyph may be shifted in a way that it overlaps the previous glyph, and the
 overwritting of source pixels makes this look wrong. This happens especially for
 italics.
 */
double rend_name (struct rglyph *pp[], int np, PIXEL *fb,
	       int width, int clip[], double fx0, double fy0,
	       double charSpace, double wordSpace, int fg)
{
	int clipx = clip [0];
	int clipy = clip [1];
	int clip_ex = clip [2] + clipx;
	int clip_ey = clip [3] + clipy;

	int i, r, c, pixel, dw, ew, c0, x=0;
	int px;
	int py;
	struct rglyph *p;
	int cS = d2fixed (charSpace);
	int wS = d2fixed (wordSpace);
	int x0 = d2fixed (fx0);
	int y0 = fy0;
	int bf = fg & 255;
	int gf = fg >> 8 & 255;
	int rf = fg >> 16 & 255;

	for (i = 0; i < np; i++) {
		p = pp [i];
		px = fixed2int (x0 + x) + p->px;
		py = y0 + p->py;
		dw = p->pitch;

//fprintf (stderr, "AD %i %i %i\n", p->px, p->advance, p->width);
		if (px >= clipx) c0 = 0;
		else if (px + p->rows <= clipx) {
			x += p->advance + (p->sp ? wS : cS);
			continue;
		} else c0 = -px;

		ew = px + p->width <= clip_ex ? p->width : clip_ex - px;
		if (py >= clipy && p->rows + py < clipy) {
			/* MMX? */
			for (r = 0; r < p->rows; r++)  {
				int pxx = px + (r + py) * width;
				for (c = c0; c < ew; c++)
					if ((pixel = p->bitmap [c + r * dw]))
						render_pixel (&fb [pxx + c], pixel, rf, gf, bf);
			}
		} else
			for (r = 0; r < p->rows; r++) {
				int pxx = px + (r + py) * width;
				if (r + py >= clip_ey)
					break;
				if (r + py < clipy)
					continue;
				for (c = c0; c < ew; c++)
					if ((pixel = p->bitmap [c + r * dw]))
						render_pixel (&fb [pxx + c], pixel, rf, gf, bf);
			}
		x += p->advance + (p->sp ? wS : cS);
	}

	return x / 64.0;
}

// for the case the font doesn't have gray tones. That's PSF
// consolefonts for the xterm, etc.  much faster.

double rend_mono_name (struct rglyph *pp[], int np, PIXEL *fb,
	       int width, int clip[], double fx0, double fy0,
	       double charSpace, double wordSpace, int fg)
{
	int clipx = clip [0];
	int clipy = clip [1];
	int clip_ex = clip [2] + clipx;
	int clip_ey = clip [3] + clipy;

	int i, r, c, dw, ew, c0, x=0;
	int px;
	int py;
	int d1, d2;
	struct rglyph *p;
	int cS = d2fixed (charSpace);
	int wS = d2fixed (wordSpace);
	int x0 = d2fixed (fx0);
	int y0 = fy0;
	const PIXEL fgc = ppack (fg);

	for (i = 0; i < np; i++) {
		p = pp [i];
		px = fixed2int (x0 + x) + p->px;
		py = y0 + p->py;
		dw = p->pitch;

		if (px >= clipx) c0 = 0;
		else if (px + p->rows <= clipx) {
			x += p->advance + (p->sp ? wS : cS);
			continue;
		} else c0 = -px;

		const unsigned char *bitmap = p->bitmap;
		ew = px + p->width <= clip_ex ? p->width : clip_ex - px;
		if (py >= clipy && p->rows + py < clip_ey)
			for (r = 0; r < p->rows; r++) {
				d1 = (r + py) * width + px;
				d2 = r * dw;
				for (c = c0; c < ew; c++)
					if (bitmap [d2 + c])
						fb [d1 + c] = fgc;
			}
		else
			for (r = 0; r < p->rows; r++) {
				if (r + py >= clip_ey)
					break;
				if (r + py < clipy)
					continue;
				d1 = (r + py) * width + px;
				d2 = r * dw;
				for (c = c0; c < ew; c++)
					if (bitmap [d2 + c])
						fb [d1 + c] = fgc;
			}
		x += p->advance + (p->sp ? wS : cS);
	}

	return x / 64.0;
}

//Todo: avoid multiple expansion of comments in the inclusion of this file.
#undef get_rgb
#undef put_rgb
#undef render_pixel
#undef ppack
#undef CON
#undef PIXEL
#undef rend_name
#undef rend_mono_name
