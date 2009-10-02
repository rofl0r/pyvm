/*
 *  Garbage collection
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

/*
 * In python the role of GC is to handle cyclic references.
 * Reference counting is superior to garbage collection in
 * almost every aspect * except cyclic references. So with this GC,
 * python has cyclic resistance reference counting memory management.
 *
 * (automatic gc has one advantage over reference counting, and that
 * is that, if applied on a C/C++ program, the programmer doesn't
 * have to take care of writting any code to manage the memory
 * allocations and this results in less bugs.  However, pyvm is
 * different.  There is only one piece of code that does memory
 * managment and that is the object destructor.  No matter how much
 * more py-code we add to the library, the allocation/deallocation
 * code in C remains the same.  So for a higher level vm where
 * memory managment is transparent to the python programmer, this
 * advantage of automatic GC is eliminated.  OTOH, refcounting has
 * the very significant advantage that it's deterministic and
 * resources (including files/inodes) are released asap.
 * Specific test cases where automatic GC performs better can
 * be constructed but with automatic GC there is the possibility
 * of cases that it is real bad (for example either walk needlessly
 * a huge object space, or when memory expands a lot in a very
 * short time before GC kics in). So unexpectedly bad things 
 * can happen and this is not good, especially with pyvm which
 * has a great variety of algorithms with completely different
 * memory allocation behaviours)
 *
 * The current collection works by trying to break cycles.
 * First we traverse the object space.  Then we walk the list of
 * containers looking for objects that weren't marked reachable.
 * When such an object is encountered, we call its __clean function
 * which is responsible for empyting the object: that is throw away
 * "all" the things contained in the container.  That should eventually
 * break cycles and consequently unreachable objects should start to
 * disappear from the containers list.
 * "all" tries to be somewhat intelligent.  For instance, in the case
 * of an instance, we empty the attribute __dict__ but we do not empty
 * the __class__ slot.  If the circle is between class and instance, then
 * the unreachable class should also be on the container list and the
 * instance will be released when the class's __clean empties the class's
 * __dict__.
 *
 * There can be weird cases that escape the collection.
 * On the other hand circular references do happen rather often when
 * we try to optimize attribute access.  So the point is to catch
 * all normal cases of circular references which *should* be allowed.
 *
 * ``A clever adversary could prevent objects from being collected''
 * -- but the programmer is not our "enemy" here so we have no interest
 * to "protect" our vm from such incidents.
 *
 * __del__ methods: This approach cannot break the cycles of instances
 * with __del__ methods.  It detects instances with __del__ methods that
 * are causing cyclic references, and they will be put on the lost+found
 * list. Really, the program is not responsible for handling these.
 *
 * Also note that __del__ zombies are not called during GC. They are
 * normally placed in the graveyard and will be invoked later by the
 * scheduler.
 *
 * [[ There is a way to attach __del__ action to objects with cyclic
 *    references programatically from python by attaching a new object
 *    with a __del__ method to the object with the cyclic references.
 *    When the latter is freed by GC, it will result in the former
 *    being released normally ]]
 *
 */

REFPTR lost_n_found;

#ifdef	DEBUG_GC
int _debug_traverse = 1;
#endif

extern void gc_interns ();
extern void reset_GC_params ();
extern void gc_weakrefs ();

void weakref_collect ()
{
	gc_interns ();
	gc_weakrefs ();
//	sys_modules.as_dict->D.clean_weak_vals ();
}

extern int gc_enabled;

