
# Make assuming the compiler doesn't need to compile itself.
# Otherwise, this fails
all:
	pyvm Make.pyc

# Use the precompiled "image" of the compiler to compile
# everything, including the compiler.
bootstrap:
	@echo
	@echo "Bootstrap (incompatible bytecode)..."
	tar xf bootstrap.bin
	pyvm --BOOTSTRAPPING-- Make.pyc force fork
	@echo "OK."

# same as bootstrap but recreate the "image" with the
# produced "pyc"s
rebuild:
	@echo
	@echo "Bootstrap (incompatible bytecode)..."
	tar xf bootstrap.bin
	pyvm --BOOTSTRAPPING-- Make.pyc force fork
	@echo "OK."
	rm -f pyc/indent.pyc pyc/installer.pyc pyc/ptest.pyc pyc/test_libsize.pyc pyc/build.pyc
	tar c pyc/*.pyc .pycs/re.pyc .pycs/itertools.pyc Make.pyc .pycs/posix.pyc .pycs/compileall.pyc .pycs/py_compile.pyc .pycs/operator.pyc .pycs/types.pyc .pycs/array.pyc .pycs/os.pyc .pycs/stat.pyc .pycs/argp.pyc > bootstrap.bin

# Produces bytecode which is compatible with Python 2.4 and
# can be executed with it. This should be preferred for development
# because running the pycs with Python can be used to find bugs
# in pyvm
bootstrap-compat:
	@echo
	@echo "Bootstrap..."
	tar xf bootstrap.bin
	pyvm Make.pyc force

no-bootstrap:
	@echo
	@echo "Bootstrap with python..."
	tar xf bootstrap.bin
	cd pyc && python test_bootstrap.py
	pyvm Make.py

image:
	rm -f pyc/indent.pyc pyc/installer.pyc pyc/ptest.pyc pyc/test_libsize.pyc pyc/build.pyc
	tar c pyc/*.pyc .pycs/re.pyc .pycs/itertools.pyc Make.pyc .pycs/posix.pyc .pycs/compileall.pyc .pycs/py_compile.pyc .pycs/operator.pyc .pycs/types.pyc .pycs/array.pyc .pycs/os.pyc .pycs/stat.pyc .pycs/argp.pyc > bootstrap.bin

clean:
	find . -name \*.pyc | xargs rm -f
	find . -name \*.so | xargs rm -f
	cd pyc && make clean
	rm -f objdir/*.o objdir/*.so


wce:
	find . -name \*.pe | xargs wc | sort -n
wcy:
	find . -name \*.py | xargs wc | sort -n
wcye:
	find . -name \*.p[ye] | xargs wc | sort -n
wcc:
	find . -name \*.pyc | grep .pycs | xargs wc -c | sort -n
