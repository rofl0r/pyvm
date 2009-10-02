/*
 *  Path rasterization operations
 *
 *  Copyright (c) 2007-2008 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

typedef struct {
	int x, y;
} point;

typedef unsigned int uint;

typedef struct {
	int y, x, w;
} hline;

/* not a very big deal quality-wise. how much faster without? */
#define ROUNDING

static inline int d2i (double d)
{
#ifdef	ROUNDING
	return __builtin_lround (d);
#else
	return d;
#endif
}

/*
cubic berzier with y-axis continuity: To sequential points are guaranteed
to always differ by one pixel in the y-axis.
The caller allocates an array with an estimated number of points. if more
are required to have the continuity, return -1

The the first point is included, the last isn't.

Todo: not optimized
*/

static inline int xPointOnCubicBezier (double x0, double x1, double x2, double x3, double t)
{
	double ax, bx, cx;
	double tSquared, tCubed;

	cx = 3.0 * (x1 - x0);
	bx = 3.0 * (x2 - x1) - cx;
	ax = x3 - x0 - cx - bx;

	tSquared = t * t;
	tCubed = tSquared * t;

	return d2i ((ax * tCubed) + (bx * tSquared) + (cx * t) + x0);
}

int curve4 (int x0, int y0, int x1, int y1, int x2,
	    int y2, int x3, int y3, int numberOfPoints, int *curve)
{
	int ry;
	double dt, t;
	int j;
	int lasty;
	double dx0 = x0, dy0 = y0, dx1 = x1, dy1 = y1, dx2 = x2, dy2 = y2, dx3 = x3, dy3 = y3;

	if (!numberOfPoints)
		return 0;

	t = dt = 1.0 / (numberOfPoints - 1);
	curve [0] = x0;
	lasty = curve [1] = y0;
	j = 2;
	while (t < 1.0) {
		ry = xPointOnCubicBezier (dy0, dy1, dy2, dy3, t);
		if (ry != lasty) {
			while (abs (ry - lasty) > 1) {
				t -= dt / 10.0;
				ry = xPointOnCubicBezier (dy0, dy1, dy2, dy3, t);
			}
			if (j == 2 * numberOfPoints)
				return -1;
			curve [j++] = xPointOnCubicBezier (dx0, dx1, dx2, dx3, t);
			lasty = curve [j++] = ry;
		}
		t += dt;
	}

	if (curve [j - 1] == y3)
		j -= 2;

	return j;
}

/* conic bezier */

static inline int xPointOnConicBezier (double x0, double x1, double x2, double t)
{
	double t1 = 1 - t;
	double tm = 2 * t * t1;

	t *= t;
	t1 *= t1;

	return d2i (t1 * x0 + tm * x1 + t * x2);
}

int curve3 (int x0, int y0, int x1, int y1, int x2, int y2, int numberOfPoints, int *curve)
{
	int ry;
	double dt, t;
	int j;
	int lasty;
	double dx0 = x0, dy0 = y0, dx1 = x1, dy1 = y1, dx2 = x2, dy2 = y2;

	if (!numberOfPoints)
		return 0;

	t = dt = 1.0 / (numberOfPoints - 1);
	curve [0] = x0;
	lasty = curve [1] = y0;
	j = 2;
	while (t < 1.0) {
		ry = xPointOnConicBezier (dy0, dy1, dy2, t);
		if (ry != lasty) {
			while (abs (ry - lasty) > 1) {
				t -= dt / 10.0;
				ry = xPointOnConicBezier (dy0, dy1, dy2, t);
			}
			if (j == 2 * numberOfPoints)
				return -1;
			curve [j++] = xPointOnConicBezier (dx0, dx1, dx2, t);
			lasty = curve [j++] = ry;
		}
		t += dt;
	}

	if (curve [j - 1] == y2)
		j -= 2;

	return j;
}

/* straight line */

void curve2 (int x0, int y0, int x1, int y1, int *curve)
{
	int i;
	double d = (x1 - x0) / (double) (y1 - y0);
	if (y1 > y0) {
		y1 -= y0;
		for (i = 0; i < y1; i++) {
			*curve++ = x0 + d2i (i * d);
			*curve++ = y0 + i;
		}
	} else {
		y1 = y0 - y1;
		for (i = 0; i < y1; i++) {
			*curve++ = x0 - d2i (i * d);
			*curve++ = y0 - i;
		}
	}
}

/*
 transformation matrix
*/

struct ctm
{
	double a, b, c, d, e, f;
};

