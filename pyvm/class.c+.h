/*
 *  Namespace, class, instance and bound method dynamic objects
 * 
 *  Copyright (c) 2006, 2007, 2008 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

static inline void callBoundMethod (__object__*, __object__*, REFPTR, REFPTR [], int);

/* --------------------------------------------------------------- */
/*								   */
/* --------------------------------------------------------------- */

NamespaceObj.NamespaceObj ()
{
	__container__.ctor ();
	__dict__.ctor (new DictObj __attrdict ());
}

NamespaceObj.NamespaceObj (__object__ *d)
{
	__container__.ctor ();
	DictObj.enforcetype (d);
	__dict__.ctor (d);
}

bool NamespaceObj.hasattr (__object__ *attr)
{
	return !!DictObj.cast (__dict__.o)->contains (attr);
}

void NamespaceObj.setattr (__object__ *attr, __object__ *val)
{
	StringObj *s = StringObj.cast (attr);

	if_unlikely (s == Interns.__dict__) {
		DictObj.enforcetype (val);
		__dict__ = val;
	} else 
		DictObj.cast (__dict__.o)->xsetitem (attr, val);
}

void NamespaceObj.delattr (__object__ *attr)
{
	DictObj.cast (__dict__.o)->xdelitem (attr);
}

__object__ *NamespaceObj.getattr (__object__ *attr)
{
	StringObj *s = StringObj.cast (attr);

	if_unlikely (s == Interns.__dict__)
		return __dict__.o;
#ifndef HOTHIT
	return __dict__.as_dict->D.hothit (attr) ?: __dict__.as_dict->xgetitem_str (s);
#else
	return __dict__.as_dict->xgetitem_str (s);
#endif
}

/* -----* overloading of operator methods *----- */

__object__ *NamespaceObj.iter ()
{
	__object__ *o = getattr (Interns.__iter__);

	if (o) {
		REFPTR xx = o, yy;
		o->call (xx, &yy, 0);
		if (xx.o != &CtxSw)
			return xx.Dpreserve ();
		return preempt_pyvm (CtxSw.vm);
	}

	if (NamespaceObj.isinstance (this))
		return __dict__.as_dict->iteritems ();

	RaiseNoAttribute (Interns.__iter__);
}

__object__ *NamespaceObj.xnext ()
{
	__object__ *o = getattr (Interns.next);

	if (o) {
		REFPTR xx = o, yy;
		o->call (xx, &yy, 0);
		if (xx.o == &CtxSw)
			return xx.o;
		return xx.Dpreserve ();
	}

	RaiseNoAttribute (Interns.next);
}

__object__ *NamespaceObj.xgetitem (__object__ *o)
{
	__object__ *a = getattr (Interns.__getitem__);

	if (a) {
		REFPTR xx [2] = { a, o };
		a->call (xx [0], xx, 1);
		if (xx [0].o != &CtxSw)
			return xx [0].Dpreserve ();
		if (RAISE_CONTEXTS)
			RaiseContext ();
		else
			return preempt_pyvm (CtxSw.vm);
	}

	RaiseNoAttribute (Interns.__getitem__);
}

void NamespaceObj.xsetitem (__object__ *o, __object__ *v)
{
	__object__ *a = getattr (Interns.__setitem__);

	if_likely (!!a) {
		REFPTR xx [3] = { a, o, v };
		a->call (xx [0], xx, 2);
		if (xx [0].o == &CtxSw)
			preempt_pyvm (CtxSw.vm);
		return;
	}

	RaiseNoAttribute (Interns.__setitem__);
}

void NamespaceObj.xdelitem (__object__ *o)
{
	__object__ *a = getattr (Interns.__delitem__);

	if (a) {
		REFPTR xx [] = { a, o };
		a->call (xx [0], xx, 1);
		if (xx [0].o == &CtxSw)
			preempt_pyvm (CtxSw.vm);
		return;
	}

	RaiseNoAttribute (Interns.__delitem__);
}

