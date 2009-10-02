/*
 *  Linux framebuffer console backend.
 *
 *  Copyright (C) 2007, 2008, 2009 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/keyboard.h>
#include <linux/vt.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/poll.h>
#include <errno.h>

/*
 This library has no dependancies. Except "linux". so we can safely
 use linux-specific calls.
*/

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * MacOS cursor, from SDL                                                    */

#define MOUSE_WIDTH	16
#define MOUSE_HEIGHT	16
#define MOUSE_HOTX	0
#define MOUSE_HOTY	0

const static unsigned char mouse_cdata [] =
{
 0x00,0x00, 0x40,0x00, 0x60,0x00, 0x70,0x00,
 0x78,0x00, 0x7C,0x00, 0x7E,0x00, 0x7F,0x00,
 0x7F,0x80, 0x7C,0x00, 0x6C,0x00, 0x46,0x00,
 0x06,0x00, 0x03,0x00, 0x03,0x00, 0x00,0x00
};
const static unsigned char mouse_cmask [] =
{
 0xC0,0x00, 0xE0,0x00, 0xF0,0x00, 0xF8,0x00,
 0xFC,0x00, 0xFE,0x00, 0xFF,0x00, 0xFF,0x80,
 0xFF,0xC0, 0xFF,0xE0, 0xFE,0x00, 0xEF,0x00,
 0xCF,0x00, 0x87,0x80, 0x07,0x80, 0x03,0x00
};

typedef unsigned char u8;
typedef unsigned short u16;
typedef struct { u8 r, g, b; } u24;
typedef unsigned int u32;

void terminate ();

static struct {
	int in_use;		/* exclusive access */
	int fd;			/* framebuffer fd */
	int kbd;		/* keyboard fd */
	union {
		void *mm;	/* mmaped framebuffer space */
		u8 *mm8;
		u16 *mm16;
		u24 *mm24;
		u32 *mm32;
	};
	int bpp, xres, yres;	/* dpy parameters */
	struct termios ts;	/* stored termios params */
	int mfd;		/* mouse fd */
	int imps2;		/* mouse protocol, ps/2 or imps/2 */
	int mx, my;		/* current mouse position */
	int rmouse;		/* refresh mouse on unlock()? */
	int mpipe [2];		/* send clicks from mouse thread to get_event() */
	int mqueued;		/* #queued mouse events. MOUSE_KILL */
	pthread_t th;		/* thread ID of mouse thread */
	sem_t sem;		/* console lock */
	union {			/* mouse unders */
		u8  um8  [MOUSE_WIDTH * MOUSE_HEIGHT];
		u16 um16 [MOUSE_WIDTH * MOUSE_HEIGHT];
		u24 um24 [MOUSE_WIDTH * MOUSE_HEIGHT];
		u32 um32 [MOUSE_WIDTH * MOUSE_HEIGHT];
	} um;
	struct {		/* key state */
		int ctrl, shift, alt;
	} k;
	int winkeyvt;		/* the winkeys are used to switch vt ? */
	int screenshot;		/* CTRL-ALT-ESC: screenshot? */

	int hibernating;	/* should mouse motion wake up */
	int switchto;		/* vt to switch to */
	int away;		/* true if out of the vt */

	/* mouse kill */
	int keepmouse;		/* call termination but keep the mouse thread running in order to.. */
	void (*killvm)();	/* ... call this function if all else fails */

	/* Acceleration */
	void *mmio;		/* MMIO address for acceleration */
	int mmio_len;		/* for munmap */
	int restore_accel;	/* or not? */
	struct fb_var_screeninfo orig_vscr;
	struct accel_funcs *accel;	/* card drivers */
} D;

#include "accel.c"

static int keymap [128];
static void init_kmap ();
static void config_mouse ();
static void *mouse_thread (void*);

/*
 MOUSE_KILL:
  If defined the display will be shut down if the mouse thread detects
  20 queued click events.  The mouse thread is always running and produces
  clicks which are consumed by the get_event() call.
  If program freezes try clicking left mouse many times.
*/
#define MOUSE_KILL

/*
 This works on the devices
	/dev/fb0
	/dev/tty0
	/dev/input/mice
 for the video, keyboard and mouse (imps2).
 According to SDL/DFB there can be other devices like tty%i, /dev/vc/%i
 but these are not documented anywhere and I can't do anything without
 specs.
*/

