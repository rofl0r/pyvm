##  Optimizing bytecode assembler
##  Based on pyassem.py from python 2.4
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) version 3 of the License.

import sys
from pycmisc import list_to_string
import pycmisc as misc
from gco import GCO
from codeobj import CodeObject

try:
	CO_OPTIMIZED
except:
	from consts import CO_OPTIMIZED, CO_NEWLOCALS, CO_VARARGS, CO_VARKEYWORDS

try:
	set
except:
	from sets import Set as set
	from pycmisc import reversed, sorted

#
# *XXXX* If 2.5 comes out, we'll have to make this polymorphic
# For now, dis==arch24
#
import arch24 as dis

class NoVal:
	pass

class FlowGraph:
	def __init__(self):
		self.intry = 0
		self.current = self.entry = Block (0)
		self.exit = Block (1, "exit")
		self.bid = 2
		self.blocks = set ([self.entry, self.exit])

	def enterTry (self):
		self.intry += 1

	def leaveTry (self):
		self.intry -= 1

	def startBlock(self, block):
		b = self.current
		if b.has_break:
			l = self.control_blocks [-1]
			if l [0] != "LOOP":
				#print "extracon!", l
				pass
			else:
				l [2].IN.add (b)
		self.current = block

	def nextBlock(self, block=None):
		# XXX think we need to specify when there is implicit transfer
		# from one block to the next.  might be better to represent this
		# with explicit JUMP_ABSOLUTE instructions that are optimized
		# out when they are unnecessary.
		#
		# I think this strategy works: each block has a child
		# designated as "next" which is returned as the last of the
		# children.  because the nodes in a graph are emitted in
		# reverse post order, the "next" block will always be emitted
		# immediately after its parent.
		# Worry: maintaining this invariant could be tricky
		if block is None:
			block = self.newBlock()

		# Note: If the current block ends with an unconditional
		# control transfer, then it is incorrect to add an implicit
		# transfer to the block graph.  The current code requires
		# these edges to get the blocks emitted in the right order,
		# however. :-(  If a client needs to remove these edges, call
		# pruneEdges().

		self.current.addNext(block)
		self.startBlock(block)

	def setprop (self, k, v):
		setattr (self.current, k, v)

	def newBlock(self):
		b = Block(self.bid, tryblock=self.intry)
		self.bid += 1
		self.blocks.add(b)
		return b

	def startNewBlock (self):
		return self.startBlock (self.newBlock ())

	def startExitBlock(self):
#		self.nextBlock(self.exit)
		self.startBlock(self.exit)

	def emit(self, inst0, inst1=NoVal):
		if isinstance (inst1, Block):
			self.current.addOutEdge (inst1)
		self.current.emit ((inst0, inst1))

	blocksInOrder = None

	def getBlocksInOrder(self):
		if self.blocksInOrder is None:
			self.blocksInOrder = self._getBlocksInOrder ()
		return self.blocksInOrder

	def _getBlocksInOrder(self):
		# Return the blocks in reverse postorder
		# i.e. each node appears before all of its successors
		# XXX make sure every node that doesn't have an explicit next
		# is set so that next points to exit
		for b in self.blocks:
			if b is self.exit:
				continue
			if not b.next:
				b.addNext(self.exit)

		order = dfs_postorder(self.entry, set())
		order.reverse()
		self.fixupOrder(order, self.exit)
		# hack alert
		if not self.exit in order:
			"pyc::type order list"
			order.append(self.exit)

		return order

	def fixupOrder(self, blocks, default_next):
		# Fixup bad order introduced by DFS

		# XXX This is a total mess.  There must be a better way to get
		# the code blocks in the right order.

		self.fixupOrderHonorNext(blocks, default_next)
		self.fixupOrderForward(blocks, default_next)

	def fixupOrderHonorNext(self, blocks, default_next):
		# Fix one problem with DFS.	** UNUSED **
		#
		# The DFS uses child block, but doesn't know about the special
		# "next" block.  As a result, the DFS can order blocks so that a
		# block isn't next to the right block for implicit control
		# transfers.
		index = {}
		for i in range(len(blocks)):
			index[blocks[i]] = i

#		return
		for i in range(0, len(blocks) - 1):
			b = blocks[i]
			n = blocks[i + 1]
			if not b.next or b.next[0] == default_next or b.next[0] == n:
				continue
			print 100*'WARNING-1', b.next, [default_next]
			print "BLOCK ORDER BROKEN. Please send testcase to pyc-list"
			print CurrentFunc
			raise SystemExit

	def fixupOrderForward(self, blocks, default_next):
		# Make sure all JUMP_FORWARDs jump forward
		index = {}
		chains = []
		cur = []
		for b in blocks:
			index[b] = len(chains)
			cur.append(b)
			if b.next and b.next[0] == default_next:
				chains.append(cur)
				cur = []
		chains.append(cur)

		while 1:
			constraints = []

			for i in range(len(chains)):
				l = chains[i]
				for b in l:
					for c in b.get_children():
						if index[c] < i:
							forward_p = 0
							for inst in b.insts:
								if inst[0] == 'JUMP_FORWARD':
									if inst[1] == c:
										forward_p = 1
							if not forward_p:
								continue
							constraints.append((index[c], i))

			if not constraints:
				break

			# XXX just do one for now
			# do swaps to get things in the right order
			goes_before, a_chain = constraints[0]
			c = chains[a_chain]
			chains.remove(c)
			chains.insert(goes_before, c)

		blocks = []
		for c in chains:
			for b in c:
				blocks.append(b)

	def getBlocks(self):
		return self.blocks

	def getRoot(self):
		return self.entry

def dfs_postorder(b, seen):
	order = []
	seen.add (b)
	for c in b.get_children():
		if c in seen:
			continue
		order.extend (dfs_postorder(c, seen))
	order.append(b)
	return order

#
# We are scanning the instruction-string of the FForm with builtins.
# Either with string.find or regular expressions
#

import re
opchar = {}
for num in range(len(dis.opname)):
	opchar [dis.opname [num]] = chr (num)
opchar ['SET_LINENO'] = ''

def inst2string (iList, opchar=opchar):
	return ''.join ([opchar [x] for x in iList])

def regstr (i):
	if type (i) is not list:
		return i
	return re.escape (inst2string (i))

def inst2pattern (*r):
	return ''.join ([regstr (x) for x in r])

def inst2regex (*r):
	return re.compile (inst2pattern (*r), re.DOTALL).search

# the regexp syntax: inst2regex takes varargs.  An argument may be a string or
# a list of OPnames.  Strings are part of the regexp.  opcodes are converted to
# characters and escaped.

NOEFFECT_class = ['PRINT_NEWLINE']
##NOEFFECT_class = ['SETUP_LOOP', 'PRINT_NEWLINE', 'SETUP_EXCEPT', 'SETUP_FINALLY']
LOADERS_class = ['DUP_TOP', 'LOAD_CONST', 'LOAD_FAST']
ELOAD_class = ['LOAD_CONST', 'LOAD_FAST']
CJUMP_class = ['JUMP_IF_TRUE', 'JUMP_IF_FALSE']
CRAZY_regex = inst2regex (
	['ROT_THREE'], '[', ['STORE_FAST', 'POP_TOP'], ']{3}|',
	2*['ROT_TWO', 'ROT_THREE'], '|',
	2*['ROT_THREE', 'ROT_TWO'], '|',
	3*['DUP_TOP'], ['ROT_THREE']
)

########################################################################

