##  AST optimizations
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) version 3 of the License.

from visitor	import walk
from pycmisc	import counter
from gco	import GCO

import ast

try:
	set
except:
	from sets import Set as set
	from pycmisc import reversed

def is_compare (node):
	return isinstance (node, ast.Compare)

#
# compare statements. This can be used to turn ifs to conditionals
#
#	if c: x = 1
#	else: x = 2
# to:	x = c ? 1 : 2
#
#	if c: f (1)
#	else: f (2)
# to:	f (c ? 1 : 2)
#
#	if c: yield 1
#	else: yield 2
# to:	yield c ? 1 : 2
#
# This is a size optimization
#

def stmts_eq_base (stmt1, stmt2):
	# if both statements have only one expression which can be optimized
	try:
		stmt1, = stmt1.nodes
		stmt2, = stmt2.nodes
	except:
		return False
	return expr_eq_base (stmt1, stmt2)


def expr_eq_base (stmt1, stmt2):
	if stmt1.__class__ is not stmt2.__class__:
		return False
	if isinstance (stmt1, ast.Assign):
		if repr (stmt1.nodes) == repr (stmt2.nodes):
			return True
		return False
	if isinstance (stmt1, ast.Yield):
		return True
	if isinstance (stmt1, ast.Discard):
		stmt1, stmt2 = stmt1.expr, stmt2.expr
		if stmt1.__class__ is not stmt2.__class__:
			return False
	if (isinstance (stmt1, ast.CallFunc) and len (stmt1.args) == 1 == len (stmt2.args) and
	   not isinstance (stmt2.args [0], ast.Keyword) and
	   not isinstance (stmt1.args [0], ast.Keyword) and
	   repr (stmt1.node) == repr (stmt2.node) and
	   stmt1.star_args == stmt2.star_args == stmt1.dstar_args == stmt2.dstar_args == None):
		return True
	return False

def stmts_get_base (stmt):
	if isinstance (stmt, ast.Assign):
		def setter (expr):
			stmt.expr = expr
			return stmt
		return setter
	if isinstance (stmt, ast.Yield):
		def setter (expr):
			stmt.value = expr
			return stmt
		return setter
	if isinstance (stmt, ast.Discard):
		if isinstance (stmt.expr, ast.CallFunc):
			def setter (expr):
				stmt.expr.args [0] = expr
				return stmt
			return setter
	if isinstance (stmt, ast.CallFunc):
		def setter (expr):
			stmt.args [0] = expr
			return stmt
		return setter

def stmts_get_diff (stmt):
	if isinstance (stmt, ast.Assign):
		return stmt.expr
	if isinstance (stmt, ast.Yield):
		return stmt.value
	if isinstance (stmt, ast.CallFunc):
		return stmt.args [0]
	if isinstance (stmt, ast.Discard):
		if isinstance (stmt.expr, ast.CallFunc):
			return stmt.expr.args [0]

def optimizeConditional (C):
	# This one can handle further optimization for
	#	if c: x = bar (foo (1))
	#	else: x = bar (foo (2))
	# which is converted to
	#	x = bar (foo (c ? 1 : 2))
	#
	if expr_eq_base (C.expr1, C.expr2):
		CC = ast.Conditional (C.cond, stmts_get_diff (C.expr1),
					 stmts_get_diff (C.expr2))
		optimizeConditional (CC)
		make_copy (C, stmts_get_base (C.expr1) (CC))

# TODO: There are other oportunities in the standard library
# exposed after optimizeConditiona. Frequent case is ast.Add
# and ast.Getattr
#--------------------------------------------------------------

def mark_targeted (node):
	# if ast.Or/ast.And/ast.Not is used in a conditional
	# (which means that the result will not be used, we only care
	# for its boolean value), add an attribute 'Targeted' to the node
	#
	if isinstance (node, ast.Or) or isinstance (node, ast.And):
		node.Targeted = True
		for i in node.nodes:
			mark_targeted (i)
	elif isinstance (node, ast.Not):
		node.Targeted = True
		mark_targeted (node.expr)
	node.Targeted = True

