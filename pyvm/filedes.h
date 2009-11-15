/*
 *  Encapsulation of file descriptor operations
 * 
 *  Copyright (c) 2006 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

#ifndef FILEDES
#define FILEDES
/*
 * object orientation of filedescriptor handling
 *
 * Details:
 * - If the mode is readonly, the file is mmaped if possible
 * - The files are opened in non-blocking mode by default.
 */
enum
{
	FD_BAD,
	FD_READ,
	FD_WRITE,
	FD_RW,
	FD_READ_MMAP,
};

__unwind__
class filedes
{
	short int type;
	int fd;
	char *mm_start;
	int len;
    public:
	filedes ()	{ type = FD_BAD; }
	filedes (const char*, int, int=0644, bool=true, bool=true, int=-1);
	filedes (int, int, bool=true);
	filedes (filedes);
	int size ();
	unsigned int read (void*, unsigned int);
	unsigned int write (const void*, unsigned int);
	bool read_all (void*, unsigned int);
	int wait_to_read ();
	void wait_to_write ();
	bool blocked;
	bool seek (int);
	~filedes ();
};

int fs_access (const char*, int);
#endif
