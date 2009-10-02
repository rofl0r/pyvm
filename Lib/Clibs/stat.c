/* fill the 'struct stat' thing, mainly for use by FUSE
   The problem with this structure is that its exact size
   is not known.  For one it depends on _FILE_OFFSET_BITS.

   (maybe since FUSE is a linux module and pyvm works only
    for x86 systems, we could assume some things. at the
    moment this is better wrapped here)
*/
#define _FILE_OFFSET_BITS 64

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

int sizeof_stat ()
{
	return sizeof (struct stat);
}

void make_stat (struct stat *stb, int mode, int size, int nlink, int atime, int mtime, int ctime, int uid, int gid)
{
	memset (stb, 0, sizeof *stb);
	stb->st_mode = mode;
	stb->st_size = size;
	stb->st_nlink = nlink;
	stb->st_atime = atime;
	stb->st_mtime = mtime;
	stb->st_ctime = ctime;
	stb->st_uid = uid;
	stb->st_gid = gid;
}

void make_stat64 (struct stat *stb, int mode, unsigned long long *size, int nlink, int atime, int mtime, int ctime, int uid, int gid)
{
	memset (stb, 0, sizeof *stb);
	stb->st_mode = mode;
	stb->st_size = *size;
	stb->st_nlink = nlink;
	stb->st_atime = atime;
	stb->st_mtime = mtime;
	stb->st_ctime = ctime;
	stb->st_uid = uid;
	stb->st_gid = gid;
}

int myuid ()
{
	return getuid ();
}

int eacces = EACCES;
int enoent = ENOENT;

const char ABI [] =
"i sizeof_stat -\n"
"- make_stat siiiiiiii\n"
"- make_stat64 sisiiiiii\n"
"i myuid -\n"
"i eacces\n"
"i enoent\n"
;
