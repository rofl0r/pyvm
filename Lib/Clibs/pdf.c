/*
 *  Encoders/Decoders and utilities for PDF
 * 
 *  Copyright (c) 2008 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

#define ncase break; case

/* hybrid tokenizer:
 - maintain a state of 'text start', 'position', 'end'
 - skip whitespace
 - determine the type of the pdf "token".
 - if a simple kind of token, parse it, otherwise let higher level
   regular expressions parse it and advance the state accordingly
*/

typedef struct {
	unsigned char *txt;
	int end, pos;
	int *rv;	/* return values array */
} pdftoks;

static const unsigned int btab [256] = {
	['0'] = 1, ['1'] = 1, ['2'] = 1, ['3'] = 1, ['4'] = 1,
	['5'] = 1, ['6'] = 1, ['7'] = 1, ['8'] = 1, ['9'] = 1,

	['+'] = 2, ['-'] = 2,

	['/'] = 4, ['%'] = 4, ['('] = 4, [')'] = 4, ['{'] = 4,
	['}'] = 4, ['['] = 4|32, [']'] = 4|32, ['<'] = 4|64, ['>'] = 4|64,

	[0] = 12, ['\t'] = 12, ['\n'] = 12, ['\f'] = 12, ['\r'] = 12, [' '] = 12,

	['a'] = 16, ['b'] = 16, ['c'] = 16, ['d'] = 16, ['e'] = 16,
	['f'] = 16, ['g'] = 16, ['h'] = 16, ['i'] = 16, ['j'] = 16,
	['k'] = 16, ['l'] = 16, ['m'] = 16, ['n'] = 16, ['o'] = 16,
	['p'] = 16, ['q'] = 16, ['r'] = 16, ['s'] = 16, ['t'] = 16,
	['u'] = 16, ['v'] = 16, ['w'] = 16, ['x'] = 16, ['y'] = 16,
	['z'] = 16,
	['A'] = 16, ['B'] = 16, ['C'] = 16, ['D'] = 16, ['E'] = 16,
	['F'] = 16, ['G'] = 16, ['H'] = 16, ['I'] = 16, ['J'] = 16,
	['K'] = 16, ['L'] = 16, ['M'] = 16, ['N'] = 16, ['O'] = 16,
	['P'] = 16, ['Q'] = 16, ['R'] = 16, ['S'] = 16, ['T'] = 16,
	['U'] = 16, ['V'] = 16, ['W'] = 16, ['X'] = 16, ['Y'] = 16,
	['Z'] = 16,
};

inline int skip_ws (const unsigned char txt[], int pos, int end)
{
	for (;pos < end;++pos)
		if (!(btab [txt [pos]] & 8))
			return pos;
	return end;
}

void initok (pdftoks *p, unsigned char *txt, int end, int *rv, int pos)
{
	p->txt = txt;
	p->end = end;
	p->rv = rv;
	p->pos = pos;
}

void setpos (pdftoks *p, int pos)
{
	p->pos = pos;
}

static inline int isnum (const unsigned char *t, int *isfloat)
{
	if (btab [t [0]] == 2) {
		int x = isnum (t + 1, isfloat);
		return x ? x + 1 : 0;
	}

	if (btab [t [0]] != 1 && (*t != '.' || btab [t [1]] != 1))
		return 0;

	const unsigned char *t0 = t;
	while (btab [*t] == 1)
		++t;
	if (*t == '.') {
		*isfloat = 1;
		++t;
		while (btab [*t] == 1)
			++t;
	}
	return t - t0;
}

int gettok (pdftoks *p)
{
	int pos = p->pos;
	pos = skip_ws (p->txt, pos, p->end);
	if (pos == p->end)
		return -1;
	
	// parses: numbers (integers, floats), /names
	// and the special tokens "<<", ">>", "[", "]"
	// and the keywords (like "obj", "R", "endobj", "true", "false", "null").
	// for none of the above, the python part will have to decide but that's
	// about 1% of the total tokens.

	const unsigned char *t = p->txt + pos;
	int isfloat = 0;
	int x = isnum (t, &isfloat);
	if (x) {
		if (!isfloat) {
			p->rv [0] = 1;
			p->rv [1] = atoi (t);
		} else {
			p->rv [0] = 2;
			p->rv [1] = pos;
		}
		return p->pos = x + pos;
	}

	if (*t == '/') {
		p->rv [0] = 3;
		x = pos + 1;
		while (x < p->end && !(btab [p->txt [x]] & 4))
			++x;
		p->rv [1] = pos + 1;
		return p->pos = x;
	}

	if (btab [*t] & 32) {
		p->rv [0] = *t == '[' ? 4 : 5;
		return p->pos = pos + 1;
	}

	if ((btab [*t] & 64) && (t [0] == t [1])) {
		p->rv [0] = *t == '<' ? 6 : 7;
		return p->pos = pos + 2;
	}

	if (btab [*t] & 16) {
		p->rv [0] = 8;
		x = pos + 1;
		while (x < p->end && (btab [p->txt [x]] & 16))
			++x;
		p->rv [1] = pos;
		return p->pos = x;
	}

	p->rv [0] = 0;
	return pos;
}

