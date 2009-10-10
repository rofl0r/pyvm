all:
	@echo "default configure"
	@cd pyvm && ./configure --no-force
	@echo
	@echo "compiling pyvm"
	@cd pyvm && make o3
	@echo
	@echo "compiling Lib with pyvm/pyc"
	@cd Lib && make bootstrap
	@echo
	@echo "system setup"
	@pyvm Lib/setupsys.pe
	@echo
	@echo "Ready to go!"

# cygwin DOES NOT work. Use vmware.
#cygwin:
#	@echo "compiling pyvm"
#	@cd pyvm && make cygwin
#	@echo "compiling Lib with pyvm/pyc"
#	@cd Lib && make no-bootstrap
#	@echo "OK."

clean:
	@cd Lib && make clean
	@cd pyvm && make distclean

# These require working pyvm and git-aware tree

tarball:
	pyvm git head > commit
	pyvm git tar --prefix=pyvm-2.0/ --add=Lib/bootstrap.bin --add=commit pyvm.tar
	bzip2 pyvm.tar

diff:
	pyvm git diff | less

g:
	pyvm git files
	pyvm git diff | pyvm diffstat --color

wd:
	pyvm gitwc wd

alltime:
	# diff cwd vs first commit ever
	pyvm git diff ce92aa4a98 | pyvm diffstat --color
