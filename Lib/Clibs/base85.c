/*
 *  Ascii85 Encoding
 * 
 *  Copyright (c) 2007 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

static inline int isws (char c)
{
	return c == 0 || c == 9 || c == 10 || c == 12 || c == 13 || c == 32;
}

int count_ws (char *buffer, int len)
{
	int ws = 0;
	while (len--)
		if (isws (*buffer++)) ++ws;
	return ws;
}

void remove_ws (char *dst, char *buffer, unsigned int len)
{
	unsigned int i;
	for (i = 0; i < len; i++)
		if (!isws (buffer [i]))
			*dst++ = buffer [i];
}

int decoded_size (char *buffer, int len)
{
	int size = 0;

	if (buffer [len-1] == '>' && buffer [len-2] == '~')
		len -= 2;
	if (buffer [0] == '<' && buffer [1] == '~') {
		buffer += 2;
		len -= 2;
	}

	while (len >= 5) {
		if (*buffer == 'z') {
			++buffer;
			--len;
		} else {
			buffer += 5;
			len -= 5;
		}
		size += 4;
	}

	if (len) size += len - 1;
	return size;
}

int decode (unsigned char *dst, const char *buffer, int len)
{
	unsigned char ch;
	unsigned int acc = 0;
	int i;

	if (buffer [len-1] == '>' && buffer [len-2] == '~')
		len -= 2;
	if (buffer [0] == '<' && buffer [1] == '~') {
		buffer += 2;
		len -= 2;
	}

	while (len >= 5) {
		if (*buffer == 'z') {
			++buffer;
			--len;
			*dst++ = 0;
			*dst++ = 0;
			*dst++ = 0;
			*dst++ = 0;
			continue;
		}
		for (acc = i = 0; i < 5; i++) {
			ch = *buffer++;
			if (ch < 33 || ch > 'u')
				return ch + 1;
			acc = acc * 85 + ch - 33;
		}
		*dst++ = acc >> 24;
		*dst++ = acc >> 16;
		*dst++ = acc >> 8;
		*dst++ = acc;
		len -= 5;
	}

	if (len) {
		if (len == 1)
			return -1;
		for (acc = i = 0; i < len; i++) {
			ch = *buffer++;
			if (ch < 33 || ch > 'u')
				return ch + 1;
			acc = acc * 85 + ch - 33;
		}
		while (i++ < 5)
			acc = acc * 85 + 84;
		while (--len) {
			*dst++ = acc >> 24;
			acc <<= 8;
		}
	}
	return 0;
}

int encode (unsigned char *dst, const unsigned char *buffer, int len)
{
	unsigned char *dst0 = dst;
	unsigned int acc;
	int i;

	while (len >= 4) {
		acc = (buffer [0] << 24) | (buffer [1] << 16) | (buffer [2] << 8) | buffer [3];
		buffer += 4;
		len -= 4;
		if (!acc) {
			*dst++ = 'z';
			continue;
		}
		for (i = 4; i >= 0; i--, acc /= 85)
			dst [i] = 33 + acc % 85;
		dst += 5;
	}

	if (len) {
		for (acc = i = 0; i < len; i++)
			acc = (acc << 8) | *buffer++;
		while (i++ < 4)
			acc <<= 8;
		for (i = 4; i >= 0; i--, acc /= 85)
			if (i <= len)
				dst [i] = 33 + acc % 85;
		dst += len + 1;
	}

	return dst - dst0;
}

const char ABI [] =
"i count_ws	si	\n"
"- remove_ws	ssi	\n"
"i decoded_size	si	\n"
"i decode	ssi	\n"
"i encode	ssi	\n"
;
