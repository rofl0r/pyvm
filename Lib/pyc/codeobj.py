##  Intermediate code object class
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) version 3 of the License.

from types import CodeType as BuildCodeObject
from gco import GCO

class ConstCollector:
	def __init__ (self):
		self.D = {}
	def add (self, x):
		t = type (x), x
		try:
			return self.D [t]
		except:
			n = len (self.D)
			self.D [t] = n
			return n
	def tolist (self):
		L = [(j, i [1]) for i, j in self.D.iteritems ()]
		L.sort ()
		L2 = []
		for i in L:
			i = i [1]
			# something might have been interned because it is used as
			# a key to a subscript. Other uses of the same string are also
			# interned.
			if i in GCO.INTERNS and GCO.INTERNS [i] is not i:
				L2.append (GCO.INTERNS [i])
			else:
				L2.append (i)
		return L2

class CodeObject:
	def __init__ (self, params):
		self.params = params

	def to_real_code (self):
		global StoredGnt

		for c in self.params [5]:
			if isinstance (c, CodeObject):
				break
		else:	# leaf
			if not StoredGnt:
				StoredGnt = True
				params = list (self.params)
				params [6] = tuple (GCO ['global_names_list'])
				#print "ShNT:", len (params [6])
				self.params = tuple (params)
			return BuildCodeObject (*self.params)

		params = list (self.params)
		consts = list (params [5])
		for i, c in enumerate (consts):
			if isinstance (c, CodeObject):
				consts [i] = c.to_real_code ()
		params [5] = tuple (consts)

		return BuildCodeObject (*params)

	def __repr__ (self):
		return "<temporary code object>"

def RealCode (c):
	global StoredGnt
	StoredGnt = not GCO ['gnt']
	if GCO ['gnc']:
		g = GCO ['global_consts_list'].tolist ()
		GCO ['global_names_list'].append (tuple (g))
	return c.to_real_code ()