def bool_final_not (node):
	return (isinstance (node, ast.And) or isinstance (node, ast.Or)) and isinstance (node.nodes [-1], ast.Not)

def neg_AST (node):
	if isinstance (node, ast.Not):
		return node.expr
	return ast.Not (node)

def make_const (node, val):
	node.__class__ = ast.Const
	ast.Const.__init__ (node, val)
	node.__name__ = ast.Const.__name__

def make_copy (nodep, nodec):
	for i in list (nodep.__dict__):
		delattr (nodep, i)
	for i in nodec.__dict__:
		setattr (nodep, i, getattr (nodec, i))
	nodep.__class__ = nodec.__class__
	nodep.__name__ = nodec.__class__.__name__

def ast_Assign (name, expr):
	return ast.Assign ([ast.AssName (name, 'OP_ASSIGN')], expr)

def ast_Getattr (name, attr):
	return ast.Getattr (ast.Name (name, 0), attr)

end_stmt = set ([ast.Return, ast.Raise, ast.Break, ast.Continue])

def List2Tuple (node):
	for i in node.nodes:
		if not i.isconst:
			# we should probably turn this into tuple as well
			break
	else:
		make_const (node, tuple ([x.value for x in node.nodes]))
		del node.nodes

class FoldConstVisitor:

	def __init__ (self):
		self.CurrentFunc = []
		self.constns = {}

	import operator

	compares = {
		'==': operator.eq,
		'!=': operator.ne,
		'<>': operator.ne,
		'<=': operator.le,
		'>=': operator.ge,
		'<': operator.lt,
		'>': operator.gt,
		'is': operator.is_,
		'is not': operator.is_not,
		'not in': lambda x,y: x not in y,	# python doesn't have operator not_in
		'in': operator.contains,
		'->': lambda x,y: False,
	}

	def defVisitUnary (FUNC):
		def visitUnary (self, node):
			self.visit (node.expr)
			if node.expr.isconst:
				try:
					make_const (node, FUNC (node.expr.value))
					del node.expr
				except:
					pass
		return visitUnary

	visitUnaryAdd = defVisitUnary (operator.pos)
	visitUnarySub = defVisitUnary (operator.neg)
	visitInvert = defVisitUnary (operator.inv)
	del defVisitUnary

	def defVisitBinary (FUNC):
		def visitBinaryOp (self, node):
			self.visit (node.left)
			self.visit (node.right)
			if node.left.isconst and node.right.isconst:
				try:
					make_const (node, FUNC (node.left.value, node.right.value))
					del node.left, node.right
				except:
					pass
		return visitBinaryOp

	visitAdd = defVisitBinary (operator.add)
	visitSub = defVisitBinary (operator.sub)
	visitDiv = defVisitBinary (operator.div)
	visitMod = defVisitBinary (operator.mod)
