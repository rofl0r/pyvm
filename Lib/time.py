from _time import *

# Get the (current) date/time in various forms

def timeit (f, *argz):
	t0=time ()
	f (*argz)
	return time()-t0
def cpu (f, *argz):
	t0=cpu_ticks ()
	f (*argz)
	return cpu_dt (t0)
def monthof (t):
	return gmtime (t)[1]
def dateof (t):
	t = gmtime (t)
	year = t [0]
	month = t [1]
	day = t [2]
	return "%02i/%02i/%i" %(day, month, year)
def sdateof (t):
	t = gmtime (t)
	year = t [0]
	month = t [1]
	day = t [2]
	return "%02i/%02i/%02i" %(day, month, year%100)
def wdateof (t):
	t = gmtime (t)
	day = t [2]
	wday = t [6]
	return "%s, %i" %(WDAY [wday], day)
def timeof (t=None):
	if t is None:
		t = time ()
	t = gmtime (t)
	hour, minute, second = t [3], t [4], t [5]
	return "%i:%02i:%02i" %(hour, minute, second)
def ltimeof (t=None):
	if t is None:
		t = time ()
	t = localtime (t)
	hour, minute, second = t [3], t [4], t [5]
	return "%i:%02i:%02i" %(hour, minute, second)
def hmtimeof (t):
	t = gmtime (t)
	hour, minute = t [3], t [4]
	return "%02i:%02i" %(hour, minute)
def hhmm ():
	t = localtime (time ())
	return t [3], t [4], t [5]

WDAY = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
WDAYL = ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"]
MONTH = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]

def rfc1123 (t):
	year, mon, mday, hour, min, sec, wday, yday, isdst = gmtime (t)
	return "%s, %02i %s %i %02i:%02i:%02i GMT" % (WDAY [wday], mday, MONTH [mon - 1],
				 year, hour, min, sec)

def monday (t):
	year, mon, mday, hour, min, sec, wday, yday, isdst = gmtime (t)
	return "%s %2i" %(MONTH [mon-1], mday)

def MMYY (t):
	year, mon, mday, hour, min, sec, wday, yday, isdst = gmtime (t)
	return "%s %s" %(MONTH [mon-1], year)

def ptime (t):
	year, mon, mday, hour, min, sec, wday, yday, isdst = gmtime (t)
	return "%s, %02i %s %i %02i:%02i:%02i" % (WDAY [wday], mday, MONTH [mon - 1],
				 year, hour, min, sec)

def ltime (t):
	year, mon, mday, hour, min, sec, wday, yday, isdst = localtime (t)
	return "%s, %02i %s %i %02i:%02i:%02i" % (WDAY [wday], mday, MONTH [mon - 1],
				 year, hour, min, sec)

def ltimetz (t):
	year, mon, mday, hour, min, sec, wday, yday, isdst = localtime (t)
	return "%s, %02i %s %i %02i:%02i:%02i %s" % (WDAY [wday], mday, MONTH [mon - 1],
				 year, hour, min, sec, LocalOffset)

# "Sat, 7 Nov 2009"
def wdddmmyy (t):
	if type (t) is str:
		t = datestr_to_secs (t)
	year, mon, mday, hour, min, sec, wday, yday, isdst = gmtime (t)
	return "%s, %i %s %i"%(WDAY [wday], mday, MONTH [mon - 1], year)

# Various classes to be used for timing

class Timit:
	def __init__ (self):
		self.start0 = self.start = time ()
	def __call__ (self):
		return time () - self.start
	def p (self):
		print time () - self.start
	def pr (self, msg=""):
		dt = time () - self.start
		print msg + "%.6f" %dt
		self.start = time ()
		return dt
	def mbpsr (self, bytes, digits=3):
		dt = time () - self.start
		print ("%%.%ifMB/sec"%digits) %(bytes/(1024*1024*dt))
		self.start = time ()
		return dt
	def r (self):
		self.start = time ()
	def prc (self):
		print time () - self.start0

class Tickit:
	def __init__ (self):
		self.start0 = self.start = cpu_ticks ()
	def __call__ (self):
		return int (cpu_dt (self.start))
	def p (self):
		print cpu_dt (self.start)
	def pr (self, msg=""):
		dt = cpu_dt (self.start)
		print msg + dt
		self.start = cpu_ticks ()
		return dt

