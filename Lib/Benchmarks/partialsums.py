################################################ build & benchmark results
# The Computer Language Shootout
# http://shootout.alioth.debian.org/
# contributed by Josh Goldfoot

#DEJAVU
'''
{
'NAME':"Partial Sums",
'DESC':"Partial sums from computer language shootout",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"1",
'BARGS':"12"
}
'''

from sys import argv

def main (n):
	from math import sin, cos, sqrt
	alt = 1.0
	twoThirds = 2.0 / 3.0
	sums0 = sums1 = sums2 = sums3 = sums4 = sums5 = \
        	sums6 = sums7 = sums8 = 0.0
	for k in xrange(1, n):
	    k2 = k * k
	    k3 = k2 * float (k)
	    ks, kc = sin(k), cos(k)
	    sums0 += twoThirds ** (k - 1)
	    sums1 += 1.0 / sqrt(k)
	    sums2 += 1.0 / (k * (k + 1))
	    sums3 += 1.0 / (k3 * ks * ks)
	    sums4 += 1.0 / (k3 * kc * kc)
	    sums5 += 1.0 / k
	    sums6 += 1.0 / k2
	    sums7 += alt / k
	    sums8 += alt / (2 * k - 1)
	    alt = -alt

	print "%0.9f\t(2/3)^k" % sums0
	print "%0.9f\tk^-0.5" % sums1
	print "%0.9f\t1/k(k+1)" % sums2
	print "%0.9f\tFlint Hills" % sums3
	print "%0.9f\tCookson Hills" % sums4
	print "%0.9f\tHarmonic" % sums5
	print "%0.9f\tRiemann Zeta" % sums6
	print "%0.9f\tAlternating Harmonic" % sums7
	print "%0.9f\tGregory" % sums8

for i in range (int (argv [1])):
	main (25001)

