/*
 *  Implementation of "array" objects
 * 
 *  Copyright (c) 2006 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

static class array_pure
{
typedef	X = 0;
typedef	Y = 0;
virtual	const int identifier, jid;

virtual	auto		void print ();
modular auto		Y obj2type (__object__*) = 0;
virtual	auto		__object__ *_getitem (int) = 0;
virtual	auto		__object__ *_getitem_nocache (int) = 0;
virtual	auto		void _setitem (int, __object__*);
virtual	auto		void setitems (REFPTR[], int);
modular	virtual auto	int itemsize ()	{ return sizeof (X); }
virtual	auto		void tolist (ListObj);
virtual auto		void setvalue (int, int, __object__*);

	void *ptr;
	unsigned int n, alloc;
};

extern DictObj ArrayMethods;

final class arrayObj : __destructible__
{
	const char *const stype = ArrayType;
	const TypeObj &type = &ArrayTypeObj;
	DictObj *type_methods = &ArrayMethods;

	array_pure *P;
static	inline	int absl_index (int i)
	{
		if (i < 0) {
			if_unlikely ((i += P->n) < 0) i = 0;
		} else if (i > P->n) i = P->n;
		return i;
	}
	__object__ *type_call (REFPTR*, int);

    public:

	int len ();
	bool Bool ()		{ return len (); }
	__object__ *xgetitem (__object__*);
	__object__ *xgetslice (int, int);
	void xsetitem (__object__*, __object__*);
	__object__ *iter ();
	void print ();
	~arrayObj ();

	__object__ *tolist ();
};

extern __object__ *array_c_int (const int*, int);
