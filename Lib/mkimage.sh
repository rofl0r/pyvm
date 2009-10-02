#!/bin/sh
#
# Script to build bootstrap.bin using Python.
# Useful if the bootstrap is broken, or to build
# pyvm from git (which doesn't track the bin file).
#
# Python 2.4 (suggested) or 2.5, is required

make clean

# compile everything with python
cd objdir
cat > tmpbuild.py << EOF
from sys import version_info as ver
if ver [0] != 2 or ver [1] < 4 or ver [1] > 5:
	print "Python 2.[45] is required to bootstrap pyvm"
	raise SystemExit ()
import compileall
compileall.compile_dir ("..")
EOF
python tmpbuild.py
rm tmpbuild.py
cd ..

# bootstrap the compiler
cd pyc
ls
python test_bootstrap.py
cd ..

# create boostrap.bin
make image

# recompile everything with the pyc compiler
make bootstrap

# create final bootstrap.bin
make image
