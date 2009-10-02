/*
 *  Irrelevant functions
 * 
 *  Copyright (c) 2006 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

// ************************************************************
// * fastlocals is just a REFPTR* pointer to an array of REFPTs
// ************************************************************

static void reset_fastlocals (REFPTR *values, int n)
{
	while (n > 0) {
		values [--n].dtor ();
		values [n].o = &UnboundLocal;
	}
}

static void destroy_fastlocals (REFPTR *values, int n)
{
	while (n > 0)
		values [--n].dtor ();
}

static void new_fastlocals (REFPTR *values, int n)
{
	for (int i = 0; i < n; i++)
		values [i].o = &UnboundLocal;
}

static inline void inititem_fastlocal (REFPTR *values, int i, __object__ *o)
{
	values [i].ctor (o);
}

static inline void setitem_fastlocal (int i, __object__ *o)
{
	Fastlocals [i] += o;
}

static inline void setitem_mvref_fastlocal (int i, REFPTR v)
{
	Fastlocals [i].__move (v);
}

static inline __object__ *getitem_fastlocal (REFPTR *values, int i)
{
	return values [i].o;
}

static const char unbound_type [] = "<UnBound Local Object>";

static class UnboundLocalObj : __permanent__
{
	unsigned int sigbit = 524288;
	const char *stype = unbound_type;
	int cmp_GEN (__object__*)	{ RaiseUnbound (); }
	int cmp_EQ (__object__*)	{ RaiseUnbound (); }
	bool Bool ()			{ RaiseUnbound (); }
	long hash ()			{ RaiseUnbound (); }
	__object__ *getattr (__object__*) { RaiseUnbound (); }
	void setattr (__object__*, __object__*) { RaiseUnbound (); }
	__object__ *xgetitem (__object__*) { RaiseUnbound (); }
	void print () { print_out (STRL (COLS"<*unbound*>"COLE)); }	// to undo pure
};

static UnboundLocalObj UnboundLocal; 
const void *UNBOUND = &UnboundLocal;

static int PyCodeObj.lookup_argname (int i, __object__ *n) noinline
{
	__object__ **oo = &varnames.as_tuplen->data [0].o;
	for (; i < argcount; i++)
		if (oo [i] == n)
			return i;
	for (int i = 0; i < argcount; i++)
		//if (oo [i]->cmp_EQ (n))
		if (StringObj.cast (oo [i])->cmp_EQ (n))
			return i;
	return -1;
}

/* * used by sys._getframe() * */

long current_frame_sig ()
{
	return (long) pvm ^ (long) pvm->FUNC.o;
}

__object__ *f_globals (int n)
{
	vm_context *v;
	for (v = pvm; v && n--; v = v->caller)
		;
	return v ? v->globals : &None;
}

__object__ *f_locals (int n)
{
	vm_context *v;
	for (v = pvm; v && n--; v = v->caller);
	if (!v || !PyFuncObj.typecheck (v->FUNC.o))
		return &None;
	return v->FastToLocals ();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static __object__ *next_attr, *gnext_attr, *unyield_attr, *iter_unyield_attr;

static __object__ *bltin_iter_unyield (REFPTR argv[])
{
	iteratorBase.cast (argv [0].o)->unyield ();
	return &None;
}

__object__ *iteratorBase.getattr (__object__ *o)
{
	if (o == Interns.next)
		return new DynMethodObj (this, next_attr);
	if (o == Interns.unyield)
		return new DynMethodObj (this, iter_unyield_attr);
	return __object__.getattr (o);
}

static	slowcold class InitNexts : InitObj {
	int priority = INIT_ATTR;
	void todo ()
	{
		next_attr = extendFunc ("next", 1, bltin_next);
		gnext_attr = extendFunc ("next", 1, bltin_gnext);
		unyield_attr = extendFunc ("unyield", 1, bltin_unyield);
		iter_unyield_attr = extendFunc ("unyield", 1, bltin_iter_unyield);
	}
};
