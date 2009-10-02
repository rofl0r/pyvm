/*
 *  Wrapper for SDL
 *
 *  Copyright (c) 2007 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

#include <SDL/SDL.h>

typedef unsigned int RRGGBB;

typedef struct {
	/* SDL is usually global. There is only one SDL display
	 * per application. So this structure could be static globals,
	 * and the entire wrapper simplified to avoid passing "Display"
	 * to each function.
	 */
	SDL_Surface *screen;
	int width, height;
	int bpp;
} Display;

int sizeof_sdl ()
{
	return sizeof (Display);
}

#if 0
/* get maximum screen size */
int screen_size (int wh[])
{
	SDL_Rect **modes;
	int i;

	if (SDL_Init (SDL_INIT_VIDEO) < 0) return 0;
	wh [0] = wh [1] = 0;
	modes = SDL_ListModes (0, SDL_FULLSCREEN);
	if (modes && (int)modes != -1)
		for (i = 0; modes [i]; i++)
			if (modes [i]->w >= wh [0] && modes [i]->h >= wh [1]) {
				wh [0] = modes [i]->w;
				wh [1] = modes [i]->h;
			}
	return 1;
}
#endif

int init (Display *d, int w, int h, int depth, int ret[])
{
	if (SDL_Init (SDL_INIT_VIDEO) < 0) return 0;

	int scrw, scrh;

	SDL_Rect **modes;
	modes = SDL_ListModes (0, SDL_FULLSCREEN);
	if (modes != (SDL_Rect**)-1) {
		scrw = modes [0]->w;
		scrh = modes [0]->h;
	} else {
		scrw = 800;
		scrh = 600;
	}

	if (!w || w > scrw) w = scrw;
	if (!h || h > scrh) h = scrh;

	if (depth)
		d->screen = SDL_SetVideoMode (w, h, depth, SDL_SWSURFACE);
	else d->screen = SDL_SetVideoMode (w, h, 0, SDL_SWSURFACE|SDL_ANYFORMAT);
	if (!d->screen) return 0;
	ret [0] = d->bpp = d->screen->format->BitsPerPixel / 8;
	ret [1] = (int) d->screen->pixels;
	d->width = ret [2] = d->screen->w;
	d->height = ret [3] = d->screen->h;
	ret [4] = (int) d->screen;
	ret [5] = d->screen->format->Rshift;
	ret [6] = d->screen->format->Gshift;
	ret [7] = d->screen->format->Bshift;
	return 1;
}

void lock (SDL_Surface *s)
{
	SDL_LockSurface (s);
}

void unlock (SDL_Surface *s)
{
	SDL_UnlockSurface (s);
}

void flush (Display *d, int x0, int y0, int x1, int y1)
{
	if (d->screen->flags & SDL_DOUBLEBUF) {
		SDL_Flip (d->screen);
		return;
	}

	if (x0 > x1)
		SDL_UpdateRect (d->screen, 0, 0, 0, 0);
	else {
		if (x0 < 0) x0 = 0;
		if (y0 < 0) y0 = 0;
		if (y1 + y0 > d->height)
			y1 = d->height;
		if (x1 + x0 > d->width)
			x1 = d->width;
		SDL_UpdateRect (d->screen, x0, y0, x1 - x0, y1 - y0);
	}
}

void set_wm_name (char *s)
{
	SDL_WM_SetCaption (s, s);
}

void where (int xy[])
{
	SDL_GetMouseState (xy, xy+1);
}

void move_mouse (int x, int y)
{
	SDL_WarpMouse (x, y);
}

void block_until_event (Display *d)
{
	SDL_WaitEvent (0);
}

int get_event (Display *d, int ret[])
{
	SDL_Event E;
	int sym;

	if (!SDL_PollEvent (&E))
		return 0;

	switch (E.type) {
	case SDL_KEYDOWN:
		ret [0] = 0;
		ret [1] = E.key.keysym.sym;
	break;
	case SDL_KEYUP:
		ret [0] = 1;
		ret [1] = E.key.keysym.sym;
	break;
	case SDL_MOUSEBUTTONDOWN:
		ret [0] = 2;
		goto bt;
	case SDL_MOUSEBUTTONUP:
		ret [0] = 3;
	bt:
		ret [1] = E.button.button;
		ret [2] = E.button.x;
		ret [3] = E.button.y;
	break;
	case SDL_VIDEOEXPOSE:
		ret [0] = 4;
	break;
	case SDL_QUIT:
		ret [0] = 5;
	break;
	default:
	case SDL_MOUSEMOTION:
		return 0;
	}

	return 1;
}

void terminate ()
{
	SDL_Quit ();
}

/*
 * Surface for sub-framebuffer
 */

void *create_fb (Display *d, int w, int h, int ret[])
{
	SDL_PixelFormat *f = d->screen->format;
	SDL_Surface *s = SDL_CreateRGBSurface (SDL_SWSURFACE, w, h, f->BitsPerPixel,
					f->Rmask, f->Gmask, f->Bmask, 0);
	ret [0] = (int) s->pixels;
	return s;
}

void blit (SDL_Surface *source, int x, int y, SDL_Surface *screen)
{
	SDL_Rect DR = { x, y, 0, 0 };
	SDL_BlitSurface (source, 0, screen, &DR);
}

void free_surface (SDL_Surface *screen)
{
	SDL_FreeSurface (screen);
}

/*
 * Accelerated functions -- override our virtual framebuffer
 */

void fill_rect (SDL_Surface *screen, int x, int y, int w, int h, RRGGBB col)
{
	SDL_Rect R = { x, y, w, h };
	unsigned int mcol = SDL_MapRGB (screen->format, col >> 16, 255&(col >> 8), 255&col);
	SDL_FillRect (screen, &R, mcol);
}

void put_image (SDL_Surface *screen, void *data, int x, int y, int sx, int sy, int w, int h, int sw, int sh)
{
	SDL_PixelFormat *f = screen->format;
	SDL_Surface *s;

	if (x < 0) {
		sx -= x;
		sw += x;
		x = 0;
	}
	if (y < 0) {
		sy -= y;
		sy += y;
		y = 0;
	}
	if (sx < 0) {
		sw += sx;
		sx = 0;
	}
	if (sy < 0) {
		sh += sy;
		sy = 0;
	}

	SDL_Rect SR = { sx, sy, sw, sh };
	SDL_Rect DR = { x, y, 0, 0 };

	s = SDL_CreateRGBSurfaceFrom (data, w, h, f->BitsPerPixel,
					f->BytesPerPixel * w, f->Rmask, f->Gmask, f->Bmask, 0);
	/* screen must not be locked */
	/* would be better to unlock-blit-relock if? */
	/* is SDL_ASYNCBLIT relevant? */
	SDL_BlitSurface (s, &SR, screen, &DR);
	SDL_FreeSurface (s);
}

const char ABI [] =
"i sizeof_sdl		-		\n"
"i init			siiip32		\n"
"- fill_rect		iiiiii		\n"
"- put_image		iviiiiiiii	\n"
"- lock!		i		\n"
"- unlock		i		\n"
"- flush		siiii		\n"
"- set_wm_name		s		\n"
"- where		p32		\n"
"- move_mouse		ii		\n"
"- block_until_event!	s		\n"
"i get_event		sp32		\n"
"- terminate		-		\n"
"i create_fb		siip32		\n"
"- blit			iiii		\n"
"- free_surface		i		\n"
;
