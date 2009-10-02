/*
 *  Implementation of dynamic list object
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

/*
 * Lists expand by doubling their size so at average we are using 3/4 of the list.
 * Not very space efficient. We can shrink lists at GC.
 */

/* >>>>>>>>>>> iterator <<<<<<<<<<< */

static class ListIterObj : iteratorBase, seg_allocd
{
#ifdef FEATURE_SETITER
	const unsigned int vf_flags |= VF_SETITER;
#endif
	int i;
	ListObj *L;
   public:
	ListIterObj (ListObj*);
	__object__ *xnext ();
	int len ()	{ return L->len; }
	void unyield ();
#ifdef FEATURE_SETITER
	void setiter (__object__*);
#endif
};

ListIterObj.ListIterObj (ListObj *x)
{
	iteratorBase.ctor (L = x);
	i = 0;
}

__object__ *ListIterObj.xnext ()
{
	/* have to check listobj every time because mutable */
	if_likely (i < L->len)
		return L->data [i++].o;
	RaiseStopIteration ();
}

void ListIterObj.unyield ()
{
	if (i) --i;
}

#ifdef FEATURE_SETITER
void ListIterObj.setiter (__object__ *o)
{
	L->data [i - 1] = o;
}
#endif

static class ReversedListIterObj : ListIterObj
{
   public:
	ReversedListIterObj (ListObj*);
	__object__ *xnext ();
};

ReversedListIterObj.ReversedListIterObj (ListObj *x)
{
	ListIterObj.ctor (x);
	i = L->len - 1;
}

__object__ *ReversedListIterObj.xnext ()
{
	if_likely (i >= 0)
		return L->data [i--].o;
	RaiseStopIteration ();
}

////////////////////////////////////////

static void ListObj.alloc (int i)
{
	data = (REFPTR*) seg_alloc ((alloc = i) * sizeof *data);
}

static void ListObj.realloc (int i)
{
	int o = alloc * sizeof *data;
	data = (REFPTR*) seg_realloc2 (data, o, (alloc = i) * sizeof *data);
}

static void ListObj.shrink_test ()
{
	if_unlikely (len < alloc / 3)
		realloc (alloc / 2);
}

ListObj.ListObj (__object__ *oarg [...])
{
	if (!oargc) {
		container_sequence.ctor ();
		alloc (1);
		len = 0;
	} else {
		Array_Base.ctor (oargv, oargc);
		alloc = nz (len);
	}
}

void ListObj.refctor (REFPTR parg [...])
{
	Array_Base.refctor (pargv, pargc);
	alloc = nz (len);
}

void ListObj.mvrefarray (REFPTR parg [...])
{
	__container__.ctor ();
	REFPTR *D = data = (REFPTR*) seg_alloc ((alloc = max (len = pargc, 4)) * sizeof *data);
	for (int i = 0; i < pargc; i++)
		D [i].__copyctor (pargv [i].preserve ());
}

void ListObj.__sizector (int x)
{
	container_sequence.ctor ();
	alloc (nz (x));
	len = 0;
}

static void ListObj.memcpyctor (REFPTR *R, int n)
{
	__container__.ctor ();
	alloc (nz (len = n));
	memcpy (data, R, n * sizeof *data);
}

__object__ *ListObj.__xgetslice__ (int start, int length)
{
	return new ListObj refctor (data + start, length);
}

__object__ *ListObj.iter ()
{
	return new ListIterObj (this);
}

__object__ *ListObj.riter ()
{
	return new ReversedListIterObj (this);
}

/* [] */

void ListObj.__xsetitem__ (int i, __object__ *o)
{
	data [i] = o;
}

void ListObj.__xdelitem__ (int i)
{
	int l = len--;
	data [i].dtor ();
	if (l - i - 1)
		memmove (&data [i], &data [1 + i], (l - i - 1) * sizeof *data);
	shrink_test ();
}

/* slicing */

void ListObj.__xdelslice__ (int start, int length)
{
	int l = postfix (len, len -= length), end = start + length;
	for (int i = start; i < end; i++)
		data [i].dtor ();
	if (l - end)
		memmove (&data [start], &data [end], (l - end) * sizeof *data);
	shrink_test ();
}

static void ListObj.realloc2len (int len)
{
	int nalloc = 2 * alloc;
	while (len >= nalloc)
		nalloc *= 2;
	realloc (nalloc);
}

void ListObj.__xsetslice__ (int start, int length, __object__ *x)
{
	int l2 = x->len (), d = length - l2, i;
	if (d > 0) {
		(*this).__xdelslice__ (start + l2, d);
		REFPTR *data = data;
		for (i = 0; i < l2; i++)
			data [i + start] = x->__xgetitem__ (i);
	} else {
		REFPTR *data = data;
		for (i = 0; i < length; i++)
			data [i + start] = x->__xgetitem__ (i);
		if (d < 0) {
			i = len - (start + length);
			if ((len += (d =- d)) >= alloc) {
				realloc2len (len);
				data = this->data;
			}
			memmove (data + start+l2, data + start+length, i * sizeof *data);
			for (i = length; i < l2; i++)
				data [i + start].ctor (x->__xgetitem__ (i));
		}
	}
}

/* append, concat, extend */

__object__ *ListObj.concat (__object__ *o)
{
	extend (o);
	return this;
}

ListObj *ListObj.append (__object__ *o)
{
	if_unlikely (len == alloc)
		realloc (2 * alloc);
	data [len++].ctor (o);
	return this;
}

