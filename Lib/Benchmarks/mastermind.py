"""
 Title: Mastermind-style code-breaking
Submitter: Raymond Hettinger (other recipes)
Last Updated: 2006/08/15
Version no: 1.1
Category: Algorithms

Description:

Framework for experimenting with guessing strategies in Master-mind style games.
"""

#DEJAVU
'''
{
'NAME':"Mastermind",
'DESC':"Mastermind-style code-breaking",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"SEM",
'BARGS':""
}
'''


import random
from itertools import izip, imap
from math import log
import sys
if 1:#sys.version_info[1] > 4:
    # Python 2.5 has a different `Random.sample` algorithm
    # and given the same seed we get different results. This
    # applies the sample algorithmm of Python 2.4
    def sample(population, k, random=random.random):

        n = len(population)
        if not 0 <= k <= n:
            raise ValueError, "sample larger than population"
        _int = int
        result = [None] * k
        if n < 6 * k:     # if n len list takes less space than a k len dict
            pool = list(population)
            for i in xrange(k):         # invariant:  non-selected at [0,n-i)
                j = _int(random() * (n-i))
                result[i] = pool[j]
                pool[j] = pool[n-i-1]   # move non-selected item into vacancy
        else:
            try:
                n > 0 and (population[0], population[n//2], population[n-1])
            except (TypeError, KeyError):   # handle sets and dictionaries
                population = tuple(population)
            selected = {}
            for i in xrange(k):
                j = _int(random() * n)
                while j in selected:
                    j = _int(random() * n)
                result[i] = selected[j] = population[j]
        return result


digits = 3
trials = 100
trials = 30
fmt = '%0' + str(digits) + 'd'
searchspace = tuple([tuple(map(int,fmt % i)) for i in range(0,10**digits)])

def compare(a, b, imap=imap, sum=sum, izip=izip, min=min):
    count1 = [0] * 10
    count2 = [0] * 10
    strikes = 0
    for dig1, dig2 in izip(a,b):
        if dig1 == dig2:
            strikes += 1
        count1[dig1] += 1
        count2[dig2] += 1
    balls = sum(imap(min, count1, count2)) - strikes
    return (strikes, balls)

def rungame(target, strategy, verbose=True, maxtries=15):
    possibles = list(searchspace)
    for i in xrange(maxtries):
        g = strategy(i, possibles)
        if verbose:
            print "Out of %7d possibilities.  I'll guess %r" % (len(possibles), g),
        score = compare(g, target)
        if verbose:
            print ' ---> ', score
        if score[0] == digits:
            if verbose:
                print "That's it.  After %d tries, I won." % (i+1,)
            break
        possibles = [n for n in possibles if compare(g, n) == score]
    return i+1

############################################### Strategy support

def info(seqn):
    bits = 0
    s = float(sum(seqn))
    for i in seqn:
        p = i / s
        bits -= p * log(p, 2)
    return bits

def utility(play, possibles):
    b = {}
    for poss in possibles:
        score = compare(play, poss)
        b[score] = b.get(score, 0) + 1
    return info(b.values())

def hasdup(play, set=set, digits=digits):
    return len(set(play)) != digits

def nodup(play, set=set, digits=digits):
    return len(set(play)) == digits

################################################ Strategies

def s_allrand(i, possibles):
    return random.choice(possibles)

def s_trynodup(i, possibles):
    for j in xrange(20):
        g = random.choice(possibles)
        if nodup(g):
            break
    return g

def s_bestinfo(i, possibles):
    if i == 0:
        return s_trynodup(i, possibles)
    plays = sample(possibles, min(20, len(possibles)))
    _, play = max([(utility(play, possibles), play) for play in plays])
    return play

def s_worstinfo(i, possibles):
    if i == 0:
        return s_trynodup(i, possibles)
    plays = sample(possibles, min(20, len(possibles)))
    _, play = min([(utility(play, possibles), play) for play in plays])
    return play

def s_samplebest(i, possibles):
    if i == 0:
        return s_trynodup(i, possibles)
    if len(possibles) > 150:
        possibles = sample(possibles, 150)
        plays = possibles[:20]
    elif len(possibles) > 20:
        plays = sample(possibles, 20)
    else:
        plays = possibles
    _, play = max([(utility(play, possibles), play) for play in plays])
    return play

## Evaluate Strategies

def average(seqn):
    return sum(seqn) / float(len(seqn))

def counts(seqn):
    limit = max(10, max(seqn)) + 1
    tally = [0] * limit
    for i in seqn:
        tally[i] += 1
    return tuple(tally[1:])

from time import clock
print '-' * 60

random.seed (3321)
print '----';
SEM = 'SEM' in sys.argv
if SEM:
   trials = 5
else:
   trials = 25

for strategy in (s_bestinfo, s_samplebest, s_worstinfo, s_allrand, s_trynodup, s_bestinfo):
    start = clock()
    data = [rungame(random.choice(searchspace), strategy, verbose=0) for i in xrange(trials)]
    print 'mean=%.2f %r  %s n=%d dig=%d' % (average(data), counts(data), strategy.__name__, len(data), digits)
    if not SEM:
        print 'Time elapsed %.2f' % (clock() - start,)
