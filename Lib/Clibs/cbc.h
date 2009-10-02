#if BLOCKSIZE != 16 && BLOCKSIZE != 8
#error "this cbc works only for 16/8 blocksize"
#endif

void cbc_encrypt(const unsigned char *in, unsigned char *out,
		     const unsigned long length, const KEY *key,
		     unsigned char *ivec, const int enc) 
{
	unsigned long n;
	unsigned long len = length;
	unsigned char tmp[BLOCKSIZE];

	if (enc) {
		while (len >= BLOCKSIZE) {
			unsigned int *t0 = (unsigned int*)tmp;
			unsigned int *i0 = (unsigned int*)in;
			unsigned int *v0 = (unsigned int*)ivec;
			// mmx could help here, even better if AES_encrypt would
			// mmx registers as i/o
			// (todo for the C version)
			t0 [0] = i0[0]^v0[0];
			t0 [1] = i0[1]^v0[1];
#if BLOCKSIZE == 16
			t0 [2] = i0[2]^v0[2];
			t0 [3] = i0[3]^v0[3];
#endif
			ENCRYPT (tmp, out, key);
			__builtin_memcpy (ivec, out, BLOCKSIZE);
			len -= BLOCKSIZE;
			in  += BLOCKSIZE;
			out += BLOCKSIZE;
		}
		if (len) {
			for(n=0; n < len; ++n)
				tmp[n] = in[n] ^ ivec[n];
			for(n=len; n < BLOCKSIZE; ++n)
				tmp[n] = ivec[n];
			ENCRYPT(tmp, tmp, key);
			__builtin_memcpy(out, tmp, BLOCKSIZE);
			__builtin_memcpy(ivec, tmp, BLOCKSIZE);
		}			
	} else {
		while (len >= BLOCKSIZE) {
			__builtin_memcpy(tmp, in, BLOCKSIZE);
			DECRYPT(in, out, key);
			unsigned int *o0 = (unsigned int*)out;
			unsigned int *v0 = (unsigned int*)ivec;
			o0 [0] ^= v0 [0];
			o0 [1] ^= v0 [1];
#if BLOCKSIZE == 16
			o0 [2] ^= v0 [2];
			o0 [3] ^= v0 [3];
#endif
			__builtin_memcpy(ivec, tmp, BLOCKSIZE);
			len -= BLOCKSIZE;
			in  += BLOCKSIZE;
			out += BLOCKSIZE;
		}
		if (len) {
			__builtin_memcpy(tmp, in, BLOCKSIZE);
			DECRYPT(tmp, tmp, key);
			for(n=0; n < len; ++n)
				out[n] = tmp[n] ^ ivec[n];
			__builtin_memcpy(ivec, tmp, BLOCKSIZE);
		}			
	}
}
