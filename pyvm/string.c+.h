/*
 *  Implementation of string object
 * 
 *  Copyright (c) 2006 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

// bug hunting
//#define CATCH_STRING
#ifdef CATCH_STRING
void teststr (char *s, int l)
{
	if (l == 19 && s [1] == 'g') {
		pprint ("WAHOOOOOOOOOOO!\n");
		whereami ();
		CRASH;
	}
}
#endif

/* >>>>>>>>>>>> iterator <<<<<<<<<<<< */

static final class StringIterObj : iteratorBase, seg_allocd
{
	int i, len;
	const char *str;
   public:
	StringIterObj (StringObj*);
	int len ()	{ return len; }
	__object__ *xnext ();
};

StringIterObj.StringIterObj (StringObj *s)
{
	/* Immutability of strings allows us to just grab (str,len)
	 * from the object.  Since moreover we are holding a reference
	 * to it, we can be sure that '+=' will fork a new string if called.
	 */
	iteratorBase.ctor ((__object__*) s);
	str = s->str;
	i = 0;
	len = s->len;
}

__object__ *StringIterObj.xnext ()
{
	/* The implementation of pyvm sais that the object returned by
	 * xnext *will* be stored to the exec_stack, unless null.
	 * iow, the caller will incref this.
	 */
	if_unlikely (i == len)
		RaiseStopIteration ();
	return char_string ((unsigned char) str [i++]);
}

/* ----------* stringObj *---------- */

static void StringObj.borrow (char *s, int l)
{
	/* special constructor to wrap char* to StringObj
	 * and do some stuff.  Destructor must never be called
	 * this is an incomplete object!
	 */
	len = l;
	str = s;
	phash = 0;
}

StringObj.StringObj (const char *s, int l)
{
#ifdef	CATCH_STRING
teststr (s, l);
#endif
	__object__.ctor ();
	len = l++;
	memcpy (str = (char*) seg_alloc (l), s, l);
	phash = 0;
}

StringObj.StringObj (const char *s, int l, long hh)
{
	__object__.ctor ();
	len = l++;
	memcpy (str = (char*) seg_alloc (l), s, l);
	phash = hh;
}

void StringObj.binctor (const char *s, int l)
{
#ifdef	CATCH_STRING
teststr (s, l);
#endif
	__object__.ctor ();
	memcpy (str = (char*) seg_alloc (1 + (len = l)), s, l);
	str [l] = 0;
	phash = 0;
}

StringObj.StringObj (const char *s1, int l1, const char *s2, int l2)
{
	__object__.ctor ();
	int l = 1 + (len = l1 + l2);
	memcpy (str = (char*) seg_alloc (l), s1, l1);
	memcpy (str + l1, s2, l2 + 1);
	phash = 0;
}

StringObj.StringObj (const char *s)
{
	ctor (s, strlen (s));
}

void StringObj.intern (const char *s, int l, long ph)
{
	binctor (s, l);
	if (ph) phash = ph;
	else mkhash ();
}

void StringObj.allocated (char *s, int l)
{
#ifdef	CATCH_STRING
teststr (s, l);
#endif
	__object__.ctor ();
	str = s;
	len = l;
	phash = 0;
}

static slow void StringObj.permctor (char *s, int l)
{
	__object__.ctor ();
	inf ();
	str = s;
	len = l;
	mkhash ();
}

StringObj *StringObj.strcat (const char *s, int l)
{
	int ln = postfix (len, len += l);
	str = seg_realloc (str, len + 1);
	memcpy (str + ln, s, l + 1);
	phash = 0;
	return this;
}

StringObj *StringObj.strcat (const char *s)
{
	return strcat (s, strlen (s));
}

StringObj *StringObj.strcat (StringObj *s)
{
	return strcat (s->str, s->len);
}

bool StringObj.cmp_EQ_same (__object__ *o)
{
	StringObj *rr = (StringObj*) o;
	return len == rr->len && str [0] == rr->str [0] && !memcmp (str, rr->str, len);
}

int StringObj.cmp_GEN_same (__object__ *o)
{
	StringObj *rr = (StringObj*) o;
	return memcmp (str, rr->str, len) ?: len-rr->len;
}

