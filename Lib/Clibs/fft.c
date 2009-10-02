/*
 *  Spectrum visualization
 *
 *  Copyright (c) 2008 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

/*
 Right now, we do FFT which produces something like "1024" frequencies and then
 we use the `bandize` function to convert this to something like "16" bands,
 logarithmically (200-400, 400-800, 800-1600, etc).

 Perhaps it would make sense to use "16" dsp filters instead and measure their
 level every "2048" input samples? hm.
*/

extern double sqrt (double);

static inline double sqr (double d)
{
	return d * d;
}

void fft (unsigned int N, const short in[], int out[], const int bitrevtab[], const double costab[],
	     const double sintab[], double real[], double imag[])
{
	double wr, wi, wpr, wpi, twr, tempr, tempi;
	unsigned int i, m, j;

	for (i = 0; i < N; i++) {
		real [i] = in [bitrevtab [i]];
		imag [i] = 0;
	}

	unsigned int dftsize, t, d2;

	for (t = 0, dftsize = 2; dftsize <= N; dftsize <<= 1, t++) {
		d2 = dftsize / 2;
		wpr = costab [t];
		wpi = sintab [t];
		wr = 1.0;
		wi = 0.0;
		for (m = 0; m < d2; m++) {
			for (i = m; i < N - d2; i += dftsize) {
				j = i + d2;
				tempr = wr * real [j] - wi * imag [j];
				tempi = wr * imag [j] + wi * real [j];
				real [j] = real [i] - tempr;
				imag [j] = imag [i] - tempi;
				real [i] += tempr;
				imag [i] += tempi;
			}
			twr = wr;
			wr = wr * wpr - wi * wpi;
			wi = wi * wpr + twr * wpi;
		}
	}

	for (i = 0; i < N/2; i++)
		out [i] = sqrt (sqr (real [i]) + sqr (imag [i]));
}

void bandize (const int spectrum[], int out[], const int bandsum[], unsigned int n)
{
	unsigned int i, j, s, x, sum;

	s = bandsum [0];
	for (i = 0; i < n; i++) {
		x = bandsum [i+1];
		if (!x) {
			out [i] = (spectrum [s] + spectrum [s + 1]) / 2;
			continue;
		}
		sum = 0;
		for (j = 0; j < x; j++)
			sum += spectrum [s++];
		out [i] = sum;
	}
}

const char ABI [] =
"- fft!		ip16p32p32pdpdpdpd	\n"
"- bandize	p32p32p32i		\n"
;
