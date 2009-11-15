##  Parser for whitespaceless
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) version 3 of the License.

#
# Reads .pe source code and generates AST
#
# ToDo:
#	- better syntax errors
#	- parenthesis aren't really needed in list comprehensions (hmm)
#
# Theoretically, you could run this with python provided there is
# a function 'evalconst' in a module 'builtins'.
#
import re
import itertools
from pyc import ast
from pyc import gco
from pyc.consts import CO_VARARGS, CO_VARKEYWORDS, CO_GENERATOR
from pyc.parser import intern_if_all_chars
from builtins import evalconst

# Implementation is very hairy and in case of bugs it sure can
# be very furstrating. Do not use by default.
AUTOMATIC_SEMICOLON = False

def ienumerate (x):
	for i, j in itertools.izip (itertools.count (), x):
		yield i, j

isval = re.compilef (r'''i?r?['"]|\d|\.\d''')
isstr = re.compile  ('''['"]''').matches
issym = re.compilef (r'\w')

RESERVED = set ((
	# removed: pass, elif, exec
	# added: method, gen, namespace, do
	'and', 'assert', 'break', 'class', 'continue', 'def', 'del',
	'else', 'except', 'finally', 'for', 'from', 'gen', 'global', 'if',
	'import', 'in', 'is', 'lambda', 'method', 'namespace', 'not', 'or',
	'print', 'raise', 'return', 'try', 'with', 'while', 'yield', 'do', '__LINE__',
	'else.for', 'else.while', 'else.if', 'if.break', 'if.continue',
	'try.break',
))

def ttype (x):
	if isval (x):
		return '0'
	if issym (x) and x not in RESERVED:
		return 's'
	return x

#
# The prefixer is a proxy parser between the Lexer and the expression compiler.
# It takes an iterable of tokens and returns a tuple where the tokens are
# reordered in prefix. For example, if we give it "x+y*3" or iow the tuple
# ('x', '+', 'y', '*', 3) it will return ('x', 'y', 3, '*', '+')
#
# But there are many exceptions, special cases and caveats.
# Feed it some input by running "pe.py -p file-with-expressions-only"
# to see what's generated each time.
#

POWER_PRI   = 4
UNARY_PRI   = 5
MULDIV_PRI  = 9
ADDSUB_PRI  = 10
SHIFT_PRI   = 11
BITAND_PRI  = 12
BITOR_PRI   = 13
BITXOR_PRI  = 14
COMP_PRI    = 15
NOT_PRI     = 16
BOOLAND_PRI = 17
BOOLOR_PRI  = 18
TENARY_PRI  = 19
EXCOR_PRI   = 20
LAMBDA_PRI  = 21
ASSIGN_PRI  = 22
KW_PRI      = 23
COLON_PRI   = 24

NEST_PRI = 100

RBINPRI = {
	POWER_PRI:  '**',
	UNARY_PRI:  ('~', '!'),
	MULDIV_PRI: ('*', '/', '%'),
	ADDSUB_PRI: ('+', '-'),
	SHIFT_PRI:  ('>>', '<<'),
	BITAND_PRI: '&',
	BITOR_PRI:  '|',
	BITXOR_PRI: '^',
	COMP_PRI:  ('==', '!=', '<>', '<', '>', '>=', '<=', 'is', 'is not', 'in', 'not in',
		    '=>', '->', '!->'),
	NOT_PRI:     'not',
	BOOLAND_PRI: 'and',
	BOOLOR_PRI:  'or',
	EXCOR_PRI:   '??',
	LAMBDA_PRI:  'def',
	ASSIGN_PRI: ('+=', '-=', '*=', '/=', '**=', '&=', '|=', '^=', '>>=', '<<=', '='),
	COLON_PRI:  (':', ':0'),
	KW_PRI:      '=kw',
}

BINPRI = {}
for k, v in RBINPRI.items ():
	if type (v) is tuple:
		for i in v:
			BINPRI [i] = k
	else:
		BINPRI [v] = k

del k, v, i, RBINPRI

class SyntaxError (Exception):
	pass

class Prefixer:

	def error (self, op):
