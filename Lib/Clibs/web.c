/* www utilities */

static const char chars [256] = {
	['a'] = 1, ['b'] = 1, ['c'] = 1, ['d'] = 1, ['e'] = 1, ['f'] = 1,
	['g'] = 1, ['h'] = 1, ['i'] = 1, ['j'] = 1, ['k'] = 1, ['l'] = 1,
	['m'] = 1, ['n'] = 1, ['o'] = 1, ['p'] = 1, ['q'] = 1, ['r'] = 1,
	['s'] = 1, ['t'] = 1, ['u'] = 1, ['v'] = 1, ['w'] = 1, ['x'] = 1,
	['y'] = 1, ['z'] = 1,
	['A'] = 1, ['B'] = 1, ['C'] = 1, ['D'] = 1, ['E'] = 1, ['F'] = 1,
	['G'] = 1, ['H'] = 1, ['I'] = 1, ['J'] = 1, ['K'] = 1, ['L'] = 1,
	['M'] = 1, ['N'] = 1, ['O'] = 1, ['P'] = 1, ['Q'] = 1, ['R'] = 1,
	['S'] = 1, ['T'] = 1, ['U'] = 1, ['V'] = 1, ['W'] = 1, ['X'] = 1,
	['Y'] = 1, ['Z'] = 1,
	['0'] = 1, ['1'] = 1, ['2'] = 1, ['3'] = 1, ['4'] = 1, ['5'] = 1,
	['6'] = 1, ['7'] = 1, ['8'] = 1, ['9'] = 1,
	[' '] = 1,
};

int urlencode_len (unsigned char str[], int len)
{
	int sz = 0, i;
	for (i = 0; i < len; i++)
		sz += chars [str [i]] ? 1 : 3;
	return sz;
}

static const char hexchar [] = "0123456789abcdef";

void urlencode (unsigned char in[], int len, unsigned char *out)
{
	int i;
	for (i = 0; i < len; i++)
		if (in [i] == ' ') *out++ = '+';
		else if (chars [in [i]]) *out++ = in [i];
		else {
			*out++ = '%';
			*out++ = hexchar [in [i] >> 4];
			*out++ = hexchar [in [i] & 15];
		}
}

const char ABI [] =
"i urlencode_len	si\n"
"- urlencode		sis\n"
;
