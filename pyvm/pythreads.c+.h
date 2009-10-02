/*
 *  Stackless tasks
 * 
 *  Copyright (c) 2006-2008 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

/*
 * py-threads.
 * these are the "co-routines" also known as pyvm Tasks
 *
 * Each task has its vm context and if it is part of
 * the R-list (RFirst, RLast, RCurrent), it will be
 * executed sooner or later by the boot_pyvm scheduler.
 *
 * Allowing other threads to run because of blocking
 * is basically a matter of the thread.c+ file which
 * has real threads managed by the OS.  But it still
 * has to pass through here to remove Tasks from the
 * R-list.
 */

Task *RF, *RL, *RC, *RALL;

void Task.move_running ()
{
	/*
	 * XXXX: To be fair, we should add the task *before* the
	 * currently running task.
	 */
	if (!RL) {
		RF = RL = this;
		make_current ();
		prev = next = 0;
	} else {
		next = 0;
		prev = RL;
		RL = RL->next = this;
	}

	state = TH_RUNNING;
}

Task *Task.move_blocked ()
{
	if (RC == this) {
		Task *n = next ?: RF;
		if (n == this) RC = 0;
		else n->make_current ();
	}

	if (next) next->prev = prev;
	else RL = prev;
	if (prev) prev->next = next;
	else RF = next;
	state = TH_SOFTBLOCK;
	return this;
}

Task.Task (vm_context *v, int id, Task *T)
{
	ID = id;
	ParentID = T ? T->ID : 0;
	if (!RF) {
		RF = RL = this;
		prev = next = 0;
	} else {
		prev = 0;
		next = RF;
		RF = RF->prev = this;
	}
	state = TH_RUNNING;
	preemptive = 0;
	vm = v;
	poller = 0;
	LOCKER.ctor ();
	INTERRUPT.ctor ();
	if (T) STDOUT.ctor (T->STDOUT.o);
	else STDOUT.ctor (stdoutObj);

	if ((_next = RALL))
		_next->_prev = this;
	RALL = this;
	_prev = 0;
	pctx = 0;
}

Task.~Task () noinline
{
	if (RC == this) {
		if ((next ?: RF) == this) RC = 0;
		else (next ?: RF)->make_current (); 
	}
	if (next) next->prev = prev;
	else RL = prev;
	if (prev) prev->next = next;
	else RF = next;
	if (_next) _next->_prev = _prev;
	if (_prev) _prev->_next = _next;
	else RALL = _next;
}

/*----------------------------------------------------------------------------*/

extern void set_stdout (__object__*);

void Task.make_current ()
{
	RC = this;
//	set_stdout (STDOUT.o);
}

static void Task.take_over ()
{
	/* The current context must have been saved for this to work.
	 * this is called from end_allow_threads where the context is
	 * saved from voluntarily context switch thanks to have_pending()
	 * or the current thread is the self which never blocked.
	 */
	move_running ();
	make_current ();
	pvm = vm;
	SET_CONTEXT
}

extern uint ThreadPID ();

void *py_begin_allow_threads ()
{
	if (!multitasking)
		return 0;
	SAVE_CONTEXT
	PROFILE_STOP_TIMING
	RC->vm = pvm;
	Task *r = RC->move_blocked ();
	/*++ must protect lwc-unwind stacktop ++*/
	/*++ probably the ONLY thing that's global and thus shared by all threads ++*/
#ifndef CPPUNWIND
	r->_dta = __lwcbuiltin_get_estack ();
#endif

	/* That's not really needed. Only for builtins.thread_status () */
	r->PID = ThreadPID ();
	r->state = TH_BLOCKED;
	/* And other similar debugging. Maybe we'll need that for callback/GIL issue */

	release_the_gil (RC != 0);			// ----> thread.c+
	return r;
}

void py_end_allow_threads (void *v)
{
	if (!v) return;
	Task *t = (Task*) v;
	acquire_the_gil ();			// ----> thread.c+
#ifndef CPPUNWIND
	__lwcbuiltin_set_estack (t->_dta);
#endif
	/* normally end_allow() pairs with begin_allow().
	 * It is possible when the user presses Ctrl-C that the main
	 * thread which was blocking, was put back to RUNNING state
	 * in order to go on with a SystemExit.  */
	if (t->state == TH_BLOCKED) {
		t->take_over ();
		PROFILE_START_TIMING
	}
}