int scan_string (char txt [], int pos)
{
	int np = 0, nc = 0;

	for (;;nc++)
		switch (txt [pos++]) {
			 case '(': np++;
			ncase ')': if (!np--) return nc;
			ncase '\\':
			switch (txt [pos++]) {
				case 'r': case 'n': case '\\': case '(': case ')':
				ncase '0'...'7': pos += 2; break;
				default: --nc;
			}
		}
}

int get_string (char txt[], int pos, char *out)
{
	int np = 0;
	char c;

#define NN(X) (X-'0')
	for (;;)
		switch (c = txt [pos++]) {
			default: *out++ = c;
			ncase '(': *out++ = c; np++;
			ncase ')': if (!np--) return pos;
				*out++ = c;
			ncase '\\':
			switch (c = txt [pos++]) {
				 case 'r': *out++ = '\r';
				ncase 'n': *out++ = '\n';
				ncase '\\': *out++ = '\\';
				ncase '(': *out++ = '(';
				ncase ')': *out++ = ')';
				ncase '0'...'7':
					*out++ = NN(c)*64 + NN(txt[pos])*8 + NN(txt[pos+1]);
					pos += 2;
			}
		}
}

/* LZW, almost copied from xpdf */
int LZWDecode (const unsigned char *buf, int bs, unsigned char *out, int os)
{
	int i, j;
	unsigned int newchar, prevcode;
	int nl = 0;
	unsigned int nbits = 9;
	unsigned int bbuf = 0, bbits = 0;
	unsigned int nextcode = 258;
	unsigned int code;
	int first = 1;
	int slen = 0;
	int op = 0;
	int curmask = (1<<nbits)-1;
	struct {
		unsigned short length;
		unsigned short head;
		unsigned char tail;
	} table [4096];

	for (;;) {
		while (bbits < nbits) {
			bbuf = (bbuf << 8) | *buf++;
			bbits += 8;
			--bs;
		}

		code = (bbuf >> (bbits - nbits)) & curmask;
		bbits -= nbits;

		if (bs <= 0 || code == 257)
			return op;
		if (code == 256) {
			/* clear */
			nextcode = 258;
			nbits = 9;
			curmask = (1 << nbits) - 1;
			first = 1;
			slen = 0;
			continue;
		}

		if (nextcode > 4096)
			return -2;

		nl = slen + 1;
		if (code < 256) {
			if (op == os) return -1;
			newchar = out [op++] = code;
			slen = 1;
		} else if (code < nextcode) {
			slen = table [code].length;
			if (op + slen >= os) return -1;
			j = code;
			for (i = slen - 1; i > 0; i--) {
				out [i + op] = table [j].tail;
				j = table [j].head;
			}
			newchar = out [op] = j;
			op += slen;
		} else if (code == nextcode) {
			if (op + slen + 1 >= os) return -1;
			for (i = 0; i < slen; i++, op++)
				out [op] = out [op - slen];
			out [op++] = newchar;
			++slen;
		} else return -3;	/* error */

		if (!first) {
			table [nextcode].length = nl;
			table [nextcode].head = prevcode;
			table [nextcode].tail = newchar;
			switch (++nextcode) {
				default: goto out;
				ncase 511:  nbits = 10;
				ncase 1023: nbits = 11;
				ncase 2047: nbits = 12;
			}
			curmask = (1 << nbits) - 1;
			out:;
		} else first = 0;
		prevcode = code;
	}
}

const char ABI [] =
"- initok	ssip32i	\n"
"i gettok	s	\n"
"- setpos	si	\n"
"i skip_ws	sii	\n"
"i scan_string	si	\n"
"i get_string	sis	\n"
"i LZWDecode	sisi	\n"
;
