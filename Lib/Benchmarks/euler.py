# Finding Eulerian path in undirected graph
# Przemek Drochomirecki, Krakow, 5 Nov 2006
#DEJAVU
'''
{
'NAME':"Euler",
'DESC':"Finding Eulerian path in undirected graph",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"SEM",
'BARGS':""
}
'''

def eulerPath(graph):
    # counting the number of vertices with odd degree
    odd = [ x for x in graph.keys() if len(graph[x])&1 ]
    odd.append( graph.keys()[0] )

    if len(odd)>3:
	print odd
        return None
    
    stack = [ odd[0] ]
    path = []
    
    # main algorithm
    while stack:
        v = stack[-1]
        if graph[v]:
            u = graph[v][0]
            stack.append(u)
            # deleting edge u-v
            del graph[u][ graph[u].index(v) ]
            del graph[v][0]
        else:
            path.append( stack.pop() )
    
    return path

print eulerPath({ 1:[2,3], 2:[1,3,4,5], 3:[1,2,4,5], 4:[2,3,5], 5:[2,3,4] })
# if anybody has a good big graph, send it!
D = { 1:[11,2,3, 10,12], 2:[1,3,4,5,10,9,12, 13], 3:[1,2,4,5,10,7], 4:[11,2,3,5], 5:[2,3,4,11],
      6:[11,7,8,9], 7:[11,6,8,9,10,3], 8:[11,6,7,9,10,13], 9:[6,11,7,8,10,2], 10:[7,8,9,1,2,3],
      11:[1,4,5,6,7,8,9,12], 12:[1,2,14,13,15,11], 13:[2,8,14,12], 14:[12,13,15,18], 15:[14,12,27],
      16:[17,18], 17:[16,18], 18:[16,17,14,21],
      19:[20,21], 20:[19,21], 21:[19,20,18,24],
      22:[23,24], 23:[22,24], 24:[22,23,21,27],
      25:[26,27], 26:[25,27], 27:[25,26,24,15]}
print eulerPath(D)
import sys
if 'SEM' not in sys.argv:
	for i in xrange (130000):
		eulerPath(D)