void ListObj.append_mvref (REFPTR r)
{
	/* List comprehensions move the reference */
	if_unlikely (len == alloc)
		realloc (2 * alloc);
	data [len++].__copyctor (r.preserve ());
}

void ListObj.extcon (ListObj *L)
{
	if (len + L->len >= alloc)
		realloc2len (len + L->len);
	memcpy (data + len, L->data, L->len * sizeof L->data [0]);
	len += L->len;
}

void ListObj.prealloc (int len)
{
	if (len > alloc)
		realloc2len (len);
}

void ListObj.extend (__object__ *o)
{
	if (ListObj.isinstance (o)) {
		/* move the references from one list to the other */
		ListObj *L = ListObj.cast (o);
		extcon (L);
		if (o->refcnt == 1) {
			L->len = 0;
			return;
		}
		for (int i = 0; i < L->len; i++)
			L->data [i].incref ();
		return;
	}
	xsetslice (len, len, o);
}

/* reverse */

ListObj *ListObj.reverse ()
{
	if (len) {
		REFPTR *rv = (REFPTR*) seg_alloc (alloc * sizeof *rv), *data = data;
		for (int i = 0, j = len - 1; j >= 0;)
			rv [j--].o = data [i++].o;
		seg_free (data);
		this->data = rv;
	}

	return this;
}

/* remove */

ListObj *ListObj.remove (__object__ *o)
{
	int i = o->__find_in (data, len);

	if_likely (i >= 0) {
		__xdelitem__ (i);
		return this;
	}

	RaiseIndexError ();
}

/* overlapping memcpy */
static inline void rmemcpy (register void ** __restrict dest,
			    register void ** __restrict src, register int n)
{
	dest += n - 1;
	src += n - 1;
	while (n--)
		*dest-- = *src--;
}

/* insert */

ListObj *ListObj.insert (int i, __object__ *o)
{
	if_unlikely (i < 0) {
		if ((i += len) < 0) i = 0;
	} else if_unlikely (i > len) i = len;

	if_unlikely (len == alloc) {
		// realloc() does one memcopy() and then we do another
		// memmove(). This version avoids realloc() and expands the code
		// so that there is only one memcopy. But this only happens in
		// the case the list expands, which is not very frequent.
#if 0
		REFPTR *data2 = (REFPTR*) seg_alloc (2 * alloc * sizeof *data);
		memcpy (data2, data, i * sizeof *data);
		memcpy (&data2 [i+1], &data [i], (len++ - i) * sizeof *data);
		seg_free (data, alloc * sizeof *data);
		alloc *= 2;
		data = data2;
	} else
#else

		realloc (2 * alloc);
	}
#endif
	rmemcpy ((void**) &data [i + 1], (void**) &data [i], len++ - i);

	data [i].ctor (o);
	return this;
}

/* pop */

__object__ *ListObj.pop (int i)
{
	/* 'pop()' does not shrink the list.  The rationale is:  if we're using
	 * a list as a stack its more efficient to avoid shrinking it.
	 * If some other method for removal is used the list will resize then;
	 * if not, we're either continuing with pops and the list will empty
	 * and then destruct, or we'll use append (push) and we did well not
	 * to shrink.
	 */

	if_likely (i == -1 && len)
		return data [--len].decref ();

	i = abs_index (i);
	__object__ *r = data [i].o;
	int l = len--;

	memmove (&data [i], &data [1 + i], (l - i - 1) * sizeof *data);
	shrink_test ();
	--r->refcnt;	/* a receiver must be there!! */

	return r;
}

/* special */

static void ListObj.fix ()
{
	if (max (len, 1) != alloc)
		realloc (max (len, 1));
}

static __object__ *ListObj.pops (unsigned int n)
{
	if_unlikely (n > len)
		RaiseListIndexOutOfRange ();
	ListObj *L = new ListObj memcpyctor (data + len - n, n);
	len -= n;
	return L;
}

Tuplen *ListObj.list_to_tuple ()
{
	/* This *destroyes* the list!  Use for internal work */
	Tuplen *T = len == alloc ? new Tuplen mvrefctor (data, len) :
		new Tuplen mvrefctor (seg_realloc (data, len * sizeof *data), len);
	data = seg_alloc (4);
	len = 0;
	alloc = 1;
	return T;
}

/* operators */

static void ListObj.concat_ctor (ListObj L1, ListObj L2)
{
	__sizector (L1.len + L2.len);
	Array_Base.concat_ctor (TupleObj.cast (&L1), TupleObj.cast (&L2));
}

__object__ *ListObj.binary_add (__object__ *o)
{
	ListObj *L = ListObj.fcheckedcast (o);
#if 1
	/* may slow down if this list has very few and the other list has too many. See:
	   permsign.py  */
	if_unlikely (this->refcnt == 1 && len > L->len)
		return (*this).concat (o);
#endif
	return new ListObj concat_ctor (this, L);
}

ListObj.~ListObj ()
{
#ifdef	DEBUG_RELEASE
	print ("freeing:", (__object__*) this, NL);
#endif
	REFPTR *data = data;
	for (int i = len - 1; i >= 0; i--)
		data [i].dtor ();
	seg_free (data, alloc * sizeof *data);
}

///// sort /////

extern void dsort (REFPTR[], int);

ListObj *ListObj.sort ()
{
	dsort (data, len);
	return this;
}