class Block:
	passed = None
	has_break = False
	has_raise = False

	def __init__(self, bid, label='', tryblock=False):
		self.INTRAOPT = False
		self.tryblock = tryblock
		self.insts = []
		self.emit = self.insts.append
		self.outEdges = set()
		self.inEdges = []
		self.label = label
		self.bid = bid
		self.next = []
		self.IN = set ()

	def __repr__(self):
		if self.label:
			return "<block %s id=%d>" % (self.label, self.bid)
		else:
			return "<block id=%d>" % (self.bid)

	__str__ = __repr__

	def printBlock (self):
		print "Block %i" %self.bid, self.label
		print self.next, self.outEdges, self.inEdges
		for x in self.insts:
			print "  ", x

	# Data Flow analysis.  this function should be called by the `exit` block
	# and will recursively walk all the basic blocks.  The outcome is in the
	# dictionary `DF`.  The keys of DF are the local variables and the
	# values are the code points where each variable is live.  The code
	# points are arbitary numbers with no special meaning, just as long as
	# they are unique for every instruction.

	def dataflow (self, DF, vars):

		# loops make recursion. avoided if the bb has
		# been already studied with all the supplied vars

		if self.passed is not None:
			if vars - self.passed:
				vars = self.passed = self.passed | vars
			else:
				return
		else:
			self.passed = set (vars)

		offset = self.bid * 1000

		# A variable is "live" between a STORE and a LOAD
		# A variable is "dead" between a LOAD and a STORE
		# we iterate backwards.

		for i in reversed (self.insts):
			if i [0] in ("STORE_FAST", "DELETE_FAST"):
				if not self.tryblock:
					vars.discard (i [1])
			elif i [0] == "LOAD_FAST":
				vars.add (i [1])
			elif i [0] in ("SELF_ATTR", "SELF_ATTRW"):
				vars.add ("self")
			for v in vars:
				try:
					DF [v].append (offset)
				except:
					DF [v] = [offset]
			offset += 1
		
		for i in self.IN:
			i.dataflow (DF, set (vars))

	def getInstructions(self):
		return self.insts

	def addOutEdge(self, block):
		self.outEdges.add(block)
		block.inEdges.append (self)

	def addNext(self, block):
		self.next.append(block)
		block.inEdges.append (self)

	def get_children(self):
		if self.next:
			self.outEdges.discard (self.next [0])
		return list (self.outEdges) + self.next

	# * * * * * * * * * * * * Optimizer * * * * * * * * * * * * *
	#

	def fform (self):
		# F-Form (fast? flat?) is a data structure convenient for quickly
		# detecting optimizable patterns in basic blocks
		# The fform is generated once for each basic block and then it's
		# regenerated each time a transformation/optimization happens.
		iList = []
		vList = []
		pList = []
		sldFAST = {'S':[], 'L':[], 'D':[]}
		varinfo = {}
		for p, i in enumerate (self.insts):
			op, val = i
			if op != 'SET_LINENO':
				iList.append (op)
				if op.endswith ('_FAST'):
					sldFAST [op [0]].append (val)
					if val not in varinfo:
						varinfo [val] = []
					varinfo [val].append ((op, p))
				vList.append (val)
				pList.append (p)
		self.STORES, self.LOADS = cntDict (sldFAST ['S']), cntDict (sldFAST ['L'])
		self.DELS = cntDict (sldFAST ['D'])
		self.hasFAST = bool (self.STORES) or bool (self.DELS) or bool (self.LOADS)
		self.VARINFO = varinfo
		self.FFORM = inst2string (iList), vList, pList

	INTRAEND_regex = inst2regex ('(?:', ['LOAD_FAST'], '|', ['DUP_TOP', 'STORE_FAST'], ')',
	'[', CJUMP_class, ']$')
	BSTART_pattern = inst2regex ('^', ['POP_TOP'], ['LOAD_FAST'])

	def setIntra (self, UB):
		#
		# If a block ends with:    LOAD_FAST i, JUMP_IF_TRUE|JUMP_IF_FALSE
		#  or:			   DUP_TOP, STORE_FAST i, JUMP_IF_TRUE|JUMP_IF_FALSE
		# and the next block starts with:   POP_TOP, LOAD_FAST i
		# it is safe to eliminate the latter two instructions if the next block is this one.
		#
		inststr, vals, _ = self.FFORM
		if self.INTRAEND_regex (inststr):
			if (self.next and self.next [0] in UB 
		           and self.BSTART_pattern (self.next [0].FFORM [0]) and 
		           self.next [0].FFORM [1][1] == vals [-2]):
				self.next [0].INTRAOPT = True
			if (vals [-1] in UB and self.BSTART_pattern (vals [-1].FFORM [0])
		           and vals [-1].FFORM [1][1] == vals [-2]):
				vals [-1].INTRAOPT = True

	INLINE_UNPACK_regex = inst2regex ('[', ['BUILD_LIST', 'BUILD_TUPLE'], ']', ['UNPACK_SEQUENCE'])

	def optimize_once (self):
		# ``swap w/ pack-unpack''
		# BUILD_SEQ1 UNPACK_SEQ1: eliminate
		# BUILD_SEQ2 UNPACK_SEQ2 -> ROT2
		# BUILD_SEQ3 UNPACK_SEQ3 -> ROT3 ROT2
		# 
		inststr, vals, indexes = self.FFORM
		i = 0
		while 1:
			xx = self.INLINE_UNPACK_regex (inststr, i)
			if not xx:
				return
			i = xx.start ()
			if 0 < vals [i] == vals [i+1] <= 3:
				if vals [i] == 2:
					del self.insts [indexes [i+1]]
					self.insts [indexes [i]] = 'ROT_TWO', NoVal
				elif vals [i] == 3:
					self.insts [indexes [i]] = 'ROT_THREE', NoVal
					self.insts [indexes [i+1]] = 'ROT_TWO', NoVal
				else:
					del self.insts [indexes [i+1]]
					del self.insts [indexes [i]]
				self.fform ()
				inststr, vals, indexes = self.FFORM
				i = 0
			else:
				i += 1

	ROTREORD_regex = inst2regex (
		['DUP_TOP', 'ROT_TWO'], '|',
		['ROT_TWO'], '[', ['STORE_FAST', 'POP_TOP'], ']{2}|',
		['ROT_THREE'], '[', ['STORE_FAST', 'POP_TOP'], ']{3}|',
		'[', ELOAD_class, ']{2}', ['ROT_TWO'], '|',
		'[', ELOAD_class, ']', ['LOAD_GLOBAL', 'ROT_TWO'], '|',
		['LOAD_GLOBAL'], '[', ELOAD_class, ']', ['ROT_TWO'], '|',
		'[', ELOAD_class, ']{3}', ['ROT_THREE']
	)

	ELIM_regex = inst2regex ('[', LOADERS_class, '][', NOEFFECT_class, ']*', ['POP_TOP'], '|',
		['ROT_TWO'], '[', NOEFFECT_class, ']*', ['ROT_TWO'])

	def optimize0 (self):
		inststr, vals, indexes = self.FFORM
		insts = self.insts

		# ``inline rot''
		# DUP_TOP, ROT_TWO -> DUP_TOP
		# ROT_TWO, STORE_FAST x1, STORE_FAST x2 -> STORE_FAST x2, STORE_FAST x1
		# ROT_THREE, STORE_FAST x1, STORE_FAST x2, STORE_FAST x3 -> STORE_FAST x3, STORE_FAST x1, SF x2
		# LOAD_FAST x1, LOAD_FAST x2, ROT_TWO -> LOAD_FAST x2, LOAD_FAST x1
		# LOAD_FAST x1, LOAD_FAST x2, LOAD_FAST x3, ROT_THREE ->  ...
		#
		xx = self.ROTREORD_regex (inststr)
		if xx:
			i, j = xx.span ()
			if j-i == 3:
				i0, i1, i2 = indexes [i:j]
				if inststr [i] == opchar ['ROT_TWO']:
					insts [i1], insts [i2] = insts [i2], insts [i1]
					del insts [i0]
				else:
					insts [i0], insts [i1] = insts [i1], insts [i0]
					del insts [i2]
			elif j-i == 2:
				del insts [indexes [j-1]]
			else:
				i0, i1, i2, i3 = indexes [i:j]
				if inststr [i] == opchar ['ROT_THREE']:
					insts [i1], insts [i2], insts [i3] = insts [i3], insts [i1], insts [i2]
					del insts [i0]
				else:
					insts [i0], insts [i1], insts [i2] = insts [i2], insts [i0], insts [i1]
					del insts [i3]
			return True

		# ``eliminate no-effect''
		# DUP_TOP, POP_TOP -> eliminate
		# LOAD_CONST, POP_TOP -> eliminate
		# LOAD_FAST, POP_TOP -> eliminate
		#
		# Between these there can be opcodes with no stack effect, like
		# SETUP_LOOP, PRINT_NEWLINE, etc, which are unaffected
		# The 'right thing' would be to use get_stack_effect for this one
		# and not the regex thing...
		#
		xx = self.ELIM_regex (inststr)
		if xx:
			i1, i2 = indexes [xx.start ()], indexes [xx.end ()-1]
			del insts [i2]
			del insts [i1]
			return True

		# ``LOAD_FAST removals''

		if not self.LOADS:
			return False

		# ``LOAD_FAST removal''
		# If we have LOAD_FAST x, <some commands>, LOAD_FAST x, and the
		# stack effect of the middle commands is (0,0) convert to:
		# LOAD_FAST x, DUP_TOP, <some commands>
		#
		# STORE_FAST x may not be in <some commands>
		#
		# If there are more than 2 LOAD_FAST x in the basic block, we must do
		# it in reverse order because both may optimizable and we'll miss the
		# second if we optimize the first, first.
		#

		for i, j in self.LOADS.iteritems ():
			if j < 2:
				continue
			for k in reversed (pairlist (self.VARINFO [i])):
				if k [0][0] == 'LOAD_FAST' == k [1][0]:
					l0, l1 = k [0][1], k [1][1]
					T = get_stack_effect (insts [l0+1:l1])
					if T == (0,0):
						del insts [l1]
						insts.insert (l0+1, ('DUP_TOP', NoVal))
						return True
					if T == (-1,-1):
						# Do it only if it doesn't increase the stack usage of the function
						# this still increases the "stack pressure" -- which may not be entirely good.
						s1 = depthFinder (self)
						del insts [l1]
						insts.insert (l0+1, ('DUP_TOP', NoVal))
						s2 = depthFinder (self)
						if s1 == s2:
							return True
			# Undo it
						del insts [l0+1]
						insts.insert (l1, ('LOAD_FAST', i))

		# ``LOAD_FAST removal 2''
		# STORE_FAST i, LOAD_FAST i -> DUP_TOP, STORE_FAST i
		#
		# Between these there can be opcodes with no stack effect, like
		# SETUP_LOOP, PRINT_NEWLINE, etc, which are unaffected
		#
		# This is a very important speed optimization because this pattern appears in
		# intense "many-times, two-liner" loops (list comps, etc)
		#
		for i in set (self.STORES.keys ()) & set (self.LOADS.keys ()):
			for j in pairlist (self.VARINFO [i]):
				if j [0][0] == 'STORE_FAST' and j [1][0] == 'LOAD_FAST':
					l0, l1 = j [0][1], j [1][1]
					if get_stack_effect (insts [l0+1:l1]) == (0,0):
						insts [l0], insts [l1] = ('DUP_TOP', NoVal), insts [l0]
						return True

	def optimize2 (self):
		inststr, vals, indexes = self.FFORM
		#
		# LOAD_CONST (C), <some commands with null stack effect>, LOAD_CONST (C)
		#    -> LOAD_CONST (C), DUP_OP, <the commands>
		#
		# increases stack pressure
		#
		LC = opchar ['LOAD_CONST']
		if LC in inststr:
			inststr = inststr.find
			i = -1
			CC = []
			while 1:
				i = inststr (LC, i + 1)
				if i == -1:
					break
				CC.append (vals [i])
			if len (CC) != len (set (CC)):
				for i in ([x for x in set (CC) if CC.count (x) > 1]):
					for l0, l1 in Indexes (self.insts, ('LOAD_CONST', i)):
						if get_stack_effect (self.insts [l0+1:l1]) == (0,0):
							del self.insts [l1]
							self.insts.insert (l0+1, ('DUP_TOP', NoVal))
							return True

	def optimize_single (self):
		self.fform ()
		self.optimize_once ()

	def optimize_block (self):
		while self.optimize0 ():
			self.fform ()

	def optimize_final (self):
		# Ok. We have 'simplifications' and 'transformations'.  Simplifications
		# are always good and we do them ASAP. Transformations on the other hand
		# may conflict and with the current -simple- optimizer we can't catch all
		# of them. These here are done last and if there is no more that can be
		# done in the other stages which are considered more important.
		# We should probably rerun dead-store optimization after that though...
		while self.optimize2 ():
			self.fform ()
			self.optimize_block ()
		if self.INTRAOPT:
			self.insts [0:self.FFORM [2][1]+1] = ()
			self.fform ()
			self.optimize_block ()

	ATTR_regex = inst2regex (['LOAD_FAST'], ['LOAD_ATTR'])

	def do_SELF_ATTR (self):
		self.fform ()
		inststr, vals, indexes = self.FFORM

		i = 0
		while 1:
			xx = self.ATTR_regex (inststr, i)
			if not xx:
				return
			i = xx.start ()
			if vals [i] == 'self':
				ii = indexes [i]
				self.insts [ii:ii+2] = [('SELF_ATTR', vals [i+1])]
				self.fform ()
				inststr, vals, indexes = self.FFORM
			else:
				i += 1

	ATTRW_regex = inst2regex (['LOAD_FAST'], ['STORE_ATTR'])

	def do_SELF_ATTRW (self):
		self.fform ()
		inststr, vals, indexes = self.FFORM

		i = 0
		while 1:
			xx = self.ATTRW_regex (inststr, i)
			if not xx:
				return
			i = xx.start ()
			if vals [i] == 'self':
				ii = indexes [i]
				self.insts [ii:ii+2] = [('SELF_ATTRW', vals [i+1])]
				self.fform ()
				inststr, vals, indexes = self.FFORM
			else:
				i += 1

	ROTREORD_regex = inst2regex (
		['DUP_TOP', 'ROT_TWO'], '|',
		['ROT_TWO'], '[', ['STORE_FAST', 'POP_TOP'], ']{2}|',
		['ROT_THREE'], '[', ['STORE_FAST', 'POP_TOP'], ']{3}|',
		'[', ELOAD_class, ']{2}', ['ROT_TWO'], '|',
		'[', ELOAD_class, ']', ['LOAD_GLOBAL', 'ROT_TWO'], '|',
		['LOAD_GLOBAL'], '[', ELOAD_class, ']', ['ROT_TWO'], '|',
		'[', ELOAD_class, ']{3}', ['ROT_THREE']
	)

	ELIM_regex = inst2regex ('[', LOADERS_class, '][', NOEFFECT_class, ']*', ['POP_TOP'], '|',
		['ROT_TWO'], '[', NOEFFECT_class, ']*', ['ROT_TWO'])

	def optimize0 (self):
		inststr, vals, indexes = self.FFORM
		insts = self.insts

		# ``inline rot''
		# DUP_TOP, ROT_TWO -> DUP_TOP
		# ROT_TWO, STORE_FAST x1, STORE_FAST x2 -> STORE_FAST x2, STORE_FAST x1
		# ROT_THREE, STORE_FAST x1, STORE_FAST x2, STORE_FAST x3 -> STORE_FAST x3, STORE_FAST x1, SF x2
		# LOAD_FAST x1, LOAD_FAST x2, ROT_TWO -> LOAD_FAST x2, LOAD_FAST x1
		# LOAD_FAST x1, LOAD_FAST x2, LOAD_FAST x3, ROT_THREE ->  ...
		#
		xx = self.ROTREORD_regex (inststr)
		if xx:
			i, j = xx.span ()
			if j-i == 3:
				i0, i1, i2 = indexes [i:j]
				if inststr [i] == opchar ['ROT_TWO']:
					insts [i1], insts [i2] = insts [i2], insts [i1]
					del insts [i0]
				else:
					insts [i0], insts [i1] = insts [i1], insts [i0]
					del insts [i2]

	def regen (self):
		self.fform ()
		self.optimized_store = 1

	def optimize_stores (self, noload, independent):
		self.fform ()
		self.optimized_store = 0

		# (1) ``remove STORE without LOAD''
		# If for the entire FForm there are STORE_FASTs for a name 'x'
		# and there are no LOAD_FASTs for it, it's safe to replace the
		# STORE_FAST with POP_TOP
		# DELETE_FASTs for this variable are removed
		#
		for i in noload:
			if i in self.VARINFO:
				for j in reversed (self.VARINFO [i]):
					if j [0] == 'STORE_FAST':
						self.insts [j [1]] = 'POP_TOP', NoVal
					else:
						del self.insts [j [1]]
				self.regen ()

		# (2) ``remove STORE before STORE''
		# If between two STOREs of a local variable 'x' there is no LOAD for this
		# variable, we can replace the first STORE with a POP_TOP.
		# If there is a DELETE_FAST of this variable between the stores, it's removed
		#
		# This optimization can be done for non-independent variables too.
		# But not inside a try block.
		#
		for i, j in self.STORES.iteritems ():
			if j < 2 or (i not in independent and self.tryblock):
				continue
			did = 0
			for k in pairlist (self.VARINFO [i]):
				if k [0][0] == 'STORE_FAST' == k [1][0]:
					self.insts [k [0][1]] = 'POP_TOP', NoVal
					did = 1
			if did:
				self.regen ()

		# (3) ``STORE+LOAD to ROT''
		# Optimize basic blocks that use independent variables
		#
		for i in independent:
			if i not in self.VARINFO:
				continue
			v = self.VARINFO [i]
			if len (v) == 2 and v [0][0] == 'STORE_FAST' and v [1][0] == 'LOAD_FAST':
				opt_indep (self.insts, v [0][1], v [1][1])
				self.regen ()

		if self.optimized_store:
			return self.optimize_block ()