bool NamespaceObj.cmp_EQ (__object__ *o)
{
	__object__ *a = getattr (Interns.__eq__);
	if (!a)
		return cmp_GEN (o) == 0;
	REFPTR xx [2] = { a, o };
	a->call (xx [0], xx, 1);
	if (xx [0].o == &CtxSw)
		xx [0] = preempt_pyvm (CtxSw.vm);
	return xx [0]->Bool ();
}

int NamespaceObj.cmp_GEN (__object__ *o)
{
	__object__ *a = getattr (Interns.__cmp__);
	if (!a)
		return stype - o->stype ?: o - (__object__*)this;
	REFPTR xx [2] = { a, o };
	a->call (xx [0], xx, 1);
	if (xx [0].o == &CtxSw)
		xx [0] = preempt_pyvm (CtxSw.vm);
	return xx [0].CheckInt ()->i;
}

long NamespaceObj.hash ()
{
	__object__ *a = getattr (Interns.__hash__);
	if_likely (!a)
		return ((long) this) >> 2;
	REFPTR xx = a;
	a->call (xx, &xx, 0);
	if (xx.o == &CtxSw)
		xx = preempt_pyvm (CtxSw.vm);
	return IntObj.fcheckedcast (xx.o)->i;
}

bool NamespaceObj.Bool ()
{
	__object__ *a = getattr (Interns.__nonzero__);
	if_likely (!a) {
		if_unlikely ((a = getattr (Interns.__len__)) != 0)
			return len ();
		return true;
	}
	REFPTR xx = a;
	a->call (xx, &xx, 0);
	if (xx.o == &CtxSw)
		xx = preempt_pyvm (CtxSw.vm);
	return xx->Bool ();
}

static __object__ *NamespaceObj.binaries (StringObj *key, __object__ *o) noinline;
static __object__ *NamespaceObj.binaries (StringObj *key, __object__ *o)
{
	__object__ *a = getattr (key);
	if_likely (!!a) {
		REFPTR xx [2] = { a, o };
		a->call (xx [0], xx, 1);
		if (RAISE_CONTEXTS)
			return xx [0].o != &CtxSw ? xx [0].o : RaiseContext ();
		else
			return xx [0].o != &CtxSw ? xx [0].o : preempt_pyvm (CtxSw.vm);
	}
	RaiseNoAttribute (key);
}

__object__ *NamespaceObj.binary_add (__object__ *o)
{
	return binaries (Interns.__add__, o);
}

__object__ *NamespaceObj.binary_mul (__object__ *o)
{
	return binaries (Interns.__mul__, o);
}

/* --------------------------------------------------------------- */
/*	Bound namespace						   */
/* --------------------------------------------------------------- */

static final class BoundNamespaceObj : NamespaceObj, seg_allocd
{
	REFPTR __inst__;
   public:
	BoundNamespaceObj (__object__*, __object__*);
	__object__ *getattr (__object__*);
	void print ();
trv	void traverse ();
cln	void __clean ();
};

BoundNamespaceObj.BoundNamespaceObj (__object__ *inst, __object__ *ns)
{
	NamespaceObj.ctor (NamespaceObj.cast (ns)->__dict__.o);
	__inst__.ctor (inst);
}

void BoundNamespaceObj.print ()
{
	print_out (STRL ("Bound namespace"));
}

__object__ *BoundNamespaceObj.getattr (__object__ *a)
{
	a = NamespaceObj.getattr (a);
	if_unlikely (!a) return 0;
	if_unlikely (a->vf_flags & VF_BOUND)
		return newDynMethod (__inst__.as_inst, a);
	if_unlikely (NamespaceObj.isinstance (a))
		return new BoundNamespaceObj (__inst__.as_inst, a);
	return a;
}

/* --------------------------------------------------------------- */
/*	Module - mostly in cold.c+				   */
/*								   */
/* the constructor must be here because it's the "key func" and	   */
/* the virtual table will be here, meaning it can use the autos,   */
/* 'cmp_GEN', etc.						   */
/* --------------------------------------------------------------- */

ModuleObj.ModuleObj ()
{
	ctor (&None, &None);
}

/* --------------------------------------------------------------- */
/*	Class							   */
/* --------------------------------------------------------------- */

extern void DynClassObj.set_bases (__object__*);