__object__ *StringObj.concat (__object__ *p)
{
	StringObj *t = StringObj.fcheckedcast (p);
	if (refcnt <= 2)
		/* if the reference count is 2 and we are in the pyvm mode
		 * this means that one reference is the variable that points
		 * to the string and the other the stack slot.  Therefore,
		 * we don't need to fork a new stringobj because there is
		 * no other referrer that needs the original string.
		 * However, this means that in non-pyvm mode we must be
		 * extremely careful using concat on strings.
		 */
		return strcat (t);
	return new StringObj (str, len, t->str, t->len);
};

bool StringObj.contains (__object__ *o)
{
	/*** doesn't work for binary data strings
	 ***/
	StringObj *c = StringObj.fcheckedcast (o);
	return (bool) (c->len == 1 ? (long) memchr (str, c->str [0], len) :
		 (long) memstr (str, c->str, len, c->len));
}

__object__ *StringObj.__xgetitem__ (int i)
{
	return char_string ((unsigned char) str [i]);
}

__object__ *StringObj.__xgetslice__ (int start, int length)
{
	return new StringObj binctor (str + start, length);
}

inline long StringObj.hash ()
{
	return phash ?: mkhash ();
}

long StringObj.mkhash ()
{
	// the standard python string hash
	register const char *p = str;
	register int len = len;
	register long x;

	x = *p << 7;
#if 0
	/* inlines too much? */
	while (len >= 4) {
		x = (1000003 * x) ^ p [0];
		x = (1000003 * x) ^ p [1];
		x = (1000003 * x) ^ p [2];
		x = (1000003 * x) ^ p [3];
		p += 4;
		len -= 4;
	}
#endif
	while (--len >= 0)
		x = (1000003 * x) ^ *p++;
	return phash = x ^ this->len;
}

__object__ *StringObj.binary_add (__object__ *s)
{
	StringObj *S = StringObj.fcheckedcast (s);
	return new StringObj (str, len, S->str, S->len);
}

__object__ *StringObj.binary_modulo (__object__ *s)
{
	return interpolate (this, s);
}

void StringObj.xprint ()
{
	/* Must escape */
	print_out (_CHAR ('\''));
	(*this).print ();
	print_out (_CHAR ('\''));
}

__object__ *StringObj.iter ()
{
	return new StringIterObj (this);
}

StringObj.~StringObj ()
{
	seg_free (str, len + 1);
}

/* ------------* one-character strings *---------------- */

static StringObj charstrings [256];
StringObj *NILstring;

static slowcold class InitStrings : InitObj {
	int priority = INIT_INTERNS0;
	void todo ()
	{
	static	char charstringdata [1024];
		int i;
		for (i = 0; i < 256; i++) {
			charstringdata [4 * i] = i;
			charstringdata [4 * i + 1] = 0;
			charstrings [i].permctor (&charstringdata [4 * i], 1);
		}
		NILstring = new StringObj permctor ("", 0);
	}
};

__object__ *char_string (unsigned int i)
{
	return &charstrings [i];
}

/* %%%%%%%%%%%%%%%%%%%%% interpolator %%%%%%%%%%%%%%%%%%%%% */

#include "cStringIO.h"

