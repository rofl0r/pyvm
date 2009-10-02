/*
 * GIF decoder
 *
 * Copyright (c) 2007 Stelios Xanthakis
 *
 * The lzw decoder is taken from the ffmpeg project
 * lzw.c which is written by Fabrice Bellard.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 */

typedef struct {
	unsigned char r, g, b;
} rgb;

// LZW decoder from ffmpeg. slightly modified

#define LZW_MAXBITS                 12
#define LZW_SIZTABLE                (1<<LZW_MAXBITS)

typedef unsigned short int uint16_t;
typedef unsigned char uint8_t;

typedef struct LZWState {
	uint8_t *pbuf, *ebuf;
	int bbits;
	unsigned int bbuf;

	int cursize;                ///< The current code size
	int curmask;
	int codesize;
	int clear_code;
	int end_code;
	int newcodes;               ///< First available code
	int top_slot;               ///< Highest code for current size
	int top_slot2;              ///< Highest possible code for current size (<=top_slot)
	int slot;                   ///< Last read code
	int fc, oc;
	uint8_t *sp;
	uint8_t stack[LZW_SIZTABLE];
	uint8_t suffix[LZW_SIZTABLE];
	uint16_t prefix[LZW_SIZTABLE];
	int bs;                     ///< current buffer size for GIF
} LZWState;

/* get one code from stream */
static int lzw_get_code (struct LZWState * s)
{
	int c, sizbuf;

	while (s->bbits < s->cursize) {
		if (!s->bs) {
			sizbuf = *s->pbuf++;
			s->bs = sizbuf;
			if (s->pbuf + sizbuf > s->ebuf || !sizbuf) {
				s->pbuf--;
				return s->end_code;
			}
		}
		s->bbuf |= (*s->pbuf++) << s->bbits;
		s->bbits += 8;
		s->bs--;
	}
	c = s->bbuf & s->curmask;
	s->bbuf >>= s->cursize;
	s->bbits -= s->cursize;
	return c;
}

static const unsigned short int mask [] =
{
	0x0000, 0x0001, 0x0003, 0x0007,
	0x000F, 0x001F, 0x003F, 0x007F,
	0x00FF, 0x01FF, 0x03FF, 0x07FF,
	0x0FFF, 0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF
};

static int ff_lzw_decode (LZWState *s, uint8_t *buf, int len)
{
	int l, c, code, oc, fc;
	uint8_t *sp;

	if (s->end_code < 0)
		return 0;

	l = len;
	sp = s->sp;
	oc = s->oc;
	fc = s->fc;

	for (;;) {
		while (sp > s->stack) {
			*buf++ = *(--sp);
			if ((--l) == 0)
				goto the_end;
		}
		c = lzw_get_code(s);
		if (c == s->end_code) {
			break;
		} else if (c == s->clear_code) {
			s->cursize = s->codesize + 1;
			s->curmask = mask[s->cursize];
			//s->curmask = (1 << s->cursize) - 1;
			s->slot = s->newcodes;
			s->top_slot = 1 << s->cursize;
			fc= oc= -1;
		} else {
			code = c;
			if (code == s->slot && fc>=0) {
				*sp++ = fc;
				code = oc;
			} else if(code >= s->slot)
				break;
			while (code >= s->newcodes) {
				*sp++ = s->suffix[code];
				code = s->prefix[code];
			}
			*sp++ = code;
			if (s->slot < s->top_slot && oc>=0) {
				s->suffix[s->slot] = code;
				s->prefix[s->slot++] = oc;
			}
			fc = code;
			oc = c;
			if (s->slot >= s->top_slot) {
				if (s->cursize < LZW_MAXBITS) {
					s->top_slot <<= 1;
					//s->curmask = (1 << ++s->cursize) - 1;
					s->curmask = mask[++s->cursize];
				}
			}
		}
	}
	s->end_code = -1;
  the_end:
	s->sp = sp;
	s->oc = oc;
	s->fc = fc;
	return len - l;
}

static int read_image (unsigned char *buf, int left, uint8_t *out, int len, int height, int interlace)
{
	LZWState s;
	unsigned char csize;

	csize = *buf++;
	--left;

	// ff_lzw_decode_init

	/* read buffer */
	s.pbuf = buf;
	s.ebuf = s.pbuf + left;
	s.bbuf = 0;
	s.bbits = 0;
	s.bs = 0;

	/* decoder */
	s.codesize = csize;
	s.cursize = s.codesize + 1;
	s.curmask = (1 << s.cursize) - 1;
	s.top_slot = 1 << s.cursize;
	s.clear_code = 1 << s.codesize;
	s.end_code = s.clear_code + 1;
	s.slot = s.newcodes = s.clear_code + 2;
	s.oc = s.fc = 0;
	s.sp = s.stack;

	s.top_slot2 = s.top_slot;

	/* do */
	if (!interlace) {
		ff_lzw_decode (&s, out, height * len);
	} else {
		int y;
		for (y = 0; y < height; y += 8)
			ff_lzw_decode (&s, out + y * len, len);
		for (y = 4; y < height; y += 8)
			ff_lzw_decode (&s, out + y * len, len);
		for (y = 2; y < height; y += 4)
			ff_lzw_decode (&s, out + y * len, len);
		for (y = 1; y < height; y += 2)
			ff_lzw_decode (&s, out + y * len, len);
	}

	/* bytes consumed from the compressed buffer */
	return 3 + s.pbuf - buf;
}

// (we'd like, if out=NULL to decode without placing to buffer)

int decode (unsigned char *buf, int offset, int left, char *out, int len, int height, int interlace)
{
	return read_image (buf + offset, left, out, len, height, interlace);
}

void expand (const unsigned char in[], rgb out[], const rgb colormap[], int len)
{
	int i;
	for (i = 0; i < len; i++)
		out [i] = colormap [in [i]];
}

const char ABI [] = 
"i decode siisiii\n"
"- expand sssi\n"
;