/* find out current vt */
int getvt ()
{
	struct vt_stat vtstate;
	if (ioctl (0, VT_GETSTATE, &vtstate) < 0)
		return -1;
	return vtstate.v_active;
}

static void clear_state ()
{
	D.in_use = 0;
	D.mm = MAP_FAILED;
	D.kbd = -1;
	D.mfd = -1;
	D.th = 0;
	D.mpipe [0] = D.mpipe [1] = -1;
	D.mmio = MAP_FAILED;
	D.restore_accel = 0;
	D.hibernating = 0;
	D.imps2 = 0;
	D.away = 0;
	D.keepmouse = 0;
}

int init (int ret[], int do_accel, int try_imps2, void (*killvm)(), int winkeyvt, int screenshot)
{
	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo f;
	struct termios ts;

	if (D.in_use) return 1;

	clear_state ();
	D.in_use     = 1;
	D.killvm     = killvm;
	D.winkeyvt   = winkeyvt;
	D.screenshot = screenshot;

	// mmap the framebuffer
	D.fd = open ("/dev/fb0", O_RDWR);
	if (D.fd == -1) return 2;
	if (ioctl (D.fd, FBIOGET_VSCREENINFO, &v) == -1) return 3;
	if (ioctl (D.fd, FBIOGET_FSCREENINFO, &f) == -1) return 3;
	v.xoffset = v.yoffset = 0;
#ifdef	FBIOPAN_DISPLAY
	if (ioctl (D.fd, FBIOPAN_DISPLAY, &v) == -1) return 3;
#else
	if (ioctl (D.fd, FBIOPUT_VSCREENINFO, &v) == -1) return 3;
#endif
	ret [0] = D.xres = v.xres;
	ret [1] = D.yres = v.yres;
	ret [2] = D.bpp = v.bits_per_pixel / 8;
//fprintf (stderr, "BITSPERPIXEL %i\n", v.bits_per_pixel);
#if 0
	D.mm = mmap (0, f.smem_len, PROT_READ|PROT_WRITE, MAP_SHARED, D.fd, 0);
#else
	D.mm = mmap (0, v.xres * v.yres * D.bpp, PROT_READ|PROT_WRITE, MAP_SHARED, D.fd, 0);
#endif
	if (D.mm == MAP_FAILED) return 4;
	ret [3] = (int) D.mm;

	ret [4] = v.red.offset;
	ret [5] = v.green.offset;
	ret [6] = v.blue.offset;

	// hardware acceleration
	if (can_accel (f.accel) && f.mmio_len && do_accel) {
		/*
		 This seems to be a hack done by DirectFB/SDL. They disable
		 acceleration of the card and thus are able to mmap the MMIOs.
		 So, what happens with the card? Can X use it any more?
		 What happens on another VT? Should we re-enable accel on vt-switch?
		*/
		D.orig_vscr = v;
		v.accel_flags = 0;
		if (ioctl (D.fd, FBIOPUT_VSCREENINFO, &v) >= 0) {
			D.restore_accel = 1;
			D.mmio = mmap (0, f.mmio_len, PROT_READ|PROT_WRITE, MAP_SHARED, D.fd, f.smem_len);
			D.mmio_len = f.mmio_len;
			D.accel = can_accel (f.accel);
		}

		if (D.mmio != MAP_FAILED)
			fprintf (stderr, "Hardware acceleration for card [%s]\n", f.id);
		else fprintf (stderr, "Hardware acceleration Failed [%s]\n", f.id);
	}

	// tty/keyboard
	D.kbd = open ("/dev/tty0", O_RDWR);
	if (D.kbd == -1) return 5;
	if (ioctl (D.kbd, KDSETMODE, KD_GRAPHICS) == -1)
		return 6;
	if (ioctl (D.kbd, KDSKBMODE, K_MEDIUMRAW) == -1)
		return 7;
	tcgetattr (D.kbd, &D.ts);
	ts = D.ts;
	ts.c_cc [VTIME] = 0;
	ts.c_cc [VMIN] = 1;	// blocking
	ts.c_lflag &= ~(ICANON|ISIG|ECHO);
	ts.c_iflag = 0;
	tcsetattr (D.kbd, TCSAFLUSH, &ts);
	D.k.ctrl = D.k.alt = D.k.shift = 0;

	// mouse
	D.mfd = open ("/dev/input/mice", O_RDWR);
	if (D.mfd == -1) return 8;
	D.mx = D.xres / 2;
	D.my = D.yres / 2;
	if (pipe (D.mpipe) == -1) return 9;
	D.mqueued = 0;
	if (try_imps2)
		config_mouse ();

	init_kmap ();

	if (sem_init (&D.sem, 0, 1) == -1)
		return 10;

	// mouse thread
	pthread_attr_t attr;
	pthread_attr_init (&attr);
	pthread_attr_setschedpolicy (&attr, SCHED_FIFO);
	if (pthread_create (&D.th, &attr, mouse_thread, 0))
		return 11;

	return 0;
}