DynClassObj.DynClassObj (__object__ *dict, __object__ *bases, __object__ *name)
{
	// this can go to cold...
	DictObj.cast (dict)->D.reorder_opt (name);
	NamespaceObj.ctor (dict);
	__bases__.ctor (NILTuple);
	__name__.ctor (name);
	set_bases (bases);
	validate_slots ();
	/* Python does something similar: If a base class has __getattr__
	 * we cache in in here. If the user modifies the base classe's
	 * __getattr__ it is not updated. IIRC
	 */
	__getattr__.ctor (getattr_c (Interns.__getattr__) ?: &None);
	__setattr__.ctor (getattr_c (Interns.__setattr__) ?: &None);
	__del__.ctor (getattr_c (Interns.__del__) ?: &None);
	/* for quick comparisons. meaning that __cmp__ and __eq__ cannot be
	 * overriden by instances! */
	has_eq = !!getattr_c (Interns.__eq__);
	has_cmp = !!getattr_c (Interns.__cmp__);
	has_hash = !!getattr_c (Interns.__hash__);
}

bool DynInstanceObj.cmp_EQ (__object__ *o)
{
	/* Even if a class declares __eq__ or __cmp__ we assume that it cannot be
	   unequal to itself!  */
	if (this == o)
		return true;
	if_likely (!__class__.as_class->has_eq)
		return cmp_GEN (o) == 0;
	__object__ *a = getattr (Interns.__eq__);
	REFPTR xx [2] = { a, o };
	a->call (xx [0], xx, 1);
	if (xx [0].o == &CtxSw)
		xx [0] = preempt_pyvm (CtxSw.vm);
	return xx [0]->Bool ();
}

int DynInstanceObj.cmp_GEN (__object__ *o)
{
	if_likely (!__class__.as_class->has_cmp)
		return stype - o->stype ?: o - (__object__*)this;
	__object__ *a = getattr (Interns.__cmp__);
	REFPTR xx [2] = { a, o };
	a->call (xx [0], xx, 1);
	if (xx [0].o == &CtxSw)
		xx [0] = preempt_pyvm (CtxSw.vm);
	return xx [0].CheckInt ()->i;
}

long DynInstanceObj.hash ()
{
	if_likely (!__class__.as_class->has_hash)
		return ((long) this) >> 2;
	__object__ *a = getattr (Interns.__hash__);
	REFPTR xx = a;
	a->call (xx, &xx, 0);
	if (xx.o == &CtxSw)
		xx = preempt_pyvm (CtxSw.vm);
	return IntObj.fcheckedcast (xx.o)->i;
}

bool DynClassObj.isparentclass (__object__ *o)
{
	/* IS 'o' a parent class of this */
	__object__ *t;
	for (int i = 0, l = __bases__.as_tuplen->len; i < l; i++)
		if ((t = __bases__.as_tuplen->__xgetitem__ (i)) == o
		|| DynClassObj.cast (t)->isparentclass (o))
			return true;
	return false;
}

static inline unsigned int superhash (const char *str, unsigned int len)
{
	/* Super-hash (TM) (it took me 3 days to calculate this,
	** please don't add any more fields!) -- just kidding the computer did it.
	** if you add any more fields you not only need to recalculate the xhash
	** but also modify the code that uses it at {get,set}attr
	*/
#define HASH_DEL	0
#define	HASH_BASES	1
#define	HASH_DICT	2
#define	HASH_NAME	3
#define	HASH_GETATTR	4
	return (str [3] ^ str [len - 4]) >> 2;
}

static int isbltin_class_attr (StringObj *S)
{
	register char *s = S->str;
	if_likely (s [0] != '_' || s [1] != '_')
		return -1;
	register int xhash;
	register StringObj *r;
	switch (xhash = superhash (S->str, S->len)) {
		default: return -1;
		ncase HASH_BASES:	r = Interns.__bases__;
		ncase HASH_GETATTR:	r = Interns.__getattr__;
		ncase HASH_DICT:	r = Interns.__dict__;
		ncase HASH_NAME:	r = Interns.__name__;
		ncase HASH_DEL:		r = Interns.__del__;
	}
	return r && (r == S || r->cmp_EQ_same (S)) ? xhash : -1;
}

