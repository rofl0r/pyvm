##  HMAC (Keyed-Hashing for Message Authentication)
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) version 3 of the License.

from md5 import digest

def strxor (s, v):
	return ''.join ([chr (ord (x) ^ v) for x in s])

def hmac (key, msg, digest=digest):
	key = key + chr (0) * (64 - len (key));
	return digest (strxor (key, 0x5C) + digest (strxor (key, 0x36) + msg));

def HMAC (key, digest):
	key += _buffer (64 - len (key), 0);
	k1 = strxor (key, 0x5C)
	k2 = strxor (key, 0x36)
	def Digest (msg):
		return digest (k1 + digest (k2 + msg))
	return Digest

if __name__ == __main__:
	print hmac ("key", "The Message").hexlify ()
