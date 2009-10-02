##  Python-derrived socket module
## 
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) version 3 of the License.

import _socket
from _JIT import fptr_wrapper
from os import remove as RemoveFile
from net.socketfile import fileobj as fileobj2
import thread

for i, j in _socket.__dict__.iteritems ():
	if i == i.upper ():
		globals()[i] = j

# This code used to try to be compatible with python sockets
# a long time ago. In the process, it has been heavily modified
# and tweaked for various network services and compatibility
# has been stopped.

socket_call          = fptr_wrapper ('i', _socket.socket, 'iii')
bind_af_inet_call    = fptr_wrapper ('i', _socket.bind_af_inet, 'iiii')
bind_af_unix_call    = fptr_wrapper ('i', _socket.bind_af_unix, 'is')
connect_af_unix_call = fptr_wrapper ('i', _socket.connect_af_unix, 'is')
listen_call          = fptr_wrapper ('i', _socket.listen, 'ii')
close_call           = fptr_wrapper ('', _socket.close, 'i')
setsockopt_call      = fptr_wrapper ('i', _socket.setsockopt, 'iiii')
getsockname_af_inet  = fptr_wrapper ('', _socket.getsockname_af_inet, 'ip32')

inet_addr = fptr_wrapper ('i', _socket.inet_addr, 's')
recv = _socket.recv
send = _socket.send
gethostname = _socket.gethostname
accept_af_inet = _socket.accept_af_inet
accept_af_unix = _socket.accept_af_unix
connect_af_inet_call = _socket.connect_af_inet
inet_ntoa = fptr_wrapper ('s', _socket.inet_ntoa, 'i')

class Timeout(Exception):
	pass

class DNSError(Exception):
	pass

STATS = 0