/*
 by default the mouse is opened in PS/2 mode which doesn't use
 the scroll wheel. However, the scroll wheel is essential these
 days, so try to enable it.
 (taken from gpm:src/drivers/imps2/i.c)
*/ 

#define GPM_AUX_SEND_ID	       0xF2
#define GPM_AUX_SET_RES        0xE8
#define GPM_AUX_SET_SCALE11    0xE6
#define GPM_AUX_SET_SCALE21    0xE7
#define GPM_AUX_GET_SCALE      0xE9
#define GPM_AUX_SET_STREAM     0xEA
#define GPM_AUX_SET_SAMPLE     0xF3
#define GPM_AUX_ENABLE_DEV     0xF4
#define GPM_AUX_DISABLE_DEV    0xF5 
#define GPM_AUX_RESET          0xFF
#define GPM_AUX_ACK            0xFA

static unsigned char basic_init[] = { GPM_AUX_ENABLE_DEV, GPM_AUX_SET_SAMPLE, 100 };
static unsigned char imps2_init[] =
 { GPM_AUX_SET_SAMPLE, 200, GPM_AUX_SET_SAMPLE, 100, GPM_AUX_SET_SAMPLE, 80 };
static unsigned char ps2_init[] =
 { GPM_AUX_SET_SCALE11, GPM_AUX_ENABLE_DEV, GPM_AUX_SET_SAMPLE, 100, GPM_AUX_SET_RES, 3 };
static unsigned char send_id[] = { GPM_AUX_SEND_ID };

void config_mouse ()
{
	unsigned char c = 0;
	write (D.mfd, basic_init, sizeof basic_init);
	write (D.mfd, imps2_init, sizeof imps2_init);
	write (D.mfd, send_id, sizeof send_id);
	if (read (D.mfd, &c, 1) < 0 || c != GPM_AUX_ACK)
		return;
	if (read (D.mfd, &c, 1) < 0)
		return;
	write (D.mfd, ps2_init, sizeof ps2_init);
	fprintf (stderr, "Mouse id %i\n", c);
	if (c == 3)
		D.imps2 = 1;
}

/*
 Draw/Remove mouse.
 Can be improved speedwise.
*/

static void put_mouse ()
{
	int x0, y0, x, y, c, sx, sy;
	int mask, col;
	x0 = D.mx - MOUSE_HOTX;
	y0 = D.my - MOUSE_HOTY;
	D.rmouse = 0;

	/* save unders */
	for (c = y = 0; y < MOUSE_HEIGHT; y++)
		for (x = 0; x < MOUSE_WIDTH; x++, c++) {
			sy = y0 + y;
			sx = x0 + x;
			if (sy < 0 || sx < 0 || sy >= D.yres || sx >= D.xres)
				continue;
			int offset = sx + sy * D.xres;
			switch (D.bpp) {
				case 1: D.um.um8  [c] = D.mm8  [offset]; break;
				case 2: D.um.um16 [c] = D.mm16 [offset]; break;
				case 3: D.um.um24 [c] = D.mm24 [offset]; break;
				case 4: D.um.um32 [c] = D.mm32 [offset]; break;
			}

		}

	/* draw cursor */
	for (c = y = 0; y < MOUSE_HEIGHT; y++)
		for (x = 0; x < MOUSE_WIDTH; x++, c++) {
			sy = y0 + y;
			sx = x0 + x;
			if (sy < 0 || sx < 0 || sy >= D.yres || sx >= D.xres)
				continue;
			mask = mouse_cmask [c/8] & (128>>(c%8));
			col = mouse_cdata [c/8] & (128>>(c%8));
			if (mask) {
				int cc = !col ? -1 : 0;
				int offset = sx + sy * D.xres;
				switch (D.bpp) {
				case 1: D.mm8 [offset] = cc; break;
				case 2: D.mm16 [offset] = cc; break;
				case 3: { u24 cc24 = { cc, cc, cc };
					D.mm24 [offset] = cc24; } break;
				case 4: D.mm32 [offset] = cc; break;
				}
			} else;
		}
}