#		if AUTOMATIC_SEMICOLON:
#			whereami ()
		raise SyntaxError ("Unexpected token \"%s\" in current context [%s] (line %i)" %(op,
			 self.state, self.it.line))

	def nesting_is (self, n):
		return self.nests [-1][1] == n

	def nesting_is_fcall (self):
		return self.nests [-1][-1] == 'f'

	def nesting_is_subsc (self):
		return self.nests [-1][-1] == 's'

	def nesting_is_tuple (self):
		m, t = self.nests [-1][-1], self.nests [-1][1]
		return (m == '' and t == '(') or (m and m in 'sFi')

	def nesting (self):
		self = self.nests [-1]
		return self [-1], self [1]

	def set_nesting (self, m, n):
		s = list (self.nests [-1])
		s [-1], s [1] = m, n
		self.nests [-1] = tuple (s)

	def checkstate (self, s, t=None):
		if self.state != s:
			self.error (t or self.current)

	# state handlers

	'pyc::membertype self.stack list'
	'pyc::membertype self.nests list'
	'pyc::membertype self.commas list'

	def binop (self, op):
		self.checkstate ('postfix')
		pri = BINPRI [op]
		self.flush (pri)
		self.stack.append ((op, pri))
		self.state = 'prefix'

	def cmpop (self, op):
		self.checkstate ('postfix')
		self.flush (COMP_PRI - 1)
		if self.stack and self.stack [-1][1] == COMP_PRI:
			L = self.stack [-1][0]
			self.stack [-1] = L + ',' + op, COMP_PRI
		else:
			self.stack.append ((op, COMP_PRI))
		self.state = 'prefix'

	def boolop (self, op):
		self.checkstate ('postfix')
		pri = BINPRI [op]
		self.flush (pri - 1)
		if self.stack and self.stack [-1][1] == pri:
			self.stack [-1] = self.stack [-1][0] + 'x', pri
		else:
			if op == 'and': op = '&&'
			elif op == 'or': op = '||'
			self.stack.append ((op, pri))
		self.state = 'prefix'

	def opunamb (self, op):
		self.stack.append ((op + 'p', UNARY_PRI))

	def opun (self, op):
		self.checkstate ('prefix')
		self.stack.append ((op, BINPRI [op]))

	def ddef (self, op):
		self.checkstate ('prefix')
		self.stack.append (('ddef', LAMBDA_PRI))
		self.state = 'lambda'

	def member (self, s):
		self.lno (self.it.line)
		self.output (s)
		self.output ('.')
		self.state = 'postfix'

	def dynmember (self, s):
		self.nest ('(')
		self.stack.append (('.$', 22))

	def dynthe (self, s):
		self.nest ('(')
		self.stack.append (('.$$', 22))

	def value (self, s):
		self.checkstate ('prefix', s)
		self.output (s)
		self.state = 'postfix'

	def LINE (self, s):
		self.value ("%i" % self.it.line)

	def symbol (self, s):
		self.checkstate ('prefix', s)
		self.lno (self.it.line)
		self.output (s)
		self.state = 'postfix'

	def dot (self, s):
		self.state = 'dot'

	def _flush_asgn (self):
		a, self.asgn = self.asgn, ''
		self.output (a)

	def comma (self, s):
		self.checkstate ('postfix')
		if not self.nesting_is_tuple () and self.asgn:
			self._flush_asgn ()
		else:
			self.flush (KW_PRI)
		if self.nesting_is ('{'):
			if self.kv:
				self.kv = False
			elif self.kv == 0:
				self.kv = None
			elif self.kv is not None:
				self.error (s)
		self.commas.append (self.i)
		self.state = 'prefix'

	def nest (self, t, m=''):
		self.nests.append ((self.commas,t,self.fors, self.tenary, self.i, self.kv, self.asgn, m))
		self.state = 'prefix'
		self.commas = []
		self.kv = 0
		self.tenary = 0
		self.fors = 0
		self.asgn = ''
		self.stack.append (('', NEST_PRI))

	def unnestc (self, te):
		self.stack.pop ()
		ff = self.fors
		self.commas,ts, self.fors, self.tenary, start, self.kv, self.asgn, mod = self.nests.pop ()
		t = ts + '%i' + te + 'L'
		if t not in ('(%i)L', '[%i]L', '{%i}L'):
			self.error (te)
		self.state = 'postfix'
		self.output (t % ff)

	def unnest (self, te, f=0):
		# This is very complicated because of tuple context 
		self.flush (KW_PRI)
		if self.asgn:
			self._flush_tup ()
			self._flush_asgn ()
		elif self.nesting ()[0] in 'Fis' and self.nesting ()[0]:
			self._flush_tup ()
		self.flush (COLON_PRI)
		self.stack.pop ()
		commas = self.commas
		N = len (self.commas)
		if not N and f:
			return
		kv = self.kv
		self.commas,ts, self.fors, self.tenary, start, self.kv, self.asgn, mod = self.nests.pop ()
		t = ts + '%i' + te
		if t not in ('(%i)', '[%i]', '{%i}'):
			self.error (te)
		if ts == '{' and kv is None:
			t = '#' + t [1:]
		if mod == 's' and self.out and self.out [-1] in (':', ':0'):
			sl = self.out.pop ()
			if self.state == 'prefix':
				sl += '1'
			mod = sl
		t += mod
		if mod and mod in 'Fi': self.state = 'comprehension'
		else: self.state = 'postfix'
		if self.i - start == 1:
			N = 0
		elif not N:
			if t == '(%i)':
				return
			N = 1
		elif commas [-1] != self.i-1:
			N += 1
		self.output (t % N)

	def ustar (self, op):
		if not self.nesting_is_fcall ():
			self.stack.append (("\\", UNARY_PRI))
		else:
			self.argz (op)

	def argz (self, op):
		if not self.nesting_is_fcall ():
			self.error (op)
		self.stack.append ((op + 'a', UNARY_PRI))

	def _flush_tup (self):
		if self.commas and self.nesting_is_tuple ():
			if self.i - self.commas [-1] > 1:
				i = 1
			else:
				i = 0
			n, self.commas = len (self.commas), []
			self.output ('(%i)'%(n+i))

	def assign (self, op):
		if self.nesting_is_fcall () and op == '=':
			self.binop ('=kw')
		else:
			if not (self.state == 'prefix' and self.commas [-1] + 1 == self.i):
				self.checkstate ('postfix')
			self._flush_tup ()
			self.asgn += '@' + op
			self.state = 'prefix'

	def colon (self, op):
		if self.tenary:
			self.checkstate ('postfix')
			self.state = 'prefix'
			self.tenary -= 1
			self.flush (TENARY_PRI)
			self.stack.pop ()
			self.stack.append (('?:', TENARY_PRI))
			return
		if self.kv in (False, 0):
			self.kv = True
		elif self.kv is None:
			self.error (op)
		if self.nesting_is ('{'):
			if self.asgn:
				self._flush_asgn ()
			self.checkstate ('postfix')
			self.state = 'prefix'
			self.flush (KW_PRI)
		elif self.nesting_is_subsc ():
			if self.state == 'prefix':
				self.opun (':0')
			else:
				if self.asgn:
					self._flush_asgn ()
				self.binop (':')
		else:
			self.error (op)

	def the (self, op):
		self.checkstate ('prefix')
		self.state = 'the'

	def dothe (self, s):
		self.lno (self.it.line)
		self.output (s)
		self.output ('$')
		self.state = 'postfix'

	def globaldot (self, op):
		self.checkstate ('prefix')
		self.output (op)
		self.state = 'gdot'

	def notop (self, op):
		if self.stack and self.stack [-1][0] == 'is':
			self.stack [-1] = 'is not', COMP_PRI
		else: self.opun (op)

	def notinop (self, op):
		self.state = 'notin'

	def donotin (self, op):
		self.state = 'postfix'
		self.cmpop ('not in')

	def tenary (self, op):
		self.checkstate ('postfix')
		self.flush (TENARY_PRI - 1)
		self.state = 'prefix'
		self.tenary += 1
		self.stack.append (('', NEST_PRI))

	def forop (self, op):
		if self.state != 'comprehension':
			self.checkstate ('postfix')
			m, n = self.nesting ()
			if m or n not in '[({':
				self.error (op)
			self.flush (KW_PRI)
			if self.commas:
				self.set_nesting ('', '(')
				self._flush_tup ()
			self.set_nesting ('L', n)
		if self.itr.next () [1] != '(':
			self.error ('(')
		self.fors += 1
		self.nest ('(', 'F')

	def ifop (self, op):
		self.checkstate ('comprehension')
		if self.itr.next () [1] != '(':
			self.error (op)
		self.nest ('(', 'i')

	def inop (self, op):
		if self.nesting ()[0] != 'F':
			return self.cmpop ('in')
		self._flush_tup ()
		self.state = 'prefix'

	def ddefargs (self, op):
		L = []
		for n, i in self.itr:
			if i != ')':
				L.append (i)
			else:
				break
		ccc = eplcc (0, ' '.join (L) + ')')
		ccc.doit ()
		ccc = ccc.tree
		self.stack.append ((ccc, LAMBDA_PRI))
		self.state = 'prefix'

	def at (self, op):
		self.checkstate ('prefix')
		a = self.itr.next () [1]
		if ttype (a) != 's':
			if ttype (a) == "0" and a [0] in ''''"''':
				a = a [1:-1]
			else:
				self.error (a)
		while 1:
			n = self.itr.next () [1]
			if n != '.':
				self.itr.unyield ()
				self.output ('~' + a)
				self.state = 'postfix'
				break
			n = self.itr.next () [1]
			if ttype (n) == 's':
				a += '.' + n
			elif ttype (n) == "0" and n [0] in ''''"''':
				a += '.' + n [1:-1]
			else:
				self.error (n)

	DFA = {
		'.': {'postfix':dot, 'gdot':dot},
		'$': the,
		'@': at,
		'+': {'prefix':opunamb, 'postfix':binop},
		'-': {'prefix':opunamb, 'postfix':binop},
		'~': opun, '!': opun,
		'**': {'postfix':binop, 'prefix':argz},
		'*': {'postfix':binop, 'prefix':ustar},
		'/': binop, '%': binop, '&': binop, '|': binop, '^': binop,
		'<<': binop, '>>': binop,
		'is': cmpop,
		'in': {'postfix':inop, 'notin':donotin},
		'or': boolop, 'and': boolop,
		'??': boolop,
		'>=': cmpop, '<=': cmpop, '==': cmpop, '!=': cmpop,
		'=': assign,
		'>': cmpop, '<': cmpop,
		'=>': cmpop,
		'->': cmpop,
		'!->': cmpop,
		'??': cmpop,
		'+=': assign, '-=': assign, '*=': assign, '/=': assign, '%=': assign,
		'&=': assign, '|=': assign, '^=': assign, '<<=': assign, '>>=': assign,
		'not': {'prefix':notop, 'postfix':notinop},
		',': comma,
		'?': tenary,
		':': colon,
		'[': { 'prefix':nest, 'postfix':lambda self,x:self.nest (x,'s') },
		'(': { 'prefix':nest, 'postfix':lambda self,x:self.nest (x,'f'),
		       'dot':dynmember, 'the':dynthe, 'lambda':ddefargs },
		']': { 'prefix':unnest, 'postfix':unnest, 'comprehension':unnestc },
		')': { 'prefix':unnest, 'postfix':unnest, 'comprehension':unnestc },
		'{': nest,
		'}': { 'prefix':unnest, 'postfix':unnest, 'comprehension':unnestc },
		's': { 'the':dothe, 'prefix':symbol, 'dot':member },
		'for': forop,
		'if': ifop,
		'def': ddef,
		'global': globaldot,
		'0': value,
		'__LINE__': LINE,
	}

	def output (self, x):
		self.out.append (x)

	def flush (self, pri):
		while self.stack and self.stack [-1][1] <= pri:
			self.output (self.stack.pop ()[0])

	def do (self, it, stopat = ';'):
		self.stack = []
		self.out = []
		nests = self.nests = []
		self.state = 'prefix'
		self.fors = self.tenary = self.i = self.commas = 0
		self.kv = 0
		self.asgn = ''
		self.nest ('(')
		self.it = it
		LNO = []
		self.lno = LNO.append
		itr = self.itr = ienumerate (it)
		##print "Parsing expresion"
		for self.i, i in itr:
			##print 'git:', i
			self.current = i
			if len (nests) == 1 and i in stopat:
				break
			### This thing here is automatic semicolon insertion
			if AUTOMATIC_SEMICOLON and self.it.sol:
				if i in ("for", "}", "if", "def") and len (nests) == 1 and ";" in stopat:
					self.it.ungetc ()
					break
			#####################################################
			j = ttype (i)
			##print j, i, self.state, self.DFA [j]
			try:
				DFA = self.DFA [j]
				if type (DFA) is not dict:
					DFA (self, i)
				else:
					DFA [self.state] (self, i)
			except:
				### This thing here is automatic semicolon insertion
				if len (nests) == 1 and self.it.sol and ';' in stopat:
					if AUTOMATIC_SEMICOLON:
						self.it.ungetc ()
						break
				#####################################################
				self.error (i)
		else:
			self.error ('EOF')
		self.unnest (')', 1)
		return self.out, LNO or [it.line]

	# comprehend, lambda

