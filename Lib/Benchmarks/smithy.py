# this appears to be a decoder of the davinci code
# from http://online.effbot.org/
#
# Next best sellers
#  "Michael Angelo and the Secret of The Crypt"
#  "Phsycotherapeutic sessions with Fyodor Dostoyevksi"

#DEJAVU
'''
{
'NAME':"Smithy",
'DESC':"Smithy Davinci Decoder",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"SEM",
'BARGS':""
}
'''


from string import uppercase as alphabet
from itertools import izip, cycle
import sys

smithy_code = "JAEIEXTOSTGPSACGREAMQWFKADPMQZVZ"

def fib(n): # from the python tutorial
    a, b = 0, 1
    while b < n:
        yield b
        a, b = b, a+b

def decode(code):
    for c, d in izip(code, cycle(fib(len(alphabet)))):
        yield alphabet[(alphabet.find(c) + d - 1) % len(alphabet)]

def main ():
	for i in xrange (400):
		SC = 100 * smithy_code;
		for i in decode(SC):
			pass

if 'SEM' in sys.argv:
	print ''.join (list (decode (smithy_code)));
else:
	main ()
