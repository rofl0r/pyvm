/*
 *  Implementation of the dictionary object
 * 
 *  Copyright (c) 2006 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

/*
 */

#define PERTURB_SHIFT 5

static const char dummy_type [] = "<dummy entry>";

static class DummyObj : __permanent__
{
	const char *stype = dummy_type;
	void print () {}	// to undo pure
	// make it look like a string
	DummyObj ()		{ len = -1; }
	int len;
};

static DummyObj dummy;

//#########################################################################################
//#########################################################################################

/*** We'll do the expanding hash table with exact python's
 *** dict parameters.  Just to avoid the hassle of benchmarks
 *** of "cases where this performs better or that performs
 *** worse". We can provide alternative dicts/trees that
 *** expand/shrink faster but the lookup costs more, as non
 *** default.
 ***/

//#########################################################################################
//#########################################################################################

void dictionary.init (int size)
{
	used = fill = itrn = 0;
	if (!size) {
		memset (tbl = smalltbl, 0, sizeof smalltbl);
		mask = DICT_MINSIZE - 1;
	} else {
		memset (tbl = __malloc (size * sizeof *tbl), 0, size * sizeof *tbl);
		mask = size - 1;
	}
	max = (mask + 1) * 2;
}

bool dictionary.cmp_key_ent (dictEntry *D, __object__ *k)
{
	return k->cmp_EQ (D->key.o);
}

bool dictionaryStr.cmp_key_ent (dictEntry *D, __object__ *k)
{
	return D->key.o == k || D->key.as_string->cmp_EQ_same (k);
}

bool dictionaryInt.cmp_key_ent (dictEntry *D, __object__ *k)
{
	return D->key.o != &dummy;
}

long dictionary.get_hash (__object__ *k)
{
	return k->hash ();
}

long dictionaryStr.get_hash (__object__ *k)
{
	return (*StringObj.cast (k)).hash ();
}

long dictionaryInt.get_hash (__object__ *k)
{
	return (*IntObj.cast (k)).hash ();
}

dictEntry *dictionary._lookup (unsigned long h, __object__ *k)
{
	register unsigned long mask = mask, i = h & mask, p;
	register dictEntry *tbl = tbl, *ep;

	ep = &tbl [i];
	if (!ep->key.o)
		return ep;

	if_likely (ep->hash == h && cmp_key_ent (ep, k))
		return ep;

	dictEntry *freeslot = 0;
	if_unlikely (ep->key.o == &dummy)
		freeslot = ep;

	for (p = h; ; p >>= PERTURB_SHIFT) {
		i = 5 * i + p + 1;
		ep = &tbl [i & mask];
		if (!ep->key.o)
			return freeslot ?: ep;
		if (ep->hash == h && cmp_key_ent (ep, k))
			return ep;
		if_unlikely (ep->key.o == &dummy && !freeslot)
			freeslot = ep;
	}
}

void dictionary.resize () noinline;

void dictionary.resize ()
{
	if_unlikely (itrn)
		RaiseNotImplemented ("RuntimeError: dictionary changed during iteration");

	unsigned int i, ns;
	dictEntry *otbl = tbl, *atbl = tbl, *ntbl;

#if 0
	if (used == 6) ns = 16;
	else
#endif
	for (i = used * (used > 5000 ? 2 : 4), ns = DICT_MINSIZE; ns <= i; ns <<= 1);

	if (ns == DICT_MINSIZE) {
		if ((ntbl = smalltbl) == otbl)
			if (fill == used)
				return;
			else {
				dictEntry *st = (dictEntry*) alloca (sizeof smalltbl);
				memcpy (otbl = st, ntbl, sizeof smalltbl);
			} else;
		memset (smalltbl, 0, sizeof smalltbl);
	} else	ntbl = (dictEntry*) __calloc (ns, sizeof *ntbl);

	mask = ns - 1;
	max = (mask + 1) * 2;
	tbl = ntbl;
	fill = i = used;

	for (; i > 0; ++otbl)
		if (otbl->val.o) {
			unsigned long h = otbl->hash & mask;

			if (ntbl [h].key.o)
				for (unsigned long p = otbl->hash; ; p >>= PERTURB_SHIFT) {
					h = 5 * h + p + 1;
					if (!ntbl [h & mask].key.o)  {
						ntbl [h & mask].copy (otbl);
						break;
					}
				}
			else ntbl [h].copy (otbl);
			--i;
		}

	if (atbl != smalltbl)
		__free (atbl);
}