if __name__ == '__main__' and '-p' in sys.argv:
	from tokenize2 import Lexer
	P = Prefixer ()
	L = Lexer (open (sys.argv [2]).read ())
	while 1:
		st, _ = P.do (L)
		print st
		print _
	raise SystemExit

#
# Generate AST from prefixed expressions.
# This class takes a tuple of tokens as generated by the Prefixer
# and returns an AST tree for the expression.
#

def streval (s):
	if s and s [0] == "i":
		i = True
		s = s [1:]
	else:
		i = False
	s = evalconst (s)
	if type (s) is not str:
		return s
	if i:
		return intern (s)
	return intern_if_all_chars (s)

def assignedNode (node, operation = 'OP_ASSIGN'):
	if node.__class__ is ast.Name:
		return ast.AssName (node.name, operation, node.lineno)
	if node.__class__ is ast.Getattr:
		return ast.AssAttr (node.expr, node.attrname, operation)
	if node.__class__ is ast.Tuple:
		return ast.AssTuple ([assignedNode (x, operation) for x in node.nodes], node.lineno)
	if node.__class__ is ast.List:
		return ast.AssList ([assignedNode (x, operation) for x in node.nodes], node.lineno)
	if node.__class__ in (ast.Subscript, ast.Slice):
		node.flags = operation
		return node
	##print repr (node)
	raise SyntaxError ("Can't assign to operator")

