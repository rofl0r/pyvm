/*
 *  Implementation of the set object
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

//
// this uses the code of the dictionary object and therefore,
// we have the unused value slot
//

/* set-related dictionary methods */
void dictionary.set_add (__object__ *k)
{
	long h;
	dictEntry *ep = lookup (k, &h);
	if_unlikely (!ep->val.o) {
		++used;
		if_likely (!ep->key.o)
			++fill;
		ep->key.ctor (k);
		ep->val.__copyctor (&None);
		ep->hash = h;
		if_unlikely (3 * fill >= max)
			resize ();
	}
}

static void dictionary.set_add_ne (__object__ *k)
{
	long h;
	dictEntry *ep = lookup (k, &h);
	++used;
	if_likely (!ep->key.o)
		++fill;
	ep->key.ctor (k);
	ep->val.__copyctor (&None);
	ep->hash = h;
	if_unlikely (3 * fill >= max)
		resize ();
}

bool dictionary.set_rmv (__object__ *k)
{
	dictEntry *ep = lookup (k);
	if_unlikely (!ep->val.o)
		return false;
	--used;
	ep->key.__copyobj (&dummy);
	ep->val.o = 0;
	return true;
}

static void dictionary.set_xor (__object__ *k)
{
	long h;
	dictEntry *ep = lookup (k, &h);
	if (!ep->val.o) {
		++used;
		if_likely (!ep->key.o)
			++fill;
		ep->key.ctor (k);
		ep->val.__copyctor (&None);
		ep->hash = h;
		if_unlikely (3 * fill >= max)
			resize ();
	} else {
		--used;
		ep->key.__copyobj (&dummy);
		ep->val.o = 0;
	}
}

void dictionary.set_ctor (dictionary D)
{
	D.test_resize ();
	memcpy (this, &D, sizeof D);
	tbl = D.smalltbl == D.tbl ? smalltbl : (dictEntry*) __malloc ((mask + 1) * sizeof *tbl);
	memcpy (tbl, D.tbl, (mask + 1) * sizeof *tbl);
	dictEntry *tbl = tbl;
	for (long i = used; i > 0; tbl++)
		if (tbl->val.o) tbl->key.incref (), --i;
}

void dictionary.set_clr ()
{
	dictEntry *tbl = tbl;
	for (long i = used; i > 0; tbl++)
		if (tbl->val.o) tbl->key.dtor (), --i;
	if (this->tbl != smalltbl)
		__free (this->tbl);
	init (0);
}

bool dictionary.set_issubset (dictionary D)
{
	if (D.used < used) return false;
	dictEntry *tbl = tbl;
	for (long i = used; i > 0; tbl++)
		if (tbl->val.o) {
			--i;
			if (!((&D)->contains (tbl->key.o)))
				return false;
		}
	return true;
}

/* ctor functions.  The dictionary to be filled MUST NOT be one of the iterated
   dictionaries.  */

void dictionary.set_intersection_ctor (dictionary small, dictionary big)
{
	ctor ();
	dictEntry *tbl = small.tbl;
	for (long i = small.used; i > 0; tbl++)
		if (tbl->val.o) {
			--i;
			if ((&big)->contains (tbl->key.o))
				set_add_ne (tbl->key.o);
		}
}

void dictionary.set_union_ctor (dictionary small, dictionary big)
{
	set_ctor (big);
	dictEntry *tbl = small.tbl;
	for (long i = small.used; i > 0; tbl++)
		if (tbl->val.o) set_add (tbl->key.o), --i;
	test_resize ();
}

void dictionary.set_symmetric_difference_ctor (dictionary small, dictionary big)
{
	set_ctor (big);
	dictEntry *tbl = small.tbl;
	for (long i = small.used; i > 0; tbl++)
		if (tbl->val.o) set_xor (tbl->key.o), --i;
	test_resize ();
}

void dictionary.set_difference_ctor_1sm (dictionary keep, dictionary discard)
{
	/* 'keep' is the small one */
	ctor ();
	dictEntry *tbl = keep.tbl;
	for (long i = keep.used; i > 0; tbl++)
		if (tbl->val.o) {
			--i;
			if (!(&discard)->contains (tbl->key.o))
				set_add_ne (tbl->key.o);
		}
}

