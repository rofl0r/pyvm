import random

# Note that pyvm's BIOS by default imports the argp parser and a couple
# of other things. Also, the "pyc" compiler is both slower than python's
# builtin compiler and it has many pycs to load. So the -precompile option
# to Dejavu.py makes a big difference.

#DEJAVU
'''
{
'NAME':"Startup",
'DESC':"startup time. Also verifies compatible random number generators",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"",
'BARGS':""
}
'''

random.seed (2233)
print random.randint (0, 1000)