class exprcc:

	# --- ACTIONS ---

	def dodot (self):
		r = self.pop ()
		l = self.pop ()
		self.push (ast.Getattr (l, r.name))

	def dotenary (self):
		r = self.pop ()
		l = self.pop ()
		c = self.pop ()
		self.push (ast.Conditional (c, l, r))


	def dokw (self):
		r = self.pop ()
		l = self.pop ()
		self.push (ast.Keyword (l.name, r))


	def doself (self):
		N = self.pop ()
		self.push (ast.Getattr (ast.Name ('self', N.lineno), N.name))

	def dyndot (self):
		r = self.pop ()
		l = self.pop ()
		self.push (ast.Getattr (l, r))

	def dynself (self):
		N = self.pop ()
		self.push (ast.Getattr (ast.Name ('self', 0), N))

	def dyndef (self):
		names, defaults, flags = self.pop ()
		N = self.pop ()
		self.push (ast.Lambda (names, defaults, flags, N, 0))

	def doxrange (self):
		N = self.pop ()
		self.push (ast.CallFunc (ast.Name ("xrange", 0), [N], None, None))

	ACTIONS = {
		'.': dodot,
		'=kw': dokw,
		'$': doself,
		'?:': dotenary,
		'.$': dyndot,
		'.$$': dynself,
		'ddef': dyndef,
		'\\': doxrange,
	}

	def mkbin (N):
		def b (self):
			r = self.pop ()
			l = self.pop ()
			self.push (N ((l, r)))
		return b

	for i, j in (('*', ast.Mul), ('/', ast.Div), ('%', ast.Mod),
		     ('+', ast.Add), ('-', ast.Sub), ('<<', ast.LeftShift), 
		     ('>>', ast.RightShift), ('**', ast.Power)):
		ACTIONS [i] = mkbin (j)

	def mkun (N):
		def u (self):
			self.push (N (self.pop ()))
		return u

	for i, j in (('+p', ast.UnaryAdd), ('-p', ast.UnarySub), ('~', ast.Invert),
		     ('not', ast.Not), ('!', ast.Not)):
		ACTIONS [i] = mkun (j)

	def mkbit (N):
		def b (self):
			r = self.pop ()
			l = self.pop ()
			self.push (N ([l, r]))
		return b

	for i, j in (('&', ast.Bitand), ('|', ast.Bitor), ('^', ast.Bitxor)):
		ACTIONS [i] = mkbit (j)

	def mkcmp (OP):
		N = ast.Compare
		def c (self):
			r = self.pop ()
			l = self.pop ()
			self.push (N (l, [(OP, r)]))
		return c

	for i in ('==', '!=', '<>', '<=', '>=', '<', '>', 'is', 'is not', 'in',
		  'not in', '=>', '->', '!->'):
		ACTIONS [i] = mkcmp (i)

	# --- FACTIONS ---

	def mkbool (N):
		def b (self, s):
			n = len (s)
			self.S [-n:] = [N (self.S [-n:])]
		return b

	def mkcontainer (self, C, n):
		if n: self.S [-n:] = [C (self.S [-n:])]
		else: self.push (C ([]))

	def dodictcomp (self, s):
		n = int (s [1:-2])
		key = self.S [-n-2]
		val = self.S [-n-1]
		fors = self.S [-n:]
		self.S [-n-2:] = [ast.DictComp (key, val, fors)]

	def dodict (self, s):
		if s [-1] == "L":
			return self.dodictcomp (s)
		n = 2 * int (s [1:-1])
		if not n: return self.mkcontainer (ast.Dict, 0)
		D = []
		t = None
		for i in self.S [-n:]:
			if not t: t = i
			else:
				D.append ((t, i))
				t = None
		self.S [-n:] = [ast.Dict (D)]

	def doset (self, s):
		n = int (s [1:-1])
		D = self.S [-n:]
		self.S [-n:] = [ast.Set (D)]

	def docomp (self, s):
		n = int (s [1:-2])
		fors = self.S [-n:]
		##print fors
		n += 1
		if s [0] == '[':
			self.S [-n:] = [ast.ListComp (self.S [-n], fors)]
		else:
			for i in fors:
				i.__class__ = ast.GenExprFor
				i.iter = i.list
				del i.list
				for i in i.ifs:
					i.__class__ = ast.GenExprIf
			self.S [-n:] = [ast.GenExpr (ast.GenExprInner (self.S [-n], fors))]


	def dobrk (self, s):
		if s [-1] == ']':
			self.mkcontainer (ast.List, int (s [1:-1]))
		elif s [3] == ':':
			s = s[4:]
			st = en = None
			if not s:
				en = self.pop ()
				st = self.pop ()
			elif s == '0':
				en = self.pop ()
			elif s == '1':
				st = self.pop ()
			self.push (ast.Slice (self.pop (), 'OP_APPLY', st, en))
		elif s [-1] == 'L':
			self.docomp (s)
		else:
			r = self.pop ()
			l = self.pop ()
			self.push (ast.Subscript (l, 'OP_APPLY', (r,)))

	def dopar (self, s):
		ss = s [-1]
		if ss == ')':
			self.mkcontainer (ast.Tuple, int (s [1:-1]))
		elif ss in 'FiL':
			if ss == 'F':
				r, l = self.pop (), assignedNode (self.pop ())
				self.push (ast.ListCompFor (l, r, []))
			elif ss == 'i':
				l = self.pop ()
				self.S [-1].ifs.append (ast.ListCompIf (l))
			else:
				self.docomp (s)
		else:
			n = int (s [1:-2]) + 1
			s = self.S [-n:][1:]
			star = dstar = None
			if s and type (s [-1]) is tuple:
				star = s.pop ()
				if star [0] == '*a':
					star = star [1]
				else:
					dstar = star [1]
					if s and type (s [-1]) is tuple:
						star = s.pop ()
						if star [0] == '*a':
							star = star [1]
						else:
							raise "Error"
					else:
						star = None
			self.S [-n:] = [ast.CallFunc (self.S [-n], s, star, dstar)]
			
	def doargz (self, s):
		self.push ((s, self.pop ()))

	def doassign (self, s):
		for i in reversed (s.split ('@')[1:]):
			r = self.pop ()
			l = self.pop ()
			if i == '=':
				l = ast.Assign ([assignedNode (l)], r)
			else:
				l = ast.AugAssign (l, i, r)
			self.push (l)

	def doat (self, s):
		self.push (ast.AtImport (s [1:]))

	FACTIONS = {
		'&': mkbool (ast.And),
		'|': mkbool (ast.Or),
		'?': mkbool (ast.ExcOr),
		'{': dodict,
		'[': dobrk,
		'(': dopar,
		'#': doset,
		'*': doargz,
		'@': doassign,
		'~': doat,
	}

	def domcmp (self, op):
		L = [(j, self.pop ()) for j in reversed (op.split (','))]
		L.reverse ()
		self.push (ast.Compare (self.pop (), L))
		
	for i in "<>=!":
		FACTIONS [i] = domcmp

	del mkbin, mkun, mkbit, i, j, dodot, dotenary, mkcmp, mkbool
	del doargz, dokw, dopar, dodict, doassign, doself

	##### main loop ########

	def do (self, (pex, lno)):
		try:
			return self._do ((pex, lno))
		except:
			if lno:
				l = str (lno [0])
			else:
				l = "<unknown>"
			raise SyntaxError ("Some error near line %s" %l)

	def _do (self, (pex, lno)):
		##print "DO:", pex, lno
		lno = iter (lno)
		self.S = S = []
		def push (x):
			S.append (x)
		def pop ():
			return S.pop ()
		self.push, self.pop = push, pop
		Name, Const = ast.Name, ast.Const
		is_val = isval
		ACTIONS, FACTIONS = self.ACTIONS, self.FACTIONS

		for i in pex:
		##	print 'next:', i, type (i);
			if i in ACTIONS:
				ACTIONS [i] (self)
			elif i [0] in FACTIONS:
				FACTIONS [i [0]] (self, i)
			else:
				if type (i) is tuple:
					push (i)
				elif not is_val (i):
					if ',' in i:
						self.domcmp (i)
					else:
						push (Name (intern (i), lno.next ()))
				else:
					push (Const (streval (i)))
		S, = S
		return S

	# "is,=="
	# global., lambda

