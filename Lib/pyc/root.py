##  root pyc module
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) version 3 of the License.

import marshal, sys, gc
import ast, parser
from pycodegen	import ast2code
from pycmisc	import import_constants_from_files
from optimizer	import optimize_tree, constify_names, marshal_builtins
from transform	import transform_tree
from symbols	import parseSymbols
from gco	import GCO
import os
getmtime = os.path.getmtime

IN_PYVM = sys.copyright.startswith ('pyvm')
if IN_PYVM:
	def packi (i):
		return "%mi" %i
	VMMAGIC = "\x6d\xf2\x0d\x0a"
else:
	from struct import pack
	def packi (i):
		return pack ("<i", i)
	import imp
	VMMAGIC = imp.get_magic ()

#
# This code is embedded into the beginning of a module if it uses --constants-files
#
DEPCHECK = """
def checkmod (cf, t):
     from os.path import getmtime
     try:
	# if getmtime fails meaning the file is not there, assume OK
	# (for the case somebody has distributed the bytecode only).
	ct = getmtime (cf)
     except:
	return
     if ct != t:
	raise RuntimeError ("DependencyError: file %s has been modified since compilation" % cf)
# Below we will add entries like:
# checkmod ('consts.py', 1122210730)
"""

#
#
def get_version ():
	return '%i.%i' %(sys.version_info [0], sys.version_info [1])


#
def eval_ast (expr, gl=None, lc=None):
	""" Provide an 'eval' function which is identical to the builtin 'eval'.
	The thing is that the expression will be evaluated on the AST nodes
	(dynamic languages cool or what?), without the generation of any
	bytecode objects.
	The existance of an 'eval' function that can evaluate code objects
	is required though (this is not pure in terms of 'no virtual machine
	that can execute python bytecode is required').  The reason is
	that for loops (list comprehensions, generator expressions) we'd
	rather evaluate it in bytecode which is faster.  """

	if gl:
		if lc:
			def lookup_name (x):
				if x in l: return lc [x] 
				if x in gl: return gl [x]
				return __builtins__ [x]
		def lookup_name (x):
			try: return gl [x]
			except: return __builtins__ [x]
	else:
		def lookup_name (x):
			return __builtins__ [x]
	GCO.new_compiler ()
	GCO ['eval_lookup'] = lookup_name
	GCO ['py2ext'] = False
	GCO ['dynlocals'] = True
	GCO ['gnt'] = False
	GCO ['gnc'] = False
	GCO ['filename'] = '*eval*'
	GCO ['docstrings'] = True
	GCO ['pe'] = False
	try:
		return parser.parse_expr (source=expr).eval ()
	finally:
		GCO.pop_compiler ()

#
def makePycHeader (filename):
	mtime = os.path.getmtime (filename)
	mtime = packi (mtime)
	try:
		MAGIC = packi (GCO ['MAGIC'])
	except:
		MAGIC = VMMAGIC
	return MAGIC + mtime

#
def compile(source, filename, mode, flags=None, dont_inherit=None, py2ext=False,
	    dynlocals=True, showmarks=False, ConstDict=None, RRot3=False, asserts=True,
	    nolno=False, renames=False, is_epl=None, autosem=False):
	"""Replacement for builtin compile() function"""
	if flags is not None or dont_inherit is not None:
		raise RuntimeError, "not implemented yet"

	ConstDict = None
	if is_epl is None:
		pe = filename.endswith ('.pe')
	else:
		pe = is_epl
	GCO.new_compiler ()
	GCO ['Asserts'] = asserts
	GCO ['dynlocals'] = dynlocals
	GCO ['renames'] = renames
	GCO ['gnt'] = False
	GCO ['gnc'] = False
	GCO ['showmarks'] = showmarks
	GCO ['rrot3'] = RRot3
	GCO ['py2ext'] = py2ext
	GCO ['filename'] = filename and os.path.abspath (filename) or '*eval*'
	GCO ['arch'] = get_version ()
	GCO ['lnotab'] = not nolno
	GCO ['pyvm'] = IN_PYVM
	GCO ['docstrings'] = True
	GCO ['pe'] = pe

	try:
		if mode == "exec":
			kw = {"filename":filename}
			if source:
				kw ["source"] = source

			if not pe:
				tree = parser.parse (**kw)
			else:
				import pe
				tree = pe.parse (autosem=autosem, **kw)

			transform_tree (tree)
			if GCO ['have_with']:
				tree.Import ('sys')
			if ConstDict:
				constify_names (tree, ConstDict)
			parseSymbols (tree)
			optimize_tree (tree)
		elif mode == "eval":
			tree = ast.Expression (parser.parse_expr (source))
			transform_tree (tree)
			parseSymbols (tree)
		elif mode == "single":
			# XXX pass an iterator from which it can read more lines!
			tree = parser.parse (source=source)
			if GCO ['have_with']:
				tree.Import ('sys')
			transform_tree (tree)
			parseSymbols (tree)
		else:
			raise ValueError("compile() 3rd arg must be 'exec' or "
                             "'eval' or 'single'")
		return ast2code (tree, mode)
	finally:
		GCO.pop_compiler ()

