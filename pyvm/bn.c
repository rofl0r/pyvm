/*
 *  Low level positive big integer number arithmetic operations
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

/* a[] + b[], and b >= a
 * sizeof (r) == sizeof (b) + 1
 */

typedef unsigned int uint;

static void bn_add (const uint *a, uint na, const uint *b, uint nb, uint *r)
{
	uint i;
	uint carry = 0, r0;

	for (i = 0; i < nb; i++)
		r [i] = b [i];
	r [i] = 0;
	for (i = 0; i < na; i++) {
		r0 = r [i];
		r [i] += a [i] + carry;
		carry = (r [i] < r0);
	}
	if (carry) while (1) {
		r0 = r [i];
		r [i] += 1;
		if (r [i] > r0) break;
		++i;
	}
}

/* a[] - b[]
 * Where: a > 0, b > 0, a > b
 * sizeof (r) == sizeof (a)
 */
static void bn_sub (const uint *a, uint na, const uint *b, uint nb, uint *r)
{
	uint i;
	long long tmp;
	int borrow = 0;

	for (i = 0; i < nb; i++) {
		tmp = (ll) a [i] - (ll) b [i] - borrow;
		if ((borrow = tmp < 0)) r [i] = (tmp + 0x100000000LL) & 0xffffffff;
		else r [i] = tmp;
	}
	for (; i < na && borrow; i++) {
		tmp = (ll) a [i] - (ll) borrow;
		if ((borrow = tmp < 0)) r [i] = -tmp;
		else r [i] = tmp;
	}
	for (;i < na; i++)
		r [i] = a [i];
}

/* a[] * b[], preferrably a < b
 * sizeof (r) == sizeof (a) + sizeof (b)
 */
static void multiply_classic (const unsigned int *a, int na,
			      const unsigned int *b, int nb, unsigned int *r)
{
	int i, j, k = 0;

	for (i = 0, j = na + nb; i < j; i++)
		r [i] = 0;

	for (i = 0; i < na; i++) {
		unsigned long long carry = 0;
		for (j = 0; j < nb; j++) {
			k = i + j;
			carry += r [k] + (ull) a [i] * b [j];
			r [k] = carry;
			carry >>= 32;
		}
		if (carry)
			r [k + 1] += (unsigned int) carry;
	}
}

/* Squaring, HAC 14.16
This doesn't work because a long-long is not enough for `tmp`.
We need more bits!
*/
#if 0
static void square (const unsigned int *x, int n, unsigned int *r)
{
//fprintf (stderr, "SQUARE\n");
	int i, j;
	unsigned long long tmp;
	unsigned long long carry;

	for (i = 0, j = 2 * n; i < j; i++)
		r [i] = 0;

	for (i = 0; i < n; i++) {
		tmp = r [2 * i] + (ull) x [i] * x [i];
		r [2 * i] = tmp;
		carry = tmp >> 32;
		for (j = i + 1; j < n; j++) {
			// OVERFLOWS 64bits!!
			tmp = r [i + j] + 2 * ((ull) x [j]) * x [i] + carry;
			r [i + j] = tmp;
			carry = tmp >> 32;
		}
		r [i + n] = carry;
	}
}
#endif

static void bn_multiply (const unsigned int *a, int na, const unsigned int *b, int nb, unsigned int *r)
{
//if (a==b)
//return square (a, na, r);
	return multiply_classic (a, na, b, nb, r);
}

static int bitcount (const unsigned int *a, int na)
{
	int i, b, j;

	for (i = na - 1; i >= 0 && !a [i]; i--);

	if (i < 0)
		return 0;

	b = 0;
	for (j = 0; j < 32; j++) {
		if (!(~b & a [i]))
			break;
		b = (b << 1) | 1;
	}

	return i * 32 + j;
}

#if 0
/* division, todo */
// inplace double/half with shifts

/* inplace a *= 2
 * must have space for an extra digit
 */
static int twice (unsigned int *a, int na)
{
	int i = na - 1;

	for (; i >= 0; i--) {
		if (a [i] & 0x80000000)
			a [i+1] |= 1;
		a [i] <<= 1;
	}

	return a [na] ? na + 1 : na;
}

/* inplace a /= 2
 */
static int half (unsigned int *a, int na)
{
	int i;
	for (i = 0; i < na - 1; i++)
		a [i] = (a [i] >> 1) | (a [i + 1] << 31);
	return (a [i] >>= 1) ? na : na - 1;
}

static bool lessthan (const unsigned int *a, int na, const unsigned int *b, int nb)
{
	if (nb > na) return 1;
	if (na < nb) return 0;
	for (int i = na - 1; i >= 0; i--)
		if (a [i] != b [i]) return a [i] < b [i];
	return 0;
}

static int size_of (const unsigned int *a, int na)
{
	while (na && !a [na - 1]) --na;
	return na;
}
#endif
