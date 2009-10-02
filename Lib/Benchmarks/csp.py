#DEJAVU
'''
{
'NAME':"CSP",
'DESC':"Constraint satisfaction problem solver by Paul McGuire c.l.py",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"",
'BARGS':""
}
'''

def prod(lst):
     # the original uses `reduce`
     s = 1
     for i in lst:
        s *= i
     return s

def perms(setSize, sampleSize):
     return prod(xrange(setSize-sampleSize+1,setSize+1))

def permutation(k, s):
     fact = 1
     s = s[:]
     for j in xrange( 2, len(s)+1):
         fact = fact * (j-1)
         idx1 = j - ((k / fact) % j)-1
         idx2 = j-1
         s[idx1],s[idx2] = s[idx2],s[idx1]
     return s

def permutations(s,sampleSize=None):
     if sampleSize is None:
         sampleSize = len(s)
     k = perms(len(s),sampleSize)
     for i in xrange(k):
         yield permutation(i,s)

d0,d1 = 1,2
def func(p):
     a0,a1,a2,b0,b1,b2,c0,c1,c2 = p

     # do application evaluation here
     b1b2 = 10*b1+b2
     a1a2 = 10*a1+a2
     c1c2 = 10*c1+c2
     if d1*a0*b1b2*c1c2 + d1*b0*a1a2*c1c2 + d1*c0*a1a2*b1b2 == d0*a1a2*b1b2*c1c2:
         return sorted( [[a0, a1, a2], [b0, b1, b2], [c0, c1, c2]] )
     else:
         return None

def main ():
    result = []
    for p in permutations(range(1,10)):
         aresult = func(p)
         if aresult is not None and aresult not in result:
             result.append(aresult)
    return result
result = main ()

for [[a0, a1, a2], [b0, b1, b2], [c0, c1, c2]] in result:
   print '  %0d     %0d     %0d     %0d' % (a0, b0, c0, d0)
   print '--- + --- + --- = ---'
   print ' %0d%0d    %0d%0d    %0d%0d     %0d' %(a1, a2, b1, b2, c1,c2, d1)
   print


