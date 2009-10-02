##  Shared library creation/loading
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) version 3 of the License.

import _JIT
from _JIT     import CStringToPyString, Memcpy, MemcpyInts, StructMember, DirectMemSetInt, CInt
from _JIT     import fptr_wrapper, libc_sym

#
# higher level functionality over _JIT module
#
# 1) provides abstraction for compiling shared libs
# 2) uses the convention for cached .dll
# 3) provides some essential CJITed functions for interaction with C libs
# 4) generally other useful stuff for when wrapping libraries
#
#
import os
import re

Silent = False
PelfWorks = True

def HavePelf (usepelf=True):
	if usepelf == "try":
		return "try"
	if not usepelf:
		return False
	from sysconfig import GETCONF
	return GETCONF ("pelfworks")

def DoubleIndirection (i):
	return StructMember (StructMember (i, 0), 0)

CC = sys.cc
SOPATH = './'

# sys.machine is the string `cc -dumpmachine`, which for gcc is
# something like i686-pc-linux-gnu

try:
	ARCH = sys.machine.split ("-")[0]
except:
	ARCH = "unknown"

# This test is not 100% correct. It's what architecture gcc produces binaries for.
# The host may be something better...

def ARCH_586 ():
	return ARCH in ("i586", "i686", "x86_64", "amd64")

dllopen = _JIT.dllopen

class DLLError:
	def __init__ (self, reason):
		self.reason = reason
	def __repr__ (self):
		return self.reason
	__str__ = __repr__

def open_lib (fnm, pelf, libs=(), syms={}):
	if not pelf:
		return _JIT.dllopen (fnm)
	return pelfopen (fnm, libs, syms)

def c_compile (infile, outfile, options=None, pelf=False, libs=(), syms={}):
	if options is None:
		options = []
	if type (options) is str:
		options = options.split ()
	loptions = []
	if not pelf:
		for i in libs:
			loptions.append ('-l' + i)
		options += ['-fpic', '-shared']
		if '-c' in options:
			options.remove ('-c')
	else:
		if '-c' not in options:
			options.append ('-c')
	cmd = ' '.join ([CC, ' '.join (options), infile, '-o', outfile, " ".join (loptions)])
	if not Silent:
		print "RUNNING:", cmd
	if os.system (cmd):
		raise DLLError (cmd)
	return open_lib (outfile, pelf, libs)

libno = 0

def CJIT (PROGRAM, options=None, pelf=False, libs=()):
	"""Compile PROGRAM and return a dynamically linked library. The file of the library
	will be removed when the library is freed"""
	if pelf == 'try':
		try:
			return CJIT (PROGRAM, options, True, libs)
		except:
			pass
		pelf = False
	global libno
	pelf = HavePelf (pelf)
	cname = '_dlltmp.c'
	file (cname, 'w').write (PROGRAM)
	libname = SOPATH + '_dlltmplib%i.so' % libno
	libno += 1
	lib = c_compile (cname, libname, options, pelf=pelf, libs=libs)
	os.unlink (libname)
	os.unlink (cname)
	return lib

OBJDIR = sys.PYVM_HOME + 'Lib/objdir';

def CachedLib (name, PROGRAM, options=None, force=False, pelf=False, libs=()):
	if pelf == 'try':
		try:
			return CachedLib (name, PROGRAM, options, force, pelf=True, libs=libs)
		except:
			pass
		pelf = False
	pelf = HavePelf (pelf)
	H = '%x' %hash (PROGRAM)
	PATH = OBJDIR
	if not pelf:
		ext = '.so'
	else:
		ext = '.o'
	fnm = PATH + '/' + name + '.' + H + ext
	if os.access (fnm, os.R_OK) and not force:
		##print "CACHED", fnm
		return open_lib (fnm, pelf, libs)
	cname = PATH + '/_dlltmp.c'
	file (cname, 'w').write (PROGRAM)
	lib = c_compile (cname, fnm, options, pelf=pelf, libs=libs)
	os.unlink (cname)
	return lib

# Load c-libraries that are placed in Lib/Clib, follow the 'const char ABI[]'
# convention to export their symbols and their object file is cached
# in Lib/objdir.

abidef = re.re (r"([-sidvf])\s+(\S+)\s*(-|(?:[-sivdfzi*]|p[fd8vs]|p16|p32)*)\s*$")