static void rmv_mouse ()
{
	int x0, y0, x, y, c, sx, sy;
	x0 = D.mx - MOUSE_HOTX;
	y0 = D.my - MOUSE_HOTY;
	D.rmouse = 1;

	/* restore unders */
	for (c = y = 0; y < MOUSE_HEIGHT; y++)
		for (x = 0; x < MOUSE_WIDTH; x++, c++) {
			sy = y0 + y;
			sx = x0 + x;
			if (sy < 0 || sx < 0 || sy >= D.yres || sx >= D.xres)
				continue;
			int offset = sx + sy * D.xres;
			switch (D.bpp) {
				case 1: D.mm8  [offset] = D.um.um8  [c]; break;
				case 2: D.mm16 [offset] = D.um.um16 [c]; break;
				case 3: D.mm24 [offset] = D.um.um24 [c]; break;
				case 4: D.mm32 [offset] = D.um.um32 [c]; break;
			}
		}
}

/*
 Handle mouse.  Update x,y pointer position and generate click events.
 We handle only the (Im)PS/2 mouse.  For further info look in
	SDL/src/video/fbcon/SDL_fbevents.c:handle_mouse()
 which handles almost all known mouse protocols (adapted from SVGAlib
 (taken from gpm, etc.))
*/
struct mousedat { char b, s; };

static inline int min (int a, int b) { return a < b ? a : b; }
static inline int max (int a, int b) { return a > b ? a : b; }

void terminate ();

static void *mouse_thread (void *x)
{
	unsigned char mb [3];
	int bstate = 0, click, wheel = 0, nn;
	struct mousedat M;

	/* flush mouse */
	struct pollfd pfd [1];
	pfd [0].fd = D.mfd;
	pfd [0].events = POLLIN | POLLHUP;

	poll (pfd, 1, -1);

	for (;;) {
		while ((nn = read (D.mfd, mb, 3)) > 0) {

			/* vt switch:
			   the mouse fd is closed because otherwise, running two applications on
			   two framebuffers will result in two mouse pointers! Another solution
			   might be to just stop reading from the mfd...  */
			if (D.away) {
				close (D.mfd);
				D.mfd = -1;
				while (D.away) 
					usleep (150000);
				D.mfd = open ("/dev/input/mice", O_RDWR);
				if (D.mfd == -1) {
					fprintf (stderr, "Can't reclaim mouse after vt switch!\n");
					terminate ();
				} else if (D.imps2)
					config_mouse ();
				continue;
			}

			/* over here in imps/2 mode we get 3 bytes and one extra byte
			   (not 4 at once). If we get just one byte before it means we're
			   out of sync with the mouse serialization and the byte is dropped */
			if (D.imps2) {
				if (nn == 1)
					continue;
				read (D.mfd, &wheel, 1);
			}

			if (wheel) {
				M.s = 0;
				M.b = 4 + (wheel < 127 ? 0 : 1);
				write (D.mpipe [1], &M, sizeof M);
				continue;
			}

			if (mb [1] || mb [2]) {
				int dx, dy, mx, my;
				dx = (mb [0] & 0x10) ? mb [1] - 256 : mb [1];
				dy = (mb [0] & 0x20) ? mb [2] - 256 : mb [2];
				mx = max (min (D.mx + dx, D.xres - 1), 0);
				my = max (min (D.my - dy, D.yres - 1), 0);
				if (mx != D.mx || my != D.my) {
					sem_wait (&D.sem);
					if (D.mfd != -1)
						rmv_mouse ();
					D.mx = mx;
					D.my = my;
					if (D.mfd != -1)
						put_mouse ();
					sem_post (&D.sem);
					if (D.hibernating) {
						M.b = -1;
						write (D.mpipe [1], &M, sizeof M);
					}
				}
			}
			if (bstate != (mb [0] & 7)) {
				click = bstate ^ mb [0];
				if (click & 1) { //left
#ifdef MOUSE_KILL
					if (D.mqueued++ == 20) {
						fprintf (stderr, "MouseKILL\n");
						D.keepmouse = 1;
						terminate ();
						fprintf (stderr, "Mouse KILL II %i \n", time(0));
						/* after 5 seconds assume the vm is in some
						   infinite loop or deadlock. kill for good  */
						sleep (5);
						fprintf (stderr, "killvm %i\n", time(0));
						D.killvm ();
					}
#endif
					M.b = 1;
					M.s = bstate & 1;
					write (D.mpipe [1], &M, sizeof M);
				}
				if (click & 2) { // right
					M.b = 3;
					M.s = !!(bstate & 2);
					write (D.mpipe [1], &M, sizeof M);
				}
				if (click & 4) { // middle
					M.b = 2;
					M.s = !!(bstate & 4);
					write (D.mpipe [1], &M, sizeof M);
				}
				bstate = mb [0];
			}
		}
		// vtswitch or terminate
		pthread_testcancel ();
		usleep (10000);
	}

	return 0;
}

