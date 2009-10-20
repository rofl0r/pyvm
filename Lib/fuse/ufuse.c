/*
	FUSE in userspace ^ 2

	A generic filesystem that is exported through a set of pipes.
	Usage:

		./ufuse [fuse-arguments] <wfifo> <rfifo> <bflags>

	`wfifo` and `rfifo` are two named pipes (see mkfifo).
	on `wfifo` this program writes requests and on `rfifo` it
	read replies.
	`bflags` is a hex number that states which methods are
	implemented by the external application.
*/

#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

static FILE *commw, *commr;
static int Debug;
static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
#define LOCK pthread_mutex_lock (&mut);
#define UNLOCK pthread_mutex_unlock (&mut);

static int WBYTE (char c)
{
	return fputc (c, commw) != -1;
}

static int WINT (int i)
{
	return fwrite (&i, 4, 1, commw) == 1;
}

static int WPATH (const char *path)
{
	int l = strlen (path);
	return WINT (l) && fwrite (path, 1, l, commw) == l;
}

static int WBUF (const char *buf, int len)
{
	return fwrite (buf, 1, len, commw) == len;
}

static int RINT ()
{
	int i;
	if (fread (&i, 4, 1, commr) != 1)
		return -1;
	return i;
}

static int RBUF (void *buf, int size)
{
	return fread (buf, 1, size, commr) == size;
}

static int DO ()
{
	fflush (commw);
	return RINT ();
}

static int ufuse_getattr(const char *path, struct stat *stbuf)
{
if (Debug)
fprintf (stderr, "c %s %i\n", __func__, sizeof *stbuf);
	LOCK
	int res = 0;
	if (!WBYTE (1)) {
		fprintf (stderr, "phaked\n");
	}
	WPATH (path);
	res = DO ();
	if (res < 0)
		goto out;
	RBUF (stbuf, sizeof *stbuf);
out:
	UNLOCK
	return res;
}

static char buffer [1024 * 10];

static int ufuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
if (Debug)
fprintf (stderr, "c %s\n", __func__);
//return -ENOENT;
	LOCK
	int nentries, i, res;
	int len;
	WBYTE (2);
	WPATH (path);
	res = DO ();
	if (res < 0) {
		UNLOCK
		return res;
	}
	nentries = RINT ();
	int want = 1;
	for (i = 0; i < nentries; i++) {
		len = RINT ();
		RBUF (buffer, len);
		if (want)
			if (filler (buf, buffer, NULL, 0))
				want = 0;
	}
	UNLOCK
	return 0;
}

static int ufuse_open(const char *path, struct fuse_file_info *fi)
{
if (Debug)
fprintf (stderr, "c %s\n", __func__);
	LOCK
	WBYTE (3);
	WINT (fi->flags);
	WPATH (path);
	int res = DO ();
	UNLOCK
	return res;
}

static int ufuse_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
if (Debug)
fprintf (stderr, "============================================c %s\n", __func__);
	LOCK
	int res;
	WBYTE (4);
	WINT (offset);
	WINT (size);
	WPATH (path);
	res = DO ();
	if (res < 0) {
		UNLOCK
		return res;
	}
	size = RINT ();
	RBUF (buf, size);
	UNLOCK
	return size;
}

static int ufuse_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
if (Debug)
fprintf (stderr, "c %s\n", __func__);
	LOCK
	WBYTE (5);
	WINT (offset);
	WINT (size);
	WPATH (path);
	WBUF (buf, size);
	int res = DO ();
	UNLOCK
	return res;
}

static int ufuse_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
if (Debug)
fprintf (stderr, "c %s mode=%x\n", __func__, mode);
	LOCK
	WBYTE (6);
	WINT (mode);
	WPATH (path);
	int res = DO ();
	UNLOCK
	return res;
}

static int ufuse_truncate(const char *path, off_t size)
{
if (Debug)
fprintf (stderr, "c %s\n", __func__);
	LOCK
	WBYTE (7);
	WINT (size);
	WPATH (path);
	int res = DO ();
	UNLOCK
	return res;
}

static int ufuse_mkdir(const char *path, mode_t mode)
{
if (Debug)
fprintf (stderr, "c %s mode=%x\n", __func__, mode);
	LOCK
	WBYTE (8);
	WINT (mode);
	WPATH (path);
	int res = DO ();
	UNLOCK
	return res;
}

static ufuse_chmod (const char *path, mode_t mode)
{
if (Debug)
fprintf (stderr, "c %s mode=%x\n", __func__, mode);
	LOCK
	WBYTE (9);
	WINT (mode);
	WPATH (path);
	int res = DO ();
	UNLOCK
	return res;
}

struct fuse_operations ufuse_oper;

int main(int argc, char *argv[])
{
	int bf = strtol (argv [argc - 1], 0, 16);
	char *chanr = argv [argc - 2];
	char *chanw = argv [argc - 3];
	argc -= 3;

#define IFBF(x) if (bf & (1<<x))
	IFBF(0)  ufuse_oper.getattr	= ufuse_getattr;
	IFBF(3)  ufuse_oper.readdir	= ufuse_readdir;
	IFBF(5)  ufuse_oper.mkdir	= ufuse_mkdir;
	IFBF(11) ufuse_oper.chmod	= ufuse_chmod;
	IFBF(13) ufuse_oper.truncate	= ufuse_truncate;
	IFBF(15) ufuse_oper.open	= ufuse_open;
	IFBF(16) ufuse_oper.read	= ufuse_read;
	IFBF(17) ufuse_oper.write	= ufuse_write;
	IFBF(21) ufuse_oper.create	= ufuse_create;
	IFBF(30) Debug = 1;

	commw = fopen (chanw, "w");
	commr = fopen (chanr, "r");
	if (!commw || !commr) {
		fprintf (stderr, "Not able to open channels!\n");
		return -1;
	}

	return fuse_main(argc, argv, &ufuse_oper, NULL);
}
