# The tokenizer is required for EPL whitespaceless
# but it does work on correct python programs too.

Group = '|'.join

Operator = Group ((r'\*\*=?',
	'>>=?',
	'<<=?',
	r'\?\?',
	'<>',
	'//=?',
	'!?[-=]>',
	r"[~.$?:@,;{}()\[\]]",
	"[-+*/%&|^=<>!]=?"
))

QS1 = r"""[iRruU]?%s(?:(?:\\\\)*|.*?[^\\](?:\\\\)*)%s""";
String1 = Group ([QS1 % (x,x) for x in ('"', "'")])
String3 = Group ([QS1 % (x,x) for x in ('"""', "'''")])
String = Group ((String3, String1))
del QS1, String1, String3

SKeyword = r'else\.for|else\.while|else\.if|else\.try|if\.break|if\.continue|try\.break'
Symbol = r'[a-zA-Z_]\w*'
Hex = r'0x[\da-zA-Z]+[lL]?'
Oct = r'0[0-7]*[lL]?'
Int = r'[1-9]\d*[lL]?'
E = r'(?:[eE][-+]?\d+)'
Float1 = r'\d+' + E
Float2 = r'(?:\d+\.\d*|\.\d+)' + E + '?'
Comment = r'#[^\n]*\n'

# the order is important
Token = Group ((SKeyword, Float2, Operator, String, Symbol, Hex, Oct, Int, Float1, Comment))

del Symbol, Hex, Oct, Int, E, Float1, Float2, String, Operator, Comment, Group

import re
import sys
if 'pyvm' in sys.copyright:
	# We'd like to have another bootstrapped testing at this point:
	# PE uses tokenize. Tokenize uses rejit. Rejit is written in PE.
	# However, this is very complicated and I haven't managed to do it.
	# Instead, if rejit has been imported (which means that PE works)
	# we use it. So in order to enable this, you must put a
	# 	import rejit
	# before compileall() in the library which is written in PE.
	Tokenize = re.compile (Token, re.DOTALL, dojit='rejit' in sys.modules).match
else:
	Tokenize = re.compile (Token, re.DOTALL).match
White = re.compile (r'\s*', re.DOTALL).match

def gentokens (t, ws=False, Tokenize=Tokenize, White=White):
	i = 0
	if not ws:
		while 1:
			R = White (t, i)
			if not R:
				break
			R = Tokenize (t, R.end (0))
			if not R:
				break
			yield R.group ()
			i = R.end (0)
	else:
		while 1:
			R = White (t, i)
			if not R:
				yield ' '
				yield ';'
				break
			yield R.group ()
			R = Tokenize (t, R.end (0))
			if not R:
				yield ';'
				break
			yield R.group ()
			i = R.end (0)

class Lexer:
	def __init__ (self, t):
		self.Gnext = gentokens (t, True).next
		self.line = 1
		self.reyield = False

	def nextc (self):
		if self.reyield:
			self.reyield = False
			return self.tok
		self.sol = False
		while 1:
			tok = self.Gnext ()
			if '\n' in tok:
				self.sol = True
				self.line += tok.count ('\n')
			tok = self.Gnext ()
			if tok [0] == '#':
				self.sol = True
				self.line += 1
				continue
			self.line += tok.count ('\n')
			self.tok = tok
			return tok

	def ungetc (self):
		self.reyield = True

	def __iter__ (self):
		while 1:
			yield self.nextc ()

def token_count (filename=None, data=None):
	i = 0
	if data is None:
		data = open (filename).read ()
	for t in Lexer (data):
		i += 1
	return i-1

if __name__ == '__main__':
	t = [];
	for f in sys.argv [1:]:
		t.append ((token_count (f)-1, f));
	t.sort ();
	for c, f in t:
		print c, "\t", f
	if len (t) > 1:
		print sum ([t [0] for t in t])
