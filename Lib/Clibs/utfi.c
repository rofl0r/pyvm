static unsigned int utf8_getchar (const unsigned char **p)
{
	const unsigned char *s = *p;

	if ((*s & 0xe0) == 0xc0) {
		*p += 2;
		return ((((unsigned long)(s [0] & 0x1F)) << 6) |
		      ((unsigned long)(s [1] & 0x3F)));
	}

	if ((*s & 0xf0) == 0xe0) {
		*p += 3;
		return ((((unsigned long)(s [0] & 0x0F)) << 12) |
		     (((unsigned long)(s [1] & 0x3F)) << 6) |
		      ((unsigned long)(s [2] & 0x3F)));
	}

	if ((*s & 0xF8) == 0xF0) {
		*p += 4;
		return ((((unsigned long)(s [0] & 0x07)) << 18) |
		     (((unsigned long)(s [1] & 0x3F)) << 12) |
		     (((unsigned long)(s [2] & 0x3F)) << 6) |
		      ((unsigned long)(s [3] & 0x3F)));
	}

	if ((*s & 0xFC) == 0xF8) {
		*p += 5;
		return ((((unsigned long)(s [0] & 0x03)) << 24) |
		     (((unsigned long)(s [1] & 0x3F)) << 18) |
		     (((unsigned long)(s [2] & 0x3F)) << 12) |
		     (((unsigned long)(s [3] & 0x3F)) << 6) |
		      ((unsigned long)(s [4] & 0x3F)));
	}

	if ((*s & 0xFC) == 0xFC) {
		*p += 6;
		return ((((unsigned long)(s [0] & 0x03)) << 30) |
		     (((unsigned long)(s [1] & 0x3F)) << 24) |
		     (((unsigned long)(s [2] & 0x3F)) << 18) |
		     (((unsigned long)(s [3] & 0x3F)) << 12) |
		     (((unsigned long)(s [4] & 0x3F)) << 6) |
		      ((unsigned long)(s [5] & 0x3F)));
	}

	++*p;
	return s [0];
}