bool DynClassObj.hasattr (__object__ *attr)
{
	if (__dict__.as_dict->contains (attr))
		return true;
	if (isbltin_class_attr (StringObj.cast (attr)) >= 0)
		return true;
	for (int i = 0; i < __bases__.as_tuple->len; i++)
		if (DynClassObj.cast (__bases__.as_tuple->__xgetitem__ (i))->hasattr (attr))
			return true;
	return false;
}

void DynClassObj.setattr (__object__ *attr, __object__ *val)
{
	int i;
	if_unlikely ((i = isbltin_class_attr (StringObj.cast (attr))) >= 0)
		switch (i) {
			case HASH_BASES: set_bases (val);
			ncase HASH_NAME: __name__ = StringObj.fcheckedcast (val);
			ncase HASH_DICT: DictObj.enforcetype (val); __dict__ = val;
			ncase HASH_GETATTR: __getattr__ = val; goto add_to_dict;
			ncase HASH_DEL: __del__ = val; goto add_to_dict;
		}
	else {
		add_to_dict: __dict__.as_dict->xsetitem (attr, val);
	}
}

void DynClassObj.delattr (__object__ *attr)
{
	if_likely (!isbltin_class_attr (StringObj.cast (attr)) < 0) {
		__dict__.as_dict->xdelitem (attr);
	}
}

__object__ *DynClassObj.getattr (__object__ *attr)
{
	__object__ *r = getattr_c (attr);
	if_likely (r != 0) return likely (!(r->vf_flags & VF_CLSMETH)) ? r : 
		 new DynMethodObj (this, DynClassMethodObj.cast (r)->__callable__.o);
	return 0;
}

__object__ *DynClassObj.getattr_c (__object__ *attr)
{
	/* Extra Complexity because of the existance of classmethod
	 */
	StringObj *s = StringObj.cast (attr);
	__object__ *r;
	int i;

#ifndef HOTHIT
	if_likely ((r = __dict__.as_dict->D.hothit (attr)) != 0)
		return r;
#endif
	if ((r = __dict__.as_dict->xgetitem_str (s)))
		return r;
	if ((i = isbltin_class_attr (s)) >= 0)
		switch (i) {
			case HASH_BASES: return __bases__.o;
			ncase HASH_NAME: return __name__.o;
			ncase HASH_DICT: return __dict__.o;
		}
	for (int i = 0; i < __bases__.as_tuple->len; i++)
		if ((r = DynClassObj.cast (__bases__.as_tuple->__xgetitem__ (i))->getattr_c (attr)))
			return r;
	return 0;
}

/*
 * ctx_caller: a special object which keeps a vm_contex.
 * The ctx_caller is used to invoke the __init__() function;
 * When we call a class that has an __init__ constructor, then
 * we get a vm_context to execute. However, unlike other
 * functions, the return value must be the created instance
 * and not the return value of the __init__ function. Thus
 * the ctx_caller which does that.
 */
static const char ctx_caller_type [] = "<ctx_caller>";

static class ctx_caller : __container__, seg_allocd
{
	const char *stype = ctx_caller_type;
	const TypeObj &type = &PyFuncTypeObj;
	vm_context *vm;
    public:
	ctx_caller (vm_context *v)	{ __container__.ctor (); vm = v; }
	void call (REFPTR ret, REFPTR[], int)	{ CtxSw.vm = vm; ret.__copyobj (&CtxSw); }
	void print () { }
trv	void traverse () { /*vm->traverse_vm ();*/ }
};

PyFuncObj *constructorFunc;

void DynClassObj.call (REFPTR ret, REFPTR argv [], int argc)
{
	__object__ *o;
	if_unlikely (__slots__.o != &None)
		o = new NamedListObj (this);
	else o = new DynInstanceObj (this);

	__object__ *init = (*this).getattr (Interns.__init__);

	if (!init) {
		ret = o;
		return;
	}

	callBoundMethod (init, o, ret, argv, argc);
	if (ret.o != &CtxSw) {
		ret = o;
		return;
	}

	if (1) {
		/* XXX: actually there is only one ctx_caller so make it a global __permanent__
		 * avoid increfing it at the constructor of argv2[0].
		 * the function does not throw. avoid unwind code.
		 */
		__object__ *cl = new ctx_caller (CtxSw.vm);
		REFPTR argv2 [] = { cl, o };
		(*constructorFunc).call (argv2 [0], argv2 - 1, 2);
	}
}

