# NOTE:
#  This makefile is really broken. The right thing to do is
#  make our own build system in pyvm that analyses the dependencies
#  and builds the right things. This makefile is good for making the
#  initial pyvm binary only.

include config.mak

OBJS = objdir/seg-malloc.o objdir/libdlmalloc.o objdir/threading.o objdir/IO.o objdir/module.sys.o\
       objdir/module.posix.o objdir/module.time.o objdir/module.thread.o objdir/module.unicode.o\
       objdir/module.__builtins__.o objdir/upoll.o\
       objdir/marshal.o objdir/util.o objdir/file.o objdir/modules.misc.o\
       objdir/module.math.o objdir/module.binascii.o objdir/cold.o \
       objdir/interned.o objdir/sort.o objdir/pyby.o objdir/module.string.o\
       objdir/module._itertools.o objdir/module.gc.o objdir/module.JIT.o objdir/module.marshal.o\
       objdir/array.o objdir/module.re.o objdir/dict2.o \
       objdir/module._bisect.o objdir/module.struct.o\
       objdir/module.pyvm_extra.o objdir/importfile.o objdir/module.__socket.o\
       objdir/module.cStringIO.o objdir/poller.o objdir/vmpoll.o objdir/iidict.o\
       objdir/stdout.o objdir/filedes.o objdir/boot.o\
       objdir/Long.o objdir/liblong.o objdir/module.zlib.o objdir/module.html.o

CFILES = objdir/seg-malloc.c objdir/threading.c objdir/file.c objdir/util.c objdir/marshal.c objdir/IO.c\
	objdir/module.sys.c objdir/module.posix.c objdir/module.time.c objdir/pyvm.c\
	objdir/module.thread.c objdir/module.__builtins__.c objdir/modules.misc.c\
	objdir/module.math.c objdir/module.binascii.c objdir/module.unicode.c \
	objdir/cold.c objdir/interned.c objdir/dict2.c objdir/sort.c objdir/pyby.c objdir/module.string.c\
	objdir/module._itertools.c objdir/module._bisect.c objdir/module.gc.c objdir/module.JIT.c\
	objdir/module.marshal.c objdir/array.c objdir/module.re.c objdir/module.pyvm_extra.c\
	objdir/importfile.c objdir/module.__socket.c objdir/module.cStringIO.c objdir/iidict.c\
	objdir/poller.c objdir/vmpoll.c objdir/stdout.c objdir/filedes.c objdir/boot.c\
	objdir/module.struct.c objdir/Long.c objdir/liblong.c objdir/module.zlib.c\
	objdir/module.html.c

# # # # NOTE # # # # #
# It does make sense to pass -march= option to BOTH lwc and gcc if 
# you don't care about binaries running on other processors. gcc will
# produce better code. lwc will pass this to the preprocessor and
# we can get properly expanded stuff from glibc headers (bswap, etc)
# Pass CC to lwc # TO-DO #

LWC=lwc/objdir/lwc

ifeq (1,1)
 CFL = -c -O3 -g -Wall $(CCFLAGS)
 CFLs = -c -Os -g -Wall $(CCFLAGS)
 CFL2 = -c -O2 -g -Wall $(CCFLAGS)
else
 CFL = -c -O1 -g -Wall $(CCFLAGS)
 CFLs = -c -O1 -g -Wall $(CCFLAGS)
 CFL2 = -c -O1 -g -Wall $(CCFLAGS)
endif

all: $(LWC) pcre/libpcre.a $(ZLIBO) $(OBJS) objdir/pyvm.o
	$(CC) $(OBJS) objdir/pyvm.o -g -lpthread -lm -ldl pcre/libpcre.a $(LIBS) $(ZLIBO)
	size a.out
	mv a.out ~/bin/pyvm

os: $(LWC) pcre/libpcre.a $(OBJS) objdir/pyvm.c 
	$(CC) -Wall objdir/pyvm.c -g -Os -o objdir/pyvm.o -c
	$(CC) $(OBJS) objdir/pyvm.o -g -lpthread -lm -ldl pcre/libpcre.a -Os $(LIBS)
	size a.out
	mv a.out ~/bin/pyvm

cfiles: config.h $(CFILES)
	@echo "lwc OK."
	@echo

objdir/pyvm.c: dynlib.h *.c+.h hutil.h VMPARAMS.h
	$(LWC) main.c+ > objdir/pyvm.c

