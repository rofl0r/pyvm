const unsigned short c1 = 52845, c2 = 22719;

void encrypt (unsigned int key, const unsigned char in[], unsigned char out[], unsigned int len)
{
	unsigned short r = key;
	unsigned int i;
	unsigned char cipher;

	for (i = 0; i < len; i++) {
		cipher = out [i] = in [i] ^ (r >> 8);
		r = (cipher + r) * c1 + c2;
	}
}

void decrypt (unsigned int key, const unsigned char in[], unsigned char out[], unsigned int len)
{
	unsigned short r = key;
	unsigned int i;
	unsigned char cipher;

	for (i = 0; i < len; i++) {
		cipher = in [i];
		out [i] = cipher ^ (r >> 8);
		r = (cipher + r) * c1 + c2;
	}
}

const char ABI [] =
"- encrypt issi\n"
"- decrypt issi\n"
;