/*
 The event types are (ev [0]):
	0: key press [keysym]
	1: key release [keysym]
	2: button press [button 0-1-2-4-5, x, y]
	   (4-5 are the scroll wheel)
	3: button release [button 0-1-2, x, y]
	4: vtswitch and redraw
	5: shutdown
	6: screenshot
*/

int get_event (int ev[])
{
	int r;
	struct pollfd pfd [2];

#ifdef MOUSE_KILL
	if (!D.in_use) {
		ev [0] = 5;
		return 1;
	}
#endif

	pfd [0].fd = D.kbd;
	pfd [0].events = POLLIN | POLLERR | POLLHUP;
	pfd [0].revents = 0;
	pfd [1].fd = D.mpipe [0];
	pfd [1].events = POLLIN | POLLERR | POLLHUP;
	pfd [1].revents = 0;
	r = poll (pfd, 2, 0);
	if (r <= 0) return 0;

	if (pfd [0].revents) {
		unsigned char ch;
		int r = read (D.kbd, &ch, 1);
		if (r > 0) {
			int press = (ch & 0x80) == 0;
			ev [0] = !press;
			ev [1] = keymap [ch & 0x7f];
			if (ev [1] >= 256 && ev [1] <= 258)
				switch (ev [1]) {
				case 256:
					if (press && D.k.shift) return 0;
					D.k.shift = press;
					break;
				case 257:
					if (press && D.k.alt) return 0;
					D.k.alt = press;
					break;
				case 258:
					if (press && D.k.ctrl) return 0;
					D.k.ctrl = press;
					break;
				}
			else if (ev [1] == 311 && press && D.winkeyvt) goto sr;
			else if (ev [1] == 310 && press && D.winkeyvt) goto sl;
			else if (!press) return 0;
			else if (D.k.ctrl) {
				if (D.k.alt) {
					if (ev [1] == 308 || ev [1] == 301) {
					  sr:
						D.switchto = 1;
						ev [0] = 4;
					} else if (ev [1] == 300) {
					  sl:
						D.switchto = -1;
						ev [0] = 4;
					} else if (ev [1] == 8) {
						// crtl-alt-backspace. kill graphics
						terminate ();
						ev [0] = 5;
					} else if (ev [1] == 27 && D.screenshot)
						ev [0] = 6;
				} else if (ev [1] == 'c') fprintf (stderr, "Ctrl-c\n");
					// send SIGINT to self?
			}
			return 1;
		}
	}

	if (pfd [1].revents) {
		struct mousedat M;
		int r = read (D.mpipe [0], &M, sizeof M);
		if (r > 0 && M.b != -1) {
			ev [0] = M.s + 2;
			ev [1] = M.b;
			ev [2] = D.mx;
			ev [3] = D.my;
#ifdef MOUSE_KILL
			D.mqueued = 0;
#endif
			return 1;
		}
	}
	return 0;
}

