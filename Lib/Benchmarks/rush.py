# Rush hour solver from http://www.craig-wood.com/nick/pub/rush_hour_solver_cut_down.py
# mentioned in the c.l.py post "ANN: ShedSkin 0.2, an experimental (restricted) Python-to-C++ compiler"
# at Jul 21 2009 by Nick Craig-Wood

#DEJAVU
'''
{
'NAME':"Rush hour solver",
'DESC':"From c.l.py post about shedskin benchmarks",
'GROUP':'real-bench',
'CMPOUT':1,
'ARGS':"SEM",
'BARGS':""
}
'''

"""
Rush Hour Solver
"""

import random
import sys
import os

#import psyco
#from psyco.classes import *
#psyco.full()

random.seed (2233)

EastWest = 0
NorthSouth = 1
vehicle_lengths_map = { 2 : 12, 3 : 4 }
vehicle_lengths = []
for vmap_l,vmap_n in vehicle_lengths_map.items():
    vehicle_lengths.extend([vmap_l] * vmap_n)
vehicle_lengths.sort()                  # smallest first
WIDTH = 6
HEIGHT = 6
EXIT_X = WIDTH/2

class Vehicle(object):
    "Class to represent a Vehicle"
#    __slots__ = ['length', 'name', 'number', 'orientation', 'player', 'x', 'y']

    def __init__(self, length=2, number=0, name=None, orientation=None):
        "Make a new vehicle"
        self.length = length
        self.rename(number, name)
        if orientation is None:
            self.orientation = random.choice((EastWest, NorthSouth))
        else:
            self.orientation = orientation
        self.place()

    def __cmp__(self, other):
        "Compare two board vehicles by x,y co-ordinate"
        return cmp(self.y, other.y) or cmp(self.x, other.x) 

    def rename(self, number=0, name=None):
        "Rename the vehicle"
        if name is not None:
            self.name = name
            self.number = ord(name) - ord('A')
        else:
            self.number = number
            self.name = chr(ord('A')+self.number)
        self.player = (self.number == 0)

    def place(self, x=None, y=None):
        "Place the North/West end of the vehicle"
        self.x = x
        self.y = y

    def pycopy(self):
        "Copy this vehicle into a new object"
        new = Vehicle(length=self.length, number=self.number, name=self.name, orientation=self.orientation)
        new.length = self.length
        new.name = self.name
        new.number = self.number
        new.orientation = self.orientation
        new.player = self.player
        new.x = self.x
        new.y = self.y
        return new

class Board(object):
    "Class to represent the board"

