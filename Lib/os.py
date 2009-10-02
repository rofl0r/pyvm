##  os module
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) version 3 of the License.

#
# Minimal os module for bootstrapping without python
# because this is part of the bootstrap, it should not import any other
# non-builtin modules unless completely necessary.
#
# only posixpath
#

# This module should, slowly replace python's os.  New extensions
# are allowed.  For example access() default mode is F_OK if unspecified, etc.

from _posix import getcwd, remove, access as _access, F_OK, R_OK, listdir, unlink, environ
from _posix import __exec, mkdir, symlink, chdir, readlink, touch, rename, chmod
from _posix import write, openpty, read, close, waitpid, open as openfd, getpid
from _posix import O_RDONLY, O_WRONLY, mkfifo, statfs as _statfs, rmdir, pipe, O_RDWR
from posix import stat, lstat

curdir = "."
pardir = ".."

def normabspath (p):
	if p == '':
		return '.'
	P = []
	for i in p.split ('/'):
		if not i or i == '.':
			continue
		if i == '..' and P:
			P.pop ()
		else:
			P.append (i)
	return '/' + '/'.join (P)

def abspath (p):
	if not p.sw ('/'):
		p = getcwd () + '/' + p
	return normabspath (p)

def dirname (p):
	h, s, t = p.rpartition ('/')
	if s:
		return h
	return ''

def basename (p):
	h, s, t = p.rpartition ('/')
	if s:
		return t
	return p

def listdir_fullpath (p):
	if p [-1] != '/':
		p += '/'
	return [p+x for x in listdir (p)]

def path_isdir (p):
	try:
		return stat (p).S_ISDIR
	except:
		return False

def path_islink (p):
	try:
		return lstat (p).S_ISLNK
	except:
		return False

def path_isfile (p):
	try:
		return lstat (p).S_ISREG
	except:
		return False

def getmtime (p):
	return stat (p).st_mtime

def getctime (p):
	return stat (p).st_ctime

def getsize (p):
	return stat (p).st_size

# environment variables:
#  the use of environment variables is for passing them to exec*d external programs
#  with calls like system(), etc. For that we do not use the libc functions getenv()
#  putenv(), etc.  Upon the initialization of the VM, a dictionary is created from
#  the `environ` variable.  After that, the calls getenv/putenv from *python* code
#  do NOT modify `environ`.  Only the dictionary is modified and this is passed down
#  to exec.
# Consequence:
#  the situation where we set an environment variable so that this will be detected
#  by some linked library to the vm does not work. The linked library will search
#  `evrinon`.  However, this is a bad use case for environment variables; should a
#  library want something, it should request it with a proper API.
# Advantage:
#  we don't use libc's getenv/putenv/unsetenv. less code to export.
#  these things are also hairy. we had some bugs starting embedded pyvm applications
#  with the remote pyvm fb because setting PYVM_DISPLAY and then calling the exec
#  didn't work as expected.

def getenv (name):
	try:
		return environ [name]
	except:
		return None

def putenv (name, value):
	if value is not None:
		environ [name] = value
	else:
		try:
			del environ [name]
		except:
			pass

##
## mini subprocess module.
##  It can be used instead of system() or to start off background servers
##  and run programs on a pseudo-tty for xterm, etc.  It does not use
##  the /bin/sh thing.
##

def system (command):
	return execbin ("/bin/sh", "-c", command)

def execbin (program, *args):
	return execute (program, args, wait=True)

def execbin_bg (program, *args):
	return execute (program, args, wait=False)

def execbin_bg_quiet (program, *args):
	# without stdout
	return execute (program, args, wait=False, infd=openfd ("/dev/null", O_WRONLY, 0))

def execpty (pty, onDone, program, *args):
	return execute (program, args, ptyfd=pty, wait=False, onDone=onDone)

def execbin_popenr_bg (program, *args):
	# return a file descriptor that should be read to get program's stdout/stderr.
	# If the data on the rfd are not consumed the application will block!
	# The data is read with the function "os.read (fd, size)"
	rfd, wfd = pipe ()
	try:
		execute (program, args, wait=False, infd=wfd)
	except:
		close (rfd)
		close (wfd)
		raise
	close (wfd)
	return rfd