# Independent variable optimization on a basic block
def opt_indep (insts, p1, p2):
	#
	# For an independent variable "x" when we have.
	#    STORE_FAST (x), <some commands>, LOAD_FAST (x)
	#
	# stack effect 0,+1 (middle commands just push one value to the stack), we do:
	#	<some commands>, ROT_TWO
	# stack effect 0,0 (no effect):
	#	<some commands>
	#
	# Also (these are good but they "intensify the stack usage")
	# stack effect -1,-1 (commands just pop one value off the stack), we do:
	#	ROT_TWO, <some commands>
	# stack effect -1,0 (pop one - push one), we do:
	#	ROT_TWO, <some commands>, ROT_TWO
	# stack effect -2,-2:
	#	ROT_THREE, <some commands>
	# stack effect -2,-1:
	#	ROT_THREE, <some commands>, ROT_TWO
	#
	# special cases enabled only when the vm is OK with "reverse ROT 3":
	# stack effect 0,+2. 
	#     <some commands> ROT_TWO, ROT_THREE, ROT_TWO
	# stack effect -1,+1. 
	#     ROT_TWO, <some commands> ROT_TWO, ROT_THREE, ROT_TWO
	#
	eff = get_stack_effect (insts [p1+1:p2])

	if eff in ((0,1),(-1,-1),(-1,0),(0,0),(-2,-2),(-2,-1)):
		if eff == (0,1):
			insts [p2] = 'ROT_TWO', NoVal
			del insts [p1]
		elif eff == (-1,-1):
			insts [p1] = 'ROT_TWO', NoVal
			del insts [p2]
		elif eff == (-1,0):
			insts [p1] = insts [p2] = 'ROT_TWO', NoVal
		elif eff == (0,0):
			del insts [p2]
			del insts [p1]
		elif eff == (-2,-2):
			insts [p1] = 'ROT_THREE', NoVal
			del insts [p2]
		elif eff == (-2,-1):
			insts [p1] = 'ROT_THREE', NoVal
			insts [p2] = 'ROT_TWO', NoVal
		return True
	else:
		if eff == (0,2) and GCO ['rrot3']:
			insts [p2:p2+1] = ('ROT_TWO', NoVal), ('ROT_THREE', NoVal), ('ROT_TWO', NoVal)
			del insts [p1]
			return True
		if eff == (-1,1) and GCO ['rrot3']:
			insts [p2:p2+1] = ('ROT_TWO', NoVal), ('ROT_THREE', NoVal), ('ROT_TWO', NoVal)
			insts [p1] = 'ROT_TWO', NoVal
			return True
