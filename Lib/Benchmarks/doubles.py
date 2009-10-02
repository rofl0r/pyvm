#DEJAVU
'''
{
'NAME':"Double-for",
'DESC':"`Improving a huge double-for cycle` from c.l.py 18/10/2008, bruno desthuilliers",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"SEM",
'BARGS':""
}
'''

import random

class Node(object):
     def __init__(self, x, y):
         self.coordinates = (x, y)
     def __repr__(self):
         return repr(self.coordinates)


# how to build src:
# src = [(random.randrange(10),random.randrange(10)) for x in xrange(100)]
src = [
     (4, 9), (5, 0), (6, 6), (7, 2), (3, 6), (9, 6), (0, 1), (1, 6),
     (0, 5), (1, 2), (8, 9), (5, 4), (1, 6), (7, 6), (9, 1), (7, 6),
     (0, 1), (7, 4), (7, 4), (8, 4), (8, 4), (3, 5), (9, 6), (6, 1),
     (3, 4), (4, 5), (0, 5), (6, 3), (2, 4), (1, 6), (9, 5), (1, 2),
     (5, 8), (8, 5), (3, 1), (9, 4), (9, 4), (3, 3), (4, 8), (9, 7),
     (8, 4), (6, 2), (1, 5), (5, 8), (8, 6), (0, 8), (5, 2), (3, 4),
     (0, 5), (4, 4), (2, 9), (7, 7), (1, 0), (4, 2), (5, 7), (0, 4),
     (2, 5), (0, 8), (7, 3), (9, 1), (0, 4), (5, 0), (4, 9), (0, 6),
     (3, 0), (3, 0), (3, 9), (8, 3), (7, 9), (8, 5), (7, 6), (1, 5),
     (0, 6), (5, 9), (6, 8), (0, 0), (4, 1), (3, 3), (5, 4), (5, 3),
     (6, 1), (5, 4), (4, 5), (5, 8), (4, 1), (3, 6), (1, 9), (0, 5),
     (6, 5), (5, 5), (6, 0), (0, 9), (2, 6), (0, 7), (5, 9), (7, 3),
     (7, 9), (5, 4), (4, 9), (2, 9)
     ]
IN = [Node(x, y) for x, y in src]


def doubles0():
     SN = []
     for i in range(len(IN)): #scan all elements of the list IN
         for j in range(len(IN)):
             #print i, j
             if i <> j:
                 if IN[i].coordinates[0] == IN[j].coordinates[0]:
                     if IN[i].coordinates[1] == IN[j].coordinates[1]:
                         SN.append(IN[i])
     return SN

def doubles1():
     "first solve an algoritmic problem"
     SN = []
     for i in range(len(IN)):
         for j in range(i+1, len(IN)):
             #print i, j
             #if i != j:
             if IN[i].coordinates[0] == IN[j].coordinates[0]:
                 if IN[i].coordinates[1] == IN[j].coordinates[1]:
                     SN.append(IN[i])
     return SN

def doubles2():
     "then avoid buildin useless lists"
     SN = []
     for i in xrange(len(IN)):
         for j in xrange(i+1, len(IN)):
             if IN[i].coordinates[0] == IN[j].coordinates[0]:
                 if IN[i].coordinates[1] == IN[j].coordinates[1]:
                     SN.append(IN[i])
     return SN


def doubles3():
     "then avoid uselessly calling a constant operation"
     SN = []
     in_len = len(IN)
     for i in xrange(in_len):
         for j in xrange(i+1, in_len):
             if IN[i].coordinates[0] == IN[j].coordinates[0]:
                 if IN[i].coordinates[1] == IN[j].coordinates[1]:
                     SN.append(IN[i])
     return SN

def doubles4():
     "then alias commonly used methods"
     SN = []
     sn_append = SN.append
     in_len = len(IN)
     for i in xrange(in_len):
         for j in xrange(i+1, in_len):
             if IN[i].coordinates[0] == IN[j].coordinates[0]:
                 if IN[i].coordinates[1] == IN[j].coordinates[1]:
                     sn_append(IN[i])
     return SN

def doubles5():
     "then alias a couple other things"
     SN = []
     sn_append = SN.append
     in_len = len(IN)
     for i in xrange(in_len):
         node_i = IN[i]
         coords_i = node_i.coordinates
         for j in xrange(i+1, in_len):
             if coords_i[0] == IN[j].coordinates[0]:
                 if coords_i[1] == IN[j].coordinates[1]:
                     sn_append(node_i)
     return SN

def doubles6():
     "then simplify tests"
     SN = []
     sn_append = SN.append
     in_len = len(IN)
     for i in xrange(in_len):
         node_i = IN[i]
         coords_i = node_i.coordinates
         for j in xrange(i+1, in_len):
             if coords_i == IN[j].coordinates:
                 sn_append(node_i)
     return SN

# Harald : uncomment this and run test_results. As far as I can tell, it
# doesn't yields the expected results

## IN7 = IN[:]
## def sortk7(n):
##     return n.coordinates[0]

## def doubles7():
##     "is ordering better ? - Nope Sir, it's broken..."
##     IN7.sort(key=sortk)
##     SN = []
##     sn_append = SN.append
##     in_len = len(IN)
##     for i in xrange(in_len):
##         node_i = IN[i]
##         coords_i = node_i.coordinates
##         for j in xrange(i+1, in_len):
##             if coords_i[0] == IN[j].coordinates[0]:
##                 if coords_i[1] == IN[j].coordinates[1]:
##                     sn_append(node_i)
##             else:
##                 break

##     return SN


def doubles8():
     "Using a set ?"
     dup = set()
     SN = []
     for item in IN:
         c = item.coordinates
         if c in dup:
             SN.append(item)
         else:
             dup.add(c)
     return SN


def doubles9():
     "Using a set is way faster - but can still be improved a bit"
     dup = set()
     dup_add = dup.add
     SN = []
     sn_append = SN.append

     for item in IN:
         c = item.coordinates
         if c in dup:
             sn_append(item)
         else:
             dup_add(c)
     return SN


# need this for tests:
names_funcs = sorted(
     (n, f) for n, f in locals().iteritems()
     if n.startswith('doubles')
     )

def test_results():
     """
     make sure all solutions give correct results,
     assuming the OP solution is at least correct !-)
     """
     results = [
        (n, set(o.coordinates for o in f()))
        for n, f in names_funcs
        ]
     _, correct = results[0]
     ok = True
     for n, r in results[1:]:
         if r != correct:
             print "error on %s ?\n expected %s\n, got %s" \
                 % (n, correct, r)
             ok = False
     return ok


def test_times():
	" And the winner is..."
	results = []
	from time import time
	tt0 = time ()
	# XXXX: The first test takes 50% of the time and the last tests 0.1%!
	# I've added a multiplication factor to test all fairly -- st.
	for (n, f), nn in zip (names_funcs, [1, 2, 2, 2, 2, 3, 5, 100, 100]):
		t0 = time ()
		for i in xrange (50*nn):
			f ()
		t0 = time () - t0
		print n, t0, t0 / nn
	print time () - tt0
		
import sys
if "SEM" in sys.argv:
	test_results ()
else:
	test_times ()