void dictionary.test_resize ()
{
	if_unlikely ((3 * fill >= max || (4 * used < fill && tbl != smalltbl)) && !itrn)
		resize ();
}

inline bool dictionary.type_good (__object__ *k)
{
	return true;
}

inline void dictionary.mutateby (__object__ *k)
{ }

inline bool dictionaryInt.type_good (__object__ *k)
{
	return IntObj.isinstance (k);
}

inline void dictionaryInt.mutateby (__object__ *k)
{
	dictionary.Mutate (this);
}

bool dictionaryStr.type_good (__object__ *k)
{
	return StringObj.isinstance (k);
}

inline void dictionaryStr.mutateby (__object__ *k)
{
	if (IntObj.isinstance (k) && !used)
		dictionaryInt.Mutate (this);
	else	dictionary.Mutate (this);
}

bool dictionaryStrAlways.type_good (__object__ *k)
{
	return StringObj.isinstance (k);
}

inline void dictionaryStrAlways.mutateby (__object__ *k)
{
	RaiseValueError ("String-only dictionary");
}

static dictEntry *dictionary.empty_entry () noinline;
static dictEntry *dictionary.empty_entry ()
{
	int i = 0;
	while (tbl [i].val.o) i++;
	return &tbl [i];
}

dictEntry *dictionary.lookup (__object__ *k)
{
	/** Query lookup
	**/
	if_likely (type_good (k))
		return _lookup (get_hash (k), k);
	return empty_entry ();
}

static inline dictEntry *dictionary.lookup (__object__ *k, long hash)
{
	/** Query lookup
	**/
	if_likely (type_good (k))
		return _lookup (hash, k);
	return empty_entry ();
}

dictEntry *dictionary.lookup (__object__ *k, long *h)
{
	/** Modifying lookup
	**/
	if_likely (type_good (k))
		return _lookup (*h = get_hash (k), k);
	mutateby (k);
	return ((dictionary*)this)->lookup (k, h);
}

__object__ *dictionary.get (__object__ *k)
{
	return lookup (k)->val.o;
}

void dictionary.insert (__object__ *k, __object__ *v)
{
	long h;
	dictEntry *ep = lookup (k, &h);
	if (ep->val.o) ep->val = v;
	else {
		/** The thing is that we do not care about correctly inc/dec refing
		*** the dummy object -- it's a __permanent__ **/
		++used;
		if_likely (!ep->key.o)
			++fill;
		ep->key.ctor (k);
		ep->val.ctor (v);
		ep->hash = h;
		if_unlikely (3 * fill >= max)
			resize ();
	}
}

void dictionary.remove (__object__ *k)
{
	dictEntry *ep = lookup (k);
	if_unlikely (!ep->val.o)
		RaiseKeyError (k);
	--used;
	ep->key.__copyobj (&dummy);
	ep->val.null ();
}

__object__ *dictionary.pop (__object__ *k)
{
	dictEntry *ep = lookup (k);
	if (ep->val.o) {
		REFPTR R = ep->val.o;
		--used;
		ep->key.__copyobj (&dummy);
		ep->val.null ();
		return R.Dpreserve ();
	}
	return 0;
}

__object__ *dictionary.setdefault (__object__ *k, __object__ *v)
{
	long h;
	dictEntry *ep = lookup (k, &h);
	if (ep->val.o) return ep->val.o;
	++used;
	if_likely (!ep->key.o)
		++fill;
	ep->key.ctor (k);
	ep->val.ctor (v);
	ep->hash = h;
	if_unlikely (3 * fill >= max)
		resize ();
	return v;
}

static __object__ *dictionary.key (__object__ *k)
{
	dictEntry *ep = lookup (k);
	if (ep->val.o)
		return ep->key.o;
	return 0;
}

