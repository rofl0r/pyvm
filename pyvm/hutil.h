/*
 *  Internal header file
 * 
 *  Copyright (c) 2006 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

/*--------- uncomment for profile build ----------*/

//#define PPROFILE

/*------------------------------------------------*/

typedef unsigned long long tick_t;

#include "include.h"
#define dstdout stdout

#include "VMPARAMS.h"

#include "threading.h"
#include "IO.h"

#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)

_lwc_config_ {
#ifdef	CPPUNWIND
	gcc34cleanup;
#else
	no_gcc34cleanup;
#endif


//	lwcdebug PARSE_ERRORS_SEGFAULT;
//	lwcdebug EXPR_ERRORS_FATAL;

//	lwcdebug DCL_TRACE;
//	lwcdebug PEXPR;
//	lwcdebug FUNCPROGRESS;
//	lwcdebug PROGRESS;
//	onebigfile;
//	x86stdcalls;
}

#include "mallok.h"

#if GCC_VERSION >= 40300
#define coldfunc __attribute__ ((cold))
#else
#define coldfunc
#endif

#define STRL(X) (X, sizeof X - 1)
#define ncase break; case
#define ndefault break; default
#define noreturn __attribute__ ((noreturn, __noinline__))
#define noinline __attribute__ ((noinline))
#define autovrt auto virtual
#define inlines static inline
#define automod	auto modular
#define constant(X) __builtin_constant_p(X)
#define APAIR(X) X, sizeof X
typedef unsigned int uint;
//#define memset __builtin_memset

/* DOES NOT WORK. lwc as a preprocessor doesn't know about __OPTIMIZE__ */
#if 0 //0defined __OPTIMIZE__
#define INLINE __attribute__((always_inline))
#else
#define INLINE
#endif

#define unlikely(...) (__builtin_expect (__VA_ARGS__, 0))
#define likely(...) __builtin_expect (__VA_ARGS__, 1)
#define if_unlikely(...) if (__builtin_expect (__VA_ARGS__, 0))
#define if_likely(...) if (__builtin_expect (__VA_ARGS__, 1)) 
#define slow __section__ (".text.slowzone")
#define slowcold __section__ ("neverused")
#define once \
	static bool _once;\
	if_unlikely (!_once && (_once = true))
#define modsection __section__ (".text.modules")
#define _module modsection static
#define trv __section__ (".text.gctraverse")
#define cln __section__ (".text.gcclean")
#define CRASH *(int*)0=0;
#define COLS "\033[01;37m"
#define COLB "\033[01;34m"
#define COLR "\033[01;31m"
#define COLX "\033[01;35m"
#define COLE "\033[0m"
#define OCC (__object__*)
#define NL "\n"
#define NN "\n"

#if 0
#define SMALL __attribute__ ((optimize_size))
#else
#define SMALL
#endif

static inline int max (int a, int b)	{ return a > b ? a : b; }
static inline int min (int a, int b)	{ return a < b ? a : b; }
static inline bool in2 (int a, int b, int c) { return a == b || a == c; }
static inline bool in3 (int a, int b, int c, int d) { return a == b || a == c || a == d; }
static inline bool in4 (int a, int b, int c, int d, int e) { return a == b || a == c || a == d || a == e; }

static inline int nz (int x)		{ return x ?: 1; }

/* Host to little-endian, little-endian to host */
#if PYVM_ENDIAN == PYVM_ENDIAN_LITTLE
#define lletoh(x) (x)
#define htolle(x) (x)
#define sletoh(x) (x)
#define htosle(x) (x)
#define lbetoh(x) bswap_32 (x)
#define htolbe(x) bswap_32 (x)
#define sbetoh(x) bswap_16 (x)
#define htosbe(x) bswap_16 (x)
#else
#define lletoh(x) bswap_32 (x)
#define htolle(x) bswap_32 (x)
#define sletoh(x) bswap_16 (x)
#define htosle(x) bswap_16 (x)
#define lbetoh(x) (x)
#define htolbe(x) (x)
#define sbetoh(x) (x)
#define htosbe(x) (x)
#endif
/* --------------------------------------------- */

extern int mytoa10 (char*, int);
extern int mytoa16 (char*, unsigned int);

static inline
class sem
{
	sem_t s;
    public:
	sem ()		{ sem_init (&s, 0, 0); }
	void post ()	{ sem_post (&s); }
	void wait ()	{ while (sem_wait (&s) == -1); }
};

__unwind__
static class bytearray
{
	char *str;
	int len, alloc;
    public:
	bytearray ()	{ str = (char*) __malloc (alloc = 256); len = 0; }
	void addstr (char *s, int l)
	{
		while (len + l > alloc)
			str = (char*) __realloc (str, alloc *= 2);
		memcpy (str + len, s, l);
		len += l;
	}
	~bytearray ()	{ __free (str); }
};

#define CC const

#ifdef __i386__
static inline tick_t get_cpu_ticks ()
{
	tick_t val;
	asm volatile ("rdtsc" : "=A" (val));
	return val;
}
#else
#ifdef PPROFILE
#error "No cpu ticks implemented for this arch. Cannot build profile vm"
#endif
#endif