PATHCACHE = {}
def findexe (progname):
	if progname in PATHCACHE:
		return PATHCACHE [progname]
	path = getenv ("PATH") or ":/bin:/usr/bin"
	for p in path.split (":"):
		if access (p + "/" + progname):
			rez = p + "/" + progname
			PATHCACHE [progname] = rez
			return rez

def execute (progname, argv, env=None, ptyfd=-1, wait=False, onDone=None, infd=-1):
	if env is None:
		env = environ
	if "/" not in progname:
		path = findexe (progname)
		if not path:
			raise Error ("exec: program [%s] not found" %progname)
	else:
		path = progname
	envz = ["%s=%s" %(k, v) for k, v in env.items ()]
	argv = [progname] + list (argv)
	pid = __exec (path, argv, envz, ptyfd, infd)
	if wait:
		# block until subprocess is finished
		return waitpid (pid)
	else:
		# don't wait but keep a background thread
		# to waitpid to avoid the zombie. onDone
		# can be used to notify the rest of the program
		# for done child.
		def bgwait ():
			waitpid (pid)
			if onDone:
				onDone ()
		import thread
		thread.start_new (bgwait)
		return pid

####################################################################

path = namespace ()
path.dirname = dirname
path.basename = basename
path.abspath = abspath
path.islink = path_islink
path.isdir = path_isdir
path.isfile = path_isfile
path.getmtime = getmtime
path.getsize = getsize
path.touch = touch

def join (a, *p):
	path = a
	for b in p:
		if b.startswith('/'):
			path = b
		elif path == '' or path.endswith('/'):
			path +=  b
		else:
			path += '/' + b
	return path

path.join = join
del join

def access (f, mode=F_OK):
	return _access (f, mode)

def USERHOME ():
	return getenv ("HOME") + "/"

def findfiles (dir):
	for f in listdir_fullpath (dir):
		if not path.isdir (f):
			yield f
		else:
			for f in findfiles (f):
				yield f

def open_mkpath (f, flags="r"):
	try:
		return open (f, flags)
	except:
		p = abspath (f).split ("/")
		d = "/"
		for dd in p [:-1]:
			d += dd + "/"
			if not access (d):
				mkdir (d)
		return open (f, flags)

def write_mkdirs (f, data):
	open_mkpath (f, "w").write (data)

def walk (dir, wantdirs=True):
	if wantdirs:
		yield dir + "/"
	for f in listdir_fullpath (dir):
		if not path.isdir (f):
			yield f
		else:
			for f in walk (f, wantdirs):
				yield f

# `rename` does not work if the files are on different partitions.
# This here will attempt to copy in case of failure
def MoveFile (old, new):
	try:
		rename (old, new)
	except:
		f = open (old)
		fw = open (new, "w")
		while 1:
			d = f.read (8192)
			if not d:
				break
			fw.write (d)
		fw.close ()
		f.close ()
		remove (old)

# Copy a file (XXXX: do a "hard link" if possible)
def CopyFile (src, dest):
	f1 = open (src)
	f2 = open (dest, "w")
	while 1:
		data = f1.read (8192)
		if not data:
			break
		f2.write (data)
	f2.close ()

# used mainly by `df`
def statfs (path):
	a = _buffer (140)
	if _statfs (path, a):
		raise OsError ()
	return array ("i", a [:40])

# A relative symbolic link
def symlink_relative (old, new):
	old = abspath (old)
	new = abspath (new)
	od = old.split ("/")
	nd = new.split ("/")
	while od and nd and od [0] == nd [0]:
		od.pop (0)
		nd.pop (0)
	relpath = "../" * (len (nd) - 1) + "/".join (od)
	symlink (relpath, new)

# readlink return it as absolute
def readabslink (file):
	link = readlink (file)
	if link [0] == "/":
		return link
	return normabspath (abspath (dirname (file)) + "/" + link)

# touch
_touch = touch
def touch (f):
	if not access (f):
		open (f, "w").write ("\0")
	else:
		_touch (f)
