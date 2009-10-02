# This code takes a reports which files from the current directory
# should be selected to fill as much as possible SIZE.
#

#DEJAVU
'''
{
'NAME':"Best-fit",
'DESC':"Best-fit knapshack. Which files fit on a disk?",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"12000",
'BARGS':"120000"
}
'''

import os, sys
SIZE = int (sys.argv [1])

def fit (lst, size, n):
	if n == 1:
		for i in reversed (lst):
			if i <= size:
				return i, (i,)
		return 0, (0,)
	mx = 0
	ml = (0,)
	for i, l in enumerate (lst):
		if l > size:
			break
		ss, ii = fit (lst [i+1:], size - l, n - 1)
		if ss and ss + l > mx:
			mx = ss+l
			ml = (l,)+ii
			if mx == size:
				return mx, ml
	return mx, ml

def main (L):
	S = tuple (sorted ([i [0] for i in L]))
	mx = 0
	for i, s in enumerate (S):
		if mx + s < SIZE:
			mx += s
		else:
			break
	print "Can fit:", i
	for i in range (1, i):
		print 'Trying', i
		if i > 5:
			print 'do divide and conquer'
			raise SystemExit
		Sum, lst = fit (S, SIZE, i)
		if Sum == SIZE:
			break
	print "You'll have to select the files with sizes:", lst, '->', sum (lst)

