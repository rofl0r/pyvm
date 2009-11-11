#if BLOCKSIZE != 16 && BLOCKSIZE != 8
#error "this cfb works only for 16/8 blocksize"
#endif

static int cfb_encrypt (unsigned char *in, unsigned char *out,
			unsigned long length, CIPHERCTX *ctx)
{
	const KEY *key = ctx->key;
	unsigned char *ivec = ctx->ivec;
	int enc = ctx->enc;

	unsigned long n;
	unsigned long len = length;
	unsigned char tmp[BLOCKSIZE];
	unsigned char tmp2[BLOCKSIZE];

	if (ctx->unused)
		return 1;

	if (enc) {
		while (len >= BLOCKSIZE) {
			unsigned int *o0 = (unsigned int*)out;
			unsigned int *i0 = (unsigned int*)in;
			unsigned int *v0 = (unsigned int*)ivec;
			unsigned int *t0 = (unsigned int*)tmp;
			ENCRYPT (ivec, tmp, key);
			o0 [0] = v0 [0] = t0 [0] ^ i0 [0];
			o0 [1] = v0 [1] = t0 [1] ^ i0 [1];
#if BLOCKSIZE == 16
			o0 [2] = v0 [2] = t0 [2] ^ i0 [2];
			o0 [3] = v0 [3] = t0 [3] ^ i0 [3];
#endif
			len -= BLOCKSIZE;
			in  += BLOCKSIZE;
			out += BLOCKSIZE;
		}
		if (len) {
			ENCRYPT (ivec, tmp, key);
			for (n=0;n<len;n++)
				out [n] = ivec [n] = tmp [n] ^ in [n];
			ctx->unused = BLOCKSIZE - len;
		}
	} else {
		while (len >= BLOCKSIZE) {
			unsigned int *o0 = (unsigned int*)out;
			unsigned int *i0 = (unsigned int*)in;
			unsigned int *t0 = (unsigned int*)tmp;
			ENCRYPT (ivec, tmp, key);
			__builtin_memcpy (tmp2, in, BLOCKSIZE);
			o0 [0] = t0 [0] ^ i0 [0];
			o0 [1] = t0 [1] ^ i0 [1];
#if BLOCKSIZE == 16
			o0 [2] = t0 [2] ^ i0 [2];
			o0 [3] = t0 [3] ^ i0 [3];
#endif
			__builtin_memcpy (ivec, tmp2, BLOCKSIZE);
			len -= BLOCKSIZE;
			in  += BLOCKSIZE;
			out += BLOCKSIZE;
		}
		if (len) {
			ENCRYPT (ivec, tmp, key);
			__builtin_memcpy (tmp2, in, len);
			for (n=0;n<len;n++)
				out [n] = tmp [n] ^ in [n];
			__builtin_memcpy (ivec, tmp2, len);
			ctx->unused = BLOCKSIZE - len;
		}
	}
	return 0;
}