#    __slots__ = ['rep','board','parent','best_child','vehicles','me_x','me_y','depth']

    def clear_board(self):
        self.rep = ["."] * (WIDTH * HEIGHT)
        self.board = [[None] * WIDTH for i in range(HEIGHT)]

    def __init__(self, string=None):
        "Make a new empty board"
        self.parent = None
        self.best_child = None
        self.clear_board()
        self.vehicles = []
        self.me_x = EXIT_X
        self.me_y = None
        self.depth = None
        if string:
            self.from_string(string)

    def canonicalise(self):
        "Make sure the vehicles are named canonically to make detecting duplicates easy"
        # sort the vehicles by x,y co-ord, but with me first
        vehicles = self.vehicles[1:]
        vehicles.sort()
        self.vehicles[1:] = vehicles
        for i in range(len(self.vehicles)):
            self.vehicles[i].rename(i)
        self.plot_vehicles()

    def from_string(self, string):
        "Fill a board up from a textual representation"
        assert len(string) == WIDTH * HEIGHT, "Board '%s' wrong length" % string
        vmap = {}     # Map of vehicle names with list of co-ordinates
        for y in range(HEIGHT):
            for x in range(WIDTH):
                c = string[x+WIDTH*y]
                if c != '.' and c != ' ':
                    vmap.setdefault(c, []).append((x,y))
        for c,v in vmap.items():
            assert len(v)==2 or len(v)==3, "Vehicle %s isn't 2 or 3 long" % c
            xsd = {}; ysd = {}
            for x,y in v:
                xsd[x] = 1; ysd[y] = 1
            xs = xsd.keys(); xs.sort()
            ys = ysd.keys(); ys.sort()
            if len(ys) == 1:
                orientation=EastWest
                xys = xs
            elif len(xs) == 1:
                orientation=NorthSouth
                xys = ys
            else:
            	assert 0, "Vehicle %s isn't EW or NS" % c
            assert len(xys) == len(v), "Vehicle %s has duplicate co-ordinates" % c
            xy0 = xys[0]
            for xy in xys[1:]:
                assert xy0 + 1 == xy,  "Vehicle %s co-ords not consecutive" % c
                xy0 = xy
            vehicle = Vehicle(length=len(xys), name=c, orientation=orientation)
            vehicle.player = 0
            vehicle.place(xs[0], ys[0])
            self.vehicles.append(vehicle)
        # Find the player - must be the lowest NorthSouth vehicle
        players = []
        for i in range(len(self.vehicles)):
            vehicle = self.vehicles[i]
            if vehicle.x == self.me_x and vehicle.orientation == NorthSouth:
                players.append((vehicle.y, i))
        assert players, "No player found on board"
        players.sort()
        i = players[-1][1]
        vehicle = self.vehicles[i]
        assert vehicle.length == 2, "Player must be of length 2"
        self.me_y = vehicle.y
        vehicle.player = 1
        self.vehicles[0], self.vehicles[i] = self.vehicles[i], self.vehicles[0]
        # Now convert the board into vehicle references and make the representation
        self.plot_vehicles()
        # Check new representation is the same as the old
        new_string = "".join(self.rep)
        assert string == new_string, "Board didn't come out the same\n in: %s\nout: %s" % (string, new_string)
        self.canonicalise()

    def __repr__(self):
        "Simple textual representation of board"
        return "".join(self.rep)

    def __str__(self):
        "Simple textual representation of board"
        s = "+" + "--" * WIDTH + "+\n";
        for y in range(HEIGHT):
            s += "|"
            for x in range(WIDTH):
                vehicle = self.board[x][y]
                if vehicle:
                    s += vehicle.name
                    s += " "
                else:
                    s += "  "
            s += "|\n"
        s += "+" + "--" * (EXIT_X) + "|-" + "--" * (WIDTH-EXIT_X-1) + "+\n";
        return s

    def pycopy(self):
        "Copy a board.  The vehicles are shallow copies, everything else is deep copied.  You are expected to make new vehicles if you need to change them"
        new = Board ()
        new.me_x = self.me_x
        new.me_y = self.me_y
        new.depth = self.depth
        new.parent = self
        new.best_child = None
        new.board = [self.board[i][:] for i in range(WIDTH)]
        new.rep = self.rep[:]
        new.vehicles = self.vehicles[:]
        return new

    def plot_vehicle(self, vehicle, d=0):
        "Plot the vehicle passed in, optionally moving it by d"
        x, y = vehicle.x, vehicle.y
        if vehicle.orientation == EastWest:
            if d:
                for l in range(vehicle.length):
                    self.board[x+l][y] = None
                    self.rep[(x+l)+WIDTH*y] = "."
            for l in range(vehicle.length):
                self.board[x+d+l][y] = vehicle
                self.rep[(x+d+l)+WIDTH*y] = vehicle.name
        else:
            if d:
                for l in range(vehicle.length):
                    self.board[x][y+l] = None
                    self.rep[x+WIDTH*(y+l)] = "."
            for l in range(vehicle.length):
                self.board[x][y+d+l] = vehicle
                self.rep[x+WIDTH*(y+d+l)] = vehicle.name
        
    def plot_vehicles(self):
        "Plot the vehicles array into the board, updating board and rep"
        self.clear_board()
        for vehicle in self.vehicles:
            self.plot_vehicle(vehicle)
        
    def move(self, i, d):
        "Move vehicle i by d"
        vehicle = self.vehicles[i]
        self.plot_vehicle(vehicle, d)
        new_vehicle = vehicle.pycopy()
        if vehicle.orientation == EastWest:
            new_vehicle.x += d
        else:
            new_vehicle.y += d
        self.vehicles[i] = new_vehicle

    def do_move(self, seen, i, d):
        "Return a board from a move, returning None if already seen"
        board = self.pycopy()
        board.move(i, d)
        s = repr(board)
        if not seen.has_key(s):
            seen[s] = 1
            return board
        return None

    def do_moves(self, seen, moves_list):
        "Return a list of boards from the list of moves (as returned by show_moves), not adding any that are already in seen"
        boards = []
        for i,d in moves_list:
            board = self.do_move(seen, i, d)
            if board:
                boards.append(board)
        return boards

    def show_moves(self):
        "Return a list of possible moves.  Each move is a tuple of vehicle number and distance"
        moves = []
        #print "Moves..."
        #print self
        for i in range(len(self.vehicles)):
            vehicle = self.vehicles[i]
            x, y = vehicle.x, vehicle.y
            if vehicle.orientation == EastWest:
                # East
                for d in range(-1,-x-1,-1):
                    if self.board[x + d][y]:
                        break;
                    moves.append((i, d))
                    #print "Move E: ",vehicle.name, " d =", d
                # West
                x_end = x + vehicle.length-1
                for d in range(1,WIDTH-x_end):
                    if self.board[x_end + d][y]:
                        break;
                    moves.append((i, d))
                    #print "Move W: ",vehicle.name, " d =", d
            else:
                # North
                for d in range(-1,-y-1,-1):
                    if self.board[x][y + d]:
                        break;
                    moves.append((i, d))
                    #print "Move N: ",vehicle.name, " d =", d
                # South
                y_end = y + vehicle.length-1
                for d in range(1,HEIGHT-y_end):
                    if self.board[x][y_end + d]:
                        break;
                    moves.append((i, d))
                    #print "Move S: ",vehicle.name, " d =", d
        return moves

    def moves(self, seen):
        "Return a list of new boards not in seen which are all the possible moves from this position"
        return self.do_moves(seen, self.show_moves())

    def solve(self, solved=None):
        "Solve this board returning a list of boards with the solution in or None"
        seen = { repr(self) : 1 }
        moves = [ self ]
        winner = [ ]
        if solved is None:
            solved = {}
        for depth in xrange(1,10000):
            new_moves = [ ]
            found = 0
            for b in moves:
                bb = solved.get(repr(b), None)
                if bb:
                    found += 1
                    new = bb.best_child.pycopy()
                    new.parent = b
                    new_moves.append(new)
                else:
                    new_moves += b.moves(seen)
            moves = new_moves
            if found:
                print "%2d: %5d moves found, %5d already found" % (depth, len(moves), found)
            else:
                print "%2d: %5d moves found" % (depth, len(moves))
            if not moves:
                self.depth = 0
                return None
            for b in moves:
                vehicle = b.board[EXIT_X][HEIGHT-1]
                if vehicle and vehicle.player:
                    winner.append(b)
            if winner:
                for b in winner:
                    solution = []
                    bb = b
                    while 1:
                        solution.append(bb)
                        solved[repr(bb)] = bb
                        parent = bb.parent
                        if not parent:
                            break
                        parent.best_child = bb
                        bb = parent
                    solution.reverse()
                self.depth = len(solution)
                print "%2d: %d winners found" % (self.depth, len(winner))
                return solution
        self.depth = 0
        return None

    def print_solution(self, solution):
        "Prints a solution which is a list of boards or None"
        if solution is None:
            print "No solution"
        else:
            print "Solution length", len(solution)
            for board in solution:
                print board

def main():
    try:
        for b, expected_length in (
            (Board("HH.N..FF.N.B.LCNJBDLCAJBDICAEKDIGGEK"), 7),
            (Board("HH.N..FF.N.B.LCNJBDLCAJBDICAEKDIGGEK"), 7),
            (Board("...AGGHHDA.NFCDIINFCKK.NJC...EJBBB.E"), 18),
            (Board("M.IKKKM.INNNCCEEGDBFFAGDBL.A.J.LHHHJ"), 32),
            (Board("LLJA..KKJA..GFIIHHGFEDDB..E..B.CCC.B"), 51),
            ):
            solution = b.solve()
            assert len(solution) == expected_length
            b.print_solution(solution)
	    if SEM and expected_length == 18:
		break
    except Exception, e:
        print "Caught exception: %s: %s" % e.__class__.__name__, e

if __name__ == "__main__":
	SEM = "SEM" in sys.argv
	main()
