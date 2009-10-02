/*
 *  Base initializer class
 * 
 *  Copyright (c) 2006 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

#ifndef INITS
#define INITS

// general initializers (lwc rocks)

class InitObj
{
	const inline virtual;
	InitObj *next;
auto	modular void REGISTER () __attribute__ ((constructor));
virtual	void todo ();
virtual	int priority = -1;
};

extern InitObj *first;

void InitObj.REGISTER ()
{
static	_CLASS_ x;
	if (priority != -1) {
		x.next = first;
		first = &x;
	}
}

extern void initialize ();

enum {
	/* the order is important */

	INIT_MEMORY,		/* the memory allocator */
	INIT_STRUCTS,		/* some global __object__ like NILTuple, etc */
	INIT_PRINT,		/* The I/O subsystem. Can use the 'print' function */
	INIT_VM,		/* boot_pyvm (0) */
	INIT_INTERNS0,		/* our internal, intern strings */
	INIT_INTERNS1,		/* our internal, intern strings */
	INIT_NUMBERS,		/* can call newIntObj */
	INIT_ATTR,		/* attribute tables for builtin types */
	INIT_BYTEASM,		/* prepare internal bytecode assembly */
	INIT_LAST,		/* all the rest for level1 */
	INIT_GLOBALDICT,	/* make some globals() to create functions, import __builtins__ */
	INIT_FUNCS,		/* Internal PyFuncObjects */
};

/* initializer prototype 
static class InitMem : InitObj {
	int priority = INIT_MEMORY;
	void todo ()
	{
		init_verifier (0);
		printf ("Initializing MEMORY\n");
	}
};
*/
/* We can easilly do an even more advanced initialization mechanism
   where each InitObj has its 'provides' and 'depends' values and
   the initialize() not only calls them in order but also detects
   deadlocks and un-called initializers.  But enough perfectionism
   is enough.   */

#endif