objdir/pyvm.o: dynlib.h *.c+.h hutil.h VMPARAMS.h objdir/pyvm.c
	$(CC) objdir/pyvm.c -c -g -o objdir/pyvm.o -Wall

pyvm.o:
	$(CC) -Wall objdir/pyvm.c -g -O3  $(CCFLAGS) -o objdir/pyvm.o -c

o3: $(LWC) pcre/libpcre.a $(ZLIBO) cfiles pyvm.o $(OBJS)
	@#$(CC) -Wall objdir/pyvm.c -g -O3 -o objdir/pyvm.o -c
	@#$(CC) -Wall objdir/pyvm.c -g -O3 -funroll-loops -o objdir/pyvm.o -c
	$(CC) -Wall -g -O3 $(OBJS) objdir/pyvm.o -Q -lpthread -lm -ldl pcre/libpcre.a $(LIBS) $(ZLIBO)
	size a.out
	mv a.out ~/bin/pyvm

$(LWC):
	@echo "compiling lwc"
	@cd lwc && make
	@echo

pcre/libpcre.a:
	cd pcre && make

zlib/libz.a: zlib/*.[ch]
	cd zlib && make

clean:
	rm objdir/*.o

objdir/seg-malloc.c: seg-malloc.c+ mallok.h
	$(LWC) seg-malloc.c+ > objdir/seg-malloc.c
objdir/seg-malloc.o: objdir/seg-malloc.c
	$(CC) objdir/seg-malloc.c $(CFL) -o objdir/seg-malloc.o

objdir/libdlmalloc.o: libdlmalloc.c
	$(CC) libdlmalloc.c $(CFL2) -o objdir/libdlmalloc.o

objdir/upoll.o: upoll.c
	$(CC) upoll.c $(CFL2) -o objdir/upoll.o

objdir/threading.c: threading.c+
	$(LWC) threading.c+ > objdir/threading.c
objdir/threading.o: objdir/threading.c
	$(CC) $(CFL2) objdir/threading.c -o objdir/threading.o 

objdir/poller.c: poller.c+
	$(LWC) poller.c+ > objdir/poller.c
objdir/poller.o: objdir/poller.c
	$(CC) $(CFL2) objdir/poller.c -o objdir/poller.o 

objdir/vmpoll.c: vmpoll.c+
	$(LWC) vmpoll.c+ > objdir/vmpoll.c
objdir/vmpoll.o: objdir/vmpoll.c
	$(CC) $(CFL2) objdir/vmpoll.c -o objdir/vmpoll.o 

objdir/stdout.c: stdout.c+
	$(LWC) stdout.c+ > objdir/stdout.c
objdir/stdout.o: objdir/stdout.c
	$(CC) $(CFLs) objdir/stdout.c -o objdir/stdout.o

objdir/boot.c: boot.c+
	$(LWC) boot.c+ > objdir/boot.c
objdir/boot.o: objdir/boot.c
	$(CC) $(CFLs) objdir/boot.c -o objdir/boot.o

objdir/filedes.c: filedes.c+
	$(LWC) filedes.c+ > objdir/filedes.c
objdir/filedes.o: objdir/filedes.c
	$(CC) $(CFLs) objdir/filedes.c -o objdir/filedes.o

objdir/file.c: file.c+
	$(LWC) file.c+ > objdir/file.c
objdir/file.o: objdir/file.c
	$(CC) $(CFL2) objdir/file.c -o objdir/file.o

objdir/util.c: util.c+ hutil.h
	$(LWC) util.c+ > objdir/util.c
objdir/util.o: objdir/util.c
	$(CC) $(CFL2) objdir/util.c -o objdir/util.o

objdir/marshal.c: marshal.c+ VMPARAMS.h
	$(LWC) marshal.c+ > objdir/marshal.c
objdir/marshal.o: objdir/marshal.c
	$(CC) $(CFL) objdir/marshal.c -o objdir/marshal.o 

objdir/IO.c: IO.c+ threading.h
	$(LWC) IO.c+ > objdir/IO.c
objdir/IO.o: objdir/IO.c
	$(CC) $(CFL2) -Wno-parentheses objdir/IO.c -o objdir/IO.o

objdir/module.sys.c: config.h module.sys.c+ VMPARAMS.h
	$(LWC) module.sys.c+ > objdir/module.sys.c
objdir/module.sys.o: objdir/module.sys.c
	$(CC) $(CFLs) objdir/module.sys.c -o objdir/module.sys.o

objdir/module.__socket.c: config.h module.__socket.c+
	$(LWC) module.__socket.c+ > objdir/module.__socket.c
objdir/module.__socket.o: objdir/module.__socket.c
	$(CC) $(CFLs) objdir/module.__socket.c -o objdir/module.__socket.o

objdir/module.pyvm_extra.c: module.pyvm_extra.c+ VMPARAMS.h fya.c+
	$(LWC) module.pyvm_extra.c+ > objdir/module.pyvm_extra.c
objdir/module.pyvm_extra.o: objdir/module.pyvm_extra.c
	$(CC) $(CFL2) objdir/module.pyvm_extra.c -o objdir/module.pyvm_extra.o

objdir/module.posix.c: module.posix.c+
	$(LWC) module.posix.c+ > objdir/module.posix.c
objdir/module.posix.o: objdir/module.posix.c
	$(CC) $(CFLs) objdir/module.posix.c -o objdir/module.posix.o

objdir/module.time.c: module.time.c+
	$(LWC) module.time.c+ > objdir/module.time.c
objdir/module.time.o: objdir/module.time.c
	$(CC) $(CFLs) -Os objdir/module.time.c -o objdir/module.time.o

objdir/module.zlib.c: module.zlib.c+
	$(LWC) module.zlib.c+ > objdir/module.zlib.c
objdir/module.zlib.o: objdir/module.zlib.c
	$(CC) $(CFL) objdir/module.zlib.c -o objdir/module.zlib.o

objdir/module.unicode.c: module.unicode.c+
	$(LWC) module.unicode.c+ > objdir/module.unicode.c
objdir/module.unicode.o: objdir/module.unicode.c
	$(CC) $(CFL) objdir/module.unicode.c -o objdir/module.unicode.o

objdir/module.cStringIO.c: module.cStringIO.c+ cStringIO.h
	$(LWC) module.cStringIO.c+ > objdir/module.cStringIO.c
objdir/module.cStringIO.o: objdir/module.cStringIO.c
	$(CC) $(CFL) objdir/module.cStringIO.c -o objdir/module.cStringIO.o

objdir/module.thread.c: module.thread.c+ VMPARAMS.h
	$(LWC) module.thread.c+ > objdir/module.thread.c
objdir/module.thread.o: objdir/module.thread.c
	$(CC) $(CFL2) objdir/module.thread.c -o objdir/module.thread.o

objdir/importfile.c: importfile.c+
	$(LWC) importfile.c+ > objdir/importfile.c
objdir/importfile.o: objdir/importfile.c
	$(CC) $(CFLs) objdir/importfile.c -o objdir/importfile.o

objdir/module.__builtins__.c: module.__builtins__.c+ VMPARAMS.h
	$(LWC) module.__builtins__.c+ > objdir/module.__builtins__.c
objdir/module.__builtins__.o: objdir/module.__builtins__.c
	$(CC) $(CFL2) objdir/module.__builtins__.c -o objdir/module.__builtins__.o -Wno-missing-braces

objdir/modules.misc.c: modules.misc.c+
	$(LWC) modules.misc.c+ > objdir/modules.misc.c
objdir/modules.misc.o: objdir/modules.misc.c
	$(CC) $(CFLs) objdir/modules.misc.c -o objdir/modules.misc.o

objdir/module.struct.c: module.struct.c+
	$(LWC) module.struct.c+ > objdir/module.struct.c
objdir/module.struct.o: objdir/module.struct.c
	$(CC) $(CFL) objdir/module.struct.c -o objdir/module.struct.o

objdir/module.math.c: module.math.c+
	$(LWC) module.math.c+ > objdir/module.math.c
objdir/module.math.o: objdir/module.math.c
	$(CC) $(CFL2) objdir/module.math.c -o objdir/module.math.o

objdir/module.binascii.c: module.binascii.c+
	$(LWC) module.binascii.c+ > objdir/module.binascii.c
objdir/module.binascii.o: objdir/module.binascii.c
	$(CC) $(CFL) objdir/module.binascii.c -o objdir/module.binascii.o

objdir/module.html.c: module.html.c+
	$(LWC) module.html.c+ > objdir/module.html.c
objdir/module.html.o: objdir/module.html.c
	$(CC) $(CFL) objdir/module.html.c -o objdir/module.html.o

objdir/Long.c: Long.c+ liblong.h
	$(LWC) Long.c+ > objdir/Long.c
objdir/Long.o: objdir/Long.c
	$(CC) $(CFL2) objdir/Long.c -o objdir/Long.o

objdir/liblong.c: liblong.c+ bn.c
	$(LWC) liblong.c+ > objdir/liblong.c
objdir/liblong.o: objdir/liblong.c
	$(CC) $(CFL) objdir/liblong.c -o objdir/liblong.o

objdir/cold.c: cold.c+ VMPARAMS.h config.h hmodules.h
	$(LWC) cold.c+ > objdir/cold.c
objdir/cold.o: objdir/cold.c
	$(CC) $(CFLs) objdir/cold.c -o objdir/cold.o

objdir/interned.c: interned.c+
	$(LWC) interned.c+ > objdir/interned.c
objdir/interned.o: objdir/interned.c
	$(CC) $(CFL2) objdir/interned.c -o objdir/interned.o

objdir/dict2.c: dict2.c+
	$(LWC) dict2.c+ > objdir/dict2.c
objdir/dict2.o: objdir/dict2.c
	$(CC) $(CFL) objdir/dict2.c -o objdir/dict2.o

objdir/sort.c: sort.c+
	$(LWC) sort.c+ > objdir/sort.c
objdir/sort.o: objdir/sort.c
	$(CC) $(CFL) objdir/sort.c -o objdir/sort.o

objdir/iidict.c: iidict.c+
	$(LWC) iidict.c+ > objdir/iidict.c
objdir/iidict.o: objdir/iidict.c
	$(CC) $(CFL2) objdir/iidict.c -o objdir/iidict.o

objdir/pyby.c: pyby.c+ VMPARAMS.h
	$(LWC) pyby.c+ > objdir/pyby.c
objdir/pyby.o: objdir/pyby.c
	$(CC) $(CFLs) objdir/pyby.c -o objdir/pyby.o

objdir/module.string.c: module.string.c+
	$(LWC) module.string.c+ > objdir/module.string.c
objdir/module.string.o: objdir/module.string.c
	$(CC) $(CFL) objdir/module.string.c -o objdir/module.string.o

objdir/module._itertools.c: module._itertools.c+
	$(LWC) module._itertools.c+ > objdir/module._itertools.c
objdir/module._itertools.o: objdir/module._itertools.c
	$(CC) $(CFL) objdir/module._itertools.c -o objdir/module._itertools.o

objdir/module._bisect.c: module._bisect.c+
	$(LWC) module._bisect.c+ > objdir/module._bisect.c
objdir/module._bisect.o: objdir/module._bisect.c
	$(CC) $(CFL) objdir/module._bisect.c -o objdir/module._bisect.o

objdir/module.gc.c: module.gc.c+
	$(LWC) module.gc.c+ > objdir/module.gc.c
objdir/module.gc.o: objdir/module.gc.c
	$(CC) $(CFL) objdir/module.gc.c -o objdir/module.gc.o

objdir/module.JIT.c: module.JIT.c+
	$(LWC) module.JIT.c+ > objdir/module.JIT.c
objdir/module.JIT.o: objdir/module.JIT.c
	$(CC) $(CFL) objdir/module.JIT.c -o objdir/module.JIT.o

objdir/module.marshal.c: module.marshal.c+ VMPARAMS.h
	$(LWC) module.marshal.c+ > objdir/module.marshal.c
objdir/module.marshal.o: objdir/module.marshal.c
	$(CC) $(CFL2) objdir/module.marshal.c -o objdir/module.marshal.o

objdir/array.c: array.c+ VMPARAMS.h
	$(LWC) array.c+ > objdir/array.c
objdir/array.o: objdir/array.c
	$(CC) $(CFL) objdir/array.c -o objdir/array.o

objdir/module.re.c: module.re.c+ VMPARAMS.h
	$(LWC) module.re.c+ > objdir/module.re.c
objdir/module.re.o: objdir/module.re.c
	$(CC) $(CFL) objdir/module.re.c -o objdir/module.re.o

backup:
	tar c *.c *.c+ *.c+.h *.h > DD.tar
	bzip2 DD.tar

cclean:
	rm -f objdir/*.o a.out objdir/*.c .preprocfile

distclean:
	rm -f objdir/*.o a.out objdir/*.c config.h .preprocfile
	cd pcre && make clean
	cd zlib && make clean
	cd lwc && make distclean

BIOS:
#	pyvm ../Lib/pyc/pyc.pyc BIOS.py
#
# 	This assumes a working BIOS.pyc:
	pyvm -cc BIOS.py