void dictionary.set_difference_ctor_1bg (dictionary keep, dictionary discard)
{
	/* 'keep' is the big one */
	set_ctor (keep);
	dictEntry *tbl = discard.tbl;
	for (long i = discard.used; i > 0; tbl++)
		if (tbl->val.o) set_rmv (tbl->key.o), --i;
}

void dictionary.set_union_update (dictionary d)
{
	dictEntry *tbl = d.tbl;
	for (long i = d.used; i > 0; tbl++)
		if (tbl->val.o) set_add (tbl->key.o), --i;
}

void dictionary.set_difference_update (dictionary d)
{
	dictEntry *tbl = d.tbl;
	for (long i = d.used; i > 0; tbl++)
		if (tbl->val.o) set_rmv (tbl->key.o), --i;
}

/* ---* set *--- */

SetObj.SetObj ()
{
	__container__.ctor ();
	D.ctor ();
}

bool SetObj.cmp_EQ_same (__object__ *o)
{
	SetObj *S = SetObj.cast (o);
	return S->D.used == D.used && S->D.set_issubset (D);
}

SetObj.SetObj (__object__ *iterable)
{
	__container__.ctor ();
	if (SetObj.isinstance (iterable)) {
		D.set_ctor (SetObj.cast (iterable)->D);
		return;
	}
	/**** IF dictionary, borrow hashes ****/
	/*** lookup_nodummies ***/
	if (TupleObj.typecheck (iterable)) {
		D.ctor ();
		TupleObj *T = TupleObj.cast (iterable);
		for (int i = 0, j = T->len; i < j; i++)
			D.set_add (T->__xgetitem__ (i));
	} else if (DictObj.typecheck (iterable)) {
		/* (xxx: Just copy the table if no dummies) */
		DictObj *X = DictObj.cast (iterable);
		D.ctor (X->D.mask + 1);
		for (dictEntry *E = 0; (E = X->__iterfast (E));)
			D.set_add (E->key.o);
	} else {
		D.ctor ();
		REFPTR itr = iterable->vf_flags & VF_ITER ? iterable : iterable->iter ();
		REFPTR item;
		try for (;;) {
				item = itr->next ();
				D.set_add (item.o);
			}
		/*** XXX:RERAISER ***/
	}
}

void SetObj.intersect_ctor (SetObj *S1, SetObj *S2)
{
	__container__.ctor ();
	if (S1->len () < S2->len ()) D.set_intersection_ctor (S1->D, S2->D);
	else D.set_intersection_ctor (S2->D, S1->D);
}

void SetObj.union_ctor (SetObj *S1, SetObj *S2)
{
	__container__.ctor ();
	if (S1->len () < S2->len ()) D.set_union_ctor (S1->D, S2->D);
	else D.set_union_ctor (S2->D, S1->D);
}

void SetObj.symdiff_ctor (SetObj *S1, SetObj *S2)
{
	__container__.ctor ();
	if (S1->len () < S2->len ()) D.set_symmetric_difference_ctor (S1->D, S2->D);
	else D.set_symmetric_difference_ctor (S2->D, S1->D);
}

void SetObj.diff_ctor (SetObj *S1, SetObj *S2)
{
	__container__.ctor ();
	if (S1->len () < S2->len ()) D.set_difference_ctor_1sm (S1->D, S2->D);
	else D.set_difference_ctor_1bg (S1->D, S2->D);
}

void SetObj.union_update (SetObj *S)
{
	D.set_union_update (S->D);
}

void SetObj.difference_update (SetObj *S)
{
	D.set_difference_update (S->D);
}

__object__ *SetObj.contains (__object__ *o)
{
	return (&D)->contains (o);
}

/* as operators */

__object__ *SetObj.iter ()
{
	return new DictIterObj (this);
}

__object__ *SetObj.inplace_sub (__object__ *o)
{
	difference_update (SetObj.checkedcast (o));
	return this;
}

