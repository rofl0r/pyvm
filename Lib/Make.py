import compileall
import py_compile
import sys

print "prepare pe (2)..."
py_compile.compile ('pe.py', ".pycs/pe.pyc", incompat=True) 

if 'fork' in sys.argv and 'pyvm' in sys.copyright:
	compileall.compile_dir ('.', force='force' in sys.argv, fork=True)
else:
	compileall.compile_dir ('.', force='force' in sys.argv)
