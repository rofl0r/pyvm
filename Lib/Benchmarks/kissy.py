# This is found in:
# http://www.willmcgugan.com/2007/04/17/kissy-lippy/
#


#DEJAVU
'''
{
'NAME':"Kissy-Lippy",
'DESC':"Kissy-lippy from Will McGugan's blog",
'GROUP':'real-bench',
'CMPOUT':1,
'DATA':"/usr/share/dict/words",
'ARGS':"SEM",
'BARGS':""
}
'''

def to_number(c):
    return "22233344455566677778889999"[ord(c)-ord('a')]

def cmp_word_lengths(w1, w2):
    return cmp(len(w1), len(w2))
 
def main (Show):
	txt_words = {}
 
	for line in file("/usr/share/dict/words"):
	    word = line.rstrip().lower ()
	    try: txt_word = "".join([to_number(c) for c in word])
	    except: continue
	    if txt_word not in txt_words:
	        txt_words[txt_word] = []
	    txt_words[txt_word].append(word)
 
	if Show:
	    for n, k, v in sorted ([(len (k), k, v) for k, v in txt_words.iteritems () if len (v) > 1]):
	        print ("[%s] %s" % (k, ", ".join(v)))
	else:
	    for n, k, v in sorted ([(len (k), k, v) for k, v in txt_words.iteritems () if len (v) > 1]):
	        c = "[%s] %s" % (k, ", ".join(v))

import sys
if "SEM" not in sys.argv:
    main (False)
    main (False)
    main (False)
    main (False)
else:
    main (True)