def MakeLib (name, options="", pelf=False, force=False, libs=(), syms={}):
	pelf = HavePelf (pelf)
	if pelf == 'try':
		try:
			return MakeLib (name, options, pelf=True, force=force, libs=libs)
		except:
			print "PELF failed. Trying shared", sys.exc_info ()
			return MakeLib (name, options, pelf=False, force=force, libs=libs)
	cfile = '%sLib/Clibs/%s.c' %(HOME, name)
	sfile = '%sLib/Clibs/%s.s' %(HOME, name)
	if not os.access (cfile) and os.access (sfile):
		cfile = sfile
	if not pelf: ext = '.so'
	else: ext = '.o'
	objfile = "%s/clib.%s%s" %(OBJDIR, name.replace ("/", "."), ext)
	getmtime = os.path.getmtime
	if force or not os.access (objfile) or getmtime (cfile) > getmtime (objfile):
		rebuilt = True
		lib = c_compile (cfile, objfile, options, pelf=pelf, libs=libs, syms=syms)
	else:
		objmtime = getmtime (objfile)
		rebuilt = False
		lib = open_lib (objfile, pelf, libs, syms)
	return rebuilt, lib, cfile, objmtime

def LoadLib (lib, cfile, abi, checkdeps=True):
	if abi is None:
		try:
			x = lib.sym ("ABI")
			abi = CStringToPyString (x)
		except:
			print "File [%s] has no symbol info" %cfile
			raise

	if checkdeps:
		try:
			x = CStringToPyString (lib.sym ("__DEPENDS__"))
			return [y.strip () for y in x.split (",")]
		except:
			pass

	D = {}
	blocking = False
	for l in abi.split ('\n'):
		if not l:
			continue
		M = abidef (l)
		if not M:
			raise Error ("Bad line in ABI specs [%s]" %l)

		ret, name, args = M [1], M [2], M [3]

		if not args:
			if ret == "i":
				try:
					D [intern (name)] = CInt (lib.sym (name))
				except:
					print "No such variable to link:", name
					raise
				continue
			raise Error ("Not implemented ABI line [%s]" %l)

		if ret in "-v":
			ret = ""
		if args in "-v":
			args = ""

		if name [-1] == '!':
			name = name [:-1]
			blocking = True
		elif name [-1] == "=":
			name = name [:-1]
			cf = "sizeof_" + name.split ("_")[-1]
			try:
				size = CInt (lib.sym (cf))
			except:
				print "DLL: was expecting an integer \"%s\" for %s" %(cf, name)
				raise
			func = lib.link ((ret, name, args), blocking)
			if not ret:
				D [intern (name)] = constructor (func, size)
			else:
				D [intern (name)] = rconstructor (func, size)
			continue
		else:
			blocking = False
		D [intern (name)] = lib.link ((ret, name, args), blocking)
	return D

# --- Main interface to the Lib/Clibs/*.c conventions ---

def Clib (name, options="", pelf=False, force=False, libs=(), abi=None, syms={}):
	rebuilt, lib, cfile, objmtime = MakeLib (name, options, pelf, force, libs, syms)
	r = LoadLib (lib, cfile, abi, checkdeps=not rebuilt)
	if type (r) is list:
		for f in r:
			if os.path.getmtime (HOME + "Lib/Clibs/" + f) > objmtime:
				rebuilt, lib, cfile, objmtime = MakeLib (name, options,
									 pelf, True, libs, syms)
				break
		r = LoadLib (lib, cfile, abi, checkdeps=False)
	return r

def Import (*args, **kwargs):
	class C (None):
		locals ().update (Clib (*args, **kwargs))
	return C

#

def constructor (callable, size):
	def f (*args):
		b = _buffer (size)
		callable (b, *args)
		return b
	return f

def rconstructor (callable, size):
	def f (*args):
		b = _buffer (size)
		return b, callable (b, *args)
	return f

def pelfopen (objfile, libs, syms):
	# This is here so pelf is not required by the bootstrap 
	import pelf
	return pelf.dllopen (objfile, libs, syms)

#
# Load functions from libc
#

def libcload (ret, sym, args, blocking=False):
	addr = libc_sym (sym)
	return fptr_wrapper (ret, addr, args, blocking)