__object__ *SetObj.type_call (REFPTR argv[], int argc)
{
	if (!argc) return new SetObj;
	if (argc == 1) return new SetObj (argv [0].o);
	RaiseNotImplemented ("Too many arguments to set()");
}

static class set_method_proxy : method_proxy
{
	__object__ *sub (__object__ *o1, __object__ *o2)
	{
		return new SetObj diff_ctor (SetObj.cast (o1), SetObj.checkedcast (o2));
	}
	__object__ *and (__object__ *o1, __object__ *o2)
	{
		return new SetObj intersect_ctor (SetObj.cast (o1), SetObj.checkedcast (o2));
	}
};

static class set_method_proxy set_meth;

TypeObj SetTypeObj ctor (SetObj._v_p_t_r_, &set_meth);

/////////////////////////// initialize ////////////////////////////////

static __object__ *add_SetObj (REFPTR argv[])
{
	argv [0].as_set->add (argv [1].o);
	return &None;
}

static __object__ *remove_SetObj (REFPTR argv[])
{
	argv [0].as_set->remove (argv [1].o);
	return &None;
}

static __object__ *discard_SetObj (REFPTR argv[])
{
	argv [0].as_set->discard (argv [1].o);
	return &None;
}

static __object__ *clear_SetObj (REFPTR argv[])
{
	argv [0].as_set->clear ();
	return &None;
}

static __object__ *copy_SetObj (REFPTR argv[])
{
	return new SetObj (argv [0].as_set);
}

static __object__ *issubset_SetObj (REFPTR argv[])
{
	return argv [0].as_set->issubset (argv [1].o) ? &TrueObj : &FalseObj;
}

static __object__ *issuperset_SetObj (REFPTR argv[])
{
	return argv [0].as_set->issuperset (argv [1].o) ? &TrueObj : &FalseObj;
}

static __object__ *intersection_SetObj (REFPTR argv[])
{
	SetObj *S1 = argv [0].as_set, *S2 = SetObj.checkedcast (argv [1].o);
	return new SetObj intersect_ctor (S1, S2);
}

static __object__ *union_SetObj (REFPTR argv[])
{
	SetObj *S1 = argv [0].as_set, *S2 = SetObj.checkedcast (argv [1].o);
	return new SetObj union_ctor (S1, S2);
}

static __object__ *symdiff_SetObj (REFPTR argv[])
{
	SetObj *S1 = argv [0].as_set, *S2 = SetObj.checkedcast (argv [1].o);
	return new SetObj symdiff_ctor (S1, S2);
}

static __object__ *diff_SetObj (REFPTR argv[])
{
	SetObj *S1 = argv [0].as_set, *S2 = SetObj.checkedcast (argv [1].o);
	return new SetObj diff_ctor (S1, S2);
}

static __object__ *union_update_SetObj (REFPTR argv[])
{
	argv [0].as_set->union_update (SetObj.checkedcast (argv [1].o));
	return &None;
}

static const method_attribute set_methods [] = {
	{"add",			"set.add", SETARGC (2, 2), add_SetObj},
	{"remove",		"set.remove", SETARGC (2, 2), remove_SetObj},
	{"discard",		"set.discard", SETARGC (2, 2), discard_SetObj},
	{"clear",		"set.clear", SETARGC (1, 1), clear_SetObj},
	{"copy",		"set.copy", SETARGC (1, 1), copy_SetObj},
	{"issubset",		"set.issubset", SETARGC (2, 2), issubset_SetObj},
	{"issuperset",		"set.issuperset", SETARGC (2, 2), issuperset_SetObj},
	{"intersection",	"set.intersection", SETARGC (2, 2), intersection_SetObj},
	{"union",		"set.union", SETARGC (2, 2), union_SetObj},
	{"symmetric_difference","set.symmetric_difference", SETARGC (2, 2), symdiff_SetObj},
	{"difference",		"set.difference", SETARGC (2, 2), diff_SetObj},
	{"update",		"set.update", SETARGC (2, 2), union_update_SetObj},
	{"key",			"set.key", SETARGC (2, 2), key_DictObj},
	MENDITEM
};