#	print '\n', CurrentFunc, eff, insts [p1:p2+1]
		pass

def cntDict (lst):
	# lst is a list [x,x,y,z,x,y,y] -> {'x':3,'y':3,'z':1}
	D = {}
	for i in set (lst):
		D [i] = lst.count (i)
	return D

# *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*

#
# optimizer utilities
#

def pairlist (lst):
	return zip (lst [:-1], lst [1:])

def Indexes (lst, itm):
	idx = []
	try:
		i = -1
		while 1:
			i = lst.index (itm, i + 1)
			idx.append (i)
	except:
		pass
	return reversed (zip (idx [:-1], idx [1:]))

stack_effect = {
	# source 
	'LOAD_FAST' : (0,1),
	'LOAD_GLOBAL' : (0,1),
	'LOAD_CONST' : (0,1),
	'SELF_ATTR' : (0,1),

	# sink 
	'STORE_FAST' : (1,0),
	'STORE_GLOBAL' : (1,0),
	'STORE_ATTR': (2,0),
	'POP_TOP' : (1,0),
	'PRINT_ITEM' : (1,0),
	'STORE_SUBSCR': (3,0),
	'DELETE_SUBSCR': (2,0),
	'LIST_APPEND': (2, 0),
	'SELF_ATTRW': (1, 0),

	# neutral
	'PRINT_NEWLINE': (0,0),

# WRONG: SETUP_LOOP stores SP and therefore no hacks!
#	'SETUP_LOOP': (0,0),
#	'SETUP_FINALLY': (0,0),
#	'SETUP_EXCEPT': (0,0),

	'SET_LINENO': (0,0),

	# active
	'LOAD_ATTR' : (1,1),
	'UNARY_NEGATIVE' : (1,1),
	'UNARY_NOT' : (1,1),
	'BINARY_SUBSCR': (2,1),
	'BINARY_ADD': (2,1),
	'BINARY_OR': (2,1),
	'BINARY_SUBTRACT': (2,1),
	'BINARY_MULTIPLY': (2,1),
	'BINARY_DIVIDE': (2,1),
	'BINARY_MODULO': (2,1),
	'COMPARE_OP': (2,1),
	'INPLACE_ADD': (2,1),
	'INPLACE_OR': (2,1),
	'INPLACE_SUBTRACT': (2,1),
	'SLICE+1': (2,1),
	'SLICE+2': (2,1),
	'DUP_TOP': (1,2),		# but doesn't change the value
	'ROT_TWO': (2,2),
	'ROT_THREE': (3,3),
}