void GC_collect (REFPTR root)
{
	if (!gc_enabled)
		return;

	if (0) {
		fprintf (stderr, COLS"*"COLE COLB"GARBAGE COLLECTION"COLE"\n");
//		whereami ();
	}

	__container__ *c, *p;
	for (c = GCFirst; c; c = c->next)
		c->sticky &= ~STICKY_TRAVERSE;

	// traverse object space and mark all reachable objects with the
	// STICKY_TRAVERSE bit
	init_vv ();
	root.traverse_ref ();
	restore_vv ();

	// unreachable instances with __del__ method:
	//  We can't do __clean() on them because if we __clean() the instance's
	//  __dict__ dictionary, that will break the code of the __del__ function
	//  which very possibly uses attributes.
	//
	// There are two cases:
	//  the instance is part of the cycle (must be __cleaned to break the cycle)
	//  the instance is a leaf node in a cycle (will be freed as a result of cycle breaking)
	//
	// So, the way to handle this is to put those instances on a special list and
	// traverse them. Later, scan this list and:
	//  if an object has a refcount of '1' it is leaf and we are OK.
	//  else it is a cycle. We'll just put this on lost+found.
	//
	REFPTR LL = new ListObj ();
	LL.as_list->sticky |= STICKY_TRAVERSE;
	for (c = GCFirst; c; c = c->next)
		if_unlikely (!(c->sticky & STICKY_TRAVERSE)
		&& DynInstanceObj.isinstance (c) && DynInstanceObj.cast (c)->delmethod ())
			LL.as_list->append (c);
	if (LL.as_list->len)
		traverse_array (LL.as_list->data, LL.as_list->len);

	// walk the linked list of all the GC-able objects (containers)
	// take objects that where not traversed out of the linked-list
	// and place them in the ALTList. All containers have two pointers
	// "next" and "prev" which are -by default- used to store the
	// object in the GC list.  Now, these pointers are used for the
	// ALTList, aka "the unreachables"
	char ALTFirst [sizeof (__container__) + 8];
	__container__ *ALTList = (__container__*) ALTFirst;
	ALTList->next = 0;

	c = GCFirst;
	while (c)
		if_likely (c->sticky & STICKY_TRAVERSE)
			c = c->next;
		else {
			p = c->next;
			c->__remove_from_GC ();

			c->prev = ALTList;
			if ((c->next = ALTList->next))
				ALTList->next->prev = c;
			ALTList->next = c;
			c = p;
		}


	// Now we walk the ALTList and call __clean() on its objects.
	// the result of __clean may be that objects are released and
	// thus are removed from the ALTList (because they believe that
	// they are removing themselves from the GCList).
	// For that reason, objects are stored into a second linked list
	// ALT2List aka "should be gone eventually".
	// We keep doing that as long as there are objects on the ALTList.
	// Note that calling __clean () may remove multiple objects from
	// the ALTList and/or ALT2List.
	char ALT2First [sizeof (__container__) + 32];
	__container__ *ALT2List = (__container__*) ALT2First;
	ALT2List->next = 0;

	while (ALTList->next) {
		c = ALTList->next;
		if (!(c->sticky & STICKY_TRAVERSE)) {
			c->sticky |= STICKY_TRAVERSE;

			// remove from the ALTList
			if (c->next) c->next->prev = c->prev;
			c->prev->next = c->next;

			// insert into ALT2List
			c->prev = ALT2List;
			if ((c->next = ALT2List->next))
				ALT2List->next->prev = c;
			ALT2List->next = c;

			c->__clean ();
		}
	}

	// The ALT2List should be eventually empty.

	// WE HAVE LEFTOVERS means that either we leak references (bad)
	// / or that we have failed to traverse EVERYTHING (not very bad)
	// / or that our __clean functions are unable to break the cycles.
	if (ALT2List->next) {
		pprint ("GC: we have leftovers");
		for (c = ALT2List->next; c; c = c->next)
			lost_n_found.as_list->append (c);
	}

	// now see the __del__ methods
	if (LL.as_list->len)
		for (int i = 0; i < LL.as_list->len; i++)
			if (LL.as_list->data [i].o->refcnt > 1) {
				pprint ("GC: cycle with __del__ method!");
				lost_n_found.as_list->append (LL.as_list->data [i].o);
			}

	//
	if (lost_n_found.as_list->len) {
		pprint ("GC: There is stuff in sys.'lost+found'");
		pprint ("::", lost_n_found.o);
	}
	reset_GC_params ();
}

/*
 */

static inline trv void traverse_array (REFPTR *x, int n)
{
	while (n--)
		x++->traverse_ref ();
}

static Task *gcTask;
extern REFPTR current_stdout;

extern void traverse_exceptions ();

/* Singleton used to start traversal of the vm contexts and all the co-routines */
void ContextSwitchObj.traverse ()
{
	_tb.traverse_ref ();
#ifdef	DMCACHE
	DMcache.traverse_ref ();
#endif
	gcTask = RC;
	pvm->traverse_vm ();	/* redundant? */

	for (gcTask = RALL; gcTask; gcTask = gcTask->_next)
		gcTask->traverse_task ();
	/* Independent roots */
	devnull.traverse_ref ();
	Graveyard.traverse_ref ();
	traverse_exceptions ();
	current_stdout.traverse_ref ();
}

/*
 * if vm_contexts where objects we would ensure no re-traversal
 * with the generic traverse_ref() function.  Now we have to keep
 * a list of visited vm_contexts, use their 'cloned' field and then
 * restore its old value.
 */

static void **vv;
static int vvi, vva;

static void init_vv ()
{
	vv = __malloc ((vva = 256) * sizeof *vv);
	vvi = 0;
}

static void restore_vv ()
{
	for (int i=0; i < vvi; i++)
		((vm_context*) vv[i])->cloned &= ~(2|4);
	__free (vv);
}

static inline bool valid_ctx (vm_context *v)
{
	/* Note: to make the vm faster.
	 */
	return vm_context_allocator.is_allocated (v);
}