__object__ *dictionary.contains (__object__ *k)
{
	if_unlikely (!type_good (k))
		return 0;

	register unsigned long h = get_hash (k);
	register unsigned long mask = mask, i = h & mask, p;
	dictEntry *tbl = tbl, *ep = &tbl [i];

	if (!ep->key.o) return 0;
	if (ep->hash == h && cmp_key_ent (ep, k))
		return ep->val.o;

	for (p = h; ; p >>= PERTURB_SHIFT) {
		i = 5 * i + p + 1;
		ep = &tbl [i & mask];
		if (!ep->key.o) return 0;
		if (ep->hash == h && cmp_key_ent (ep, k)) return ep->val.o;
	}
}

dictEntry *dictionary.__iterfast (dictEntry *d)
{
	if_unlikely (!d) {
		if (tbl [0].val.o)
			return &tbl [0];
		d = &tbl [0];
	}

	for (++d; d <= tbl + mask && d > tbl; d++)
		if (d->val.o)
			return d;
	return 0;
}

void dictionary.ins_keys (ListObj L)
{
	dictEntry *tbl = tbl;
	REFPTR *data = L.data;

	for (unsigned long i = L.len = used; i; tbl++)
		if (tbl->val.o)
			data++->ctor (tbl->key.o), --i;
}

void dictionary.ins_vals (ListObj L)
{
	dictEntry *tbl = tbl;
	REFPTR *data = L.data;

	for (unsigned long i = L.len = used; i; tbl++)
		if (tbl->val.o)
			data++->ctor (tbl->val.o), --i;
}

void dictionary.ins_itms (ListObj L)
{
	dictEntry *tbl = tbl;
	REFPTR *data = L.data;

	for (unsigned long i = L.len = used; i; tbl++)
		if (tbl->val.o)
			data++->ctor (new Tuplen (tbl->key.o, tbl->val.o)), --i;
}

dictionary.~dictionary ()
{
	dictEntry *tbl = tbl, *atbl = tbl;
	unsigned long i = used;
	this->used = 0;
	this->tbl = smalltbl;

	for (; i; tbl++)
		if (tbl->val.o) {
			tbl->key.dtor ();
			tbl->val.dtor ();
			--i;
		}

	if (atbl != smalltbl)
		__free (atbl);
}

// these are used for setattr/getattr.  We know that k is definitelly a string

__object__ *dictionaryStrAlways.lookup_stri (__object__ *k)
{
	return (*this).lookup (k)->val.o;
}

static void dictionaryStrAlways.insert_new (dictEntry*, __object__*, __object__*, long) noinline;

static void dictionaryStrAlways.insert_new (dictEntry *ep, __object__ *k, __object__ *v, long h)
{
	++used;
	if_likely (!ep->key.o)
		++fill;
	ep->key.ctor (k);
	ep->val.ctor (v);
	ep->hash = h;
	if_unlikely (3 * fill >= max)
		resize ();
}

void dictionaryStrAlways.insert_stri (__object__ *k, __object__ *v)
{
	long h = get_hash (k);
	dictEntry *ep = _lookup (h, k);
	if_likely (ep->val.o != 0) ep->val = v;
	else insert_new (ep, k, v, h);
}

// weakref cleaner

void dictionary.clean_weak_vals (register int rc)
{
	if (itrn)
		return;

	register dictEntry *tbl = tbl;
	register long i = used;

	for (; i > 0; tbl++)
		if (tbl->val.o) {
			if (tbl->val.o->refcnt == rc) {
				tbl->key.__copyobj (&dummy);
				tbl->val.null ();
				--used;
			}
			--i;
		}

	test_resize ();
}

void dictionary.clean_weak_set ()
{
	if (itrn)
		return;

	dictEntry *tbl = tbl;
	long i = used;

	for (; i > 0; tbl++)
		if (tbl->val.o) {
			if (tbl->key->refcnt == 1) {
				tbl->key.__copyobj (&dummy);
				tbl->val.o = 0;
				--used;
			}
			--i;
		}

	test_resize ();
}

//#########################################################################################
//#########################################################################################
//#########################################################################################
//#########################################################################################
/* >>>>>>>>>>> iterators <<<<<<<<<<< */

