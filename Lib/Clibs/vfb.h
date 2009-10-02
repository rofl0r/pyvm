#include "rgb.h"

typedef struct
{
	int w, h;
	int depth;	/* bytes per pixel (1, 2, 3, 4) */
	union {
		void *fb;
		uchar *fb8;
		ushort *fb16;
		u24 *fb24;
		uint *fb32;
	};
} fb;

#define RGB332

#define ncase break; case
#define ndefault break; default
#define SCRX f->w
#define SCRY f->h

/*
 * memcopy
 */
#if 1
#define memcopy __builtin_memcpy
#else
static inline void memcopy (void *dstv, const void *srcv, int n)
{
	if ((int)dstv%4 == 0 && (int)srcv%4 == 0 && n%4 == 0) {
		int *dst = dstv;
		const int *src = srcv;
		n >>= 2;
		while (n--) *dst++ = *src++;
	} else {
__builtin_memcpy (dstv, srcv, n);
		// xxx: optimize with aligned word copy
//		char *dst = dstv;
//		const char *src = srcv;
//		while (n--) *dst++ = *src++;
	}
}
#endif
