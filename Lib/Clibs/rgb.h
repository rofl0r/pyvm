typedef unsigned char uchar;
typedef unsigned short int ushort;
typedef unsigned int uint;
typedef struct { uchar r, g, b; } u24;
typedef unsigned int RRGGBB;

/* All routines that take a `color` argument, get it in normalized
  0xRRGGBB form.  These functions convert to custom packs that can
  be written to the framebuffer memory.  */

#define RED ((col >> 16) & 255)
#define GREEN ((col >> 8) & 255)
#define BLUE (col & 255)

static inline uchar pack8 (RRGGBB col)
{
	return col;
//	return (RED & 0xe0) | (GREEN & 0xe0) >> 3 | BLUE >> 6;
}

static inline ushort pack16 (RRGGBB col)
{
	return (RED & 0xf8) << 8 | (GREEN & 0xfc) << 3 | BLUE >> 3;
}

static inline u24 pack24 (RRGGBB col)
{
	u24 c = { RED, GREEN, BLUE };
	return c;
}

static inline uint pack32 (RRGGBB col)
{
	return col;
	return (RED << 16 | GREEN << 8 | BLUE);
}

#define pack_uchar	pack8
#define pack_ushort	pack16
#define pack_u24	pack24
#define pack_uint	pack32

/* pixel comparisons, for macroization */

static inline int cmp_uchar (uchar a, uchar b)
{
	return a == b;
}

static inline int cmp_ushort (ushort a, ushort b)
{
	return a == b;
}

static inline int cmp_u24 (u24 a, u24 b)
{
	return a.r == b.r && a.g == b.g && a.b == b.b;
}

static inline int cmp_uint (uint a, uint b)
{
	return a == b;
}

// for the text renderer

static inline void get_rgb_u24 (const u24 *dest, int *r, int *g, int *b)
{
	*r = dest->r;
	*g = dest->g;
	*b = dest->b;
}

static inline void put_rgb_u24 (u24 *dest, int r, int g, int b)
{
	dest->r = r;
	dest->g = g;
	dest->b = b;
}

static inline void get_rgb_ushort (const ushort *dest, int *r, int *g, int *b)
{
	ushort p = *dest;
	*r = (p >> 8) & 0xf8;
	*g = (p >> 3) & 0xfc;
	*b = (p << 3) & 0xf8;
}

static inline void put_rgb_ushort (ushort *dest, int r, int g, int b)
{
	*dest = (r & 0xf8) << 8 | (g & 0xfc) << 3 | b >> 3;
}

static inline void get_rgb_uint (const uint *dest, int *r, int *g, int *b)
{
	uchar *p = (uchar*) dest;
	*r = p [2];
	*g = p [1];
	*b = p [0];
}

static inline void put_rgb_uint (uint *dest, int r, int g, int b)
{
	uchar *p = (uchar*) dest;
	p [2] = r;
	p [1] = g;
	p [0] = b;
}