#
# Toplevel statement parser. Invokes exprcc and Prefixer for expressions.
#

class eplcc:

	def __init__ (self, filename, source, autosem=False):
		self.filename = filename
		self.autosem = autosem
		self.ns = None
		from tokenize2 import Lexer
		if not source:
			source = open (filename).read ()
		L = self.L = Lexer (source)
		P = self.P = Prefixer ()
		E = self.E = exprcc ()
		Edo, Pdo = E.do, P.do
		def getexpr (t):
			return Edo (Pdo (L, t))
		self.getexpr = getexpr
		self.nextc = L.nextc
		self.ungetc = L.ungetc

		# build the dispatcher
		def NA ():
			print "NOT IMPLEMENTED STATEMENT"
		def else_error ():
			raise SyntaxError ("else without if/for/while/except at %i" % L.line)
		def mkn (N):
			def f ():
				try:
					self.expect (';')
				except:
					if not (self.L.sol and AUTOMATIC_SEMICOLON):
						raise
					self.ungetc ()
				return N ()
			return f
		self.dispatcher = {
			'{': self.compound_statement,
			';': ast.Pass,
			'if': self.if_statement,
			'if.break': self.ifbreak_statement,
			'if.continue': self.ifcontinue_statement,
			'while': self.while_statement,
			'do': self.do_statement,
			'return': self.return_statement,
			'yield': self.yield_statement,
			'for': self.for_statement,
			'def': self.def_statement,
			'gen': self.gen_statement,
			'class': self.class_statement,
			'with': self.with_statement,
			'try': self.try_statement,
			'try.break': self.trybreak_statement,
			'break': mkn (ast.Break),
			'continue': mkn (ast.Continue),
			'del': self.del_statement,
			'global': self.global_statement,
			'import': self.import_statement,
			'from': self.from_statement,
			'assert': NA,
			'raise': self.raise_statement,
			'print': self.print_statement,
			'method': self.method_statement,
			'else': else_error,
			'finally': else_error,
			'namespace': self.namespace_statement,
			'else.for': else_error,
			'else.try': else_error,
			'else.while': else_error,
			'else.if': else_error,
			'__automatic_semicolon__': self.enable_automatic_semicolon,
			'__autosem__': self.enable_automatic_semicolon,
		}

	def doit (self):
		# do it
		if self.filename == 0:
			self.tree = self.get_defargs ()
		else:
			self.tree = ast.Module (self.translation_unit (self.autosem))

	# handy

	def expect (self, t):
		if self.nextc () != t:
			raise SyntaxError ("expected %s at line %i"% (t, self.L.line))

	def mustbe (self, a, b):
		if a != b:
			raise SyntaxError ("expected %s got %s (line %i)" % (a, b, self.L.line))

	def nextis (self, t):
		try:
			if self.nextc () == t:
				return True
			self.ungetc ()
		except StopIteration:
			pass
		return False

	def nextnl (self):
		if not AUTOMATIC_SEMICOLON:
			return False
		try:
			self.nextc ()
			nl = self.L.sol
			self.ungetc ()
		except StopIteration:
			nl = True
		return nl

	def next_oneof (self, *t):
		x = self.nextc ()
		if x in t:
			return x
		raise SyntaxError ("in line %i, expected %s, got %s" %(self.L.line, t, x))

	# statement states

	def expression_statement (self):
		e = self.getexpr (';')
		if not isinstance (e, ast.Assign) and not isinstance (e, ast.AugAssign):
			e = ast.Discard (e)
		return e

	def compound_statement (self):
		S = []
		while 1:
			if self.nextis ('}'):
				return ast.Stmt (S or [ast.Pass ()])
			try:
				S.append (self._statement ())
			except StopIteration:
				raise SyntaxError ('missing "}"')

	def statexpr (self):
		Not = self.nextis ("!")
		self.expect ('(')
		e = self.getexpr (')')
		if Not:
			e = ast.Not (e)
		return e

	def if_statement (self):
		T = self.statexpr ()
		S = self.statement ()
		if self.nextis ('else'): E = self.statement ()
		elif self.nextis ('else.if'): E = self.statement ()
		else: E = None
		return ast.If ([(T, S)], E)

	def ifbreak_statement (self):
		N = self.if_statement ()
		N.tests [0][1].nodes.append (ast.Break ())
		return N

	def ifcontinue_statement (self):
		N = self.if_statement ()
		N.tests [0][1].nodes.append (ast.Continue ())
		return N

	def while_statement (self):
		T = self.statexpr ()
		S = self.statement ()
		if self.nextis ('else'): E = self.statement ()
		elif self.nextis ('else.while'): E = self.statement ()
		else: E = None
		return ast.While (T, S, E)

	def do_statement (self):
		S = self.statement ()
		self.expect ('while')
		self.expect ('(')
		T = self.getexpr (')')
		if self.nextis ('else'): E = self.statement ()
		else: E = None
		return ast.DoWhile (T, S, E)

	def for_statement (self):
		self.expect ('(')
		V = assignedNode (self.getexpr (('in',)))
		I = self.getexpr (')')
		S = self.statement ()
		if self.nextis ('else'): E = self.statement ()
		elif self.nextis ('else.for'): E = self.statement ()
		else: E = None
		return ast.For (V, I, S, E)

	def with_statement (self):
		self.expect ('(')
		T = self.getexpr (')')
		return ast.With (T, None, self.statement ())

	def getstarexpr (self, t):
		v = self.nextc ()
		if v not in ("*", "**"):
			self.ungetc ()
			return self.getexpr (t)
		x = self.getexpr (t)
		return ast.Star (x, v)

	def get_comma_expr (self, endchar, accept_star=False):
		if self.nextis (endchar):
			return ()
		self.ungetc ()
		t = endchar + ','
		if accept_star:
			f = self.getstarexpr
		else:
			f = self.getexpr
		L = [f (t)]
		while self.P.current == ',':
			a = f (t)
			L.append (a)
		return L

	def try_statement (self):
		T = self.statement ()
		if self.nextis ('finally'):
			return ast.TryFinally (T, self.statement ())
		if not self.nextis ('except'):
			return ast.TryExcept (T, [(None, None, ast.Pass ())], None)
		H = []
		while 1:
			t = a = None
			if self.nextis ('('):
				L = self.get_comma_expr (')')
			else:
				L = ()
			if len (L) == 1:
				t, = L
			elif L:
				t, a = L
				a = assignedNode (a)
			H.append ((t, a, self.statement ()))
			if self.nextis ('except'):
				continue
			if self.nextis ('else'): E = self.statement ()
			elif self.nextis ('else.try'): E = self.statement ()
			else: E = None
			return ast.TryExcept (T, H, E)

	def trybreak_statement (self):
		T = self.try_statement ()
		T.body.nodes.append (ast.Break ())
		return T

	def get_defargs (self):
		# XXXXX: do checks that argument names are symbols and that
		# '$' is allowed only for methods. Right now "def foo (+,-,/)" is allowed!!
		nextc, nextis, next_oneof = self.nextc, self.nextis, self.next_oneof
		a = []
		d = []
		f = 0
		if not nextis (')'):
			while 1:
				n = nextc ()
				if n[0] == '*':
					a.append (nextc ())
					if n == '*': f |= CO_VARARGS
					else: f |= CO_VARKEYWORDS
				else:
					if n[0] == '(':
						t = []
						while 1:
							n = nextc ()
							if n == '$':
								n = '$' + nextc ()
							t.append (n)
							if next_oneof (')', ',') == ')':
								n = tuple (t)
								break
					elif n == '$':
						n = '$' + nextc ()
					a.append (n)
					if nextis ('='):
						d.append (self.getexpr (',)'))
						if self.P.current == ')':
							break
						continue
				if next_oneof (')', ',') == ')':
					break
		return a, d, f

	def def_statement (self):
		N, n = self.get_dotted_name ()
		if n != '(':
			self.error ('(')
		A, D, F = self.get_defargs ()
		ns, self.ns = self.ns, None
		C = self.statement ()
		self.ns = ns
		return ast.Function (None, N, A, D, F, C, self.L.line)

	def gen_statement (self):
		F = self.def_statement ()
		F.flags |= CO_GENERATOR
		return F

	def method_statement (self):
		F = self.def_statement ()
		F.argnames.insert (0, 'self')
		if self.ns:
			F.iname = self.ns + "." + F.name
		return F

	def class_statement (self):
		N, n = self.get_dotted_name ()
		self.ns, ns = N, self.ns
		B = []
		if n == '(':
			B = self.get_comma_expr (')', True)
		else:
			self.ungetc ()
		C = self.statement ()
		self.ns = ns
		return ast.Class (N, B, C, self.L.line)

	def namespace_statement (self):
		N, n = self.get_dotted_name ()
		if n == "__constant__":
			const = True
		else:
			const = False
			self.ungetc ()
		self.expect ('{')
		cls = ast.Class (N, [ast.Const (None)], self.compound_statement (), self.L.line)
		cls.constant = const
		return cls

	def return_statement (self):
		if self.nextis (';') or self.nextnl ():
			E = ast.Const (None)
		else: E = self.getexpr (';')
		return ast.Return (E)

	def yield_statement (self):
		return ast.Yield (self.getexpr (';'))

	def raise_statement (self):
		# raise class, inst: not allowed
		if self.nextis (';') or self.nextnl ():
			E = None
		else: E = self.getexpr (';')
		return ast.Raise (E, None, None, self.L.line)

	def del_statement (self):
		return assignedNode (self.getexpr (';'), 'OP_DELETE')

	def print_statement (self):
		S = []
		if self.nextis (';') or self.nextnl ():
			return ast.Printnl (S, None)
		while 1:
			if self.nextis (';') or self.nextnl ():
				return ast.Print (S, None)
			S.append (self.getexpr (';,'))
			if self.P.current != ',':
				if self.P.current != ';':
					self.ungetc ()
				return ast.Printnl (S, None)

	def get_dotted_name (self):
		nextc = self.nextc
		n, d = nextc (), nextc ()	# check symbol
		# supposing we're called from def_statement and in method ...
		if n == '$':
			return 'self.' + d, nextc ()
		while d == '.':
			n += '.' + nextc ()
			d = nextc ()
		return n, d

	def import_statement (self):
		P = []
		nextc = self.nextc
		while 1:
			n, d = self.get_dotted_name ()
			if isstr (n):
				n = n [1:-1]
			a = None
			if d == 'as':
				a, d = nextc (), nextc ()
			P.append ((n, a))
			if d == ';':
				return ast.Import (P)
			if d != ',':
				if self.L.sol and AUTOMATIC_SEMICOLON:
					self.ungetc ()
					return ast.Import (P)
				raise SyntaxError ("expected [,] at line %i"% self.L.line)

	def from_statement (self):
		I, form = self.get_dotted_name ()
		if isstr (I):
			I = I [1:-1]
		if form != 'update':
			self.mustbe (form, 'import')
		N = []
		nextc = self.nextc
		while 1:
			n, d = nextc (), nextc ()
			a = None
			if d == 'as':
				a, d = nextc (), nextc ()
			N.append ((n, a))
			if d == ';':
				if form != 'update':
					return ast.From (I, N)
				return ast.UpdateFrom (I, N)
			if d != ",":
				if self.L.sol and AUTOMATIC_SEMICOLON:
					self.ungetc ()
					if form != 'update':
						return ast.From (I, N)
					return ast.UpdateFrom (I, N)
				raise SyntaxError ("expected [,] at line %i"% self.L.line)

	def global_statement (self):
		if self.nextis ("."):
			N = self.nextc ()
			self.ungetc ()
			return ast.Stmt ([ast.Global (N), self.expression_statement ()])
		N = []
		while 1:
			N.append (self.nextc ())
			try:
				if self.next_oneof (';', ',') == ';':
					return ast.Global (N)
			except:
				if AUTOMATIC_SEMICOLON and self.L.sol:
					self.ungetc ()
					return ast.Global (N)
				raise

	def _statement (self):
		ft = self.nextc ()
		if ft in self.dispatcher:
			return self.dispatcher [ft] ()
		else:
			self.ungetc ()
			return self.expression_statement ()

	def statement (self):
		s = self._statement ()
		if not isinstance (s, ast.Stmt):
			s = ast.Stmt ([s])
		return s

	def enable_automatic_semicolon (self):
		global AUTOMATIC_SEMICOLON
		AUTOMATIC_SEMICOLON = True
		return ast.Pass ()

	def translation_unit (self, autosem):
		global AUTOMATIC_SEMICOLON
		AUTOMATIC_SEMICOLON = autosem

		S = []
		try:
			while 1:
				S.append (self._statement ())
		except StopIteration:
			pass

		return ast.Stmt (S)

def parse (filename, source=None, autosem=False):
	try:
		e = eplcc (filename, source, autosem)
		e.doit ()
		return e.tree
	except SyntaxError, E:
		raise SyntaxError (filename + ": " + E.x)

if __name__ == '__main__':
	gco.GCO.new_compiler ()
	try:
		E = eplcc (sys.argv [1])
		print repr (E.tree)
	except:
		print sys.exc_info ()
	gco.GCO.pop_compiler ()
