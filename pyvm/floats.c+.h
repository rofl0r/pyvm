/*
 *  Floating point number objects
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

//NEW_ALLOCATOR (FloatObj)

////////////////////////////////////////

FloatObj.FloatObj (double x)
{
	f = x;
	__object__.ctor ();
}

void FloatObj.__release ()
{
	delete *this;
}

int FloatObj.cmp_GEN_same (__object__ *o)
{
	FloatObj *s = (FloatObj*) o;
	return s->f < f ? 1 : -1;
}

int FloatObj.cmp_GEN (__object__ *o)
{
	if (o->vf_flags & VF_NUMERIC) {
		double ff = f - ((FloatObj.isinstance (o) ? FloatObj.cast (o)->f
			: (double) IntObj.cast (o)->i)) ;
		return ff < 0 ? -1 : ff > 0 ? 1 : 0;
	}
	return stype - o->stype;
}

__object__ *FloatObj.binary_modulo (__object__ *o)
{
	return new FloatObj (fmod (f, o->todouble ()));
}
