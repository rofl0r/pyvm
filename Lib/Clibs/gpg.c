long crc24 (unsigned char *octets, unsigned int len)
{
	long crc = 0xB704CEL;
	int i;
	while (len--) {
		crc ^= (*octets++) << 16;
		for (i = 0; i < 8; i++) {
			crc <<= 1;
			if (crc & 0x1000000)
				crc ^= 0x1864CFBL;
		}
	}
	return crc & 0xFFFFFFL;
}

unsigned short int csum (const unsigned char *str, unsigned int offset, unsigned int len)
{
	unsigned short int a = 0;
	str += offset;

	while (len--)
		a += *str++;

	return a;
}

const char ABI [] =
"i crc24	si\n"
"i csum		sii\n"
;
