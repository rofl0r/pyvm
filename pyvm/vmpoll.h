/*
 *  Implementations of notifiers based on the poller library
 * 
 *  Copyright (c) 2006 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

class NotifyObj
{
	inline virtual const;
	Task *T;
	void set_retval (__object__*);
    public:
	NotifyObj ();
virtual bool do_notify (int);
virtual ~NotifyObj () {}
};

extern __object__ *vmpollin (NotifyObj*, int, int);
extern __object__ *vmpollout (NotifyObj*, int, int);
extern void vmrepollin (NotifyObj*, int, int);
