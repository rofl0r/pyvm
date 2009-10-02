##  pyvm boot script
## 
##  Copyright (c) 2006-2008 Stelios Xanthakis
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) any later version.

#
# This is the BIOS of pyvm
#
# We are not allowed to call any python bytecode
# unless it's protected inside try..except
#
# 'traceback' is a builtin in pyvm for the moment
#
# __import_compiled__ is a special builtin which should
# probably be named '__execcompiled__' and it runs a pyc
# file from its path location (doesn't use sys.path) and
# without using the import lock.
#
# 'make BIOS' to recompile
#
import sys, traceback

__version__ = 'pyvm 2.0'
__help__ = '''usage:
 pyvm file [options]		: runs file (search/alias if not .pe, .py, .pyc)
 pyvm -cc file			: compile file (optimized, incompatible with python)
 pyvm -cc[2.3|2.4|2.5] file	: compile producing bytecode for the specified version
 pyvm -V|--version		: version info

Common options:
 -display DISPLAY
'''

# this file is indented with spaces on purpose so we will
# be reluctant to modify without having thought about it.

def _COMPAT ():
    del _COMPAT

    def Exception_repr (self):
        try:
            return repr (self.x)
        except:
            return repr (self.__class__)

    Exception.__repr__ = Exception.__str__ = Exception_repr

    # Python provides some builtins which'd better be done in python
    # because either speed doesn't matter or there are newer 2.4 features
    # that deprecate them. By implementing them in python here, we reduce
    # the size of the binary of pyvm.
    # However, we must make sure that they aren't used by pyc in the bootstrap
    # procedure.

    class super:
        def __getattr__ (self, x):
            a = getattr (self.t1, x)
            if callable (a) and self.ot:
                x = self.ot
                return lambda *args, **kwargs: a (x, *args, **kwargs)
            return a

    def _super (t, ot=None):
        s = super ()
        s.t1 = t.__bases__ [0]
        s.ot = ot
        return s

    def sorted (lst, **kwargs):
        lst = list (lst)
        if 'cmp' in kwargs:
            lst.sort (kwargs ['cmp'])
        else:
            lst.sort ()
        return lst

    def _issubclass (C, B):
        if C is B:
            return True
        for i in C.__bases__:
            if _issubclass (i, B):
                return True
        return False
    def issubclass (C, B):
        if type (B) is tuple:
            for i in B:
                if _issubclass (C, i):
                    return True
        else:
            return _issubclass (C, B)

    __builtins__.super = _super
    __builtins__.sorted = sorted
    __builtins__.issubclass = issubclass
    __builtins__.__main__ = "__main__"

    # file methods
    sys.stdout.__dict__ ['readlines'] = lambda f: [xxx for xxx in f]
    def readline (f, size=0):
        try:
            return f.next ()
        except StopIteration:
            return ''
    sys.stdout.__dict__ ['readline'] = readline

    # originals
    sys.__stdout__ = sys.stdout
    sys.__stdin__ = sys.stdin

    # the pyc compiler transforms `print>>S,x` to `__print_to__(S.write,x)`
    __builtins__.__print_to__ = lambda W, *args: map (W, map (str, args))

    # better in python
    __builtins__.oct = lambda x: '%o'%x
    __builtins__.hex = lambda x: '%x'%x

    # exit func
    sys.ExitVal = 0
    sys.ExitMessage = None
    def exit (n=1, msg=None):
        if type (n) is str:
            msg = n
            n = 1
        sys.ExitVal = n
        sys.ExitMessage = msg
        raise SystemExit
    __builtins__.exit = exit

    # expression-raise
    def Raise (x):
        raise x
    __builtins__.Raise = Raise

    # add a new builtin `ints`
    def ints (arg):
        return [int (x) for x in arg]
    __builtins__.ints = ints

    # make izip a builtin
    from _itertools import izip
    __builtins__.izip = izip

    from binascii import unhexlify
    __builtins__.unhexlify = unhexlify

    # add a new builtin `now()`
    import _time
    __builtins__.now = _time.time
    __builtins__.sleep = _time.sleep

    def udiv (a, b):
        d = a / b
        if a % b:
            d += 1
        return d
    __builtins__.udiv = udiv

    # auto-import sys !!
    __builtins__.sys = sys
    sys.prefix = '/usr/local'
    sys.etcdir = sys.PYVM_HOME + 'etc/'
    import gc
    sys.gc = gc

    # string.stuff (bw compat. yawn)
    import string
    string.lower = str.lower
    string.upper = str.upper
    string.rfind = str.rfind
    string.join = lambda l, d: d.join (l)
    string.split = lambda s, *a: s.split (*a)
    string.strip = lambda s, *a: s.strip (*a)
    string.atoi = lambda *a: int (*a)
    string.find = str.find
    def rjust (s, n):
        if len (s) < n:
            s = ' ' * (n - len (s)) + s
        return s
    string.rjust = rjust

    # new..
    def sww (s, *args):
        s = s.sw
        for i in args:
            if s (i):
                return True
        return False
    __builtins__.sww = sww
    def eww (s, *args):
        s = s.ew
        for i in args:
            if s (i):
                return True
        return False
    __builtins__.eww = eww

    def ords (x):
        return [ord (x) for x in x]
    __builtins__.ords = ords

    def any (iterable):
        for i in iterable:
            if i:
                return True
        return False
    __builtins__.any = any

    def all (iterable):
        for i in iterable:
            if not i:
                return False
        return True
    __builtins__.all = all

    def minmax (minlim, value, maxlim):
        return max (min (value, maxlim), minlim)
    __builtins__.minmax = minmax

    # could be in __builtins__ but less code if here
    from _posix import access, F_OK
    def havefile (f):
        return access (f, F_OK)
    __builtins__.havefile = havefile

    # make sure a file ends with "/"
    def ewslash (x):
        if x and x [-1] == "/":
            return x
        return x + "/"
    __builtins__.ewslash = ewslash

    # nostdin should be used to achieve the thing where we type
    # stuff while a program is running and after the program is done
    # what we typed becomes part of the bash prompt. yes?
    def nostdin ():
        from _posix import close
        close (0)
    __builtins__.nostdin = nostdin

    # void: A function that does nothing
    def void (*args, **kwargs):
        pass
    __builtins__.void = void

    # importing fb is used often. make a shorthand
    def importFB ():
        from graphics.framebuffer import FrameBuffer
        return FrameBuffer
    __builtins__.importFB = importFB

    # unreferencer
    class __unreference__:
        def __init__ (self, d, *args):
            self.d = d
            self.args = args
        def __del__ (self):
            self.d (*self.args)
    __builtins__.__unreference__ = __unreference__

    # argp smart option parser
    if not sys.bootstrapping:
        try:
            import argp
            sys.argp = argp
        except:
            print "BIOS: error while importing argp!"

    # methods of object
    def obj__new__ (S, *args, **kwargs):
        return S (*args, **kwargs)
    def obj__str__ (S):
        return '<%s.%s object at %i>' %(__module__, S.__class__.__name__, id (S)) 
    object.__dict__ ['__new__'] = obj__new__
    object.__dict__ ['__str__'] = obj__str__

    # python 2.5 has a function in `thread.stack_size` which is needed to import `threading`
    import _thread
    def stack_size (s=0):
        raise "stack_size not available in pyvm"
    _thread.stack_size = stack_size

    # auto importer
    class autoimport:
        def __getattr__ (self, x):
            return __import__ (x)
        def __str__ (self):
            return "Auto-importer"
    __builtins__.modules = autoimport ()
    __builtins__.HOME = sys.PYVM_HOME
    __builtins__.USERHOME = sys.PYVM_HOME + "user/"

    # general purpose error
    class Error (Exception):
        pass
    __builtins__.Error = Error
    Error.__module__ = ""

    # with (prex()) print any exceptions that happen
    class prex:
        def __context__ (self):
            return self
        def __enter__ (self):
            pass
        def __exit__ (self, a, b, c):
            if c:
		print "### Reported exception ###"
                print " ", a
                print " ", c
    __builtins__.prex = prex

    # Dictionaries have another method 'update2'. In d1.update2(d2) values are
    # taken from d2 into d1 if they don't have a key in d1.
    def update2 (d, du):
        for k in du:
            if k not in d:
                d [k] = du [k]
    {}.__dict__ ["update2"] = update2

    # update-from
    def __updatefrom__ (dest, src, names=None):
        if type (src) is not dict:
            src = src.__dict__
        if names:
            for n, a in names:
                if not a:
                    a = n
                dest [a] = src [n]
        else:
            dest.update (src)
    __builtins__.__updatefrom__ = __updatefrom__

    # at-import -- the `@` operator in whitespaceless
    def __atimport__ (m):
        m0, _, rest = m.partition (".")
        im0 = None
        while (1):
            if im0 is None:
                try:
                    im0 = sys.modules [m0]
                except:
                    __import__ (m0)
                    im0 = sys.modules [m0]
            if not rest:
                return im0;
            m1, _, rest = rest.partition (".")
            try:
                m1notinim0 = m1 not in im0
            except:
                print "Can't import:", m
                print "(error looking up attribute '%s')" %m1
                raise
            if m1notinim0:
                m0 = m0 + "." + m1
                im0 = None
            elif not rest:
                return im0.__dict__ [m1]
            else:
                im0 = im0.__dict__ [m1]
    __builtins__.__atimport__ = __atimport__
    def writefile (filename, data):
        open (filename, "w").write (data)
    __builtins__.writefile = writefile

    # sys.atexit
    def atexit (func, *args, **kwargs):
        sys.atexit.append ((func, args, kwargs))
    sys.register_atexit = atexit
    sys.atexit = []