void vm_context.traverse_vm (int active=1)
{
	/* Traversing vm_contexts.
	 * There are two cases where we will have to traverse a context (stack+locals).
	 * (1) when we are in the active call chain. In this case we also traverse the
	 * vm of the 'caller' context down to 0. This starts from the 'pvm' current context.
	 * (2) when we have a generator object. A generator is nothing more than a frozen
	 * vm context. However in the case of generator we don't traverse the vm of the
	 * caller because a caller may not exist. We can traverse upwards to vm contexts
	 * of generators that are found in the stack/local.
	 *
	 * To understand this better. Currently python has 'distance 1' generators;
	 * where a call flow in a signle threaded program normally goes:
	 *	main() --> foo() --> bar() --> zoo ()
	 * The addition of generators to the language allows us to do things like:
	 *	main() --> foo() --> bar() --> zoo()
	 *                  ^                   ^
	 *                  |-generator1()      |-generator2 () --> f ()
	 *                        ^
	 *                        |-generator3()
	 *
	 */
	if (!valid_ctx (this))
		return;

	int vbit = 1 << (1+active);
	if (cloned & vbit) return;
	if (FUNC.o == &None) {
		if (this != pree) return;
		if (!gcTask || !gcTask->pctx || !gcTask->pctx->v) return;

		/*
		 * using the preempt-link
		 */
#if 1
		preempt_link *pctx = gcTask->pctx;
		gcTask->pctx = pctx->outer;
		pctx->v->traverse_vm ();
		gcTask->pctx = pctx;
#else
		gcTask->pctx->v->traverse_vm ();
#endif
		return;
	}

	if_unlikely (vvi == vva)
		vv = __realloc (vv, (vva *= 2) * sizeof *vv);
	vv[vvi++] = this;

	cloned |= vbit;
	S.traverse_stack ();
	traverse_array (fastlocals, FUNC.as_func->nfast);
	FUNC.traverse_ref ();
	if (active && caller) caller->traverse_vm ();
}

void exec_stack.traverse_stack ()
{
	traverse_array (STACK, maxTOS);
}

void Task.traverse_task ()
{
	if (this != RC)
		vm->traverse_vm ();
	LOCKER		.traverse_ref ();
	STDOUT		.traverse_ref ();
	INTERRUPT	.traverse_ref ();
}

void NamespaceObj.traverse ()
{
	__dict__	.traverse_ref ();
}

void DynInstanceObj.traverse ()
{
	__dict__	.traverse_ref ();
	__class__	.traverse_ref ();
}

void DynClassObj.traverse ()
{
	__dict__	.traverse_ref ();
	__bases__	.traverse_ref ();
	__getattr__	.traverse_ref ();
	__setattr__	.traverse_ref ();
	__del__		.traverse_ref ();
	__slots__	.traverse_ref ();
}

void DynMethodObj.traverse ()
{
	__self__	.traverse_ref ();
	__method__	.traverse_ref ();
}

void DynStaticMethodObj.traverse ()
{
	__callable__	.traverse_ref ();
}

void DynClassMethodObj.traverse ()
{
	__callable__	.traverse_ref ();
}

void PyCodeObj.traverse ()
{
	consts		.traverse_ref ();
	names		.traverse_ref ();
	varnames	.ptraverse_ref ();
	freevars	.traverse_ref ();
	cellvars	.traverse_ref ();
	module		.traverse_ref ();
}

void PyFuncObj.traverse ()
{
	codeobj		.traverse_ref ();
	LOCALS		.traverse_ref ();
	GLOBALS		.traverse_ref ();
	closures	.traverse_ref ();
}

void PyFuncObj_dflt.traverse ()
{
	PyFuncObj	.traverse ();
	default_args	.traverse_ref ();
}

void PyGeneratorFuncObj.traverse ()
{
	GTOR		.traverse_ref ();
}

void PyGeneratorObj.traverse ()
{
	vm->traverse_vm (0);	/* inactive */
}

void TupleObj.traverse ()
{
#ifdef	TUPLEVIEW
	if (Tuplen.isinstance ((__object__*) this) && Tuplen.cast ((__object__*)this)->tview)
		(*(REFPTR*)&Tuplen.cast ((__object__*)this)->tview).traverse_ref ();
	else
#endif
	traverse_array (data, len);
}

void iteratorBase.traverse ()
{
	obj.traverse_ref ();
}

void dictionary.traverse_dict ()
{
	test_resize ();
	dictEntry *tbl = tbl;

	if (dictionary.IsType (this)) {
		for (long i = used; i > 0; tbl++)
			if (tbl->val.o) {
				tbl->key.traverse_ref ();
				tbl->val.traverse_ref ();
				--i;
			}
	} else
		for (long i = used; i > 0; tbl++)
			if (tbl->val.o) {
				tbl->val.traverse_ref ();
				--i;
			}
}