static class DictIterObj : iteratorBase
{
#ifdef FEATURE_SETITER
	const unsigned int vf_flags |= VF_SETITER;
#endif
	int i, n, c;
	dictionary &D;
	__object__ *i_th (dictEntry *ep) { return ep->key.o; }
   public:
	DictIterObj (DictObj*);
	DictIterObj (SetObj*);
auto	__object__ *xnext ();
	int len ()		{ return D.used - n; }
#ifdef FEATURE_SETITER
	void setiter (__object__*);
#endif
	~DictIterObj ()		{ if (!c) --D.itrn; }
};

DictIterObj.DictIterObj (DictObj *x)
{
	iteratorBase.ctor (x);
	dereference D = &x->D;
	++D.itrn;
	c = n = i = 0;
}

DictIterObj.DictIterObj (SetObj *x)
{
	iteratorBase.ctor (x);
	dereference D = &x->D;
	++D.itrn;
	c = n = i = 0;
}
__object__ *DictIterObj.xnext ()
{
	if_unlikely (n >= D.used) {
		if (!c) {
			c = 1;
			--D.itrn;
		}
		RaiseStopIteration ();
	}
	++n;
	register int i = i;
	register dictEntry *tbl = D.tbl;

	while (!tbl [i].val.o) ++i;
	this->i = i + 1;
	return i_th (&tbl [i]);
}

#ifdef FEATURE_SETITER
void DictIterObj.setiter (__object__ *o)
{
	D.tbl [i-1].val = o;
}
#endif

static class DictIterItemsObj : DictIterObj {
	REFPTR tp;
	__object__ *i_th (dictEntry*);
	DictIterItemsObj (DictObj*);
	void trv traverse ();
};

DictIterItemsObj.DictIterItemsObj (DictObj *x)
{
	DictIterObj.ctor (x);
	tp.ctor (new Tuplen (&None, &None));
}

__object__ *DictIterItemsObj.i_th (dictEntry *ep)
{
	if (tp->refcnt == 1) {
		tp.as_tuplen->__xsetitem__ (0, ep->key.o);
		tp.as_tuplen->__xsetitem__ (1, ep->val.o);
		return tp.o;
	}
	return newTuple (ep->key.o, ep->val.o);
}

static class DictIterValuesObj : DictIterObj {
	__object__ *i_th (dictEntry *ep) { return ep->val.o; }
};

//////////////////////////////////////////////
//////////// dictEntry methods ///////////////
//////////////////////////////////////////////

void dictEntry.copy (dictEntry *source)
{
	memcpy (this, source, sizeof *this);
}

/* ---------* DictObj *--------- */

DictObj.DictObj ()
{
	__container__.ctor ();
	D.ctor ();
}

void DictObj.__attrdict ()
{
	__container__.ctor ();
	(*(dictionaryStrAlways*) &D).ctor ();
}

__object__ *DictObj.contains (__object__ *o)
{
	return (&D)->contains (o);
}

__object__ *DictObj.pop (__object__ *o)
{
	return D.pop (o);
}

__object__ *DictObj.xgetitem (__object__ *o)
{
	return D.get (o) ?: RaiseKeyError (o);
}

__object__ *DictObj.xgetitem_noraise (__object__ *o)
{
	return D.get (o);
}

static inline __object__ *dictionary.hothit (__object__ *o)
{
	// hothit works as long as "the same object cannot be
	// unequal to itself". If it was, one would be unable
	// to find it in the dictionary anyway.
	StringObj *S = StringObj.cast (o);

	if_likely (tbl [S->phash & mask].key.o == o)
		return tbl [S->phash & mask].val.o;
	return 0;
}

__object__ *DictObj.xgetitem_str (__object__ *o)
{
#if 1
	StringObj *S = StringObj.cast (o);
	/* XXXX: Theoretically, *all* objects *should* have S->phash,
	 * iow, should be interned strings. But there was a case one
	 * wasn't. Investigate and remove the test.  */
	if (S->phash) {
		__object__ *p = D.tbl [S->phash & D.mask].key.o;
		if (p == o)
			return D.tbl [S->phash & D.mask].val.o;
		if (!p) return 0;
	}
	return (*(dictionaryStrAlways*) &D).lookup_stri (o);
#else
	return D.hothit (o) ?: (*(dictionaryStrAlways*) &D).lookup_stri (o);
#endif
}