/* --------------------------------------------------------------- */
/*	Instance						   */
/* --------------------------------------------------------------- */

DynInstanceObj.DynInstanceObj (__object__ *_class)
{
	NamespaceObj.ctor ();
	__class__.ctor (_class);
}

static int isbltin_inst_attr (__object__ *o)
{
	if_unlikely (o == Interns.__dict__)
		return 1;
	if_unlikely (o == Interns.__class__)
		return 2;
	return 0;
}

bool DynInstanceObj.hasattr (__object__ *attr)
{
	if (__dict__.as_dict->contains (attr))
		return true;
	if (__class__.as_class->hasattr (attr))
		return true;
	if_unlikely (isbltin_inst_attr (attr))
		return true;
	__object__ *o;
	try o = getattr (attr);
	else o = 0;
	if (o) {
		REFPTR O = o;
		return true;
	}
	return false;
}

void DynInstanceObj.setattr (__object__ *attr, __object__ *val)
{
	int i;
	if_unlikely (i = isbltin_inst_attr (attr))
		if (i == 1) {
			DictObj.enforcetype (val);
			__dict__ = val;
		} else {
			DynClassObj.enforcetype (val);
			__class__ = val;
		}
	else if_unlikely (__class__.as_class->__setattr__.o != &None) {
		REFPTR xx [] = { __class__.as_class->__setattr__.o, this, attr, val };
		__class__.as_class->__setattr__->call (xx [0], xx, 3);
		if (xx [0].o == &CtxSw)
			preempt_pyvm (CtxSw.vm);
	} else {
	 	__dict__.as_dict->xsetitem_str (attr, val);
	}
}

void DynInstanceObj.delattr (__object__ *attr)
{
	if_likely (!isbltin_inst_attr (attr))
		__dict__.as_dict->xdelitem (attr);
}

#ifndef HOTHIT
static inline __object__ *dictionary.hothit (__object__ *o)
{
	StringObj *S = StringObj.cast (o);

	if_likely (tbl [S->phash & mask].key.o == o)
		return tbl [S->phash & mask].val.o;
	return 0;
}
#endif

static inline __object__ *newDynMethod (__object__ *I, __object__ *r)
{
	return new DynMethodObj (I, r);
}

__object__ *DynInstanceObj.getattr (__object__ *attr)
{
	__object__ *r;
	int i;

#ifndef HOTHIT
	if_likely (!!(r = __dict__.as_dict->D.hothit (attr)))
		return r;
#endif
	if_likely ((r = __dict__.as_dict->xgetitem_str (attr)) != 0)
		return r;
	if_unlikely ((i = isbltin_inst_attr (attr)))
		return i == 1 ? __dict__.o : __class__.o;
	if_likely ((r = __class__.as_class->getattr_c (attr)) != 0) {
		if_likely (r->vf_flags & VF_BOUND)
			return newDynMethod (this, r);
		if_unlikely (NamespaceObj.isinstance (r))
			return new BoundNamespaceObj (this, r);
		return r;
	}

	/* This is clearly broken but since python does the same thing... */
	if (__class__.as_class->__getattr__.o != &None) {
		REFPTR xx [] = { __class__.as_class->__getattr__.o, this, attr };
		__class__.as_class->__getattr__->call (xx [0], xx, 2);
		if (xx [0].o != &CtxSw)
			return xx [0].Dpreserve ();
		__object__ *r = 0;
		try (Interrupt *I) {
			/* XXX-LWCBUG: lwc generated code SEGFAULTs if we return now
			 * because it tries to 'pop_unwind' from the current setjmp
			 * context while 'xx' is installed in the outer setjmp context.
			 * Instead of fixin lwc do the workaround here, but be careful
			 * to avoid similar cases!!! no time to fix lwc now. */
		    	/*return preempt_pyvm (CtxSw.vm);*/
			r = preempt_pyvm (CtxSw.vm);
		} else catch_no_attribute (I);
		return r;
	}

	return 0;
}

