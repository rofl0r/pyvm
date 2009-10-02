#DEJAVU
'''
{
'NAME':"states",
'DESC':"NPR states puzzle",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"SEM",
'BARGS':""
}
'''

'''
Subject: A Simple Programming Puzzle Seen Through Three Different Lenses

The other day, I stumbled across Mark Nelson's blog post
<http://marknelson.us/2007/04/01/puzzling/> describing a fairly simple NPR
word puzzle: "Take the names of two U.S. States, mix them all together, then
rearrange the letters to form the names of two other U.S. States. What
states are these?"

Mark Nelson is a programmer, so his first instinct of course was to write a
small program to solve it. Mine too. I immediately stopped reading his post,
opened a new emacs buffer and spent five minutes coming up with this:

'''

states = ["alabama","alaska","arizona","arkansas","california","colorado",
         "connecticut","delaware","florida","georgia","hawaii","idaho",
         "illinois","indiana","iowa","kansas","kentucky","louisiana",
         "maine","maryland","massachusetts","michigan","minnesota",
         "mississippi","missouri","montana","nebraska","nevada",
         "newhampshire","newjersey","newmexico","newyork","northcarolina",
         "northdakota","ohio","oklahoma","oregon","pennsylvania","rhodeisland",
         "southcarolina","southdakota","tennessee","texas","utah","vermont",
         "virginia","washington","westvirginia","wisconsin","wyoming",]

def find ():
   seen = dict()
   for state1 in states:
     for state2 in states:
         if state1 == state2:
             continue
         letters = list(state1 + state2)
         letters.sort()
         key = "".join(letters)
         if seen.has_key(key):
             (old1,old2) = seen[key]
             if old1 == state2 and old2 == state1:
                 continue
             else:
                 return (state1,state2,old1,old2)
         else:
             seen[key] = (state1,state2)

import sys
if "SEM" in sys.argv:
        print "found it: %s + %s, %s + %s" % find ()
else:
	for i in xrange (140):
		find ()

#Sure enough, it gave me the answer (and like Mark Nelson, seeing it made me
#realize that it probably would have been even more productive to just think
#about it for a minute instead of writing the code).


