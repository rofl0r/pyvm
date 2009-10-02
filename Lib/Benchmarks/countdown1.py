'''
Date: Mon, 21 Jan 2008 10:12:54 +0100
From: Terry Jones
To: Arnaud Delobelle
Newsgroups: gmane.comp.python.general
Subject: Re: Just for fun: Countdown numbers game solver
'''

#DEJAVU
'''
{
'NAME':"Countdown",
'DESC':"Countdown numbers game solver",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"",
'BARGS':""
}
'''

try:
	any
except:
	def any (x):
		for i in x:
			if i:
				return True	
		return False

from operator import *

def countdown(target, nums, numsAvail, value, partialSolution, solutions, ops=(add, mul, sub, div)):
    if value == target or not any(numsAvail):
        # Ran out of available numbers. Add the solution, if we're right.
        if value == target:
            solutions.add(tuple(partialSolution))
    elif value is None:
        # Use each distinct number as a starting value.
        used = set()
        for i, num in enumerate(nums):
            if num not in used:
                numsAvail[i] = False
                used.add(num)
                partialSolution.append(num)
                countdown(target, nums, numsAvail, num, partialSolution, solutions, ops)
                numsAvail[i] = True
                partialSolution.pop()
    else:
        for op in ops:
            for i, num in enumerate(nums):
                if numsAvail[i]:
                    numsAvail[i] = False
                    moreAvail = any(numsAvail)
                    try:
                        lastNum, lastOp = partialSolution[-2:]
                    except ValueError:
                        lastNum, lastOp = partialSolution[-1], None
                    # Don't allow any of:
                    if not any((
                        # Div: after mul, by 1, by 0, producing a fraction.
                        (op == div and (lastOp == 'mul' or num <= 1 or value % num != 0)),
                        # If initial mul/add, canonicalize to 2nd operator biggest.
                        ((op == mul or op == add) and lastOp is None and num > lastNum),
                        # Don't allow add or sub of 0.
                        ((op == add or op == sub) and num == 0),
                        # Don't allow mult by 1.
                        (op == mul and num == 1),
                        # Don't allow sub after add (allow add after sub).
                        (op == sub and lastOp == 'add'),
                        # If same op twice in a row, canonicalize operand order.
                        (lastOp == op.__name__ and num > lastNum)
                        )):
                        partialSolution.extend([num, op.__name__])
                        countdown(target, nums, numsAvail, op(value, num), partialSolution, solutions, ops)
                        del partialSolution[-2:]
                    numsAvail[i] = True

def cmpf (a, b):
	return cmp(len(a), len(b)) or cmp(a, b)

for nums, target in (((100, 9, 7, 6, 3, 1), 253),
                     ((100, 9, 7, 6, 3, 1), 234),
                     ((2, 3, 5), 21),
                     ((7, 8, 50, 8, 1, 3), 923),
                     ((8, 8), 16),
                     ((8, 8, 8), 8),
                     ((8, 0), 8),
                     ((7,), 8),
                     ((), 8),
                     ((8, 8, 8, 8), 32)):
    solutions = set()
    countdown(target, nums, [True,] * len(nums), value=None, partialSolution=[], solutions=solutions)
    print "%d solutions to: target %d, numbers = %s" % (len(solutions), target, list (nums))
    for s in sorted(solutions, cmp=cmpf):
        print "\t%s"% list (s)