# Sigint handler.  When the VM receives a SIGINT (for example with Ctrl-C)
# it releases the __klock__ lock.  This thread is always sleeping on __klock__.
# When it wakes up, it tries to terminate the main thread, first with a gentle
# signal that makes blocking calls raise SystemExit.  On the second ctrl-C
# it will try to interrupt the main thread even if it's running (infinite loop
# for example).  The third ctrl-C will also set the SCHED_KILL flag which
# will instruct the fourth -and final- ctrl-C to kill the VM internally.

def SigInt ():
    from _thread import __klock__, interrupt
    __klock__.acquire ()
    try:
        # Interrupt soft blocking
        interrupt (0, SystemExit)
    except:
        pass
    __klock__.acquire ()
    try:
        # Interrupt soft blocking & running
        interrupt (0, SystemExit, 1)
    except:
        pass
    __klock__.acquire ()
    __set_kill__ ()
    try:
        # Interrupt soft blocking, running & real blocking.
        # the vm enters a fatal state that should only
        # lead to the exit asap.
        interrupt (0, SystemExit, 2)
    except:
        pass

ALIASES = "mp3", "ip2country", "pdf", "avplay"

def BIOS ():

    del BIOS

    ##print "pyvm BIOS 2.0"

    import sys
    sys.to_delete = []
    from sys import argv

    if sys.program_name in ALIASES:
        argv.insert (0, sys.program_name)

    if "--BOOTSTRAPPING--" in argv:
        argv.remove ("--BOOTSTRAPPING--")
        sys.bootstrapping = True
        sys.set_bootstrap ()
    else:
        sys.bootstrapping = False

    # special options that are global to all applications.
    def getappopt (opt):
        if opt in argv:
            try:
                i = argv.index (opt)
                ret = argv [i + 1]
                argv.pop (i)
                argv.pop (i)
                return ret
            except:
                print "Bad use for:", opt

    # Defaults for global cmd line options
    sys.GuiFontFamily = sys.GuiGeometry = sys.GuiDisplay = None
    o = getappopt ("-display")
    if o:
        sys.GuiDisplay = o
    o = getappopt ("-geometry")
    if o:
        sys.GuiGeometry = o
    o = getappopt ("-font")
    if o:
        sys.GuiFontFamily = o

    try:
        _COMPAT ()
    except:
        print "BIOS: (errors in _COMPAT)"

    if not argv or argv [0] in ('-h', '--help'):
        print __help__
        __set_exit__ (0)
        return

    if argv [0] in ('-V', '--version'):
        print __version__
        __set_exit__ (0)
        return

    delfile = False
    implicit_pyc = False
    prog = sys.script_name = argv [0]

    if '.' not in prog and "/" not in prog and not prog.startswith ("-"):
        try:
            import findrun
            a = findrun.find (prog)
            if a == prog:
                print "BIOS: no such script '%s'"% prog
                return
            prog = a
            implicit_pyc = True
        except:
            print "BIOS: findrun script failed!"

    if prog.endswith ('.pyc'):
        pycfile = prog
        if not havefile (pycfile):
            print "BIOS: no such pyc file '%s'" %pycfile
            return
    elif prog.endswith ('.py') or prog.endswith ('.pe'):
        if not havefile (prog):
            print "BIOS: file not found '%s'" %prog
            return
        try:
            import pyc
        except:
            print "BIOS: Cannot import the pyc compiler", sys.exc_info ()
            return
        try:
            # we create a pyc file and possibly overwrite an existing pyc file.
            # then, if a pyc file didn't exist before us, we remove it later.
            # this all is done to achieve a python-like effect where we don't
            # pollute the filesystem with .pyc files if the user runs a .py file.
            from os import access
            if access (prog [:-1] + 'yc'):
                x = prog [:-1] + 'yc'
            else:
                x = None
            pycfile = pyc.compileFile (prog,
                                                 pyvm=True,
                                                 dynlocals=False,
                                                 marshal_builtin=True, 
                                                 arch='pyvm')
            __clear_kill__ ()
            if x is None and ".pycs/" not in pycfile:
                delfile = True
        except:
            print 'BIOS:', prog, ":Syntax Error", traceback.format_exc ()
            return
    elif prog.startswith ('-cc'):
        try:
            import pyc
            if len (prog) == 3:
                pyc.compileFile (argv [1], pyvm=True, dynlocals=False, marshal_builtin=True, arch='pyvm')
            else:
                pyc.compileFile (argv [1], arch=prog [3:])
            __set_exit__ (0)
        except:
            try:
                print "Compilation Failed", sys.exc_info ()
            except:
                print "Error while formatting exception", sys.exc_info ()
        return
    else:
        print "BIOS: No script"
        return

    # Add the base directory to sys path for further imports
    if '/' in pycfile:
        try:
            import os
            basedir = os.dirname (os.abspath (pycfile)) + '/'
            # If the pyc file is a file in .pycs/ as supplied by `findrun`
            # add the parent directory instead.
            if implicit_pyc and basedir.ew ("/.pycs/"):
                basedir = basedir [:-6]
            sys.path.insert (0, basedir)
        except:
            print "BIOS: cannot import os.path. Will not add basedir to sys.path"

    from _thread import start_new_thread

    try:
        start_new_thread (SigInt, ())
    except:
        print "BIOS: SigInt failed"

    try:
        # * * * * * * * run program * * * * * * * * *
        __import_compiled__ (pycfile, '__main__', delfile)
        __clear_kill__ ()
        __set_exit__ (0)
    except SystemExit:
        try:
            n = sys.ExitVal
            if sys.ExitMessage:
                print sys.ExitMessage
        except:
            n = 4
        __set_exit__ (n)
    except:
        try:
             ext, exv, tb = sys.exc_info ()
             print "BIOS: Uncaught exception:", ext, exv
             print tb
        except:
            try:
                print "BIOS: exception while formatting exception!!", sys.exc_info ()
            except:
                pass

    try:
        sys.exitfunc ()
        __clear_kill__ ()
    except:
        pass

    try:
        for f, args, kwargs in sys.atexit:
            try:
                f (*args, **kwargs)
                __clear_kill__ ()
            except:
                pass
    except:
        pass

    try:
        from _posix import remove
        for f in sys.to_delete:
            try:
                remove (f)
            except:
                pass
    except:
        print "BIOS: error in to delete"

try:
    BIOS ()
except:
    print "pyvm: exception escaped from BIOS!"