/* Will return when the VT is back */
void switch_vt ()
{
	struct vt_stat vtstate;
	unsigned short thisvt, othervt;

	if (ioctl (D.kbd, VT_GETSTATE, &vtstate) < 0)
		return;
	thisvt = vtstate.v_active;
	othervt = thisvt + D.switchto;
	D.away = 1;
	ioctl (D.kbd, KDSKBMODE, K_XLATE);
	ioctl (D.kbd, KDSETMODE, KD_TEXT);
	if (!ioctl (D.kbd, VT_ACTIVATE, othervt)) {
		ioctl (D.kbd, VT_WAITACTIVE, othervt);
		while (ioctl (D.kbd, VT_WAITACTIVE, thisvt) < 0) {
			if (errno != EAGAIN) break;
			usleep (64000);
		}
	}
	D.away = 0;
	ioctl (D.kbd, KDSETMODE, KD_GRAPHICS);
	ioctl (D.kbd, KDSKBMODE, K_MEDIUMRAW);
	D.k.ctrl = D.k.alt = D.k.shift = 0;
}

void where (int xy[])
{
	xy [0] = D.mx;
	xy [1] = D.my;
}

void move_mouse (int x, int y)
{
	rmv_mouse ();
	D.mx = max (min (x, D.xres - 1), 0);
	D.my = max (min (y, D.yres - 1), 0);
	put_mouse ();
}

int block_until_event ()
{
	if (D.kbd == -1)
		return -1;

	struct pollfd pfd [2];

	pfd [0].fd = D.kbd;
	pfd [0].events = POLLIN | POLLERR | POLLHUP;
	pfd [1].fd = D.mpipe [0];
	pfd [1].events = POLLIN | POLLERR | POLLHUP;
	D.hibernating = 1;
	poll (pfd, 2, -1);
	D.hibernating = 0;
	return 0;
}

int lock (int minx, int miny, unsigned int maxx, unsigned int maxy)
{
	if (!D.in_use)
		return -1;
	sem_wait (&D.sem);
	if (!(D.mx + MOUSE_WIDTH < minx || D.mx > maxx || D.my + MOUSE_HEIGHT < miny || D.my > maxy))
		rmv_mouse ();
	return 0;
}

void unlock ()
{
	if (D.rmouse)
		put_mouse ();
	sem_post (&D.sem);
}

void terminate ()
{
	if (!D.in_use) return;
	D.in_use = 0;
	if (D.kbd != -1) {
		tcsetattr (D.kbd, TCSAFLUSH, &D.ts);
		ioctl (D.kbd, KDSKBMODE, K_XLATE);
		ioctl (D.kbd, KDSETMODE, KD_TEXT);
		close (D.kbd);
	}
	if (D.th && !D.keepmouse)
		pthread_cancel (D.th);
	if (D.mfd != -1)
		close (D.mfd);
	if (D.mpipe [0] != -1) {
		close (D.mpipe [0]);
		close (D.mpipe [1]);
	}
	if (D.mmio != MAP_FAILED)
		munmap (D.mmio, D.mmio_len);
	if (D.restore_accel)
		ioctl (D.fd, FBIOPUT_VSCREENINFO, &D.orig_vscr);
	if (D.mm != MAP_FAILED)
		munmap (D.mm, D.xres * D.yres * D.bpp);
	if (D.fd != -1)
		close (D.fd);
	clear_state ();
}

void (*termfunc) () = terminate;

/* acceleration */

void accel_fill_rect (int x, int y, int w, int h, uint c)
{
	if (x < 0) {
		w += x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		y = 0;
	}
	if (x + w > D.xres)
		w = D.xres - x;
	if (y + h > D.yres)
		h = D.yres - y;

	if (w <= 0 || h <= 0 || w > D.xres || h > D.yres)
		return;

	D.accel->fillrect (x, y, w, h, c);
}