void DictObj.xdelitem (__object__ *o)
{
	D.remove (o);
}

void DictObj.xsetitem (__object__ *k, __object__ *v)
{
	D.insert (k, v);
}

__object__ *DictObj.setdefault (__object__ *k, __object__ *x)
{
	return D.setdefault (k, x);
}

static __object__ *DictObj.key (__object__ *k)
{
	return D.key (k);
}

void DictObj.xsetitem_str (__object__ *k, __object__ *v)
{
	(*(dictionaryStrAlways*) &D).insert_stri (k, v);
}

__object__ *DictObj.iter ()
{
	return new DictIterObj (this);
}

ListObj *DictObj.keys ()
{
	ListObj *L = new ListObj __sizector (D.used);
	D.ins_keys (L);
	return L;
}

ListObj *DictObj.values ()
{
	ListObj *L = new ListObj __sizector (D.used);
	D.ins_vals (L);
	return L;
}

ListObj *DictObj.items ()
{
	ListObj *L = new ListObj __sizector (D.used);
	D.ins_itms (L);
	return L;
}

dictEntry *DictObj.__iterfast (dictEntry *d)
{
	return D.__iterfast (d);
}

__object__ *DictObj.iteritems ()
{
	return new DictIterItemsObj (this);
}

__object__ *DictObj.itervalues ()
{
	return new DictIterValuesObj (this);
}

modsection __object__ *DictObj.min_max (int max)
{
	dictEntry *E = __iterfast (0);
	if_unlikely (!E)
		RaiseValueError ("min/max on empty dict");
	__object__ *m = E->key.o;

	if (max) {
		while ((E = __iterfast (E)))
			if (m->cmp_GEN (E->key.o) < 0)
				m = E->key.o;
	} else while ((E = __iterfast (E)))
		if (m->cmp_GEN (E->key.o) > 0)
			m = E->key.o;

	return m;
}

bool DictObj.cmp_EQ_same (__object__ *o)
{
//	DictObj *d = DictObj.cast (o);
//	if (d->len () != len ())
//		return false;
	RaiseNotImplemented ("dictionary comparison");
}

int DictObj.cmp_GEN_same (__object__ *o)
{
	RaiseNotImplemented ("dict comparison");
}

void DictObj.update (DictObj U)
{
#if 0
	for (dictEntry *E = 0; (E = U.__iterfast (E));)
		(*this).xsetitem (E->key.o, E->val.o);
#else
	dictEntry *tbl;
	__object__ *k, *v;
	long i, h;

	/* if this dict is empty, copy over the items */
	if_unlikely (!D.used && D.tbl == D.smalltbl)
		return copy_from (U);

	tbl = U.D.tbl;

	for (i = U.D.used; i; tbl++)
		if (tbl->val.o) {
			k = tbl->key.o;
			v = tbl->val.o;
			h = tbl->hash;
			dictEntry *ep = D.lookup (k, h);
			if (ep->val.o) ep->val = v;
			else {
				++D.used;
				if_likely (!ep->key.o)
					++D.fill;
				ep->key.ctor (k);
				ep->val.ctor (v);
				ep->hash = h;
				if_unlikely (3 * D.fill >= D.max)
					D.resize ();
			}

			--i;
		}
#endif
}

static void DictObj.copy_from (DictObj U)
{
	dictEntry *tbl;
	long i;

	if (U.D.tbl != U.D.smalltbl)
		D.tbl = __malloc ((U.D.mask + 1) * sizeof *D.tbl);
	memcpy (D.tbl, U.D.tbl, (U.D.mask + 1) * sizeof *D.tbl);
	D.MutateTo (&U.D);
	D.mask = U.D.mask;
	D.fill = U.D.fill;
	D.max  = U.D.max;
	D.used = U.D.used;
	if ((i = D.used))
		for (tbl = D.tbl; ; tbl++)
			if (tbl->val.o) {
				tbl->val.incref ();
				tbl->key.incref ();
				if (!--i)
					break;
			}
}

void dictionary.clear ()
{
	/* Not safe that the dict may not recusrively decref */
	dtor ();
	ctor (0);
}