#
# a directory with files
#	[(os.stat (x), x) for x in os.listdir ('.')]
#
Listing = [
(10069 , "richards.py"),
(2334 , "bwt.pyc"),
(7848 , "module.__socket.c+"),
(2669 , "ints.h.c+"),
(70 , "q.py"),
(21472 , "pymodules.h.c+"),
(1929 , "zipmap.pyc"),
(1802 , "super.py"),
(6762 , "bpnn2.py"),
(4816 , "bpnn.pyc"),
(12769 , "richards.pyc"),
(35585 , "cold.c+"),
(1100 , "queens.pyc"),
(1659 , "bencho.pyc"),
(668 , "list.pyc"),
(266 , "Make.pyc"),
(12778 , "pyby.c+"),
(1455 , "z-alloc.h.c+"),
(934 , "lists.py"),
(5887 , "dict2.c+"),
(331 , "findall.pyc"),
(424 , "digest.pyc"),
(5235 , "importfile.c+"),
(8254 , "IO.c+"),
(2241 , "MM.py"),
(3124 , "nprpuzzle.py"),
(145 , "sortsize.log"),
(4546 , "sortsize.pyc"),
(170 , "trip.pyc"),
(2347 , "Retest.pyc"),
(3423 , "MM.pyc"),
(335 , "module.gc.c+"),
(1926 , "fq.py"),
(213 , "ha.py"),
(1419 , "cvt.pyc"),
(1927 , "interned.c+"),
(12126 , "module.re.c+"),
(2153 , "nprpuzzle.pyc"),
(182 , "q.pyc"),
(1179 , "op.py"),
(1740 , "ta.py"),
(1712 , "bintree.pyc"),
(839 , "fib2.pyc"),
(1798 , "Pyvm_API.c+"),
(1306 , "kna.pyc"),
(3221 , "ryield.py"),
(2476 , "stdout.c+"),
(954 , "STESUP1.py"),
(1064 , "zipmap.py"),
(190 , "digest.py"),
(2580 , "module.cStringIO.c+"),
(4719 , "SSS.py"),
(4931 , "sharedref.py"),
(1163 , "fibonacci.py"),
(22019 , "markdown.pyc"),
(709 , "filter.py"),
(1098 , "module._bisect.c+"),
(3845 , "foodbill.py"),
(1853 , "Tim.py"),
(7064 , "supern.pyc"),
(2493 , "module.math.c+"),
(2720 , "random_markov_py.py"),
(996 , "templ.py"),
(186 , "list.py"),
(5229 , "tuple.h.c+"),
(5817 , "SSS.pyc"),
(1105 , "main.c+"),
(3148 , "CRC16.py"),
(23385 , "ElementTree.pyc"),
(24372 , "marshal.c+"),
(3002 , "dokosu.pyc"),
(1216 , "vmpoll.c+"),
(5433 , "sharedref.pyc"),
(3848 , "filedes.c+"),
(1319 , "fibonacci.pyc"),
(3297 , "sudoku.pyc"),
(189 , "findall.py"),
(29139 , "Retest.py"),
(463 , "fib2.py"),
(2061 , "super.pyc"),
(5792 , "sort.c+"),
(6353 , "bpnn2.pyc"),
(5378 , "supern.py"),
(12903 , "gc.h.c+"),
(634 , "floats.h.c+"),
(3234 , "module.time.c+"),
(5912 , "file.c+"),
(4034 , "CRC16.pyc"),
(1058 , "smithy.pyc"),
(8734 , "module.string.c+"),
(146 , "mkmk.py"),
(2763 , "putter.py"),
(10250 , "string.h.c+"),
(26086 , "module.__builtins__.c+"),
(542 , "cvt.py"),
(2038 , "module.md5.c+"),
(45948 , "lisp_py.pyc"),
(2236 , "dokosu.py"),
(7490 , "fbench.pyc"),
(7745 , "poller.c+"),
(1311 , "fq.pyc"),
(754 , "ha.pyc"),
(6137 , "fya.c+"),
(2029 , "except.pyc"),
(1584 , "quine22.py"),
(50 , "trip.py"),
(944 , "0kna.py"),
(714 , "kna.py"),
(3128 , "module.hashlib.c+"),
(1452 , "filter.pyc"),
(1793 , "module.sha.c+"),
(2688 , "locals.h.c+"),
(2078 , "module.binascii.c+"),
(97 , "lsf.py"),
(8263 , "module._itertools.c+"),
(3593 , "module.sys.c+"),
(6225 , "basic.h.c+"),
(6721 , "list.h.c+"),
(3468 , "cutter.pyc"),
(2096 , "wild.pyc"),
(1579 , "aryield.pyc"),
(1371 , "op.pyc"),
(1203 , "mandel.pyc"),
(645 , "pop.py"),
(138 , "Make.py"),
(8380 , "sortsize.py"),
(4921 , "threading.c+"),
(4965 , "PyFontify.py"),
(1395 , "ta.pyc"),
(12288 , ".kna.py.swp"),
(8829 , "module.pyvm_extra.c+"),
(826 , "smithy.py"),
(5194 , "module.thread.c+"),
(826 , "lists.pyc"),
(51762 , "lisp_py.py"),
(10697 , "set.h.c+"),
(5782 , "module.silly.c+"),
(13713 , "dict.h.c+"),
(1475 , "cartesianprod.pyc"),
(529 , "util.c+"),
(1108 , "bearophile.pyc"),
(3104 , "foodbill.pyc"),
(4865 , "module.marshal.c+"),
(2689 , "pythreads.h.c+"),
(2114 , "sudoku.py"),
(5040 , "flatten.pyc"),
(2142 , "Tim.pyc"),
(8929 , "array.c+"),
(893 , "aryield.py"),
(1333 , "cartesianprod.py"),
(2493 , "phash.pyc"),
(4752 , "typecall.h.c+"),
(62 , "bina.py"),
(9729 , "seg-malloc.c+"),
(4108 , "flatten.py"),
(5662 , "module.posix.c+"),
(3875 , "boot.c+"),
(3385 , "sortsize10.zip"),
(3704 , "ryield.pyc"),
(5019 , "bpnn.py"),
(21362 , "class.h.c+"),
(1811 , "bintree.py"),
(1796 , "module._random.c+"),
(707 , "pop.pyc"),
(9777 , "fbench.py"),
(2815 , "cutter.py"),
(60826 , "pyvm.h.c+"),
(2042 , "generators.h.c+"),
(3254 , "memfs.c+"),
(19611 , "module.JIT.c+"),
(1698 , "quine22.pyc"),
(3642 , "random_markov_py.pyc"),
(1424 , "templ.pyc")
]

main (Listing)
