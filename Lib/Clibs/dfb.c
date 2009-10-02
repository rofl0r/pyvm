/*
 *  Wrapper for DirectFB
 *
 *  Copyright (C) 2007, Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

/*
 * The headers of DirectFB are required.
 * This has been tested with 0.9.25.1
 *
 * In order for this thing to work you need to allow -rw- permission
 * on several devices, including, but not limited to:
 *	/dev/fb?
 *	/dev/input/mice
 *	/dev/tty
 *
 * We are interested in fullscreen directfb only.
 * I hope I'm doing the right thing!
 */

// TODODO:
//  how do we enable vt swithing?
//  how do we pick another resolution?

#define DO_FLIP

#include <stdio.h>
#include <unistd.h>
#include <directfb.h>

typedef struct {
	IDirectFB *dfb;
	IDirectFBSurface *primary;
	IDirectFBEventBuffer *ev;
	IDirectFBDisplayLayer *layer;

	int screen_width;
	int screen_height;
	int bpp;
	struct {
		int shift, ctrl, alt;
	} pressed;
	struct {
		int x, y;
	} pointer;
	DFBSurfacePixelFormat pixel_format;
} Display;

int sizeof_dfb ()
{
	return sizeof (Display);
}

int init (Display *d, int wanted_bpp, int ret[])
{
#define CHECK(x) if (x != DFB_OK) return 0;

	int argc = 0;
	char **argv = 0;

	DFBSurfaceDescription dsc;
	CHECK (DirectFBInit (&argc, &argv));
	CHECK (DirectFBCreate (&d->dfb));

	/* we need a layer in order to use the mouse, although
	 * we will not use any windowing facilities of DirectFB.  */
	CHECK (d->dfb->GetDisplayLayer (d->dfb, DLID_PRIMARY, &d->layer));
	CHECK (d->layer->SetCooperativeLevel (d->layer, DLSCL_ADMINISTRATIVE));
	CHECK (d->layer->EnableCursor (d->layer, 1));

	/* fullscreen would be nice, but at the time the mouse
	 * DOES NOT work in this mode. Hope we don't miss any speed.  */
	//CHECK (d->dfb->SetCooperativeLevel (d->dfb, DFSCL_FULLSCREEN));

	dsc.flags = DSDESC_CAPS;
	dsc.caps  = DSCAPS_PRIMARY;
#ifdef DO_FLIP
	dsc.caps |= DSCAPS_FLIPPING;
#endif

	/* request bpp */
	if (wanted_bpp) {
		dsc.flags |= DSDESC_PIXELFORMAT;
		switch (wanted_bpp) {
			case 1: dsc.pixelformat = DSPF_RGB332; break;
			case 2: dsc.pixelformat = DSPF_RGB16; break;
			case 3: dsc.pixelformat = DSPF_RGB24; break;
			case 4: dsc.pixelformat = DSPF_RGB32; break;
			default: return 0;
		}
	}

	CHECK (d->dfb->CreateSurface (d->dfb, &dsc, &d->primary ));

	/* check if we can work with this bpp */
	DFBSurfacePixelFormat pixel_format;
	CHECK (d->primary->GetPixelFormat (d->primary, &pixel_format));
	switch (pixel_format) {
		case DSPF_RGB16: d->bpp = 2; break;
		case DSPF_RGB24: d->bpp = 3; break;
		case DSPF_RGB32: d->bpp = 4; break;
		case DSPF_RGB332: d->bpp = 1; break;
		default:
			fprintf (stderr, "Unsupported pixel format");
			return 0;
	}
	d->pixel_format = pixel_format;
	CHECK (d->primary->GetSize (d->primary, &d->screen_width, &d->screen_height));
	ret [0] = d->bpp;
	ret [1] = d->screen_width;
	ret [2] = d->screen_height;

	/* set up inputs */ //xxx: check for mouse AND keyboard, otherwise fail
	CHECK (d->dfb->CreateInputEventBuffer (d->dfb, DICAPS_ALL, DFB_TRUE, &d->ev))
	d->pressed.shift = d->pressed.alt = d->pressed.ctrl = 0;
	d->pointer.x = d->pointer.y = 0;

	return 1;
}

void set_color (Display *d, int c)
{
	d->primary->SetColor (d->primary, c>>16&255, c>>8&255, c&255, 0xff);
}

void fill_rect (Display *d, int x, int y, int w, int h)
{
	d->primary->FillRectangle (d->primary, x, y, w, h);
}

void draw_line (Display *d, int x1, int y1, int x2, int y2)
{
	d->primary->DrawLine (d->primary, x1, y1, x2, y2);
}