void DynInstanceObj.call (REFPTR ret, REFPTR argv [], int argc)
{
	__object__ *c = (*this).getattr (Interns.__call__);
	if_unlikely (!c)
		RaiseNoAttribute (Interns.__call__);
	ret = c;
	c->call (ret, argv, argc);
}

bool DynInstanceObj.delmethod ()
{
	return __class__.as_class->__del__.o != &None;
}

void DynInstanceObj.__release ()
{
	/* This is the __del__ method.  __del__ methods are deferred for a later
	 * point where we are safe to run them, namely boot_pyvm.  This is an important
	 * semantic difference but __del__ methods *are* crippled anyway one way or
	 * another, and they are very special, and may have mindbogling implications.
	 * The win is that we can declare the REFPTR destructor non-throwing, and
	 * consequently, all decrefs non-throwing.  On the other hand it is terribly
	 * easy to ignore exceptions at preempt_pyvm.  So the real reason is that
	 * python designers wouldn't mind the day that python would use real garbage
	 * collection.  If that day came, __del__ methods can be deferred for much later.
	 * So depending on immediate __del__ sideffects is forbiden.  The win for
	 * us is that we don't have to protect our structures in the case each decref
	 * does crappola (for example setslice invokes __del__ which modifies the list).
	 */

	if_likely (!delmethod ()) {
		delete *this;
		return;
	}
	// (we can speed this up by getting class->__del__.o and making a bound method.
	//  but is it worth it?)
	Graveyard.as_list->append (getattr (Interns.__del__));
	DoSched |= SCHED_DEL;
}

/* --------------------------------------------------------------- */
/*	Bound method						   */
/* --------------------------------------------------------------- */

static inline void callBoundMethod (__object__ *method, __object__ *self, REFPTR ret,
				    REFPTR argv [], int args)
{
	int nargs = (args & 255) + ((args >> 7) & 254);
	REFPTR newargs [nargs + 2];
	memcpy (newargs + 2, argv + 1, nargs * sizeof newargs[0]);
	newargs [1].o = self;
	newargs [0].o = method;
	method->call (ret, newargs, args + 1);
}

NEW_ALLOCATOR (DynMethodObj)

DynMethodObj.DynMethodObj (__object__ *self, __object__ *func)
{
	__container__.ctor ();
	__self__.ctor (self);
	__method__.ctor (func);
}

void DynMethodObj.call (REFPTR ret, REFPTR argv [], int args)
{
	callBoundMethod (__method__.o, __self__.o, ret, argv, args);
}

__object__ *DynMethodObj.getattr (__object__ *o)
{
	StringObj *s = StringObj.cast (o);
	if (s->len == 7) {
		if (!memcmp (s->str, "im_self", 7))
			return __self__.o;
		if (!memcmp (s->str, "im_func", 7))
			return __method__.o;
	}
	return s->len == 8 && !memcmp (s->str, "im_class", 8) ?
		(__object__*) __self__.as_inst->__class__.o : __method__->getattr (o);
}

/* --------------------------------------------------------------- */
/*	static method						   */
/* --------------------------------------------------------------- */

DynStaticMethodObj.DynStaticMethodObj (__object__ *o)
{
	__container__.ctor ();
	__callable__.ctor (o);
}

void DynStaticMethodObj.call (REFPTR ret, REFPTR argv [], int argc)
{
	__callable__->call (ret, argv, argc);
}

/* --------------------------------------------------------------- */
/*	class method						   */
/* --------------------------------------------------------------- */

/* explained.
 *   There are two ways to get a classmethod. From a class and from and instance
 *   In the case of a class, class.getattr detects the classmethod and returns
 *    a bound method to cls.
 *   In the case of an instance, to avoid the extra checks, classmethod acts as
 *    a normal method and it becomes bound to the instance.  classmethod.call is
 *    therefore invoked in this case and it converts self to self.__class__
 */
