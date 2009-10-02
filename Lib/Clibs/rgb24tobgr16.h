/*
 * MMX Routines (~15% speed up)
 * Adapted from ffmpeg/libswscale. Original copyright (GPL):
 * -----------------------------------------------------------------
 *  Written by Nick Kurshev.
 * -----------------------------------------------------------------
 *
 * (sxanth: converted from GNU __asm__ to macros)
 */

typedef unsigned long long uint64_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;

static const uint64_t red_16mask  __attribute__((aligned(8))) = 0x0000f8000000f800ULL;
static const uint64_t green_16mask __attribute__((aligned(8)))= 0x000007e0000007e0ULL;
static const uint64_t blue_16mask __attribute__((aligned(8))) = 0x0000001f0000001fULL;

/* Convert 4 pixels: 12 bytes in, 8 bytes out */

static inline void rgb24tobgr16_mmx_quantum (const uint8_t *s, uint16_t *d)
{
	movd_m2r (*s, mm0);
	movd_m2r (*(s+3), mm3);
	punpckldq_m2r (*(s+6), mm0);
	punpckldq_m2r (*(s+9), mm3);
	movq_r2r (mm0, mm1);
	movq_r2r (mm0, mm2);
	movq_r2r (mm3, mm4);
	movq_r2r (mm3, mm5);
	psllq_i2r (8, mm0);
	psllq_i2r (8, mm3);
	pand_r2r (mm7, mm0);
	pand_r2r (mm7, mm3);
	psrlq_i2r (5, mm1);
	psrlq_i2r (5, mm4);
	pand_r2r (mm6, mm1);
	pand_r2r (mm6, mm4);
	psrlq_i2r (19, mm2);
	psrlq_i2r (19, mm5);
	pand_m2r (blue_16mask, mm2);
	pand_m2r (blue_16mask, mm5);
	por_r2r (mm1, mm0);
	por_r2r (mm4, mm3);
	por_r2r (mm2, mm0);
	por_r2r (mm5, mm3);
	psllq_i2r (16, mm3);
	por_r2r (mm3, mm0);
	// movntq or sfence ??
	movq_r2m (mm0, *d);
	// sfence ??
}

static inline void rgb24tobgr16_mmx_init ()
{
	movq_m2r (red_16mask, mm7);
	movq_m2r (green_16mask, mm6);
}
