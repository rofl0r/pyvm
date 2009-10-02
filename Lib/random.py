##  Random numbers
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) version 3 of the License.

# our goal is to have a random number generator compatible with
# python 2.4/2.5, mainly for running benchmarks, where given
# the same seed we'll get the same sequence of random numbers.
# This works for the simple functions random(), randint(), etc.

lib = modules.DLL.Clib ("mt19937ar", "-O2 -g", pelf="try")
init_by_array = lib ["init_by_array"]
# Initial seed
def init ():
	import time
	t = str (time.time ()) + time.cpu_ticks () + str (id (time))
	init_by_array (t, len (t) / 4)
init ()
# 'seed' should be used *only* to replicate benchmarks. Our own initialization
# should be the best option for non-predictable seeds.
def seed (i):
	init_by_array ("%ai" % i, 1)
random = lib ["genrand_res53"]
random_int32 = lib ["genrand_int32"]
def randint (a, b):
	return int (a + int (random () * (1 + b - a)))
def choice (seq):
	return seq [int (random () * len (seq))]
def shuffle (x, random=random, int=int):
	for i in xrange (len (x) - 1, 0, -1):
		# pick an element in x[:i+1] with which to exchange x[i]
		j = int (random () * (i+1))
		x[i], x[j] = x[j], x[i]
	return x
def coin ():
	return random_int32 () & 1
def randstr (n):
	return "".join ([chr (random_int32 () & 255) for x in range (n)])
def hrandstr (n):
	return "".join ([chr (random_int32 () & 255) for x in range (n)]).hexlify ()
del lib, init