class socket:
	# limited to 2GB each
	NetStats = [0, 0]
	DoNetStats = False

	def __init__ (self, family=AF_INET, type=SOCK_STREAM, proto=0, fd=-1, timeout=-1):
		if family != AF_INET:
			self.DoNetStats = False

		if gethostbyname is None:
			import_dns ()
		self.secured = False
		self._timeout_ = timeout
		self.family = family
		self.type = type
		self.proto = proto
		self.addr = None
		if fd == -1:
			fd = socket_call (family, type, proto)
			if fd == -1:
				raise Error
		self.fd = fd
		if STATS:
			self.BytesIn = self.BytesOut = 0

	# connections
	def bind (self, addr, reusable=False):
		if reusable:
			self.setsockopt (SOL_SOCKET, SO_REUSEADDR, 1)
		if self.family == AF_INET: ccall = bind_af_inet_call
		elif self.family == AF_UNIX: ccall = bind_af_unix_call
		r = self.bind_or_connect (addr, ccall, -1)
		if r == -1:
			raise Error ("Address already in use")
		if r >= 0 and self.family == AF_UNIX:
			self.unixname = addr
		return r
	def connect (self, addr, timeout=-1):
		self.addr = addr
		if self.family == AF_INET: ccall = connect_af_inet_call
		elif self.family == AF_UNIX: ccall = connect_af_unix_call
		if self.bind_or_connect (addr, ccall, timeout) == -1:
			raise Error ("Connection refused")
	def bind_or_connect (self, addr, ccall, timeout):
		# XXXX: nslookup if address is 'daring.cwi.nl'
		if timeout is None:
			timeout = -1
		if self.family == AF_INET:
			addr, port = addr
			if type (addr) is str:
				addr = addr or '0.0.0.0'
				try:
					addr = gethostbyname (addr)
					if not addr:
						raise DNSError
					addr = inet_addr (addr)
				except SystemExit:
					raise
				except thread.Interrupt:
					raise
				except:
					print sys.exc_info ()
					raise
			r = ccall (self.fd, addr, port, timeout)
			return r
		if self.family == AF_UNIX:
			return ccall (self.fd, addr);
		raise Error ("family not AF_INET");
	def listen (self, n=1):
		return listen_call (self.fd, n)
	def accept (self, timeout=-1):
		if timeout is None:
			timeout = -1
		if self.family == AF_INET:
			fd, host, port = accept_af_inet (self.fd, timeout)
			if fd == -1:
				raise Error ("accept returned -1")
			if fd == -2:
				raise Timeout
			return socket (AF_INET, self.type, self.proto, fd), (inet_ntoa (host), port)
		elif self.family == AF_UNIX:
			fd = accept_af_unix (self.fd, timeout)
			if fd == -1:
				raise Error ("accept returned -1")
			if fd == -2:
				raise Timeout
			return socket (AF_UNIX, self.type, self.proto, fd)
		raise Error ("family not supported")

	# Base send/recv
	def low_recv (self, buflen, flags, timeout):
		if timeout is None:
			timeout = -1
		data = recv (self.fd, buflen, flags, timeout)
		if data == -3:
			# This has been observed in the bitorrent client.
			# It is NOT supposed to happen since the recv happens
			# after the poll() has given us the OK. Maybe two
			# simultaneous threads using recv() bug?
			##print "EAGAIN in recv!"
			return self.low_recv (buflen, flags, timeout)
		if data == -2:
			raise Timeout
		if data == -1:
			raise Error ("error in recv")
		if self.DoNetStats:
			try:
				self.NetStats [1] += len (data)
			except:
				print "TEH CRAPS:", data
		return data
	def low_send (self, data, flags, timeout):
		# unlike python, which returns the number of bytes sent, this
		# function:
		# 1) will send *all* the data
		# 2) - if this works it will return True
		#    - if the socket is closed (ECONNRESET) it will return False
		#    - for any other error (not connected, etc) or timeout, it will raise
		# Generally, a lot of protocols do not have to test the success of
		# send(). Many of them expect a confirmation which will detect failure
		# at the recv(). So putting a `try send()` every time is not practical;
		# protocols that want to verify sends will have to check the return value.
		if timeout is None:
			timeout = -1
		elif timeout >= 0:
			ttime = now () + timeout
		else:
			timeout = -1
		res = send (self.fd, data, flags, timeout)
		if res == -3:
			return False
		if res == -1:
			raise Error ("error in send")
		if res == -2:
			raise Timeout
		if self.DoNetStats:
			self.NetStats [0] += res
		if 0 < res < len (data):
			# (xxx: pass offset instead of slice/copy)
			if timeout != -1:
				timeout = max (ttime - now (), 0)
			return self.send (data [res:], flags, timeout)
		return True
	# counting
	def cnt_send (self, data, flags, timeout):
		self.BytesOut += len (data)
		return self.low_send (data, flags, timeout)
	def cnt_recv (self, buflen, flags, timeout):
		data = self.low_recv (buflen, flags, timeout)
		self.BytesIn += len (data)
		return data;

	#
	def do_send (self, data, flags, timeout):
		if STATS:
			return self.cnt_send (data, flags, timeout)
		return self.low_send (data, flags, timeout)
	def do_recv (self, buflen, flags, timeout):
		if STATS:
			return self.cnt_recv (buflen, flags, timeout)
		return self.low_recv (buflen, flags, timeout)


	# misc
	def setsockopt (self, level, optname, value):
		return setsockopt_call (self.fd, level, optname, value)
	def getsockname (self):
		v = array ('i', (0,0))
		getsockname_af_inet (self.fd, v)
		return inet_ntoa (v[0]), v[1]
	# interesting extension
	def recvall (self, timeout=None):
		L = []
		while 1:
			x = self.recv (timeout=timeout)
			if not x:
				return "".join (L)
			L.append (x)
	# mostly for compat
	def makefile (self, mode, bufsize=4096):
		return fileobj (self, mode, bufsize)
	def settimeout (self, val):
		self._timeout_ = val

	# like `makefile` but more advanced for pyvm
	def socketfile (self):
		return fileobj2 (self)

	# shutdown
	def close (self):
		if self.fd != -1:
			if STATS:
				print "Closed socket %i Kbytes in, %i Kbytes out" %(self.BytesIn/1024,
										  self.BytesOut/1024)
			close_call (self.fd)
			self.fd = -1
			if hasattr (self, 'unixname'):
				RemoveFile (self.unixname)
	def __del__ (self):
		self.close ()

	### Secure with the tls.pe ###
	def secure_tls (self):
		from net import tls
		self.tls_channel = tls.TLS (self).SecureChannel ()
		self.secured = "tls"
	crypted = False
	def set_crypt (self, encryptf, decryptf):
		self.crypted = True
		self.encryptf = encryptf
		self.decryptf = decryptf
	#
	def send (self, data, flags=0, timeout=None):
		if self.secured:
			return self.tls_channel.send (self.do_send, data, flags, timeout)
		if self.crypted:
			data = self.encryptf (data)
		return self.do_send (data, flags, timeout)
	def recv (self, buflen=4*4096, flags=0, timeout=None):
		if self.secured:
			return self.tls_channel.recv (self.do_recv, buflen, flags, timeout)
		if self.crypted:
			data = self.do_recv (buflen, flags, timeout)
			return self.decryptf (data)
		return self.do_recv (buflen, flags, timeout)

