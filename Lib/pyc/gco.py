##  Global Compiler Options
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) version 3 of the License.

# Used for recusrively reentrant compilation. 

try:
	set
except:
	from sets import Set as set

class GCO:
    def __init__ (self):
	self.CSTACK = []
	self.CURRENT = None
	self.INTERNS = {}
    def Intern (self, x):
	if len (x) < 2:
		return x
	try:
		return self.INTERNS [x]
	except:
		x = self.INTERNS [x] = intern (x)
		return x
    def new_compiler (self):
	self.CSTACK.append ((self.CURRENT, self.INTERNS))
	self.CURRENT = { 'have_with':False }
	self.INTERNS = {}
    def pop_compiler (self):
	self.CURRENT, self.INTERNS = self.CSTACK.pop ()
    def __getitem__ (self, x):
	return self.CURRENT [x]
    def __setitem__ (self, x, y):
	self.CURRENT [x] = y

GCO = GCO()
