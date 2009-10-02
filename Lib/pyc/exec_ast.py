from gco import GCO
import ast

# Execute a single expression of the form
#	NAME = expr
# If it evaluates without errors, add the value to the dictionary `globals`,
# otherwise *remove it* from `globals` if already there.

def do_exec (node, globals=None):
	if globals is None:
		globals = {}
	def lookup_name (x):
		try: return globals [x]
		except: return __builtins__ [x]
	def store_name (x, y):
		globals [x] = y
	if not isinstance (node, ast.Assign) or len (node.nodes) != 1 or not\
	isinstance (node.nodes [0], ast.AssName) or node.nodes [0].flags != "OP_ASSIGN":
		return globals
	name = node.nodes [0].name
	GCO.new_compiler ()
	GCO ['eval_lookup'] = lookup_name
	GCO ['store_name'] = store_name
	GCO ['py2ext'] = False
	GCO ['dynlocals'] = True
	GCO ['gnt'] = False
	GCO ['gnc'] = False
	GCO ['filename'] = '*eval*'
	GCO ['docstrings'] = True
	GCO ['pe'] = False
	try:
		try:
			v, ast.WarnEval = ast.WarnEval, False
			node.eval ()
		except:
			try:
				del globals [name]
			except:
				pass
		ast.WarnEval = v
		return globals
	finally:
		GCO.pop_compiler ()
