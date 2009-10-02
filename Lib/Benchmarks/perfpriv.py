#DEJAVU
'''
{
'NAME':"Perfect Privacy",
'DESC':"Sample perfect privacy implementation",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"1",
'BARGS':"316"
}
'''

import sys

NNN = int (sys.argv [1])

def ords (s):
	return [ord (x) for x in s]

##
##  Level 0.  Non linear pad generation from tree
##

def ichain (t):
	while 1:
		for i in t:
			yield i

class node:
	def __init__ (self, val):
		self.val = val
		self.left = self.right = None
	def inorder (self, L):
		if self.left:
			self.left.inorder (L)
		L.append (self.val)
		if self.right:
			self.right.inorder (L)

class ctree:
	def __init__ (self, key):
		self.kv = ichain (key)

	def insert (self, v):
		n = self.root
		for d in self.kv:
			if d:
				if n.left:
					n = n.left
				else:
					n.left = node (v)
					break
			else:
				if n.right:
					n = n.right
				else:
					n.right = node (v)
					break

	def insert_values (self, vals):
		self.root = node (vals [0])
		for v in vals [1:]:
			self.insert (v)

	def inorder (self):
		L = []
		self.root.inorder (L)
		return L

# Procedure T

def method_T (values, key):
	t = ctree (key)
	t.insert_values (values)
	return t.inorder ()

print "For Key:", [1,0,0,1,0,1,1]

# The first example in the tutorial
print "Procedure T:"
print method_T (range (12), [1, 0, 0, 1, 0, 1, 1])

# Procedure C
print "Procedure C:"
def method_C (values, key):
	return method_T (method_T (values, key), key)

print method_C (range (12), [1, 0, 0, 1, 0, 1, 1])

print '=============================='

##
##  Level 1
##

##
##
## Generally, perfect privacy uses 128-bits keys (16 bytes).
## The MD5 digest is also 16 bytes, so the MD5 of something gives
## another suitable key.
##
##

import md5
def MD5 (key):
	return md5.new (key).digest ()

def bitstring (s):
	L = []
	for i in s:
		i = ord (i)
		for b in (1,2,4,8,16,32,64,128):
			L.append (bool (i & b))
	return tuple (L)

# Procedure PP0

def PP0 (M, K1, XORVALUES=None):
	K2 = MD5 (K1)
	if XORVALUES is None:
		XORVALUES = range (len (M))
	C1 = method_C (XORVALUES, bitstring (K1))
	C2 = method_C (range (len (M)), bitstring (K2))
	E1 = [0] * len (M)
	E2 = [0] * len (M)
	# XOR pad from K1
	for i in range (len (M)):
		E1 [i] = ord (M [i]) ^ (C1 [i] % 256)
	# transpose on K2
	for i in range (len (M)):
		E2 [i] = E1 [C2 [i]]
	return ''.join ([chr (x) for x in E2])

key = '0123456789abcdef'
message = "Hello world" * NNN
cipher = PP0 ("Hello world", key)

#print "Procedure PP0 on message [%s] with key [%s], gives cipher [%s]" % (message, key,
#		 [ord (x) for x in cipher])

# Procedure RP0, decrypt

def RP0 (M, K1, XORVALUES=None):
	K2 = MD5 (K1)
	if XORVALUES is None:
		XORVALUES = range (len (M))
	C1 = method_C (XORVALUES, bitstring (K1))
	C2 = method_C (range (len (M)), bitstring (K2))
	E1 = [0] * len (M)
	E2 = [0] * len (M)
	for i in range (len (M)):
		E2 [C2 [i]] = ord (M [i])
	for i in range (len (M)):
		E1 [i] = E2 [i] ^ (C1 [i] % 256)
	return ''.join ([chr (x) for x in E1])

message2 = RP0 (cipher, key)

print "Verified that decryption works:", message == message2
print '===================================='

##
##  Level 2.  Perfect privacy (is not negotiable)
##

##
## The random key is going to be encrypted with procedure
## PP0 at bit level
##

from random import random, seed
seed (12345) # same random every time

def expandbits (s):
	r = ''
	for i in bitstring (s):
		if i: r += '\1'
		else: r += '\0'
	return r

def restorbits (s):
	v = [0] * (len (s) / 8)
	for i in range (len (s)):
		if s [i] == '\1':
			v [i / 8] |= 1 << (i % 8)
	return ''.join ([chr (x) for x in v])

print restorbits (expandbits (key)) == key

def encrypt (M, K):
	Kr = MD5 ('%.10f' %random ())
	E1 = PP0 (M, Kr)
	M2 = Kr + MD5 (Kr)
	M2Bits = expandbits (M2)
	E2 = PP0 (M2Bits, K, [0, 1] * 128)
	E2 = restorbits (E2)
	return E2 + E1, Kr

def decrypt (M, K):
	E2 = expandbits (M [:32])
	E1 = M [32:]
	Keys = RP0 (E2, K, [0, 1] * 128)
	Kr = restorbits (Keys [:128])
	if MD5 (Kr) == restorbits (Keys [128:]):
		print "OK, random key is [%s] and verified" %ords (Kr)
		return RP0 (M [32:], Kr)

cipher, kr = encrypt (message, key)
#print "Perfect privacy encryption for message [%s]\n with key [%s]\n gives cipher [%s]"% (
#	message, key, [ord (x) for x in cipher])
#print "The random key was:", ords (kr)

print "Decryption"
message2 = decrypt (cipher, key)
print "Perfect privacy successful:", message2 == message