#
import arch24

def compileFile(filename, display=0, dynlocals=True, py2ext=False, showmarks=False,
		constfiles=None, rrot3 = False, mtime=True, marshal_builtin=False,
		asserts=True, nolno=False, altdir=None, pyvm=False, output=None,
		arch=None, docstrings=True, renames=True):

#	print "COMPILEFILE:", filename, output
	# compileFile returns the filename of the pyc file on success.
	# it's possible that we won't be able to store the pyc next to the py

	if arch is None:
		arch = get_version ()
	if arch not in ('2.4', '2.3', 'pyvm', '2.5'):
		print "Cannot produce bytecode for %s" %arch
		raise SystemExit
	if arch == "pyvm":
		pyvm = True
	if pyvm:
		rrot3 = True

	constfiles=None
	pe = filename.endswith ('.pe')
	GCO.new_compiler ()
	GCO ['dynlocals'] = dynlocals
	GCO ['gnt'] = arch == 'pyvm'
	GCO ['gnc'] = arch == 'pyvm'
	GCO ['showmarks'] = showmarks or True
	GCO ['rrot3'] = rrot3
	GCO ['py2ext'] = filename.endswith ('.py2') or py2ext or pe
	GCO ['filename'] = os.path.abspath (filename)
	GCO ['Asserts'] = asserts
	GCO ['renames'] = renames
	if arch == 'pyvm':
		MAGIC = arch24.MAGIC_PYVM
	elif arch == '2.4':
		MAGIC = arch24.MAGIC
	elif arch == '2.5':
		MAGIC = arch24.MAGIC25
	else:
		MAGIC = arch24.MAGIC23
	GCO ['MAGIC'] = MAGIC
	GCO ['arch'] = arch
	GCO ['lnotab'] = not nolno
	GCO ['pyvm'] = pyvm
	GCO ['docstrings'] = docstrings
	GCO ['pe'] = pe

	gc.disable ()
	try:
		if not pe:
			tree = parser.parse (filename)
		else:
			import pe
			tree = pe.parse (filename)
		if GCO ['have_with']:
			tree.Import ('sys')
		transform_tree (tree)
		if constfiles:
			constify_names (tree, import_constants_from_files (constfiles))

			# Dependency check
			if mtime:
				TestDep = DEPCHECK + '\n'.join (
		          ['checkmod ("%s", %i)' % (intern (os.path.abspath (i)), getmtime (i)) for
			  i in constfiles]) + '\ndel checkmod\n'
				tree.node.nodes.insert (0, parser.parse (source=TestDep).node)
		parseSymbols (tree)
		if marshal_builtin:
			marshal_builtins (tree)
		optimize_tree (tree)
###		study_specialize (tree.Functions)
		codeobj = ast2code (tree, 'exec')

		output_specified = False
		if output:
			outfile = output
			output_specified = True
		elif filename.endswith ('.py2'):
			outfile = filename [:-1] + 'c'
		elif pe:
			outfile = filename [:-1] + 'yc'
		else:
			outfile = filename + 'c'

		# If the directory .pycs/ exists, save it there
		if not output_specified:
			dirname = os.path.dirname (filename)
			if os.path.isdir (dirname + "/.pycs"):
				outfile = dirname + "/.pycs/" + os.path.basename (outfile)

		try:
			f = open(outfile, "wb")
		except IOError:
			if altdir is None:
				raise
			outfile = altdir (outfile)
			f = open (outfile, "wb")
		f.write(makePycHeader (filename))

		if arch == 'pyvm':
			marshal.dump(codeobj, f, 2006)
		else:
			marshal.dump(codeobj, f)
		return outfile
	finally:
		GCO.pop_compiler ()
		gc.enable ()

#
def altpyc (fnm):
	return os.path.join (sys.pyctmp, fnm.replace ('/', '_'))

def compileFile_internal (filename, *args, **kwargs):
#	print "COMPILEFILE INTERNAL:", filename
	# this is what pyvm uses internally to compile files
	# if the file pyc output can't be saved next to the py file,
	# it goes to the temporaries
	# xxx: reentrant lock
	kwargs ['altdir'] = altpyc
	kwargs ['arch'] = "pyvm"
	kwargs ['dynlocals'] = False
	kwargs ['rrot3'] = True
	kwargs ['marshal_builtin'] = True
	kwargs ['renames'] = True
	dirname = os.path.dirname (filename)
	if os.path.isdir (dirname + "/.pycs"):
		f = os.path.basename (filename)
		if f.ew (".py2"):
			f = f [:-1] + "c"
		elif f.ew (".pe"):
			f = f [:-1] + "yc"
		elif f.ew (".py"):
			f += "c"
		else:
			f = ""
		if f:
			kwargs ["output"] = dirname + "/.pycs/" + f
			if os.access (dirname + "/" + f, os.F_OK):
				os.remove (dirname + "/" + f)
	rez = compileFile (filename, *args, **kwargs)
	gc.collect ()
	return rez
