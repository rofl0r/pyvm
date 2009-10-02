/*
 *  Stackless vm threading
 * 
 *  Copyright (c) 2006 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

#define TAKE_OVER (void*)1

extern int nthreads;
extern bool multitasking;

bool fork_thread ();
void release_the_gil (bool);
void acquire_the_gil ();
void main_thread ();
bool have_pending ();
bool am_GIL_owner ();