void dictionary.traverse_set ()
{
	test_resize ();
	dictEntry *tbl = tbl;

	if (dictionary.IsType (this))
		for (long i = used; i > 0; tbl++)
			if (tbl->val.o)
				tbl->key.traverse_ref (), --i;
}

void DictObj.traverse ()
{
	D.traverse_dict ();
}

void SetObj.traverse ()
{
	D.traverse_set ();
}

void DictIterItemsObj.traverse ()
{
	tp.traverse_ref ();
	DictIterObj.traverse ();
}

void NamedListObj.traverse ()
{
	__class__.traverse_ref ();
	__slots__.traverse_ref ();
	traverse_array (data, __slots__.as_list->len);
}

/*
 * The __clean method.  It exists for containers and it will empty
 * the container from all the data it references.  Releasing some
 * data may very possibly do a circle and result in the release of
 * the container, so we keep the data in temporary storage.
 */ 

void __container__.__clean ()
{ }

void NamespaceObj.__clean ()
{
	/* __dict__ is also on the list, so this is redundant */
	__dict__.as_dict->__clean ();
}

void Tuplen.__clean ()
{
	/* That would be needed if we had a tuple directly containing itself.
	 * But since that is impossible, a circular reference will have to go
	 * through a some other container.  So we'll leave to the __clean of others
	 * to release unreachable tuples
	 */
}

void ListObj.__clean ()
{
	REFPTR *_data = data;
	int _len = len, _alloc = alloc;

	data = seg_alloc ((alloc = len = 1) * sizeof *data);
	data [0].ctor ();

	/* cannot use 'this' any more! */
	for (int i = 0; i < _len; i++)
		_data [i].dtor ();
	seg_free (_data, _alloc * sizeof *data);
}

void BoundNamespaceObj.traverse ()
{
	__dict__.traverse_ref ();
	__inst__.traverse_ref ();
}

void BoundNamespaceObj.__clean ()
{
//	REFPTR inst __movector (__inst__);
	__dict__.breakref ();
}

static dictionary.dictionary (dictionary *D)
{
	memcpy (this, D, sizeof *D);
	if (D->tbl == D->smalltbl)
		tbl = smalltbl;
}

void DictObj.__clean ()
{
	dictionary _D (&D);
	D.ctor (0);
}

void DynMethodObj.__clean ()
{
	REFPTR _self __movector (__self__);
//	__method__.breakref ();
}

void DynInstanceObj.__clean ()
{
	if (delmethod ())
		/* actually this is wrong! We should: scan the list of
		 * unreachable objects for instances with __del__ methods,
		 * add those instances to the graveyard and retraverse them
		 * so everything they reference becomes reachable. That should
		 * be done before we get here. XXXXXX
		 */
		return;

	/* don't release __class__, because it's needed in order to lookup __del__
	   circular references of the form : 'class -> instance -> class' will be
	   broken when we __clean the class (if unreachable)  */

	/* ACTUALLY: If the instance is unreachable, so will its __dict__.
	 * So there is no need to clean the dict. Maybe we should put the
	 * dict out of the to-clean list if we have delmethod instead.
	 */
//pprint ("garbage collected instance:", (void*) this, __class__.o);
//	__dict__.c->__clean ();

// ZZZZ: NEWS: I think that is now fixed. We don't ever clean something with a __del__
// destructor. So we can safely call __dict__.c->__clean()
// test on first oportunity.
}

void PyCodeObj.__clean ()
{
	REFPTR _MoDuLe __movector (module);
	consts.breakref ();
}

void SetObj.__clean ()
{
	dictionary _D (&D);
	D.ctor (0);
}

void PyFuncObj.__clean ()
{
	if (codeobj.as_code->nclosure && codeobj.as_code->self_closure != -1) {
		REFPTR tmp = (__object__*) this;
		closures.as_tuplen->data [codeobj.as_code->self_closure] = &None;
		if (tmp->refcnt == 1)
			return;
	}
	GLOBALS.breakref ();
}

void PyFuncObj_dflt.__clean ()
{
	REFPTR _dflt __movector (default_args);
	PyFuncObj.__clean ();
}

void DynClassObj.__clean ()
{
	__dict__.c->__clean ();
}

void NamedListObj.__clean ()
{
	REFPTR *d = data;
	int l = __slots__.as_list->len;

	data = 0;
	for (int i = 0; i < l; i++)
		d [i].dtor ();
	seg_free (d);
	// we don't break __slots__ and __class__. we should but unlikely to be needed.
}
