/*
 *  Base64
 * 
 *  Copyright (c) 2007 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

static const char Base64EncTable [] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

void Encode64 (const unsigned char *p, int len, unsigned char Encoded [], int offset)
{
	unsigned char *e;
	const unsigned char *f;

	p += offset;
	f = p + len;
	e = Encoded;

	while (1)
	{
		if (p == f) break;
		*e++ = (*p >> 2) & 63;
		*e = (*p & 3) << 4;
		*e++ |= (*++p >> 4) & 15;
		if (p == f) 
		{
			*e++ = 64;
			*e++ = 64;
			break;
		}
		*e = (*p & 15) << 2;
		*e++ |= (*++p & 192) >> 6;
		if (p == f) {
			*e++ = 64;
			break;
		}
		*e++ = *p++ & 63;
	}
	
	unsigned char *x;
	for (x = Encoded; x < e ; x++)
		*x = Base64EncTable [*x];
}

const static unsigned char Base64DecTable [] = {
	128,128,128,128, 128,128,128,128, 128,128,128,128, 128,128,128,128,
	128,128,128,128, 128,128,128,128, 128,128,128,128, 128,128,128,128,
	128,128,128,128, 128,128,128,128, 128,128,128, 62, 128,128,128, 63,
	 52, 53, 54, 55,  56, 57, 58, 59,  60, 61,128,128, 128, 64,128,128,
	128,  0,  1,  2,   3,  4,  5,  6,   7,  8,  9, 10,  11, 12, 13, 14,
	 15, 16, 17, 18,  19, 20, 21, 22,  23, 24, 25,128, 128,128,128,128,
	128, 26, 27, 28,  29, 30, 31, 32,  33, 34, 35, 36,  37, 38, 39, 40,
	 41, 42, 43, 44,  45, 46, 47, 48,  49, 50, 51,128, 128,128,128,128,
	128,128,128,128, 128,128,128,128, 128,128,128,128, 128,128,128,128,
	128,128,128,128, 128,128,128,128, 128,128,128,128, 128,128,128,128,
	128,128,128,128, 128,128,128,128, 128,128,128,128, 128,128,128,128,
	128,128,128,128, 128,128,128,128, 128,128,128,128, 128,128,128,128,
	128,128,128,128, 128,128,128,128, 128,128,128,128, 128,128,128,128,
	128,128,128,128, 128,128,128,128, 128,128,128,128, 128,128,128,128,
	128,128,128,128, 128,128,128,128, 128,128,128,128, 128,128,128,128,
	128,128,128,128, 128,128,128,128, 128,128,128,128, 128,128,128,128
};

int Decode64 (const unsigned char *p, int len, unsigned char *txt)
{
	unsigned char a, b, c, d;
	int i;

	len = len / 4;

	for (i = 0; i < len - 1; i++) {
		a = Base64DecTable [*p++];
		b = Base64DecTable [*p++];
		c = Base64DecTable [*p++];
		d = Base64DecTable [*p++];
		if ((a | b | c | d) & (64|128))
			return -1;
		*txt++ = (a << 2) | (b >> 4);
		*txt++ = ((b & 15) << 4) | ((c>>2) & 15);
		*txt++ = ((c & 3) << 6) | d;
	}
	a = Base64DecTable [*p++];
	b = Base64DecTable [*p++];
	if ((a | b) & 128)
		return -1;
	if (a == 64 || b == 64)
		return -1;
	*txt++ = (a << 2) | (b >> 4);
	c = Base64DecTable [*p++];
	if (c & 128)
		return -1;
	if (c == 64)
		return 0;
	*txt++ = ((b & 15) << 4) | ((c>>2) & 15);
	d = Base64DecTable [*p++];
	if (d & 128)
		return -1;
	if (d == 64)
		return 0;
	*txt++ = ((c & 3) << 6) | d;

	return 0;
}

const char ABI [] =
"- Encode64 sisi\n"
"i Decode64 sis\n"
;