modsection static StringObj *interpolate (StringObj S, __object__ *arg)
{
static	const char fmt_chars [256] = {
		['#'] = 1, ['-'] = 1, ['+'] = 1, [' '] = 1, ['0'] = 1,
		['d'] = 2, ['i'] = 2, ['o'] = 2, ['u'] = 2, ['x'] = 2, ['X'] = 2,
		['f'] = 3, ['F'] = 3, ['e'] = 3, ['E'] = 3, ['g'] = 3, ['G'] = 3,
		['c'] = 4, ['s'] = 5, ['r'] = 6,
		//packs
		['m'] = 7,
		['M'] = 8,
#if PYVM_ENDIAN == PYVM_ENDIAN_LITTLE
		['a'] = 7
#else
		['a'] = 8
#endif
	};

	cStringIO E;
	char *s = S.str, *ep, tmp [70], sv, sv2;
	int l = S.len, i, j, n, cs;
	int tp = Tuplen.isinstance (arg) ? 1 : DictObj.isinstance (arg) ? 2 : 0;
	int minlen;
	int ival;
	double fval;
	__object__ *obj;
	REFPTR rpr;

#define ISDIGITT(x) (x <= '9' && x >= '0')

	for (n = j = i = 0; i < l;)
		if (s [i] != '%') i++;
		else {
			E.strcat (s + j, i++ - j);
			if (s [i] == '%') {
				j = i++;
				continue;
			}
			/* key -- get the object to print */
			if (s [i] == '(') {
				//if (tp != 2) RaiseTypeError ("dict", "something else");
				int st = ++i;
				while (i < l && s [i] != ')') i++;
				/*** localloc does not invoke destuctors. Good! ***/
				StringObj *KEY = localloc StringObj borrow (s + st, i++ - st);

				//* PROBLEM WITH RAISE CONTEXT
				if_unlikely (!(obj = arg->xgetitem (KEY)))
					RaiseIndexError ();
			} else if (tp == 1) {
				if (n >= TupleObj.cast (arg)->len)
					RaiseNotImplemented ("This is an IndexError in reallity");
				obj = TupleObj.cast (arg)->__xgetitem__ (n++);
			} else if (n == 0) obj = arg;
			else RaiseNotImplemented ("interpolation: Certainly not");
			cs = i - 1;
			minlen = -1;
			if (fmt_chars [(int) s [i]] <= 1) {
				/* conversion */
				while (fmt_chars [(int) s [i]] == 1) i++;
				/* width */
				if (s [i] == '*') RaiseNotImplemented ("* minimum length");
				else if (ISDIGITT (s [i])) {
					minlen = strtol (s + i, &ep, 10);
					i = ep - s;
				}
				/* precission */
				if (s [i] == '.') {
					if (s [++i] == '*') RaiseNotImplemented ("* precission");
					else while (ISDIGITT (s [i])) i++;
				}
			}
			/* do it */
			sv = s [i + 1];
			switch (fmt_chars [(int) s [i]]) {
			case 2:
				ival = IntObj.isinstance (obj) ? IntObj.cast (obj)->i :
					(int) FloatObj.fcheckedcast (obj)->f;
				s [i + 1] = 0;
				sv2 = s [cs];
				s [cs] = '%';
				E.strcat (tmp, sprintf (tmp, s+ cs, ival));
				s [cs] = sv2;
			ncase 3:
				fval = IntObj.isinstance (obj) ? IntObj.cast (obj)->i :
					FloatObj.fcheckedcast (obj)->f;
				s [i + 1] = 0;
				sv2 = s [cs];
				s [cs] = '%';
				E.strcat (tmp, sprintf (tmp, s+cs, fval));
				s [cs] = sv2;
			ncase 4:
				s [i + 1] = 0;
				sv2 = s [cs];
				s [cs] = '%';
				if (IntObj.typecheck (obj))
					E.strcat (tmp, sprintf (tmp, s+ cs, IntObj.cast (obj)->i));
				else {
					E.strcat (tmp, sprintf (tmp, s+ cs,
						 StringObj.fcheckedcast (obj)->str [0]));
				}
				s [cs] = sv2;
			ncase 5: {
				StringObj *SS;
				if (StringObj.isinstance (obj)) {
					SS = StringObj.cast (obj);
				} else {
					SS = StringObj.cast (
						StringObj.type_call ((REFPTR*) &obj, 1));
					rpr = SS;
				}
				if_unlikely (ISDIGITT (s [cs + 1])) {
					int nn = atoi (s + cs + 1);
					if (SS->len < nn) {
						char x [nn - SS->len];
						memset (x, ' ', nn - SS->len);
						E.strcat (x, nn - SS->len);
					}
				}
				E.strcat (SS->str, SS->len);
				if_unlikely (s [cs + 1] == '-' && ISDIGITT (s [cs + 2])) {
					int nn = atoi (s + cs + 2);
					if (SS->len < nn) {
						char x [nn - SS->len];
						memset (x, ' ', nn - SS->len);
						E.strcat (x, nn - SS->len);
					}
				}
			}
			ncase 6: {
				rpr = obj->repr ();
				E.strcat (rpr.as_string->str, rpr.as_string->len);
			}
			ncase 7: {
				char op = s [++i];
				sv = s [i + 1];
				if (op == 'h' || op == 'H') {
					*(short*)tmp = htosle (IntObj.fcheckedcast (obj)->i);
					E.strcat (tmp, 2);
				} else if (op == 'i' || op == 'I') {
					*(int*)tmp = htolle (IntObj.fcheckedcast (obj)->i);
					E.strcat (tmp, 4);
				} else goto error;
			}
			ncase 8: {
				char op = s [++i];
				sv = s [i + 1];
				if (op == 'h' || op == 'H') {
					*(short*)tmp = htosbe (IntObj.fcheckedcast (obj)->i);
					E.strcat (tmp, 2);
				} else if (op == 'i' || op == 'I') {
					*(int*)tmp = htolbe (IntObj.fcheckedcast (obj)->i);
					E.strcat (tmp, 4);
				} else goto error;
			}
			ndefault: error: RaiseNotImplemented ("%specifier");
			}
			s [j = ++i] = sv;
		}

	if (j < i)
		E.strcat (s + j, i - j);
	return E.getvalue ();
}

