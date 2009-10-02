##  Necessary AST transformations
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) version 3 of the License.

import ast
from visitor	import walk
from gco	import GCO
from pycmisc	import counter
from optimizer	import make_copy, ast_Assign, ast_Getattr
from consts	import OP_ASSIGN

class TransformVisitor:

	def __init__ (self):
		self.dictcompn = 0

	# convert:  print >> X, text
	# to: __print_to__ (X.write, text, '\n')
	# for pyvm only

	def visitPrint (self, node, nl = 0):
		if not node.dest or not GCO ['pyvm']:
			for child in node.nodes:
				self.visit (child)
			return
		L = [ast.Getattr (node.dest, 'write')] + node.nodes
		if nl:
			L.append (ast.Const ('\n', node.lineno))
		make_copy (node, ast.Discard
			(ast.CallFunc (ast.Name ('__print_to__', node.lineno), L)))
		self.visit (node)

	def visitPrintnl (self, node):
		self.visitPrint (node, 1)

	# convert exec to __exec__()
	# for pyvm only

	def visitExec (self, node):
		self.visit (node.expr)
		if node.globals:
			self.visit (node.globals)
		if node.locals:
			self.visit (node.locals)
		if GCO ['pyvm']:
			make_copy (node, ast.Discard 
			(ast.CallFunc (ast.Name ('__exec__', node.lineno), list (node.getChildNodes ()))))

	# with-statement. Convert:
	#
	#	with EXPR as VAR:
	#		BLOCK
	# to:
	#	abc = (EXPR).__context__ ()
	#	exc = (None, None, None)
	#	VAR = abc.__enter__ ()
	#	try:
	#		try:
	#			BLOCK
	#		except:	exc = sys.exc_info (); raise
	#	finally: abc.__exit__ (*exc)
	#
	# if BLOCK doesn't have return/break/continue, do it as
	#
	#	abc = (EXPR).__context__ ()
	#	VAR = abc.__enter__ ()
	#	try:
	#		BLOCK
	#	except:	abc.__exit__ (*sys.exc_info ()); raise
	#	else: abc.__exit__ (None, None, None)
	#
	withcnt = counter()

	def visitWith (self, node):

		i = self.withcnt ()
		abc, exc = 'WiTh_CoNtExT__%i' %i, 'WiTh_ExC__%i' %i
		lno = 0 # XXXXXXXX: worry
		self.visit (node.expr)
		self.visit (node.code)
		rbc = hasRBC (node.code)
		stmts = []
		stmts.append (ast_Assign (abc, ast.CallFunc (ast.Getattr (node.expr, '__context__'), [])))
		if rbc:
			stmts.append (ast_Assign (exc, ast.Const ((None, None, None))))
		enter = ast.CallFunc (ast_Getattr (abc, '__enter__'), [])
		if node.var:
			enter = ast_Assign (node.var, enter)
		else:
			enter = ast.Discard (enter)
		stmts.append (enter)
		if rbc:
			stmts.append (ast.TryFinally (
		ast.TryExcept (
			node.code,
			[(None, None, ast.Stmt ([
				ast_Assign (exc, ast.CallFunc (ast_Getattr ('sys', 'exc_info'), [])),
				ast.Raise (None, None, None, lno)
			]))],
			None, lno
		),
		ast.Discard (ast.CallFunc (ast_Getattr (abc, '__exit__'), [], ast.Name (exc, 0))),
		lno
	    ))
		else:
			stmts.append (ast.TryExcept (
				node.code,
				[(None, None, ast.Stmt ([
					ast.Discard (ast.CallFunc (ast_Getattr (abc, '__exit__'), [],
					ast.CallFunc (ast_Getattr ('sys', 'exc_info'), []))),
					ast.Raise (None, None, None, lno)
				]))],
				ast.Stmt ([
					ast.Discard (ast.CallFunc (ast_Getattr (abc, '__exit__'),
					3 * [ast.Const (None)]))
				])
				, lno
			))
		make_copy (node, ast.Stmt (stmts))

	# For a big dictionary definition, it's more space efficient to build a list
	# and call dict_from_list. If the elements are constant, the list
	# will be marshalled as a tuple.

	def visitDict (self, node):
		for k, v in node.items:
			self.visit (k)
