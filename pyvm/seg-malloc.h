/*
 *  General purpose segmented allocator
 * 
 *  Copyright (c) 2006 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

#define FASTCALL1 __attribute__ ((regparm(1)))
#define FASTCALL2 __attribute__ ((regparm(2)))

#if 1

extern void *seg_alloc (size_t) FASTCALL1 __attribute__ ((malloc)) nothrow;
template seg_malloc(n) {
	(__builtin_constant_p (n) ? ({
		void *r;
		switch (n) {
		case 52:
		case 56: r = seg_alloc56 (); break;
		case 28:
		case 32: r = seg_alloc32 (); break;
		case 92:
		case 96: r = seg_alloc96 (); break;
		default: fprintf (stderr, "No handler for %i\n", n); r = 0;
		}
		r;
	}) : seg_alloc (n))
}

extern void *seg_alloc96 () FASTCALL1 __attribute__ ((malloc)) nothrow;
extern void *seg_alloc56 () FASTCALL1 __attribute__ ((malloc)) nothrow;
extern void *seg_alloc32 () FASTCALL1 __attribute__ ((malloc)) nothrow;
extern void *seg_alloc24 () FASTCALL1 __attribute__ ((malloc)) nothrow;

extern void seg_free (void*) FASTCALL1 nothrow;
extern void seg_free (void*,size_t) FASTCALL2 nothrow;
extern void seg_freeXX (void*) FASTCALL1 nothrow;

extern void *seg_realloc (void*, size_t) nothrow;
extern void *seg_realloc2 (void*, size_t, size_t) nothrow;
extern int seg_n_segments ();

#else	// enable valgrind 

#define seg_alloc malloc
#define seg_alloc56() malloc(56)
#define seg_alloc32() malloc(32)
#define seg_free free
#define seg_freeXX free
#define seg_realloc realloc
#define seg_realloc2(P,O,S) realloc (P,S)

#endif