def CALL_effect (argc):
	hi, lo = divmod(argc, 256)
	return 1 + lo + hi * 2, 1

dyn_stack_effect = {
	'CALL_FUNCTION': CALL_effect,
	'BUILD_TUPLE': lambda x: (x,1),
	'BUILD_LIST': lambda x: (x,1),
	'BUILD_MAP': lambda x: (2*x,1),
	'UNPACK_SEQUENCE': lambda x: (1,x),
	'DUP_TOPX': lambda x: (1,x+1),
}

def get_stack_effect (insts):
	stk, maxpop = 0, 0

	for op, val in insts:
		try:
			pop, push = stack_effect [op]
		except:
			if op not in dyn_stack_effect:
				return None
			pop, push = dyn_stack_effect [op] (val)
		stk -= pop
		if stk < maxpop:
			maxpop = stk
		stk += push

	return maxpop, stk

# * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

def neg_jmp (jmp):
	if jmp == 'JUMP_IF_TRUE':
		return 'JUMP_IF_FALSE'
	return 'JUMP_IF_TRUE'

class PyFlowGraph(FlowGraph):

	def __init__(self, name, args=(), optimized=0, klass=None,
		dynlocals=True, isfunc=False):
		FlowGraph.__init__ (self)
		self.name = name
		self.iname = None
		self.isfunc = isfunc
		self.docstring = None
		self.args = args # XXX
		self.argcount = getArgCount(args)
		self.klass = klass
		self.dynlocals = dynlocals
		if optimized:
			self.flags = CO_OPTIMIZED | CO_NEWLOCALS
		else:
			self.flags = 0
		self.consts = []
		if GCO ['gnt']:
			self.names = GCO ['global_names_list']
		else:
			self.names = []
		if GCO ['gnc']:
			self.gconsts = GCO ['global_consts_list']
		else:
			self.gconsts = None
		# Free variables found by the symbol table scan, including
		# variables used only in nested scopes, are included here.
		self.freevars = []
		self.boundfreevars = []
		self.cellvars = []
		# The closure list is used to track the order of cell
		# variables and free variables in the resulting code object.
		# The offsets used by LOAD_CLOSURE/LOAD_DEREF refer to both
		# kinds of variables.
		self.closure = []
		self.varnames = list(args)
		for i, var in enumerate (self.varnames):
			if isinstance(var, TupleArg):
				self.varnames[i] = var.getName()
			elif var [0] == '$':
				self.varnames [i] = var [1:]

	def setDocstring(self, doc):
		if GCO ['docstrings']:
			self.docstring = doc

	def setFlag(self, flag):
		self.flags = self.flags | flag
		if flag == CO_VARARGS:
			self.argcount -= 1

	def setFreeVars(self, names):
		self.freevars = list(names)

	def setBoundFreeVars (self, names):
		self.boundfreevars = list (names)

	def setCellVars(self, names):
		self.cellvars = names

	def getCodeParams (self):
		self.optimize_unaries ()
		self.flattenGraph()
		self.convertArgs()
		self.makeByteCode()
		return self.newCodeObjectParams()