# convenient TCP client/server functions

def Listen (port, reusable=False):
	s = socket ()
	s.bind (("", port), reusable=reusable)
	s.listen (2)
	return s

def Connect (host, port, timeout=-1):
	s = socket ()
	s.connect ((host, port), timeout)
	return s

del _socket, fptr_wrapper

# socket-as-file object.  In pyvm we have another better buffering
# object in net.socketfile.  This exists for compatibility with python
# apps, but will be removed at some time.

class fileobj:
	def __init__ (self, sock, mode, bufsize=8192):
		self.sock = sock
		if bufsize < 0:
			bufsize = 8192
		self.maxlinelen = 2 * bufsize
		self.bufsize = bufsize
		self.mode = mode
		if 'w' in mode:
			self.wbuf = []
			self.wlen = 0
		if 'r' in mode:
			self.rbuf = ''
	def write (self, s):
		self.wbuf.append (s)
		self.wlen += len (s)
		if self.wlen > self.bufsize:
			self.flush ()
	def send (self, s):
		self.write (s)
		self.flush ()
	def flush (self):
		try:
			if self.wbuf:
				self.sock.send (''.join (self.wbuf))
				self.wbuf = []
				self.wlen = 0
		except SystemExit:
			raise
		except:
			pass
	def close (self):
		self.flush ()
		self.sock = None

	def closed (self):
		return self.sock is None

	def readline (self, timeout=-1, nl='\n'):
		while nl not in self.rbuf:
			data = self.sock.recv (self.bufsize, timeout=timeout)
			if not data:
				data = self.rbuf
				self.rbuf = ''
				return data
			self.rbuf += data
			if len (self.rbuf) > self.maxlinelen:
				raise ValueError, "pyvm/Lib/socket.py: line too long"
		i = self.rbuf.find (nl) + len (nl)
		data = self.rbuf [:i]
		self.rbuf = self.rbuf [i:]
		return data
	def read (self, size=-1, timeout=None):
		if size <= 0:
			return self.recv (size, timeout)
		if size <= len (self.rbuf):
			data = self.rbuf [:size]
			self.rbuf = self.rbuf [size:]
			return data
		buf = self.rbuf
		rbuf = []
		buffers = [buf]
		l = len (buf)
		while l < size:
			data = self.sock.recv (size - l, timeout=timeout)
			if not data:
				break
			buffers.append (data)
			l += len (data)
		buf = ''.join (buffers)
		self.rbuf = buf [size:]
		return buf [:size]
	def recv (self, size=-1, timeout=None):
		if self.rbuf:
			r, self.rbuf = self.rbuf, ''
			return r
		return self.sock.recv (size, timeout=timeout)
	def __iter__ (self):
		while True:
			l = self.readline ()
			if not l:
				return
			yield l

# we are using our own resolver and not the one found in
# libc. This has a circular import issue: dns uses sockets
# and sockets need gethostbyname() from dns. This function
# will resolve this situation and will be set from net.dns
# once the module is ready.

def import_dns ():
	import net.dns

gethostbyname = None

# This is from Python/Lib/socket.py

def getfqdn(name=''):
	name = name.strip()
	if not name or name == '0.0.0.0':
		name = gethostname()
	try:
		hostname, aliases, ipaddrs = gethostbyaddr(name)
	except:
		pass
	else:
		aliases.insert(0, hostname)
		for name in aliases:
			if '.' in name:
				break
		else:
			name = hostname
	return name
#####

# sleep for `dt` seconds, yielding packets that come in from a socket in the meantime.
# extremely useful for network client-server testing to send and receive from the same
# thread.

def recvsleep (sock, dt):
	t = now () + dt

	while 1:
		dt = t - now ()
		if dt <= 0:
			break
		try:
			p = sock.recv (timeout=dt)
			yield p
			if not p:
				break
		except (Timeout):
			return
