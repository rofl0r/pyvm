##  Regular expression based tokenizer
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) version 3 of the License.

# This should be used with re.findall() on lines. If the last
# token of the findall list ends in a newline, we have an incomplete
# string literal or \\\n, and the user should understand that more
# lines have to be read.
#
# The only deviation from the standard is that this tokenizer
# allows multi-line, single-quoted string literals.  And why not?
# What advantage does the restriction of single quoted literals
# to one line, offer?
# This way we are also consistent with the vi syntax colorizer (+1).

Group = lambda *x: '|'.join (x)

Operator = Group (r'\*\*=?',
		   '>>=?',
		   '<<=?',
		   '<>',
		   '//=?',
		  r'[~.$?:@,;{}()\[\]]',
		   '[-+*/%&|^=<>!]=?')

QS      = r"""%s(?:(?:(?:\\\\)*|.*?[^\\](?:\\\\)*)%s|.*\n)""";
String  = '[rRuU]?(?:' + Group (*[QS % (x,x) for x in ('"""', "'''", '"', "'")]) + ')'

Symbol  = r'[a-zA-Z_]\w*'
Hex     = r'0x[\da-zA-Z]+[lL]?'
Oct     = r'0[0-7]*[lL]?'
Int     = r'[1-9]\d*[lL]?'
E       = r'(?:[eE][-+]?\d+)'
Float1  = r'\d+' + E
Float2  = r'(?:\d+\.\d*|\.\d+)' + E + '?'
Comment = r'#.*'
Concat  = r'\\\r?\n'

# the order is important
Token = Group (Float2, Operator, String, Symbol, Float1, Hex, Oct, Int, Comment, Concat)

import re

## Exported ##

TokenizeEx = re.compile (Token).findall
Evaluable = re.compile (r'''\d|\..|.?['"]''')
try:  Evaluable = Evaluable.matches # pyvm
except: Evaluable = Evaluable.match
Quotes = re.compile ('''[rRuU]?(['"]{1,3})''').match
EndMultiLine = {
	"'''":re.compile (r"""(.*?[^\\](?:\\\\)*'''|''')(.*)""", re.DOTALL).match,
	'"""':re.compile (r'''(.*?[^\\](?:\\\\)*"""|""")(.*)''', re.DOTALL).match,
	'"':re.compile (r'''(.*?[^\\](?:\\\\)*"|")(.*)''', re.DOTALL).match,
	"'":re.compile (r"""(.*?[^\\](?:\\\\)*'|')(.*)""", re.DOTALL).match
}

del re, Symbol, Hex, Oct, Int, E, Float1, Float2, Comment, Concat, String, Group, Token, x, Operator, QS
