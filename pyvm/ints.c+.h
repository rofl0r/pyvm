/*
 *  Integer number object
 * 
 *  Copyright (c) 2006 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

NEW_ALLOCATOR (IntObj)

///////////////////////////////////////////////////

class IntObj SmallInts [INTTAB_MIN + INTTAB_MAX];

IntObj *IntObj_0, *IntObj_1;

static slowcold class InitNums : InitObj {
	int priority = INIT_NUMBERS;
	void todo ()
	{
		for (int i = 0; i < INTTAB_MAX + INTTAB_MIN; i++) {
			SmallInts [i].i = i - INTTAB_MIN;
			SmallInts [i].inf ();
		}
		IntObj_0 = newIntObj (0);
		IntObj_1 = newIntObj (1);
	}
};

IntObj.IntObj (long x)
{
	i = x;
	__object__.ctor ();
}

void IntObj.__release ()
{
	delete *this;
}

void IntObj.print ()
{
	print_out (i);
}

IntObj *newIntObj (long x)
{
	if (x > -INTTAB_MIN && x < INTTAB_MAX)
		return &SmallInts [x + INTTAB_MIN];

	return new IntObj (x);
}

int IntObj.cmp_GEN (__object__ *o)
{
	if (o->vf_flags & VF_NUMERIC) {
		if_unlikely (FloatObj.isinstance (o)) {
			double ff = i - FloatObj.cast (o)->f;
			return ff < 0 ? -1 : ff > 0 ? 1 : 0;
		}
		return i - IntObj.cast (o)->i;
	}
	if (LongObj.isinstance (o))
		return -LongObj.cast (o)->cmp_GEN (this);
	return stype - o->stype;
}

__object__ *IntObj.binary_mul (__object__ *o)
{
	if (TupleObj.typecheck (o)) {
		int len = TupleObj.cast (o)->len, i = i, j;
		TupleObj *L;
		if_unlikely (i < 0)
			RaiseNotImplemented ("array * negative");
		if (ListObj.isinstance (o)) 
			L = new ListObj __sizector (len * i);
		else {
			if (!len || !i)
				return NILTuple;
			L = new Tuplen __sizector (len * i);
		}
		L->len = len * i;
		REFPTR *xdata = TupleObj.cast (o)->data, *vdata = L->data;

		if (len == 1) {
			__object__ * const v = xdata [0].o;
			for (j = 0; j < i; j++)
				vdata [j].o = v;
		} else {
			int chs = len * sizeof (REFPTR);
			for (j = 0; j < i; j++)
				memcpy (vdata + j*len, xdata, chs);
		}
		for (j = 0; j < len; j++)
			xdata [j].o->refcnt += i;
		return L;
	}
	if (StringObj.isinstance (o))
		return StringObj.cast (o)->binary_mul (this);
	return o->binary_mul (this);
}

__object__ *TupleObj.binary_mul (__object__ *o)
{
	return IntObj.fcheckedcast (o)->binary_mul (this);
}

slow bool IntObj.permanent ()
{
	return i > -INTTAB_MIN && i < INTTAB_MAX;
}

/****
 **** pow(). Just use libm pow() on doubles and then see it the result has
 **** no decimal digits and it fits inside a long.
 **** XXX - does not always work
 ****/

extern double pow (double, double), fmod (double, double);

__object__ *Num_Power (__object__ *base, __object__ *exponent)
{
	double rez = pow (base->todouble (), exponent->todouble ());
	if (!FloatObj.isinstance (base) && fmod (rez, 1.0) == 0.0
	 && rez <= (double) $LONG_MAX && rez >= (double) $LONG_MIN)
			return newIntObj ((long) rez);
	return new FloatObj (rez);
}

__object__ *ORD (unsigned char x)
{
	return &SmallInts [x + INTTAB_MIN];
}

__object__ *ORDS (char x)
{
	return &SmallInts [x + INTTAB_MIN];
}
