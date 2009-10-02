/*
 *  String XOR
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

void strxor (const char in[], const char pad[], char out[], unsigned int len)
{
	unsigned int i;
	for (i = 0; i < len; i++)
		out [i] = in [i] ^ pad [i];
}

const char ABI [] = "- strxor sssi";
