#! /usr/bin/env python

# Print digits of pi forever.
#
# The algorithm, using Python's 'long' integers ("bignums"), works
# with continued fractions, and was conceived by Lambert Meertens.
#
# See also the ABC Programmer's Handbook, by Geurts, Meertens & Pemberton,
# published by Prentice-Hall (UK) Ltd., 1990.

#DEJAVU
'''
{
'NAME':"Pi",
'DESC':"Digits of pi (long numbers) from Python/Demos",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"1800 SEM",
'BARGS':"4000"
}
'''

import sys

if 'pyvm' not in sys.copyright:
	Long = long

def pi():
    k, a, b, a1, b1 = Long (2), Long (4), Long (1), Long (12), Long (4)
    while 1:
        # Next approximation
        p, q, k = k * k, k * 2 + 1, k + 1
        a, b, a1, b1 = a1, b1, p * a + q * a1, p * b +q * b1
        # Print common digits
        d, d1 = a / b, a1 / b1
        while d == d1:
            yield d
            a, a1 = (a % b) * 10, (a1 % b1) * 10
            d, d1 = a / b, a1 / b1

def bench (N):
	j = 0
	for i in pi ():
		j += 1
		if j > N:
			break

def check (N):
	j = 0
	for i in pi ():
		j += 1
		if j > N:
			break
		print int (i),
	print

if 'SEM' in sys.argv:
	check (int (sys.argv [1]))
else:
	bench (int (sys.argv [1]))
