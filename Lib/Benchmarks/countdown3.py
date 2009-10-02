'''
Date: Sat, 26 Jan 2008 16:13:01 +0000
From: Arnaud Delobelle
To: python-list@python.org
Newsgroups: gmane.comp.python.general
Subject: Re: Just for fun: Countdown numbers game solver
====================== countdown.py =======================
'''

#DEJAVU
'''
{
'NAME':"Countdown",
'DESC':"Different methods for countdown problem",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"SEM",
'BARGS':""
}
'''

def getop(h):
	if isinstance(h, int):
		return 'n'
	return h[1]

# An ops function takes two numbers with histories and yield all  suitable
# ways of combining them together.

def ops(a, b):
     if a < b: a, b = b, a
     x, hx = a
     y, hy = b
     yield x + y, (a, '+', b)
     if x != 1 and y != 1:
         yield x * y, (a, '*', b)
     if x != y:
         yield x - y, (a, '-', b)
     if not x % y and y != 1:
         yield x / y, (a, '/', b)

def cleverops(a, b, getop=getop):
     if a < b: a, b = b, a
     x, hx = a
     y, hy = b
     opx, opy = getop(hx), getop(hy)
     # rx is the right operand of hx (or x if no history)
     if opx == 'n':
	rx = x
     else:
        rx = hx[2][0]
     if opy not in '+-':
         # Only allow a+b+c-x-y-z if a >= b >= c...
         if (opx == '+' and rx >= y) or (opx not in '+-' and x >= y):
             yield x + y, (a, '+', b)
         # ... and x >= y >= z
         if x > y and (opx != '-' or rx >= y):
             yield x - y, (a, '-', b)
     if y != 1 and opy not in '*/':
         # Only allow a*b*c/x/y/z if a >= b >= c...
         if (opx == '*' and rx >= y) or (opx not in '*/' and x >= y):
             yield x * y, (a, '*', b)
         # ... and x >= y >= z
         if not x % y and (opx != '/' or rx >= y):
             yield x / y, (a, '/', b)

# a method function takes a list of numbers, an action, and and ops
# function.  It should go through all ways of combining the numbers
# together (using ops) and apply action to each.

def fold(nums, action, ops=cleverops):
     "Use the 'origami' approach"
     nums = zip(nums, nums) # Attach a history to each number
     def recfold(start=1):
         for i in xrange(start, len(nums)):
             a, ii = nums[i], i-1     # Pick a number;
             for j in xrange(i):
                 b = nums.pop(j)      # Take out another before it;
                 for x in ops(a, b):  # combine them
                     nums[ii] = x     # into one;
                     action(*x)       # (with side-effect)
                     recfold(ii or 1) # then fold the shorter list.
                 nums.insert(j, b)
             nums[i] = a
     recfold()

def divide(nums, action, ops=cleverops):
     "Use the 'divide and conquer' approach"
     def partitions(l):
         "generate all 2-partitions of l"
         for i in xrange(1, 2**len(l)-1, 2):
             partition = [], []
             for x in l:
                 i, r = divmod(i, 2)
                 partition[r].append(x)
             yield partition
     def recdiv(l):
         if len(l) == 1: # if l in a singleton,
             yield l[0]  # we're done.
         else:
             for l1, l2 in partitions(l):    # Divide l in two;
                 for a in recdiv(l1):        # conquer the two
                     for b in recdiv(l2):    # smaller lists;
                         for x in ops(a, b): # combine results
                             action(*x)      # (with side-effect)
                             yield x         # and yield answer.
     for x in recdiv(zip(nums, nums)): pass

# Countdown functions

def all_countdown(nums, target, method=fold, ops=cleverops):
     "Return all ways of reaching target with nums"
     all = []
     def action(n, h):
         if n == target:
             all.append(h)
     method(nums, action, ops)
     return all

def print_countdown(nums, target, method=fold, ops=cleverops):
     "Print all solutions"
     def action(n, h):
         if n == target:
             print pretty(h)
     method(nums, action, ops)

class FoundSolution(Exception):
     "Helper exception class for first_countdown"
     def __init__(self, sol):
         self.sol = sol

def first_countdown(nums, target, method=fold, ops=cleverops):
     "Return one way of reaching target with nums"
     def action(n, h):
         if n == target:
             raise FoundSolution(h)
     try:
         method(nums, action, ops)
     except FoundSolution, fs:
         return pretty(fs.sol)

# Pretty representation of a number's history

lbracket = ['+*', '-*', '+/', '-/', '/*']
rbracket = ['*+', '*-', '/+', '/-', '/*', '-+', '--']

def pretty(h):
     "Print a readable form of a number's history"
     if isinstance(h, int):
         return str(h)
     else:
         x, op, y = h
         x, y = x[1], y[1]
         x, y, xop, yop = pretty(x), pretty(y), getop(x), getop(y)
         if xop + op in lbracket: x = "(%s)" % x
         if op + yop in rbracket: y = "(%s)" % y
         return ''.join((x, op, y))

# This test function times a call to a countdown function, it allows
# comparisons between differents things (methods, ops, ...)

def test(enumkey=None, **kwargs):
     from time import time
     def do_countdown(countdown=all_countdown, target=758,
                       nums=[2, 4, 5, 8, 9, 25], **kwargs):
         return countdown(nums, target, **kwargs)
     if enumkey:
          enum = kwargs.pop(enumkey)
     else:
         enum = ['time']
     for val in enum:
         if enumkey: kwargs[enumkey] = val
         t0 = time()
         do_countdown(**kwargs)
         t1 = time()
         yield t1-t0

# Tools for generating random countdown problems and doing random
# tests.

import sys
if 'SEM' in sys.argv:
	v = [1,2,3,4,5]
	r = 43
	n = 1
else:
	v = [50,8,5,4,9]
	r = 983
	n = 10

for i in xrange (n):
	print_countdown(v, r, method=divide, ops=cleverops)
	print_countdown(v, r, method=fold, ops=cleverops)
	print_countdown(v, r, method=divide, ops=ops)
	print_countdown(v, r, method=fold, ops=ops)
