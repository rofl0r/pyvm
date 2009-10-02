/*
 *  Type calling for various objects
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

__object__ *TypeObj.getattr (__object__ *o)
{
	__object__ *r = TYPE2VPTR (typeptr)->type_methods->xgetitem_str (o) ?: get_type_method (o);

	if (r) return r;
	if (o == Interns.__name__)
		return new StringObj (TYPE2VPTR (typeptr)->stype);
	if (o == Interns.__sigbit__)
		return newIntObj (TYPE2VPTR (typeptr)->sigbit);
	return 0;
}

__object__ *IntObj.type_call (REFPTR argv[], int argc)
{
	int rez;
	if_unlikely (!argc) rez = 0;
	else {
		if (StringObj.typecheck (argv [0].o)) {
			// xx: check for '123 abc' which is an error
			char *endptr;
			char *startp = StringObj.cast (argv [0].o)->str;
			if_unlikely (!*startp)
				RaiseValueError ("strtoul failed");
			while (in4 (*startp, ' ', '\n', '\t', '\r')) startp++;
			rez = strtoul (StringObj.cast (argv [0].o)->str, &endptr, argc == 2
					? IntObj.fcheckedcast (argv [1].o)->i : 10);
			if_unlikely (*endptr) {
				while (in4 (*endptr, ' ', '\n', '\t', '\r')) endptr++;
				if (*endptr || endptr == startp)
					RaiseValueError ("strtoul failed");
			}
		} else if (FloatObj.isinstance (argv [0].o))
			rez = (int) FloatObj.cast (argv [0].o)->f;
		else if (LongObj.isinstance (argv [0].o))
			rez = LongObj.cast (argv [0].o)->to_int ();
		else if (BoolObj.isinstance (argv [0].o))
			rez = BoolObj.cast (argv [0].o)->i;
		else return IntObj.fcheckedcast (argv [0].o);
	}
	return newIntObj (rez);
}


__object__ *FloatObj.type_call (REFPTR argv[], int argc)
{
	if (argc) {
		if (IntObj.isinstance (argv [0].o))
			return new FloatObj ((double) IntObj.cast (argv [0].o)->i);
		if (StringObj.isinstance (argv [0].o))
			return new FloatObj (atof (StringObj.cast (argv [0].o)->str));
		FloatObj.fenforcetype (argv [0].o);
		return argv [0].o;
	}
	return new FloatObj (0.0);
}

__object__ *StringObj.type_call (REFPTR argv[], int argc)
{
	if (IntObj.isinstance (argv [0].o)) {
		char tmp [100];
		return new StringObj binctor (tmp, mytoa10 (tmp, IntObj.cast (argv [0].o)->i));
	}
	return argv [0]->str ();
}

__object__ *TypeObj.type_call (REFPTR argv[], int argc)
{
	if_unlikely (argc > 1) {
		StringObj.enforcetype (argv [0].o);
		Tuplen.enforcetype (argv [1].o);
		DictObj.enforcetype (argv [2].o);
		/**XXX: Must check that it's a StringAlways dictionary **/
		return new DynClassObj (argv [2].o, argv [1].o, argv [0].o);
	}
	return argv [0]->TypeOf ();
}

__object__ *listfromiter (__object__ *arg)
{
	/**** XXX: d'rather use bytecode ****/
	if (arg->vf_flags & VF_ITER)
		return iteratorBase.cast (arg)->to_list ();
	REFPTR iter = arg->iter ();
	/*** XXX: This is wrong for instances */
	return iteratorBase.cast (iter.o)->to_list ();
}

__object__ *ListObj.type_call (REFPTR argv[], int argc)
{
	if_unlikely (!argc)
		return new ListObj;
	if (TupleObj.typecheck (argv [0].o))
		return new ListObj refctor (argv [0].as_tuple->data, argv [0].as_tuple->len);
	if (is_array (argv [0].o))
		return array_to_list (argv [0].o);
	return listfromiter (argv [0].o);
}

__object__ *TupleObj.type_call (REFPTR argv[], int argc)
{
	/* may be called from boot_pyvm only.  If refcnt of the argument is 1
	   then we can assume the argument is on the stack and can only be destroyed.  */
	if_unlikely (!argc)
		return NILTuple;
	if (TupleObj.typecheck (argv [0].o)) {
		if (!argv [0].as_tuple->len) return NILTuple;
		if_unlikely (argv [0]->refcnt <= 1)
			return ListObj.isinstance (argv [0].o) ? argv [0].as_list->list_to_tuple ()
				: argv [0].o;
		return new Tuplen refctor (argv [0].as_tuple->data, argv [0].as_tuple->len);
	}
	REFPTR L = listfromiter (argv [0].o);
	return !L.as_list->len ? (__object__*) NILTuple : L.as_list->list_to_tuple ();
}

extern PyFuncObj *dict_mappingFunc;

__object__ *DictObj.type_call (REFPTR argv[], int argc)
{
	/**** XXXXX: improve *****/
	if_unlikely (!argc)
		return new DictObj;

	DictObj *D = new DictObj;
	REFPTR DR = D;
	if (DictObj.typecheck (argv [0].o)) {
		DictObj *U = DictObj.cast (argv [0].o);
		if (!U->D.used) return DR.Dpreserve ();
		if (U->D.fill == U->D.used)
			D->copy_from (*U);
		else for (dictEntry *E = 0; (E = U->__iterfast (E));)
			D->xsetitem (E->key.o, E->val.o);
	} else if (TupleObj.typecheck (argv [0].o)) {
		REFPTR *data = argv [0].as_tuple->data;
		int len = argv [0].as_tuple->len;
		for (int i = 0; i < len; i++) {
			Tuplen *T = Tuplen.fcheckedcast (data [i].o);
			if_unlikely (T->len != 2)
				RaiseValueError ("dict() needs tuples with 2 elements");
			D->xsetitem (T->__xgetitem__ (0), T->__xgetitem__ (1));
		}
	} else {
		/* There are two cases, either as a mapping or as an iterable.
		   See the comment at pyby.c+
		   Only the case of iterable is accepted. The other one is
		   implemented but it sucks (uses keys() and __getitem__()).
		 */
		REFPTR argv2 [] = { &None, D, argv [0].o };
		(*dict_mappingFunc).call (argv2 [0], argv2, 2);
		return &CtxSw;
	}
	return DR.Dpreserve ();
}

__object__ *NamespaceObj.type_call (REFPTR argv [], int argc)
{
	return new NamespaceObj ();
}

__object__ *BoolObj.type_call (REFPTR argv[], int argc)
{
	if_unlikely (!argc)
		return &FalseObj;
	return (IntObj.isinstance (argv [0].o) ? argv [0].as_int->i : argv [0]->Bool ())
		? &TrueObj : &FalseObj;
}