#	visitLeftShift = defVisitBinary (operator.lshift)
	visitRightShift = defVisitBinary (operator.rshift)
	visitPower = defVisitBinary (operator.pow)
	del defVisitBinary

	def visitLeftShift (self, node):
		self.visit (node.left)
		self.visit (node.right)
		if node.left.isconst and node.right.isconst:
			try:
				make_const (node, node.left.value << node.right.value)
				del node.left, node.right
			except:
				pass

	def visitMul (self, node):
		self.visit (node.left)
		self.visit (node.right)
		if node.left.isconst and node.right.isconst:
			try:
				# We don't want to make things like "50000 * (0,)" constants!
				val = node.left.value * node.right.value
				if (type (val) is str and len (val) > 30 or type (val) is tuple and
				   len (var) > 10):
					return
				make_const (node, val)
				del node.left, node.right
			except:
				pass

	def defBitOp (FUNC):
		def visitMultiOp (self, node):
			all_const = True
			for i in node.nodes:
				self.visit (i)
				if all_const and not i.isconst:
					all_const = False
			if all_const:
				try:
					value = node.nodes [0].value
					for i in node.nodes [1:]:
						value = FUNC (value, i.value)
					make_const (node, value)
					del node.nodes
				except:
					pass
		return visitMultiOp

	visitBitand = defBitOp (operator.and_)
	visitBitor  = defBitOp (operator.or_)
	visitBitxor = defBitOp (operator.xor)
	del defBitOp

	negate_op = { 'in':'not in', 'is':'is not', 'is not':'is', 'not in':'in' }

	def visitNot (self, node):
		self.visit (node.expr)
		if node.expr.isconst:
			try:
				make_const (node, not node.expr.value)
				del node.expr
			except:
				pass
		elif (is_compare (node.expr) and len (node.expr.ops) == 1 and 
		     node.expr.ops [0][0] in self.negate_op):
			# not a in b -- > a not in b
			# not a is b -- > a is not b
			# not a is not b -- > a is b
			# not a not in b -- > a in b
			node.expr.ops [0] = (self.negate_op [node.expr.ops [0][0]], node.expr.ops [0][1])
			make_copy (node, node.expr)

	def visitEarlyTerminator (self, node, FLAG):
		visit = self.visit
		for i in node.nodes:
			visit (i)
		while 1:
			if node.nodes [0].isconst and bool (node.nodes [0].value) == FLAG:
				make_const (node, node.nodes [0].value)
				break
			if hasattr (node, 'Targeted'):
				removables = [x for x in node.nodes if x.isconst and bool (x.value) == (not FLAG) ]
			else:
				removables = [x for x in node.nodes[:-1] if x.isconst and bool (x.value) == (not FLAG) ]
			if removables:
				for i in removables:
					node.nodes.remove (i)
				if not node.nodes:
					make_const (node, FLAG)
					break
				continue
			for i in node.nodes:
				if i.isconst and bool (i.value) == FLAG:
					node.nodes = node.nodes [:node.nodes.index (i)+1]
					break
			break
		if isinstance (node, ast.And) or isinstance (node, ast.Or):
			for i in node.nodes:
				if not isinstance (i, ast.Not):
					return
			# convert "not a or not b" -> "not (a and b)"
			opast = isinstance (node, ast.And) and ast.Or or ast.And
			node2 = ast.Not (opast ([i.expr for i in node.nodes]))
			make_copy (node, node2)

	def visitCompare (self, node):
		self.visit (node.expr)
		for i, code in node.ops:
			if i in ('in', 'not in') and isinstance (code, ast.Set):
				self.visitSet (code, True)
				continue
			self.visit (code)
			if i in ('in', 'not in') and isinstance (code, ast.List):
				List2Tuple (code)
		if node.expr.isconst and len (node.ops) == 1 and node.ops [0][1].isconst:
			make_const (node, self.compares [node.ops [0][0]] (node.expr.value, node.ops [0][1].value))

	def visitSubscript (self, node):
		self.visit (node.expr)
		self.visit (node.sub)
		# if any string literals are used as subscripts (=> keys to dicts),
		# intern them. They should've been interned if less than 15 characters
		# by the parser but this code interns even larger ones.
		# useful for example so that "border-bottom-color" is interned.
		if isinstance (node.sub, ast.Const) and type (node.sub.value) is str:
			if len (node.sub.value) < 25:
				node.sub.value = GCO.Intern (node.sub.value)

	def visitOr (self, node):
		self.visitEarlyTerminator (node, True)
	def visitAnd (self, node):
		self.visitEarlyTerminator (node, False)

	def visitTuple (self, node):
		# Turn BUILD_TUPLE of consts to a const tuple
		#
		visit = self.visit
		all_consts = True
		for i in node.nodes:
			visit (i)
			if not i.isconst:
				all_consts = False
		if all_consts:
			make_const (node, tuple ([x.value for x in node.nodes]))
			del node.nodes

	def visitName (self, node, xlater = {'None':None, 'True':True, 'False':False}):
		if node.name in xlater:
			make_const (node, xlater [node.name])
			del node.name

	def visitIf (self, node):
		# eliminate if-const
		#
		removables = []
		trueconst = None
		visit = self.visit

		for test, body in node.tests:
			mark_targeted (test)
			visit (test)
			visit (body)
			if test.isconst:
				if not test.value:
					removables.append ((test, body))
				elif not trueconst:
					trueconst = test, body
			elif bool_final_not (test):
				#
				# convert "if a and not b --> if not (not a or b)"
				# we have a difficulty removing the UNARY_NOT in the first case...
				#
				make_copy (test, ast.Not ((isinstance (test, ast.And)
				 and ast.Or or ast.And) ([neg_AST (i) for i in test.nodes])))
		if node.else_:
			visit (node.else_)
		for i in removables:
			node.tests.remove (i)
		if not node.tests:
			if node.else_:
				make_copy (node, node.else_)
			else:
				make_copy (node, ast.Pass (node.lineno))
		elif trueconst:
			if trueconst == node.tests [0]:
				make_copy (node, trueconst [1])
			else:
				node.else_ = trueconst [1]
				node.tests = node.tests [:node.tests.index (trueconst)]

		if not isinstance (node, ast.If):
			return

		node.fallthrough = False
		if node.else_:
			for test, stmt in node.tests:
				if not stmts_eq_base (stmt, node.else_):
					break
			else:
				basenode = stmts_get_base (node.else_.nodes [0])
				N = stmts_get_diff (node.else_.nodes [0])
				for test, stmt in reversed (node.tests):
					N = ast.Conditional (test, stmts_get_diff
								 (stmt.nodes [0]), N)
					optimizeConditional (N)
				basenode = basenode (N)
				make_copy (node, basenode)

	def visitStmt (self, node):
		# Remove statements with no effect (docstrings have already been acquired)
		# Remove nodes after Return/Raise/Break/Continue
		# If an 'if' statement has constant True condition, embody its Stmt and rerun
		#
		visit = self.visit
		for i in node.nodes:
			visit (i)
		while 1:
			for i, j in enumerate (node.nodes):
				if isinstance (j, ast.Stmt):
					node.nodes [i:i+1] = j.nodes
					break
			else:
				break

		for i, j in enumerate (node.nodes):
			if isinstance (j, ast.Discard) and j.expr.isconst:
				# pyc::hints in strings for optimizations
				if (type (j.expr.value) is str
				and j.expr.value.startswith ('pyc::')):
					pyc_Hints (self.CurrentFunc [-1], j.expr.value)
				node.nodes [i] = ast.Pass (j.lineno)
			elif j.__class__ in end_stmt and node.nodes [i+1:]:
				node.nodes = node.nodes [:i+1]
				return

	def visitFor (self, node):
		self.visit (node.list)
		if isinstance (node.list, ast.List):
			List2Tuple (node.list)
		self.visit (node.assign)
		self.visit (node.body)
		if node.else_:
			self.visit (node.else_)

	def visitFunction (self, node):
		self.CurrentFunc.append (node)
		for c in node.getChildNodes ():
			self.visit (c)
		self.CurrentFunc.pop ()

	def visitSet (self, node, can_const=False):
		visit = self.visit
		all_consts = True
		for i in node.nodes:
			visit (i)
			if not i.isconst:
				all_consts = False
			elif type (i.value) is str:
				i.value = GCO.Intern (i.value)
		# XXX: do this after we see that set is used read-only: fronzen
		if can_const and all_consts and GCO ['arch'] != '2.3':
			make_const (node, set ([x.value for x in node.nodes]))
			del node.nodes
		else:
			if all_consts:
				t = ast.Const (tuple ([i.value for i in node.nodes]))
			else:
				t = ast.Tuple (node.nodes)
			make_copy (node, ast.CallFunc (ast.Name ('set', 0), [t]))

	del operator

	def visitClass (self, node):
		self.visitFunction (node)
		if not (len (node.bases) == 1 and isinstance (node.bases [0], ast.Const)):
			return
		if node.name == "__constant__" or node.constant:
			try:
				import exec_ast
				g = {}
				for n in node.code.nodes:
					try:
						exec_ast.do_exec (n, g)
					except:
						pass
				self.constns [node.name] = g
			except:
				print "Failed evaluating __constant__ namespace"

	def visitGetattr (self, node):
		self.visit (node.expr)
		if isinstance (node.expr, ast.Name) and node.expr.name in self.constns:
			try:
				make_copy (node, ast.Const (self.constns [node.expr.name][node.attrname]))
			except:
				pass