#	def dump(self, io=None):
#		if io:
#			save = sys.stdout
#			sys.stdout = io
#		pc = 0
#		for op, val in self.insts:
#			if op == "SET_LINENO":
#				print
#			if val is NoVal:
#				print "\t", "%3d" % pc, op
#				pc = pc + 1
#			else:
#				print "\t", "%3d" % pc, op, val
#				pc = pc + 3
#		if io:
#			sys.stdout = save

	def optimize_unaries (self):
		#
		# Convert UNARY_NOT, JUMP_IF_TRUE/FALSE -> JUMP_IF_FALSE/TRUE, when possible.
		# possible: when the target jump is a JUMP_IF...
		#
		# This actually takes care of eliminating UNARY_NOTs in code like:
		#	if a and not b:
		#
		# We were supposed to do this at pycodegen but its a real PITA.
		# This is a sort of a higher level jump target optimization.
		#
		# XXX: stupid case: UNARY_NOT, UNARY_NOT, JUMP_IF_TRUE
		#
		for i in self.getBlocks ():
			insts = i.insts
			if (len (insts) > 1 and insts [-2][0] == 'UNARY_NOT' 
			and insts [-1][0].startswith ('JUMP_IF')):
				blk = insts [-1][1]
				insts2 = blk.insts
				if len (insts2) == 1 and insts2 [0][0].startswith ('JUMP_IF'):
					del insts [-2]
					if insts [-1][0] != insts2 [0][0]:
						blk2 = blk.next [0]
					else:
						blk2 = insts2 [0][1]
					insts [-1] = neg_jmp (insts [-1][0]), blk2
					blk.inEdges.remove (i)
					blk2.inEdges.append (i)

	def computeStackDepth(self):
		self.stacksize = depthFinder (self.entry)

	end_instr = set (['RETURN_VALUE', 'RAISE_VARARGS', 'BREAK_LOOP', 'CONTINUE_LOOP'])

	def studyIntraBlocks (self):
		# Find all the single-entry blocks and then ask each BB if it can do the intra-block
		# optimization. We get the information that this is possible but do it at the end so that
		# we'll not miss other optimization because of this"""
		ub = set ([x for x in self.getBlocks() if len (x.inEdges) == 1])
		for i in self.getBlocks ():
			i.setIntra (ub)

	def studyLocalVars (self):
		# For FlowGrapgh of functions, get all the STORES/LOADS/DELS of the function.
		# Also detect indepedent variables.  Invoke the dead-store optimizer."""
		LOADS, STORES, DELS, INDEP = set(), set(), set(), set ()
		fblocks = [i for i in self.getBlocks () if i.hasFAST]
		for i in fblocks:
			LOADS.update (set (i.LOADS.keys ()))
			STORES.update (set (i.STORES.keys ()))
			if i.DELS:
				DELS.update (set (i.DELS.keys ()))

		#
		# Independent variables: ``If for all the basic blocks that there is a LOAD of a local
		# variable x, there is a STORE of the same variable before all the LOADs, the variable
		# is marked "independent"''.
		# A variable that is independent does not need to provide a STORE for use outside its
		# basic block.
		#
		# Existance of a DELETE_FAST for a variable cancels the optimization.
		#
		for i in LOADS & STORES:
			S = 'STORE_FAST', i
			L = 'LOAD_FAST', i
			for j in fblocks:
				if i in j.LOADS:
					if i not in j.STORES:
						break
					if j.insts.index (S) > j.insts.index (L):
						break
			else:
				INDEP.add (i)
		NOLOAD = STORES - LOADS
		j = False
		for i in fblocks:
			if i.optimize_stores (NOLOAD, INDEP):
				j = True
		return j

	def optimizeGraph (self):
		if not self.isfunc:
			return
		global CurrentFunc
		CurrentFunc = self.name
		B = self.getBlocks ()
		for i in B:
			i.optimize_single ()
		self.studyIntraBlocks ()
		for i in B:
			i.optimize_block ()
		if not self.dynlocals:
			while self.studyLocalVars ():
				pass
		for i in B:
			i.optimize_final ()
		if not self.dynlocals:
			while self.studyLocalVars ():
				pass
		# implement SELF_ATTR opcode
		if GCO ['arch'] == 'pyvm' and self.varnames and self.varnames [0] == 'self' and 1:
			for i in B:
				i.do_SELF_ATTR ()
				i.do_SELF_ATTRW ()


	# Rename is the process where we rename local variables in order to
	# minimize the number of locals.  In this mode, local variable names
	# are just symbolic for the algorithm, but the VM works with different
	# (fewer) local variables.  For example in,
	#
	#	def f (X):
	#		for x in X():
	#			print x
	#
	# the variable `x` can be renamed to `X` and use just one local variable.
	#
	# The disadvantage would be that this optimization breaks debugging
	# and we can't inspect the variables of a function any more. At the
	# moment pyvm does not use any such debugging.  Also the optimization
	# is not performed for functions with dynlocals (that use locals(), etc)
	#
	# The advantage is that fewer locals means faster function calls.
	# Every time a function is called, the vm has to initialize all the
	# local variables to the <unbound> object and when the function
	# returns, it has to clean up the locals.

	Renames = {}

	def connectBlocks (self):
		# Normally, blocks should be already properly connected.
		# However, something's really broken in the code derrived
		# from python's compiler and I just can't figure out what's
		# going wrong.  So this code here does what was supposed to
		# happen anyway.

		exits = []
		tovisit = [self.entry]
		visited = set ()
		while tovisit:
			b = tovisit.pop (0)
			g = []
			for i, v in b.insts:
				if isinstance (v, Block):
					g.append (v)
			passes_on = True
			if b.insts:
				i, v = b.insts [-1]
				if i in ("JUMP_FORWARD", "JUMP_ABSOLUTE", "RETURN_VALUE", "RAISE_VARARGS"
					"BREAK_LOOP", "CONTINUE_LOOP"):
					passes_on = False
			if passes_on:
				if b.next:
					if len (b.next) > 1:
						print 100*"FUCK"
					g.append (b.next [0])
					
			for bb in g:
				bb.IN.add (b)
				if bb not in visited:
					visited.add (bb)
					tovisit.append (bb)
			if not g and not b.has_break:
				exits.append (b)
		return exits

	def rename (self):
		return
		exits = self.connectBlocks ()

		DF = {}
		for n in self.varnames:
			DF [n] = []
		for e in exits:
			e.dataflow (DF, set ())
		for v in self.cellvars:
			if v in DF:
				del DF [v]
		D = [(k, set (v)) for k, v in DF.items ()]
		DF = dict (D)

		# find independant variables (O(n**2) speed-up)
		R = []
		for k, v in D:
			if k not in self.varnames:
				RP = []
				for k2, v2 in D:
					if not v & v2:	# speedup
						RP.append (k2)
				if RP:
					RP.sort ()
					R.append ((len (RP), k, RP))

		if not R:
			return

		R.sort ()
		R = [(a, b) for x, a, b in R]
		RENAMES = {}
		while R:
			k, r = R.pop (0)
			if not r:
				continue
			r = r [0]
			RENAMES [k] = r
			DF [r] = DF [r] | DF [k]
			for kx, vx in R:
				if r in vx and DF [r] & DF [kx]:
					vx.remove (r)
				if k in vx:
					vx.remove (k)
				if kx == r:
					for x in [x for x in vx if DF [x] & DF [r]]:
						vx.remove (x)
		if RENAMES:
			self.Renames = RENAMES
			if 0:
				print self.name, "Renames:", len (RENAMES), RENAMES


	def flattenGraph(self):
		# Arrange the blocks in order and resolve jumps

		#####
		self.optimizeGraph ()
		if self.isfunc and not self.dynlocals and GCO ['renames']:
			self.rename ()
		self.computeStackDepth()
		#####

		self.insts = insts = []
		pc = 0
		pci = 0
		begin = {}
		end = {}
		pc2idx = {}
		_NoVal = NoVal
		end_instr = self.end_instr

		if GCO ['arch'] == 'pyvm' and 1:
			for b in self.getBlocksInOrder():
				inst = b.getInstructions()
				if ('UNPACK_SEQUENCE', 2) in inst:
					for n, op in enumerate (inst):
						if op == ('UNPACK_SEQUENCE', 2):
							inst [n] = 'UNPACK2', _NoVal

		for b in self.getBlocksInOrder():
			begin[b] = pc
			for inst in b.getInstructions():
				insts.append(inst)
				op, val = inst
				pc2idx [pc] = pci
				pci += 1
				if val is _NoVal:
					pc += 1
				elif op != "SET_LINENO":
					pc += 3

				# optimization: I think it's safe to remove unreachable code
				# after return/raise/break/continue(jump_abs)
				if op in end_instr:
					break

			end[b] = pc

		pc = 0
		jumpset, hasjrel, hasjabs = self.jumpset, self.hasjrel, self.hasjabs

		for i, (op, val) in enumerate (insts):
			if val is _NoVal:
				pc += 1
			elif op != "SET_LINENO":
				pc += 3
				if op in hasjrel:
					offset = begin[val] - pc
					insts[i] = op, offset
				elif op in hasjabs:
					insts[i] = op, begin[val]

		#
		# skip jumps to jumps to the final target
		#
		idx2pc = {}
		for i, j in pc2idx.iteritems ():
			idx2pc [j] = i

		for i in xrange(len(insts)):
			while 1:
				inst = insts[i]
				#
				# x:JUMP_IF_TRUE y  y: JUMP_IF_TRUE z ---> x: JUMP_IF_TRUE z
				# x:JUMP_IF_TRUE y  y: JUMP_IF_FALSE z ---> x: JUMP_IF_TRUE z+3
				#
				if inst [0] in ('JUMP_IF_FALSE', 'JUMP_IF_TRUE'):
					addr = pc2idx [inst [1] + idx2pc [i] + 3]
					if insts [addr][0] in ('JUMP_IF_FALSE', 'JUMP_IF_TRUE'):
						inst2 = insts [addr]
						if inst [0] == inst2 [0]:
							insts [i] = inst [0], (inst2 [1] + idx2pc [addr]) - idx2pc [i]
						else:
							insts [i] = inst [0], (idx2pc [addr]) - idx2pc [i]
						continue
				#
				# Replace jumps to unconditional jumps
				# x:JUMP_* y  y: JUMP_FORWARD/JUMP_ABS z ---> x: JUMP_* z
				#
				if inst [0] in jumpset:
					if inst [0] in hasjrel: 
						addr = pc2idx [inst [1] + idx2pc [i] + 3]
					else:
						addr = pc2idx [inst [1]]
					if insts [addr][0] in ('JUMP_FORWARD', 'JUMP_ABSOLUTE'):
						if insts [addr][0] == 'JUMP_FORWARD':
							tgt = pc2idx [insts [addr][1] + idx2pc [addr] + 3]
						else:
							tgt = pc2idx [insts [addr][1]]
						if inst [0] == 'JUMP_FORWARD':
							inst = 'JUMP_ABSOLUTE', inst [1]
						if inst [0] in hasjabs:
							insts [i] = inst [0], idx2pc [tgt]
							continue
						if tgt >= i:
							insts [i] = inst [0], idx2pc [tgt] - idx2pc [i] - 3
							continue
				break

	def convertArgs(self):
		# Convert arguments from symbolic to concrete form
		if self.docstring:
			self.consts.insert(0, self.docstring)
		self.sort_cellvars()
		for i, (opname, oparg) in enumerate (self.insts):
			if oparg is not NoVal:
				conv = self._converters.get(opname, None)
				if conv:
					self.insts [i] = opname, conv(self, oparg)

	def sort_cellvars(self):
		# Sort cellvars in the order of varnames and prune from freevars.
		if not self.cellvars:
			if self.freevars:
				self.closure = sorted (self.freevars)
			if self.boundfreevars:
				self.closure = sorted (self.boundfreevars)
			return
		cells = {}
		for name in self.cellvars:
			cells[name] = 1
		self.cellvars = [name for name in self.varnames
                         if cells.has_key(name)]
		for name in self.cellvars:
			del cells[name]
		self.cellvars = self.cellvars + cells.keys()
		self.closure = self.cellvars + sorted (self.freevars)

	def _lookupName(name, lst):
		# Return index of name in list, appending if necessary
		#
		# This routine uses a list instead of a dictionary, because a
		# dictionary can't store two different keys if the keys have the
		# same value but different types, e.g. 2 and 2L.  The compiler
		# must treat these two separately, so it does an explicit type
		# comparison before comparing the values.
		t = type(name)
		for n, i in enumerate (lst):
			if i == name and type (i) is t:
				return n
		end = len(lst)
		"pyc::type lst list"
		lst.append(name)
		return end
	_lookupName = staticmethod (_lookupName)

	def _lookupStrName(name, lst):
		try:
			return lst.index (name)
		except:
			i = len (lst)
			"pyc::type lst list"
			lst.append (name)
			return i
	_lookupStrName = staticmethod (_lookupStrName)

	_converters = {}
	def _convert_LOAD_CONST(self, arg):
		if self.gconsts is not None and not isinstance (arg, CodeObject):
			return self.gconsts.add (arg) + 30000
		return self._lookupName(arg, self.consts)

	def _convert_LOAD_FAST(self, arg):
		if arg in self.Renames:
			arg = self.Renames [arg]
		self._lookupStrName(arg, self.names)
		return self._lookupStrName(arg, self.varnames)
	_convert_STORE_FAST = _convert_LOAD_FAST
	_convert_DELETE_FAST = _convert_LOAD_FAST

	def _convert_LOAD_NAME(self, arg):
		return self._lookupStrName(arg, self.names)

	def _convert_NAME(self, arg):
		return self._lookupStrName(arg, self.names)
	_convert_STORE_NAME = _convert_NAME
	_convert_DELETE_NAME = _convert_NAME
	_convert_IMPORT_NAME = _convert_NAME
	_convert_IMPORT_FROM = _convert_NAME
	_convert_STORE_ATTR = _convert_NAME
	_convert_LOAD_ATTR = _convert_NAME
	_convert_SELF_ATTR = _convert_NAME
	_convert_SELF_ATTRW = _convert_NAME
	_convert_DELETE_ATTR = _convert_NAME
	_convert_LOAD_GLOBAL = _convert_NAME
	_convert_STORE_GLOBAL = _convert_NAME
	_convert_DELETE_GLOBAL = _convert_NAME

	def _convert_DEREF(self, arg):
		self._lookupName(arg, self.names)
		return self._lookupName(arg, self.closure)
	_convert_LOAD_DEREF = _convert_DEREF
	_convert_STORE_DEREF = _convert_DEREF

	def _convert_LOAD_CLOSURE(self, arg):
		return self._lookupName(arg, self.closure)

	def _convert_COMPARE_OP(self, arg):
		return self._cmp.index(arg)

	# similarly for other opcodes...

	for name, obj in locals().items():
		if name[:9] == "_convert_":
			opname = intern (name[9:])
			_converters[opname] = obj
	del name, obj, opname

	def makeByteCode(self):
		self.lnotab = lnotab = LineAddrTable()
		lnotab_addCode1 = lnotab.addCode1
		lnotab_addCode = lnotab.addCode
		opnum = self.opnum
		_NoVal = NoVal
		for opname, oparg in self.insts:
			if oparg is _NoVal:
				lnotab_addCode1(opnum [opname])
			else:
				if opname == "SET_LINENO":
					lnotab.nextLine(oparg)
					continue
				try:
					lnotab_addCode(opnum [opname], oparg)
				except ValueError:
					print opname, oparg
					print opnum[opname], lo, hi
					raise

		# break cyclic references which results in the blocks
		# of the current Flow being released. Otherwise, we'd
		# have to wait for gc.

		for b in self.blocks:
			b.insts = b.inEdges = b.outEdges = b.next = b.IN = None
		del self.blocks

	def newCodeObjectParams(self):

		# I think that freevars are NEVER also part of the varnames. IOW,
		# something cannot be both a local and a closure.  If it is so,
		# we have to remove all names appearing is freevars from varnames.
		# The internal CPython's compiler appears to be doing something
		# similar, judging by the bytecode it makes...
