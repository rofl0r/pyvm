/* $Cambridge: exim/exim-src/src/pcre/CONFIG.h,v 1.1 2004/10/07 13:04:13 ph10 Exp $ */

/*************************************************
*           CONFIG.h for PCRE for Exim           *
*************************************************/

/* The PCRE sources include CONFIG.h, which for a free-standing PCRE build gets
set up by autoconf. For the embedded version in Exim, this file, which is 
manually maintained, is used.

The only configuration thing that matters for the PCRE library itself is
whether the memmove() function exists or not. It should be present in all
Standard C libraries, but is missing in SunOS4. PCRE expects autoconf to set
HAVE_MEMMOVE to 1 in CONFIG.h when memmove() is present. If that is not set, it
defines memmove() as a macro for bcopy().

Exim works differently. It handles this case by defining memmove() as a macro 
in its os.h-SunOS4 file. We interface this to PCRE by including the os.h file 
here, and then defining HAVE_MEMOVE so that PCRE's code in internal.h leaves 
things alone. */

//#include "../os.h"
#define HAVE_MEMMOVE 1

/* We also set up directly a number of parameters that, in the freestanding 
PCRE, can be adjusted by "configure". */

#define NEWLINE                 '\n'
#define LINK_SIZE               2
#define MATCH_LIMIT             10000000
#define POSIX_MALLOC_THRESHOLD  10

/* There is some stuff in the PCRE sources for compilation on non-Unix systems
and non-ASCII systems. For Exim's purposes, just flatten it all. */

#define EBCDIC 0
#define EXPORT

/* End */