#
# Constify Names
#

class NameConstifier:

	def __init__ (self, D, node):
		self.D = D
		self.K = set (D.keys ())
		self.constify_names (node)

	def constify_names (self, node):
		# Walk the AST tree and replace Name()s that belong in the dictionary D
		# with Const()s with the value of it.
		if isinstance (node, ast.Name):
			if node.name in self.D:
				make_const (node, self.D [node.name])
		elif isinstance (node, ast.AssName):
			if node.name in self.D:
				raise SyntaxError, "assign to constant '%s' at %s:%i" % (node.name,
					 GCO ['filename'], node.lineno)
		elif isinstance (node, ast.Function):
			if self.K | set (node.argnames):
				D = dict (self.D)
				for i in node.argnames:
					if i in self.K:
						del D [i]
				self.D, D = D, self.D
				for i in node.getChildNodes ():
					self.constify_names (i)
				self.D = D
			else:
				for i in node.getChildNodes ():
					self.constify_names (i)
		else:
			for i in node.getChildNodes ():
				self.constify_names (i)

#
# Known type optimizer (LIST_APPEND)
#

class TypeVisitor:

	def __init__ (self, vtypes=None, atypes=None):
		if vtypes:
			vtypes = set ([x for x in vtypes if vtypes [x] is list])
			self.pred = lambda n: isinstance (n, ast.Name) and n.name in vtypes
		else:
			vtypes = set ([(s, a) for s, a, t in atypes if t is list])
			vtypes = vtypes | set ([a for s, a in vtypes])
			self.pred = lambda n: (isinstance (n, ast.Getattr) and n.attrname in vtypes
						and isinstance (n.expr, ast.Name) and
						(n.expr.name, n.attrname) in vtypes)

	def visitDiscard (self, node):
		n = node.expr
		if (isinstance (n, ast.CallFunc) and isinstance (n.node, ast.Getattr) and
		n.node.attrname == 'append' and self.pred (n.node.expr)):
			if len (n.args) != 1:
				raise SyntaxError ("list.append can only take 1 argument")
			make_copy (node, ast.ListAppend (n.node.expr, n.args [0]))
		elif isinstance (n, ast.LeftShift) and self.pred (n.left):
			make_copy (node, ast.ListAppend (n.left, n.right))
		else:
			return
			##self.visit (n)