#		if self.freevars:
#	    	self.varnames = tuple (set (self.varnames) - set (self.freevars))
#		print '-', CurrentFunc, self.varnames
#
		if (self.flags & CO_NEWLOCALS) == 0:
			nlocals = 0
		else:
			nlocals = len (self.varnames)
		argcount = self.argcount
		if self.flags & CO_VARKEYWORDS:
			argcount -= 1

		if GCO ['gnt']:
			names = None
		else:
			names = tuple (self.names)

		freevars = tuple (self.freevars or self.boundfreevars)
		name = intern (self.name)
		if self.iname and self.iname != name:
			iname = self.iname
		else:
			iname = name
		# Make filename and name interns 
		return (argcount, nlocals, self.stacksize, self.flags,
                        self.lnotab.getBinaryCode (), self.getConsts (),
                        names, tuple (self.varnames),
                        intern (GCO ['filename']), name, self.lnotab.firstline,
                        self.lnotab.getTable (), freevars,
                        tuple (self.cellvars), iname)

	def getConsts(self):
		"""Return a tuple for the const slot of the code object"""
		return tuple (self.consts)

class PyFlowGraph24(PyFlowGraph):
	import arch24 as dis

	# maybe we should use metaclasses to abstract these
	# for all py-architectures
	hasjrel = set ([dis.opname [i] for i in dis.hasjrel])
	hasjabs = set ([dis.opname [i] for i in dis.hasjabs])
	jumpset = hasjabs | hasjrel
	_cmp = list(dis.cmp_op)
	opnum = {}
	for num in range(len(dis.opname)):
		opnum[dis.opname[num]] = num
	del num


class TupleArg:
	"""Helper for marking func defs with nested tuples in arglist"""
	def __init__(self, count, names):
		self.count = count
		self.names = names
	def __repr__(self):
		return "TupleArg(%s, %s)" % (self.count, self.names)
	def getName(self):
		return ".%d" % self.count

def getArgCount(args):
	argcount = len(args)
	if args:
		for arg in args:
			if isinstance(arg, TupleArg):
				numNames = len(misc.flatten(arg.names))
				argcount = argcount - numNames
	return argcount