DynClassMethodObj.DynClassMethodObj (__object__ *o)
{
	__container__.ctor ();
	__callable__.ctor (o);
}

void DynClassMethodObj.call (REFPTR ret, REFPTR argv [], int argc)
{
	/*** Has become bound and argv [1]==self ***/
	argv [1].o = DynInstanceObj.cast (argv [1].o)->__class__.o;
	__callable__->call (ret, argv, argc);
}

/* --------------------------------------------------------------- */
/*	named list						   */
/* --------------------------------------------------------------- */

/*
 * The named list uses a list of __slots__ which contains the
 * names of the indexes.  The __slots__ are searched linearly
 * by object identity.  That should work because strings should
 * be interned.  The search time is O(N) for N members, so this
 * is not good for too many slots.  (for near 4/5 slots it's
 * close to the dictionary though, and for less quite faster).
 *
 * From the named list, by setting the __class__ variable
 * we can have "instances of classes with slots", where everything
 * works as above but if an attribute is requested that is not
 * in the slots, we look for it in the class and may return
 * bound methods, etc.
 *
 * Comparison is plain named lists (without a class) is not
 * implemented. It uses comparison by identity.
 */

NamedListObj.NamedListObj (__object__ *cls)
{
	__container__.ctor ();
	__class__.ctor (cls);
	__slots__.ctor (DynClassObj.cast (cls)->__slots__.o);
	int l = __slots__.as_list->len;
	data = seg_malloc (l * sizeof *data);
	for (int i = 0; i < l; i++)
		data [i].ctor ();
}

bool NamedListObj.cmp_EQ (__object__ *o)
{
	if (o == this)
		return true;
	if (__class__.o == &None)
		return false;
	if (!__class__.as_class->has_eq)
		return false;
	__object__ *a = getattr (Interns.__eq__);
	REFPTR xx [2] = { a, o };
	a->call (xx [0], xx, 1);
	if (xx [0].o == &CtxSw)
		xx [0] = preempt_pyvm (CtxSw.vm);
	return xx [0]->Bool ();
}

static int NamedListObj.islot (__object__ *a)
{
	int i, l = __slots__.as_list->len;
	for (i = 0; i < l; i++)
		if (__slots__.as_list->data [i].o == a)
			return i;
	return -1;
}

__object__ *NamedListObj.getattr (__object__ *a)
{
	int s = islot (a);
	if_likely (s != -1)
		return data [s].o;

	if_unlikely (a == Interns.__class__)
		return __class__.o;

	if (__class__.o != &None) {
		__object__ *r;
		if ((r = __class__.as_class->getattr_c (a)) != 0) {
			if_likely (r->vf_flags & VF_BOUND)
				return newDynMethod (this, r);
			if_unlikely (NamespaceObj.isinstance (r))
				return new BoundNamespaceObj (this, r);
			return r;
		}
	}

	return 0;
}

void NamedListObj.setattr (__object__ *a, __object__ *v)
{
	int s = islot (a);
	if_likely (s != -1) {
		data [s] = v;
		return;
	}
	if_unlikely (a == Interns.__class__) {
		DynClassObj *cls = DynClassObj.checkedcast (v);
		Tuplen.enforcetype (cls->__slots__.o);
		__class__ = v;
		__slots__ = cls->__slots__.o;
		return;	
	}
	RaiseNoAttribute (a);
}

void NamedListObj.delattr (__object__ *a)
{
	int s = islot (a);
	if_likely (s != -1) {
		data [s].setNone ();
		return;
	}
	RaiseNoAttribute (a);
}

void NamedListObj.__release ()
{
	if (__class__.o == &None || __class__.as_class->__del__.o == &None) {
		delete *this;
		return;
	}
	Graveyard.as_list->append (getattr (Interns.__del__));
	DoSched |= SCHED_DEL;
}

NamedListObj.~NamedListObj ()
{
	REFPTR *data = data;
	if (data) {
		int i, l = __slots__.as_list->len;
		for (i = 0; i < l; i++)
			data [i].dtor ();
		seg_free (data, l * sizeof *data);
	}
}
