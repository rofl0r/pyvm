#
# Converted to python from the original solution in C by Manuel Bleichenbacher
# Original comments of C file:
# /*
# 1995-96 ACM International Collegiate Programming Contest
# Southwestern European Regional Contest
# ETH Zurich, Switzerland
# December 9, 1995
#
#
# Problem: Coloring
#
# Idea and first implementation:	Andreas Wolf, ETH Zurich
# Implementation:					Manuel Bleichenbacher, Head Judge
# */

# see http://www.acm.inf.ethz.ch/ProblemSetArchive
# for more problems

#DEJAVU
'''
{
'NAME':"Graph Coloring",
'DESC':"Problem from ACM International Collegiate Programming Contest 1995",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"SEM",
'BARGS':""
}
'''

MaxNode = 101

BLACK = 1
WHITE = 2

class Coloring:
	def dup (self):
		c = Coloring ()
		c.nBlack = self.nBlack
		c.nWhite = self.nWhite 
		c.node = self.node [:]
		return c
	def copy (self, c):
		self.nBlack = c.nBlack
		self.nWhite = c.nWhite
		self.node = c.node [:]

def color (firstNode, coloring):
	if coloring.nWhite + coloring.nBlack == nNodes:
		if coloring.nBlack > bestColoring.nBlack:
			bestColoring.copy (coloring)
		return

	i = firstNode
	while (i <= nNodes):
		if (coloring.node [i] == 0):
			c = coloring.dup ()
			c.nBlack += 1
			c.node [i] = BLACK
			for j in xrange (1, nNodes+1):
				if edge [i][j]:
					if c.node [j] == BLACK:
						break
					elif c.node [j] == 0:
						c.node [j] = WHITE
						c.nWhite += 1
			else:
				if nNodes - c.nWhite > bestColoring.nBlack:
					color (firstNode + 1, c)
		i += 1

bestColoring = Coloring ()
nNodes = 0

rl = iter (open ("DATA/coloring.in")).next
import sys
sem = "SEM" in sys.argv

def ints (x):
	return [int (x) for x in x]

for NN in xrange (int (rl ())):
	bestColoring.nBlack = bestColoring.nWhite = 0
	nNodes, nEdges = ints (rl ().split ())
	bestColoring.node = [0] * MaxNode
	edge = [[0]*MaxNode for xx in xrange (MaxNode)]
	coloring = bestColoring.dup ()
	for xx in xrange (nEdges):
		fr, tt = ints (rl ().split ())
		edge [fr][tt] = edge [tt][fr] = 1
	color (1, coloring)
	print bestColoring.nBlack
	print " ".join ([str (i) for i in xrange (1, nNodes+1) if (bestColoring.node [i] == BLACK)])
	if sem and NN == 5:
		break