class LineAddrTable:
	# lnotab
	#
	# This class builds the lnotab, which is documented in compile.c.
	# XXX: And not only! It also builds the code table!!!
	# YYY: JIT it, it's pretty static!
	#
	# Here's a brief recap:
	#
	# For each SET_LINENO instruction after the first one, two bytes are
	# added to lnotab.  (In some cases, multiple two-byte entries are
	# added.)  The first byte is the distance in bytes between the
	# instruction for the last SET_LINENO and the current SET_LINENO.
	# The second byte is offset in line numbers.  If either offset is
	# greater than 255, multiple two-byte entries are added -- see
	# compile.c for the delicate details.

	def __init__(self):
		self.code = []
		self.addCode1 = self.code.append
		self.firstline = self.lastline = self.lastoff = 0
		self.lnotab = []

	'pyc::membertype self.code list'

	def addCode(self, op, arg):
		self.code.append (op)
		hi, lo = divmod (arg, 256)
		self.code.append (lo)
		self.code.append (hi)

	def nextLine(self, lineno):
		if not self.firstline:
			self.lastline = self.firstline = lineno
		else:
			# compute deltas
			addr = len (self.code) - self.lastoff
			line = lineno - self.lastline
			# Python assumes that lineno always increases with
			# increasing bytecode address (lnotab is unsigned char).
			# Depending on when SET_LINENO instructions are emitted
			# this is not always true.  Consider the code:
			#     a = (1,
			#          b)
			# In the bytecode stream, the assignment to "a" occurs
			# after the loading of "b".  This works with the C Python
			# compiler because it only generates a SET_LINENO instruction
			# for the assignment.
			if line >= 0:
				push = self.lnotab.append
				while addr > 255:
					push(255); push(0)
					addr -= 255
				while line > 255:
					push(addr); push(255)
					line -= 255
					addr = 0
				if addr > 0 or line > 0:
					push(addr); push(line)
				self.lastline = lineno
				self.lastoff = len (self.code)

	if 'pyvm' not in sys.copyright:
		def getBinaryCode(self):
			return ''.join ([chr (x) for x in self.code])
	else:
		def getBinaryCode(self):
			return array ('c', self.code).tostring ()

	def getTable(self):
		if not GCO ['lnotab']:
			return ''
		return list_to_string (self.lnotab)

class StackDepthTracker:

	effect = {
		'POP_TOP': -1,
		'DUP_TOP': 1,
		'SLICE+1': -1,
		'SLICE+2': -1,
		'SLICE+3': -2,
		'STORE_SLICE+0': -1,
		'STORE_SLICE+1': -2,
		'STORE_SLICE+2': -2,
		'STORE_SLICE+3': -3,
		'DELETE_SLICE+0': -1,
		'DELETE_SLICE+1': -2,
		'DELETE_SLICE+2': -2,
		'DELETE_SLICE+3': -3,
		'STORE_SUBSCR': -3,
		'DELETE_SUBSCR': -2,
		'PRINT_ITEM': -1,
		'PRINT_ITEM_TO':-2,
		'PRINT_NEWLINE_TO':-1,
		'RETURN_VALUE': -1,
		'YIELD_VALUE': -1,
		'EXEC_STMT': -3,
		'BUILD_CLASS': -2,
		'STORE_NAME': -1,
		'STORE_ATTR': -2,
		'DELETE_ATTR': -1,
		'STORE_GLOBAL': -1,
		'BUILD_MAP': 1,
		'COMPARE_OP': -1,
		'STORE_FAST': -1,
		'STORE_DEREF': -1,
		'IMPORT_STAR': -1,
		'IMPORT_NAME': 0,	# -1 in 2.5
		'IMPORT_FROM': 1,
		'LIST_APPEND': -2,
		'LOAD_ATTR': 0, # unlike other loads
		'SELF_ATTR': 1,
		'SELF_ATTRW': -1,
		# close enough...
		'SETUP_EXCEPT': 0,
		#'SETUP_EXCEPT': 3,
		'SETUP_FINALLY': 3,
		'END_FINALLY':-3,
		'FOR_ITER': 1,
		'GET_ITER':0,
		'SET_LINENO':0,
		'SETUP_LOOP':0,
		'JUMP_ABSOLUTE':0,
		'POP_BLOCK':0,
		'JUMP_FORWARD':0,
		'JUMP_IF_TRUE':0,
		'JUMP_IF_FALSE':0,
		'PRINT_NEWLINE':0,
		'BREAK_LOOP':0,
		'ROT_TWO':0,
		'ROT_THREE':0,
		'SLICE+0':0,
		'DELETE_NAME':0,
		'DELETE_GLOBAL':0,
		'DELETE_FAST':0,
		'CONTINUE_LOOP':0,
	}
	# use pattern match
	patterns = [
		('BINARY_', -1),
		('LOAD_', 1),
		('UNARY_', 0),
		('INPLACE_', -1),
	]

	for opnam in dis.opmap.keys ():
		for pat, patdelta in patterns:
			if opnam.startswith (pat) and opnam not in effect:
				effect [opnam] = patdelta

	del opnam, pat, patdelta, patterns

	branch_effect = {
		'FOR_ITER':-2,
		'SETUP_EXCEPT':3
	}

	def CALL_FUNCTION(argc):
		hi, lo = divmod(argc, 256)
		return -(lo + hi * 2)
	def CALL_FUNCTION1(argc):
		hi, lo = divmod(argc, 256)
		return -(lo + hi * 2) - 1
	def CALL_FUNCTION2(argc):
		hi, lo = divmod(argc, 256)
		return -(lo + hi * 2) - 2
	def BUILD_SLICE(argc):
		if argc == 2:
			return -1
		elif argc == 3:
			return -2

	dyneffect = {
		'RAISE_VARARGS': (lambda count:-count),
		'UNPACK_SEQUENCE': (lambda count:count-1),
		'BUILD_TUPLE': (lambda count:-count+1),
		'BUILD_LIST': (lambda count:-count+1),
		'MAKE_FUNCTION': (lambda argc:-argc),
		'MAKE_CLOSURE': (lambda argc:-argc), # XXX need to account for free variables too!
		'CALL_FUNCTION': CALL_FUNCTION,
		'CALL_FUNCTION_VAR': CALL_FUNCTION1,
		'CALL_FUNCTION_KW': CALL_FUNCTION1,
		'CALL_FUNCTION_VAR_KW': CALL_FUNCTION2,
		'BUILD_SLICE': BUILD_SLICE,
		'DUP_TOPX': (lambda argc: argc)
	}
	del CALL_FUNCTION, CALL_FUNCTION1, CALL_FUNCTION2, BUILD_SLICE

	def __init__ (self):
		self.seen = {}

	uncond_branch = set (['JUMP_ABSOLUTE', 'JUMP_FORWARD', 'RETURN_VALUE', 'BREAK_LOOP',
			'CONTINUE_LOOP', 'RAISE_VARARGS'])

	# the new depth finder

	def findDepth2(self, bb):
		if bb in self.seen:
			return self.seen [bb]

		self.seen [bb] = 0

		depth = 0
		maxDepth = 0
		effect = self.effect
		branch_effect = self.branch_effect.get
		dyneffect = self.dyneffect

		opname = ""

		for opname, oparg in bb.getInstructions ():

			try:
				depth += effect [opname]
			except:
				depth += dyneffect [opname] (oparg)

			if depth > maxDepth:
				maxDepth = depth

			if isinstance (oparg, Block):
				submax = self.findDepth2 (oparg)
				d = branch_effect (opname, 0)

				if submax+depth+d > maxDepth:
					maxDepth = submax+depth+d

		if opname not in self.uncond_branch and bb.next:
			submax = self.findDepth2 (bb.next [0])
			if submax+depth > maxDepth:
				maxDepth = submax+depth

		self.seen [bb] = maxDepth

		return maxDepth

def depthFinder (bb):
	return StackDepthTracker ().findDepth2 (bb) or 1