def type_optimizer (root):
	if GCO ['arch'] == '2.3':
		# no LIST_APPEND in 2.3
		return

	for i in root.Functions:
		if i.symtab.know_types ():
			walk (i, TypeVisitor (i.symtab.defs))
	for c in root.Classes:
		if c.membertypes:
			for i in c.code.nodes:
				if isinstance (i, ast.Function):
					walk (i, TypeVisitor (atypes = c.membertypes))

#####

def rename (node, n1, n2):
	if isinstance (node, ast.Name) or isinstance (node, ast.AssName):
		if node.name == n1:
			node.name = n2
	else:
		for i in node.getChildNodes ():
			rename (i, n1, n2)

#
# Marshal builtins. This is an advanced NameConstifier
#

MB = set (__builtins__) & set (('IndexError', 'unicode', 'isinstance', 'NameError', 'dict',
	 'list', 'iter', 'round', 'cmp', 'set', 'reduce', 'intern', 'sum',
	 'getattr', 'abs', 'hash', 'len', 'frozenset', 'ord', 'TypeError', 'filter',
	 'range', 'pow', 'float', 'StopIteration', 'divmod', 'enumerate', 'apply', 'LookupError',
	 'basestring', 'zip', 'long', 'chr', 'xrange', 'type', 'Exception', 'tuple',
	 'reversed', 'hasattr', 'delattr', 'setattr', 'str', 'property', 'int', 'KeyError',
	 'unichr', 'id', 'OSError', 'min', 'max', 'bool', 'ValueError', 'NotImplemented',
	# builtin in pyvm
	 'array',
	# special pyvm features
	 '__SET_ITER__', '_list2tuple', '_buffer', '_setattr', '__sigbit__', '__rgb__',
	 '__prealloc__', 'tuple_or',
	 'ord16b', 'ord16bs', 'ord16l', 'ord16ls', 'ord32b', 'ord32l', 'ord_signed',
	 '_sid_',
	 'dict_from_list',
	 'signed_rshift', 'signed_lshift',
	 'lround',
	 'remove_sublist', 'tupdict_getitem', 'tupdict_hasitem',
	# complex
	 'map', 'buffer', 'object', 'callable', 'ZeroDivisionError', 'AttributeError'))