class wTimer:
	def __init__ (self, msg, active=True, ms=False):
		self.active = active
		self.msg = msg
		self.ms = ms
	def __context__ (self):
		return self
	def __enter__ (self):
		if self.active:
			self.t0 = time ()
	def __exit__ (self, *args):
		if self.active:
			dt = time () - self.t0
			if self.ms:
				print self.msg, "%.3fms" % (1000*dt)
			else:
				print self.msg, "%.5f" % dt

def wmsTimer (*args, **kwargs):
	kwargs ["ms"] = True
	return wTimer (*args, **kwargs)

## Generic date-string to seconds since epoch

import re

MONTHS = "|".join (MONTH)
WDAYS = "|".join (WDAY)
WDAYLS = "|".join (WDAYL)

# ToDo: write an intelligent datestr parser that understands all the
# formats below and figures out month/day names, timezones, dates, etc.
# Those formats have been discovered through thousands of Mime Date:
# headers and HTTP cookies

FMTS = [
	"DAY DD MONTH YYYY TIME",
	"DD MONTH YYYY TIME",
	"DAY DD-MONTH-YYYY TIME",
	"DAY MONTH DD YYYY TIME",
	"DAY MONTH DD TIME_NOTZ YYYY",
	# http cookies
	"DAY MONTH DD TIME_NOTZ YYYY TIMEZONE",
	"LDAY DD-MONTH-YYYY TIME",
	"LDAY DD-MONTH-YYYY TIME_NOTZ",
]

R = []
for f in FMTS:
	f = f.replace ("LDAY", "(?:"+WDAYLS+")")
	f = f.replace ("DAY", "(?:"+WDAYS+")")
	f = f.replace ("MONTH", "(?P<month>"+MONTHS+")")
	f = f.replace ("DD", r"(?P<day>\d+)")
	f = f.replace ("YYYY", r"(?P<year>\d+)")
	f = f.replace ("TIME_NOTZ", r"(?P<hour>\d\d):(?P<min>\d\d):(?P<sec>\d\d)")
	f = f.replace ("TIMEZONE", r"(?P<tz>\S+)")
	f = f.replace ("TIME", r"(?P<hour>\d\d?):(?P<min>\d\d?)(:?:(?P<sec>\d\d?))?(:?\s+(?P<tz>\S+))?")
	f = f.replace (" ", r"\s+")
	R.append (re.re (f, re.I))


def parse (d):
	d = d.replace (",", "")
	for r in R:
		r = r (d)
		if not r:
			continue
		try:
			return r ["day"], r ["month"], r ["year"], r ["hour"], r ["min"], r ["sec"], r ["tz"]
		except:
			return r ["day"], r ["month"], r ["year"], r ["hour"], r ["min"], r ["sec"], "GMT"
	else:
		print "NOMATCH:", d

#-----------------------------

TZN = {
	"GMT":0, "UTC":0,
	"PDT":-7, "EDT":-4,
	"EST":-5, "PST":-8,
	"CST":-6, "CDT":-5,
	"MST":-7, "MDT":-6
}

def datestr_to_secs (d, notz=False):
	try:
		day, month, year, hour, minute, sec, tz = parse (d)
		if sec is None:
			sec = 0
		if tz is None:
			tz = "GMT"
	except:
		raise Error ("Unknown date format [%s]" %d)

	month = MONTH.index (month)
	day = int (day)
	year = int (year) - 1900
	hour = int (hour)
	minute = int (minute)
	sec = int (sec)
	tm = mktime ((year, month, day, hour, minute, sec, 0, 0, -1))
	try:
		tz = 3600 * int (tz) / 100
	except:
		if tz in TZN:
			tz = TZN [tz] * 3600
		else:
			print "BAD TIMEZONE [%s]"% tz
			tz = 0
	if notz:
		return tm, secs_to_offset (tz)
	return tm - tz

# compute the local time offset (for example "+0300" for EEST)
def local_offset ():
	n = now ()
	return secs_to_offset (datestr_to_secs (ltime (n)) - datestr_to_secs (ptime (n)))

def secs_to_offset (dsec):
	if dsec < 0:
		ew = "-"
		dsec = -dsec
	else:
		ew = "+"
	offset = dsec / 60
	offset = (offset % 60) + 100 * (offset / 60)
	offset = ew + "%04i"%offset
	return offset

LocalOffset = local_offset ()

if __name__ == __main__:
	print ltimetz (now ())
