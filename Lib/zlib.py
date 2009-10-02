##  zlib module
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) version 3 of the License.

from _JIT import fptr_wrapper
import _zlibfuncs	# (xxx: deprecated TBR...)
import _zlib

class decompressor:

	def __init__ (self, data, offset=0, length=None, raw=False):
		if length is None:
			length = len (data) - offset
		self.z = _zlib.init (data, length, offset, int (raw))
		self.finished = False

	def unzip (self, n):
		out = _buffer (n)
		n2 = _zlib.unzip (self.z, out, n)
		if n2 < 0:
			raise Error ("zlib error")
		if n2 == n:
			return out
		self.finished = True
		return out [:n2]

	def unzip_all (self, explen=0):
		chunks = []
		if explen:
			chunks.append (self.unzip (explen))
		while not self.finished:
			chunks.append (self.unzip (8192))
		return "".join (chunks)

	def zavail (self):
		return _zlib.zavail_in (self.z)

	def __del__ (self):
		_zlib.end (self.z)

#
# zlib from the internally linked libz.a
#
_adler32 = fptr_wrapper ('i', _zlibfuncs.adler32, 'isi')
_crc32 = fptr_wrapper ('i', _zlibfuncs.crc32, 'isi')
compress2 = fptr_wrapper ('i', _zlibfuncs.compress2, 'sp32sii', True)
del fptr_wrapper, _zlibfuncs

Z_BUF_ERROR = -5
Z_SYNC_FLUSH = 2

#
#

def adler32 (string, value=0):
    return _adler32 (value, string, len (string))

def crc32 (string, value=0):
    return _crc32 (value, string, len (string))

def compress (string, level=6):
    sl = l = len (string)
    deststr = _buffer (l)
    destlen = array ('i', [l])
    while 1:
	rez = compress2 (deststr, destlen, string, sl, level)
	if rez == Z_BUF_ERROR:
	    l *= 2
	    deststr = _buffer (l)
	    destlen = array ('i', [l])
	    continue
	if rez:
	    raise Error ("ZLIB ERROR %i" %rez)
	break
    return deststr [:destlen [0]]

def decompress (string, wbits=None, bufsize=None, raw=False):
    return decompressor (string, raw=raw).unzip_all (bufsize)

def gunzip (zdata):
	from datastream import data_parser
	f = data_parser (zdata)
	f.match_raise ("\x1f\x8b")
	meth = f.r8 ()
	if meth != 8:
		raise Error ("unknown compression method")
	flags = f.r8 ()
	mtime = f.r32l ()
	eflags = f.rbyte ()
	os = f.rbyte ()

	if flags & 4:
		f.skip (f.r16l ())
	if flags & 8:
		while 1:
			x = f.r8 ()
			if not x:
				break
	if flags & 16:
		while 1:
			x = f.r8 ()
			if not x:
				break
	if flags & 2:
		f.rn (2)

	return decompressor (zdata, offset=f.offset, raw=True).unzip_all ()

if __name__ == '__main__':
	#XXXX: act as gunzip?
	D = open ('zlib.py').read ()
	print decompress (compress (D)) == D
