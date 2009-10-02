#
# Script used to build the standard library
#

import os
import py_compile

def compile_dir (dir, maxlevels=10, force=0, fork=False, **kw):
#	print 'Listing', dir, '...'
	allfiles = os.listdir (dir)
	if "NOCOMPILE" in allfiles:
		return
	allfiles.sort ()
	files = []
	subdirs = []
	for name in allfiles:
		fullname = os.path.join (dir, name)
		if os.path.isfile (fullname):
			files.append (name)
		elif name != os.curdir and name != os.pardir and \
		     os.path.isdir(fullname) and \
		     not os.path.islink(fullname):
			subdirs.append (name)
	for name in files:
		fullname = os.path.join (dir, name)
		head, tail = name[:-3], name[-3:]
		if tail == '.py' or tail == '.pe':
			if tail == '.py':
				cfile = fullname + 'c'
			else:
				cfile = fullname [:-1] + 'yc'

			pycs = os.path.dirname (fullname) + "/.pycs/"
			if os.access (pycs, os.R_OK):
				cfile2 = pycs + os.path.basename (cfile)
			else:
				cfile2 = ""

			ftime = os.stat(fullname).st_mtime
			try: ctime = os.stat(cfile).st_mtime
			except: ctime = 0
			if cfile2:
				try: ctime2 = os.stat(cfile2).st_mtime
				except: ctime2 = 0
				ctime = max (ctime, ctime2)

			if (ctime > ftime) and not force: continue
			print 'compiling', fullname, '...'
			try:
				py_compile.compile(fullname, cfile2 or cfile, None, True, fork, kw)
			except:
				print "  - FAILED -", sys.exc_info ()
				raise
	for name in subdirs:
		fullname = os.path.join (dir, name)
		compile_dir(fullname, maxlevels - 1, force, fork, **kw)