void accel_put_image (void *data, int x, int y, int sx, int sy, int w, int h, int sw, int sh)
{
	int DW = D.xres, DH = D.yres;

	if (sx < 0) {
		x -= sx;
		sx = 0;
	}

	if (sy < 0) {
		y -= sy;
		sy = 0;
	}

	if (w <= 0 || h <= 0) return;

	if (x < 0) {
		sx -= x;
		sw += x;
		x = 0;
	}
	if (y < 0) {
		sy -= y;
		sh += y;
		y = 0;
	}

	if (x > DW || y > DH)
		return;

	if (x + sw > DW)
		sw = DW - x;
	if (y + sh > DH)
		sh = DH - y;

	if (sx > w || sy > h)
		return;
	if (sx + sw > w)
		sw = w - sx;
	if (sy + sh > h)
		sh = h - sy;
	if (sh <= 0 || sw <= 0)
		return;

	if (D.accel->imageblit)
		D.accel->imageblit (data, x, y, sx, sy, w, h, sw, sh);
}

/*++++++++++++*/
/* export abi */
/*++++++++++++*/

const char ABI [] =
"i getvt		-		\n"
"i init			p32iiiii	\n"
"i get_event		p32		\n"
"- switch_vt!		-		\n"
"i block_until_event!	-		\n"
"i lock			iiii		\n"
"- unlock		-		\n"
"- where		p32		\n"
"- move_mouse		ii		\n"
"- terminate		-		\n"
"- accel_fill_rect	iiiii		\n"
"- accel_put_image	siiiiiiii	\n"
"i termfunc				\n"
;

// the lock() function is not declared blocking because basically it
// can only wait for a very little while until we draw the mouse
// pointer. Almost instant. In the case of vt switch the application
// is not supposed to perform any drawing since it will be blocked in
// the switch_vt() function and drawing activity by other threads must
// have been disabled.
//
// The case where the framebuffer is used in fullscreen as a pyvm
// framebuffer where drawing happens by C code, has not happened yet.
// (games, demos, etc).

/*
 Convert scancodes (what the /dev/tty character device returns)
 to ascii and special values.
*/

static const struct {
	int k, v;
} konvert [/*inspired from KDE*/] = {
		{K_SHIFT, 256},
		{K_SHIFTL, 256},
		{K_SHIFTR, 256},
		{K_ALT, 257},
		{K_ALTGR, 257},
		{K_CTRL, 258},
		{K_CTRLR, 258},
		{K_CTRLL, 258},
		{K_DOWN, 259},
		{K_LEFT, 300},
		{K_RIGHT, 301},
		{K_UP, 302},
		{K_INSERT, 303},
		{K_FIND, 304}, //home
		{K_SELECT, 305}, //end
		{K_PGUP, 306},
		{K_PGDN, 307},
		{K_REMOVE, 127},
		{K_ENTER, 10},
		{K_F1, 308},
		{K_CAPS, 309},
		// winkeys. what's their K_ ??
		{528, 310},
		{529, 311},
};

static void init_kmap ()
{
	struct kbentry en;
	int i, j;

	en.kb_table = K_NORMTAB;
	for (i = 0; i < 128; i++) {
		en.kb_index = i;
		en.kb_value = 0;
		ioctl (D.kbd, KDGKBENT, &en);
		int v = en.kb_value;
		int t = KTYP (v);
		int r;
		if (i == 14)
			r = 8;	// backspace. totally undocumented ?!
		else if (t == KT_LATIN || t == KT_ASCII || t == KT_LETTER)
			r = KVAL (v);
		else {
			for (j = 0; j < sizeof konvert / sizeof *konvert; j++)
				if (konvert [j].k == v) {
					r = konvert [j].v;
					goto found;
				}
			r = -i; /* unknown/skip */
		}
	found:
		keymap [i] = r;
	}
}

//////////////////////// C testing /////////////////////////////

/*
 toDodo:
*/
#ifdef MAIN
#define NN 10
int main ()
{
#if 0
	int rez[4], i, k [4], b[1000], j;
	if (init (rez)) {
		fprintf (stderr, "FAILUER\n");
		return 1;
	}
	memset (D.mm, 23, D.xres * D.bpp * 440);
	memset (D.mm, 123, D.xres * D.bpp * 140);
	for (j = 0; j < NN;) {
		if (get_event (k)) {
			printf ("--Event type [%i], val [%i]\n", k [0], k [1]);
			++j;
		}
	}
	terminate ();
#endif
#if 0
	if (PROGRAM_READY /*&& !(TOO_OFTEN || TOO_EARLY)*/)
		release ();
#endif
}
#endif