void flush (Display *d)
{
#ifdef DO_FLIP
	d->primary->Flip (d->primary, NULL, DSFLIP_WAITFORSYNC | DSFLIP_BLIT);
#else
	d->dfb->WaitForSync (d->dfb);
#endif
}

/* Image surface */

void *create_image (Display *d, int w, int h, char *data, int bpp)
{
	IDirectFBSurface *im;
	DFBSurfaceDescription dsc;

	dsc.flags = DSDESC_PIXELFORMAT | DSDESC_WIDTH | DSDESC_HEIGHT
		  | DSDESC_CAPS | DSDESC_PREALLOCATED;
	dsc.caps = DSCAPS_SYSTEMONLY;
	dsc.pixelformat = bpp == 1 ? DSPF_RGB332 : bpp == 2 ? DSPF_RGB16 : bpp == 3 ?
			DSPF_RGB24 : DSPF_RGB32;
	dsc.width = w;
	dsc.height = h;
	dsc.preallocated [0].data = data;
	dsc.preallocated [0].pitch = w * d->bpp;
	dsc.preallocated [1].data = 0;
	dsc.preallocated [1].pitch = 0;

	CHECK (d->dfb->CreateSurface (d->dfb, &dsc, &im));

	return im;
}

void put_image (Display *d, void *v, int x, int y, int sx, int sy, int w, int h)
{
	DFBRectangle R;
	R.x = sx;
	R.y = sy;
	R.w = w;
	R.h = h;
	d->primary->Blit (d->primary, v, &R, x, y);
}

void destroy_image (void *v)
{
	IDirectFBSurface *im = v;
	im->Release (im);
}

void lock_surface (IDirectFBSurface *s)
{
	s->Lock (s, DSLF_WRITE, 0, 0);
}

void unlock_surface (IDirectFBSurface *s)
{
	s->Unlock (s);
}

// events

void block_until_event (Display *d)
{
	DFBInputEvent ev;
	d->ev->PeekEvent (d->ev, DFB_EVENT (&ev));
}

int has_event (Display *d)
{
	return d->ev->HasEvent (d->ev) == DFB_OK;
}

int get_event (Display *d, int rz[])
{
	/* translate events to backend */
	DFBInputEvent ev;
	d->ev->GetEvent (d->ev, DFB_EVENT (&ev));

	switch (ev.type) {
	case DIET_KEYPRESS:
		rz [0] = 1;
		switch (ev.key_id) {
		case DIKI_SHIFT_L:
		case DIKI_SHIFT_R:
			if (d->pressed.shift) return 0;
			d->pressed.shift = 1;
		break;
		case DIKI_ALT_L:
		case DIKI_ALT_R:
			if (d->pressed.alt) return 0;
			d->pressed.alt = 1;
		break;
		case DIKI_CONTROL_L:
		case DIKI_CONTROL_R:
			if (d->pressed.ctrl) return 0;
			d->pressed.ctrl = 1;
		}
		rz [1] = ev.key_symbol;
	break;
	case DIET_KEYRELEASE:
		rz [0] = 2;
		switch (ev.key_id) {
		case DIKI_SHIFT_L:
		case DIKI_SHIFT_R: d->pressed.shift = 0; break;
		case DIKI_ALT_L:
		case DIKI_ALT_R: d->pressed.alt = 0; break;
		case DIKI_CONTROL_L:
		case DIKI_CONTROL_R: d->pressed.ctrl = 0; break;
		default: return 0;
		}
		rz [1] = ev.key_symbol;
	break;
	case DIET_BUTTONPRESS:
		rz [0] = 3;
		goto bt;
	case DIET_BUTTONRELEASE:
		rz [0] = 4;
	bt:
		rz [1] = ev.button == 0 ? 1 : ev.button == 1 ? 3 : 0;
		d->layer->GetCursorPosition (d->layer, &rz [2], &rz [3]);
	break;
	default: return 0;
	}

	return 1;
}

void where (Display *d, int xy[])
{
	d->layer->GetCursorPosition (d->layer, &xy [0], &xy [1]);
}

void terminate (Display *d)
{
	d->ev->Release (d->ev);
	d->primary->Release (d->primary);
	d->layer->Release (d->layer);
	d->dfb->Release (d->dfb);
}

const char ABI [] =
"i sizeof_dfb		-\n"
"i init			sip32\n"
"- set_color		si\n"
"- fill_rect		siiii\n"
"- draw_line		siiii\n"
"- flush		s\n"
"i create_image		siisi\n"
"- put_image		siiiiiii\n"
"- destroy_image	i\n"
"- lock_surface!	i\n"
"- unlock_surface	i\n"
"- block_until_event!	s\n"
"i has_event		s\n"
"i get_event		sp32\n"
"- where		sp32\n"
"- terminate		s\n"
;