def cc_builtin (root, names):
	for i in root.getChildNodes ():
		if isinstance (i, ast.Name):
			if i.name in names:
				make_copy (i, ast.Const (__builtins__ [i.name]))
		elif not isinstance (i, ast.Const):
			cc_builtin (i, names)

def cc_type2compare (root):
	for i in root.getChildNodes ():
		if isinstance (i, ast.Compare):
			if (len (i.ops) == 1 and isinstance (i.expr, ast.CallFunc)
			and isinstance (i.expr.node, ast.Const)
			and i.expr.node.value is type
			and i.ops [0][0] == 'is'):
				# todo: is not !-> -!>
				make_copy (i, ast.Compare (i.expr.args [0], [('->', i.ops [0][1])]))
		elif not isinstance (i, ast.Const):
			cc_type2compare (i)

def marshal_builtins (module):
	s = module.symtab
	MMB = MB - set (s.defs)
	for i in module.Functions:
		s = i.symtab
		gl = s.globals - set (s.maybe_builtins)
		if gl | MMB:
			vg = gl | set (s.defs)
			MMB -= vg
	for i in module.Functions:
		bs = (MMB & i.symtab.uses)
		if bs:
			cc_builtin (i, bs)
			if 'type' in bs and not GCO ['pe']:
				cc_type2compare (i)


#
# pyc::Hints
# special hints for the compiler placed in strings
#

def pyc_Hints (func, hint):
	hint, args = hint [5:].split (' ', 1)
	if hint == 'type':
		varname, vtype = args.split (' ')
		if vtype == 'list':
			if varname in func.symtab.defs:
				func.symtab.defs [varname] = list
			else:
				print 'Function %s has no variable %s' % (func.name, varname)
		else:
			print 'Unsupported type in function %s' % func.name
	elif hint == 'membertype':
		varname, vtype = args.split (' ')
		varname, attr = varname.split ('.')
		if not isinstance (func, ast.Class):
			print '"membertype" must be applied at class level'
			return
		if vtype != 'list':
			print 'Unsupported type in class %s' % func.name
			return
		func.membertypes.append ((varname, attr, list))
	else:
		print 'Unsupported hint: %s' % hint

#
# __iter__ = 42
#

class IterVisitor:
	def visitAssign (self, node):
		if isinstance (node.nodes [0], ast.AssName) and node.nodes [0].name == '__iter__':
			if len (node.nodes) != 1 or not node.discarded:
				raise "Bug. Please hack the pyc compiler and improve __iter__"
			make_copy (node, ast.Discard (ast.CallFunc (ast.Const (__SET_ITER__), [node.expr])))

def do_iter (root):
	for i in root.Functions:
		if '__iter__' in i.symtab.defs:
			walk (i, IterVisitor ())

#
# public
#

def constify_names (node, D):
	NameConstifier (D, node)

def optimize_tree (tree):
	walk(tree, FoldConstVisitor())
	type_optimizer (tree)
	do_iter (tree)
