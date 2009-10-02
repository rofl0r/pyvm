#DEJAVU
'''
{
'NAME':"Sudoku",
'DESC':"Constraint based sudoku solver by Peter Norvig",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"SEM",
'BARGS':""
}
'''

## Solve Every Sudoku Puzzle

## See http://norvig.com/sudoku.html

## Throughout this program we have:
##   r is a row,    e.g. 'A'
##   c is a column, e.g. '3'
##   s is a square, e.g. 'A3'
##   d is a digit,  e.g. '9'
##   u is a unit,   e.g. ['A1','B1','C1','D1','E1','F1','G1','H1','I1']
##   g is a grid,   e.g. 81 non-blank chars, e.g. starting with '.18...7...
##   values is a dict of possible values, e.g. {'A1':'123489', 'A2':'8', ...}

rows = 'ABCDEFGHI'
cols = '123456789'
digits   = '123456789'
squares  = [r+c for r in rows for c in cols]
unitlist = ([[r+c for r in rows] for c in cols] +
	    [[r+c for c in cols] for r in rows] +
	    [[rows[r+dr]+cols[c+dc] for dr in (0,1,2) for dc in (0,1,2)]
	     for r in (0,3,6) for c in (0,3,6)])
units = dict((s, [u for u in unitlist if s in u]) 
	     for s in squares)
peers = dict((s, set(s2 for u in units[s] for s2 in u if s2 != s))
             for s in squares)
  
def search(values):
    "Using backtracking search and propagation, try all possible values."
    if values is False:
        return False ## Failed earlier
    if all(len(values[s]) == 1 for s in squares): 
        return values ## Solved!
    ## Chose the unfilled square s with the fewest possibilities
    _,s = min([(len(values[s]), s) for s in squares if len(values[s]) > 1])
    return some(search(assign(values.copy(), s, d)) 
		for d in values[s])

def assign(values, s, d):
    "Eliminate all the other values (except d) from values[s] and propagate."
    if all(eliminate(values, s, d2) for d2 in values[s] if d2 != d):
        return values
    else:
        return False

def eliminate(values, s, d):
    "Eliminate d from values[s]; propagate when values or places <= 2."
    if d not in values[s]:
        return values ## Already eliminated
    values[s] = values[s].replace(d,'')
    if len(values[s]) == 0:
	return False ## Contradiction: removed last value
    elif len(values[s]) == 1:
	## If there is only one value (d2) left in square, remove it from peers
        d2, = values[s]
        if not all(eliminate(values, s2, d2) for s2 in peers[s]):
            return False
    ## Now check the places where d appears in the units of s
    for u in units[s]:
	dplaces = [s for s in u if d in values[s]]
	if len(dplaces) == 0:
	    return False
	elif len(dplaces) == 1:
	    # d can only be in one place in unit; assign it there
            if not assign(values, dplaces[0], d):
                return False
    return values
		    
def parse_grid(grid):
    "Given a string of 81 digits (or .0-), return a dict of {cell:values}"
    grid = [c for c in grid if c in '0.-123456789']
    values = dict((s, digits) for s in squares) ## Each square can be any digit
    for s,d in zip(squares, grid):
        if d in digits and not assign(values, s, d):
	    return False
    return values

def solve_file(filename, sep='\n'):
    "Parse a file into a sequence of 81-char descriptions and solve them."
    results = [search(parse_grid(grid)) 
               for grid in file(filename).read().strip().split(sep)]
    print "## Got %d out of %d" % (len (results), len (results) - results.count (False))
    return results

def printboard(values):
    "Used for debugging."
    width = 1#1+max([len(values[s]) for s in squares])
    line = '\n' + '+'.join(['-'*(width*3)]*3)
    for r in rows:
        print ''.join([values[r+c]#.center(width)
		+(c in '36' and '|' or '')
                      for c in cols]) + (r in 'CF' and line or '')
    print

def all(seq):
    for e in seq:
        if not e: return False
    return True

def some(seq):
    for e in seq:
        if e: return e
    return False

if __name__ == '__main__':
    import sys
    if 'SEM' in sys.argv:
	F = 'top10.txt'
    else:
	F = 'top95.txt'
    for values in solve_file("DATA/"+F):
        printboard(values)

## References used:
## http://www.scanraid.com/BasicStrategies.htm
## http://www.krazydad.com/blog/2005/09/29/an-index-of-sudoku-strategies/
## http://www2.warwick.ac.uk/fac/sci/moac/currentstudents/peter_cock/python/sudoku/
