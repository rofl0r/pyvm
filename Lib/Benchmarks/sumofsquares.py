#DEJAVU
'''
{
'NAME':"Sum or Squares",
'DESC':"c.l.py thread 'not homework... something i find an interesting problem'",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"100",
'BARGS':"280"
}
'''

"""
   <H1>not homework... something i find an interesting problem</H1>
    <B>MRAB</B> 
    <A HREF="mailto:python-list%40python.org?Subject=Re%3A%20not%20homework...%20something%20i%20find%20an%20interesting%20problem&In-Reply-To=%3C49EE2D3B.5060407%40mrabarnett.plus.com%3E"
       TITLE="not homework... something i find an interesting problem">google at mrabarnett.plus.com
       </A><BR>
    <I>Tue Apr 21 22:31:55 CEST 2009</I>
"""


import math

def sumsq(n, depth=0):
     if n == 0:
         return [[]]
     root = int(math.sqrt(n))
     square = root ** 2
     sums = [[square] + s for s in sumsq(n - square, depth + 1)]
     while root > 0:
         square = root ** 2
         if square * len(sums[0]) < n:
             break
         more_sums = [[square] + s for s in sumsq(n - square, depth + 1)]
         if len(more_sums[0]) == len(sums[0]):
             sums.extend(more_sums)
         elif len(more_sums[0]) < len(sums[0]):
             sums = more_sums
         root -= 1
     sums = set(tuple(sorted(s)) for s in sums)
     sums = [list(s) for s in sorted(sums)]
     return sums

import sys

for n in range(1, int (sys.argv [1])):
     print n
     for rez in sumsq(n):
	print rez