void init_ctm (struct ctm *m, double a, double b, double c, double d, double e, double f)
{
	m->a = a;
	m->b = b;
	m->c = c;
	m->d = d;
	m->e = e;
	m->f = f;
}

void apply_ctm (struct ctm *m, int x, int y, int rez[])
{
	rez [0] = d2i (m->a * x + m->c * y + m->e);
	rez [1] = d2i (m->b * x + m->d * y + m->f);
}

void add_points (int dest[], int offset, int src[], int n)
{
	__builtin_memcpy (dest + offset, src, n*4);
}

/*
 * Given an array of hlines and a framebuffer (of depth 1), draw them
 */

#define DRAWLINE(X,Y,W,COL) {\
	uint WW = W;\
	if ((Y) >= fh || (X) >= fw) continue;\
	if ((X) + (W) >= fw)\
		WW = fw - (X);\
	__builtin_memset (fb + (Y) * fw + (X), COL, WW);\
	}

void put_hlines (unsigned char *fb, uint fw, uint fh, hline *line, int nlines, int aa)
{
	uint y, x, w, i;

	for (i = 0; i < nlines; i++) {
		y = line [i].y;
		x = line [i].x;
		w = line [i].w;

		DRAWLINE (x,y,w,0)
	}
}

typedef struct {
	int x, y;
} xy;

void remove_corners (xy points[], int npoints)
{
	int i;
	int lasty;

	lasty = points [0].y;
	for (i = 1; i < npoints - 1; i++) {
		if (lasty == points [i+1].y)
			points [i].y = 0xffff;
		else lasty = points [i].y;
	}
	if (points [1].y == points [npoints - 1].y)
		points [0].y = 0xffff;
	if (points [0].y == points [npoints - 2].y)
		points [npoints - 1].y = 0xffff;
}

void minmax (const xy points[], int npoints, int ret[])
{
	int minx, maxx, miny, maxy, i, x, y;

	minx = miny = 10000;
	maxx = maxy = -1000;
	for (i = 0; i < npoints; i++) {
		x = points [i].x;
		y = points [i].y;
		if (y == 0xffff)
			continue;
		if (x < minx) minx = x;
		if (x > maxx) maxx = x;
		if (y < miny) miny = y;
		if (y > maxy) maxy = y;
	}

	ret [0] = minx;
	ret [1] = maxx;
	ret [2] = miny;
	ret [3] = maxy;
}

void collect_lines (int miny, int ny, const xy points[], int npoints, int aa[], int N)
{
	int i, j, k, x, y;
	int *ap;

	for (i = 0; i < npoints; i++) {
		x = points [i].x;
		y = points [i].y;
		if (y == 0xffff)
			continue;
		y -= miny;
		ap = aa + y*N;
		for (j = 0; j < N; j++)
			if (ap [j] == 0xffff) {
				ap [j] = x;
				break;
			} else if (ap [j] > x) {
				for (k = N-1; k > j; k--)
					ap [k] = ap [k - 1];
				ap [j] = x;
				break;
			}
	}

	for (y = 0; y < ny; y++) {
		ap = aa + y*N;
		for (i = 0; i < N; i += 2)
			if (ap [i] != 0xffff) {
				if (ap [i+1] == 0xffff) {
					ap [i] = 0xffff;
					break;
				}
				ap [i + 1] -= ap [i];
			} else break;
	}
}

void put_hlines2 (unsigned char *fb, int fw, int fh, int aa[], int ny, int miny, int N)
{
	int y, i, yy, x, w;
	int *ap;

	for (y = 0; y < ny; y++) {
		ap = aa + y*N;
		yy = fw * (y + miny);
		for (i = 0; i < N; i += 2)
			if (ap [i] != 0xffff) {
				x = ap [i];
				w = ap [i+1];
				if (x < 0) {
					w += x;
					x = 0;
				}
				if (x + w > fw)
					w = fw - x;
				if (w <= 0 || x >= fw)
					continue;
				__builtin_memset (fb + yy + x, 0, w);
			} else break;
	}
}

const char ABI [] =
"- init_ctm		sdddddd		\n"
"- apply_ctm		siip32		\n"
"- curve2		iiiip32		\n"
"i curve3		iiiiiiip32	\n"
"i curve4		iiiiiiiiip32	\n"
"- add_points		p32ip32i	\n"
"- put_hlines		siip32ii	\n"
"- remove_corners	p32i		\n"
"- minmax		p32ip32		\n"
"- collect_lines	iip32ip32i	\n"
"- put_hlines2		siip32iii	\n"
;
