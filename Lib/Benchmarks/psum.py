#DEJAVU
'''
{
'NAME':"Partial sum",
'DESC':"dynamic_ncpus.pe test from parallel python (for one cpu)",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"500000",
'BARGS':"4000000"
}
'''

if 1:
   #!/usr/bin/python
   # File: dynamic_ncpus.py
   # Author: Vitalii Vanovschi
   # Desc: This program demonstrates parallel computations with pp module
   # and dynamic cpu allocation feature.
   # Program calculates the partial sum 1-1/2+1/3-1/4+1/5-1/6+... (in the limit it is ln(2))
   # Parallel Python Software: http://www.parallelpython.com
   
   import sys
   
   def part_sum(start, end):
       """Calculates partial sum"""
       sum = 0
       for x in xrange(start, end):
           if x % 2 == 0:
              sum -= 1.0 / x
           else:
              sum += 1.0 / x
       return sum
   
   start = 1
   end = int (sys.argv [1])
   
   print "%.7f" %part_sum (start, end)