/* ----* string.join() *---- */

StringObj *StringObj.join (__object__ *o)
{
	if_unlikely (!(o->vf_flags & VF_ISEQ))
		RaiseTypeError ("string.join(seq)");
	if (TupleObj.typecheck (o)) {
		/** XXX can be optimized if we have a list of strings indeed **/
		ListObj *L1 = ListObj.cast (o);
		if_unlikely (!L1->len)
			return NILstring;
		ListObj *L2 = L1;
		REFPTR xx;
		StringObj *S;
		int tot, i, j, k;
		for (i = tot = 0, j = L1->len; i < j; i++) {
			__object__ *nx = L1->__xgetitem__ (i);
			if_unlikely (!StringObj.isinstance (nx)) {
				// list contains non-string objects. Recreate new list
				// where all objects are str()d.  Maybe we should just
				// raise an error instead? -- this is rare
				L2 = new ListObj __sizector (L1->len);
				xx = L2;
				for (k = 0; k < i; k++)
					L2->append (L1->__xgetitem__ (k));
				for (/*--i*/; i < j; i++) {
					nx = L1->__xgetitem__ (i);
					S = StringObj.isinstance (nx) ? StringObj.cast (nx) : nx->str ();
					L2->append (S);
					tot += S->len;
				}
				break;
			}
			tot += StringObj.cast (nx)->len;
		}
		int len = len;
		tot += len * (i - 1);
		char * __restrict s = (char*) seg_alloc (tot + 1);
		if (!len) for (i = k = 0, --j; i < j; i++) {
			S = StringObj.cast (L2->__xgetitem__ (i));
			memcpy (s + k, S->str, S->len);
			k += S->len;
		} else for (i = k = 0, --j; i < j; i++) {
			S = StringObj.cast (L2->__xgetitem__ (i));
			memcpy (s + k, S->str, S->len);
			k += S->len;
			memcpy (s + k, str, len);
			k += len;
		}
		S = StringObj.cast (L2->__xgetitem__ (i));
		memcpy (s + k, S->str, S->len);
		s [k + S->len] = 0;
		return new StringObj allocated (s, tot);
	}
	if (o->vf_flags & VF_ITER) {
		/* XXX: using to_list here undoes all the good things about generator
		 * expressions. We should iterate normally and add to an expanding string
		 */
		REFPTR L = iteratorBase.cast (o)->to_list ();
		return join (L.o);
	}
	RaiseNotImplemented (".join(this)");
}

ListObj *StringObj.to_list ()
{
	int len = len, i;
	char *str = str;
	ListObj *L = new ListObj __sizector (len);
	for (i = 0; i < len; i++)
		L->__inititem (i, char_string ((unsigned char) str [i]));
	L->len = len;
	return L;
}