#			if k.isconst and type (k.value) is str:
#				k.value = GCO.Intern (k.value)
			self.visit (v)
		if GCO ['pyvm'] and len (node.items) > 3:
			L = []
			for k, v in node.items:
				L.append (k)
				L.append (v)
			l = ast.CallFunc (ast.Name ('dict_from_list', 0),
				 [ast.Tuple (L)])
			make_copy (node, l)

	# Dict comprehensions are transformed to code:
	#	{x:y for (x in z)};
	# transforms to:
	#	`{0}` = {}
	#	for (x in z)
	#		`{0}` [x] = y
	#	`{0}`

	def visitDictComp (self, node):
		# XXX: should probably DELETE the temporary afterwards.
		# also, in the usual case of assignment, don't use temporary.
		v = "_{%i}" %self.dictcompn
		self.dictcompn += 1
		for x in node.getChildNodes ():
			self.visit (x)
		self.dictcompn -= 1
		S = []
		S.append (ast.Assign ( [ast.AssName (v, 'OP_ASSIGN')], ast.Dict ([])))
		SS = S
		for n in node.quals:
			S2 = []
			SS.append (ast.For ( n.assign, n.list, ast.Stmt (S2), None))
			SS = S2
			for nn in n.ifs:
				S2 = []
				SS.append (ast.If ([(nn.test, ast.Stmt (S2))], None))	
				SS = S2
		SS.append (ast.Assign (
			[ast.Subscript (ast.Name (v, 0), 'OP_ASSIGN', [node.kexpr])], node.vexpr,
			discarded=True
		))
		S.append (ast.Name (v, 0))

		st = ast.Stmt (S)
		make_copy (node, st)

#	def visitDiscard (self, node):
#		return

	def visitAugAssign (self, node):
		# pyvm does not have some augmented assignment opcodes
		# like INPLACE_SLHIFT and INPLACE_AND. We convert
		# 	x<<=1
		# to
		#	x = x << 1
		# if x is a name. We *can* implement the opcodes whenever
		# we want, but they are really rare and we can just keep
		# the main interpreter loop smaller with this transformation.
		self.visit (node.node)
		self.visit (node.expr)
		n = node.op
		if n in ('<<=', '&='):
			if isinstance (node.node, ast.Name):
				if n == '&=':
					a = ast.Bitand ([node.node, node.expr])
				else:
					a = ast.LeftShift ((node.node, node.expr))
				dis = node.discarded
				make_copy (node, ast.Assign ([ast.AssName (node.node.name,
					   OP_ASSIGN)], a))
				node.discarded = dis

class RBCFinder:
	# Traverse AST and look for return/break/continue that affect the
	# current node

	def __init__ (self):
		self.inloop = 0

	def visitReturn (self, node):
		raise RBCFinder

	def visitBreak (self, node):
		if not self.inloop:
			raise RBCFinder

	visitContinue = visitBreak

	def visitWhile (self, node):
		self.inloop += 1
		self.visit (node.body)
		self.inloop -= 1
		if node.else_:
			self.visit (node.else_)

	visitDoWhile = visitFor = visitWhile

	def visitClass (self, node):
		pass

	visitFunction = visitClass
	visitDicard = visitClass
	visitAssign = visitClass
	visitAugAssign = visitClass

def hasRBC (node):
	try:
		walk (node, RBCFinder ())
		return False
	except RBCFinder:
		return True

def transform_tree (tree):
	walk(tree, TransformVisitor())

	# scan the __bind__ directive
	tree.BoundNames = ()
	for n in tree.node.nodes:
		if (isinstance (n, ast.Assign) and len (n.nodes) == 1 and
		   isinstance (n.nodes [0], ast.AssName) and n.nodes [0].name == "__bind__"
		   and n.nodes [0].flags == "OP_ASSIGN"):
			if isinstance (n.expr, ast.List):
				CN = []
				for n in n.expr.nodes:
					if isinstance (n, ast.Const) and type (n.value) is str:
						CN.append (n.value)
					else:
						print "__bind__: NOT A NAME"
				tree.BoundNames = CN
			else:
				print "__bind__: Not a list!"
