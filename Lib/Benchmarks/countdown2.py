'''
Date: Mon, 21 Jan 2008 01:15:50 -0800 (PST)
From: Dan Goodman
Newsgroups: gmane.comp.python.general
Subject: Re: Just for fun: Countdown numbers game solver
'''

#DEJAVU
'''
{
'NAME':"Countdown",
'DESC':"Another solution for the countdown puzzle",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"SEM",
'BARGS':""
}
'''

# The original uses raise to avoid some computations. Since
# this is a useful bench we should use the raise code. the
# problem is that pyvm with TRACEBACK_LEVEL>=2 is quite
# slow with raises, while otherwise its rather faster.
# Let's turn off raises for now. -- St.

RAISING = False

if RAISING:
	sub = lambda x,y:x-y
	def add(x,y):
	    if x<=y: return x+y
	    raise ValueError
	def mul(x,y):
	    if x<=y or x==1 or y==1: return x*y
	    raise ValueError
	def div(x,y):
	    if not y or x%y or y==1:
	        raise ValueError
	    return x/y
else:
	sub = lambda x,y:x-y
	def add(x,y):
	    return x+y
	def mul(x,y):
	    return x*y
	def div(x,y):
	    if not y or x%y:
	        raise ValueError
	    return x/y

disps = {
	add:"+",
	mul:"*",
	sub:"-",
	div:"/"
}
#add.disp = '+'
#mul.disp = '*'
#sub.disp = '-'
#div.disp = '/'

standard_ops = [ add, sub, mul, div ]

def strexpression(e):
    if len(e)==3:
        return '('+strexpression(e[1])+disps [e[0]]+strexpression(e[2])+')'
    elif len(e)==1:
        return str(e[0])

# I don't like this function, it's nice and short but is it clear
# what it's doing just from looking at it?
def expressions(sources,ops=standard_ops,minremsources=0):
    for i in range(len(sources)):
        yield ([sources[i]],sources[:i]+sources[i+1:],sources[i])
    if len(sources)>=2+minremsources:
        for e1, rs1, v1 in expressions(sources,ops,minremsources+1):
            for e2, rs2, v2 in expressions(rs1,ops,minremsources):
                for o in ops:
                    try: yield ([o,e1,e2],rs2,o(v1,v2))
                    except ValueError: pass

def findfirsttarget(target,sources,ops=standard_ops):
    for e,s,v in expressions(sources,ops):
        if v==target:
            return strexpression(e)
    return None

import sys
if "SEM" in sys.argv:
	print findfirsttarget(253,[100,9,7,6,3,1])
else:
	print findfirsttarget(931,[7,8,50,1,3])
	print findfirsttarget(253,[100,9,7,6,3,1])
	print findfirsttarget(2785,[7,8,50,8,1])
#	print findfirsttarget(923,[7,8,50,8,1,3])

'''
print findfirsttarget(923,[7,8,50,8,1,3])

gives:

((7*(((8*50)-1)/3))-8)

Dan Goodman
-- 
http://mail.python.org/mailman/listinfo/python-list

'''
