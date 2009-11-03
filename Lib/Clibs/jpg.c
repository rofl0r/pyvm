/*
 *  JPEG Decoder
 *
 *  Copyright (C) 2008 Stelios Xanthakis
 *   This code is largely based on TonyJpegDecoder.py by Dr. Tony Lin,
 *   on the official IJG jpeglib sources and xpdf:Stream.cc
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

#include <stdio.h>
#include "rgb.h"
#define BPP16

#define unlikely(X) __builtin_expect (X, 0)

/******************************************************************************

  There are three popular types of jpeg data supported:

  1) each tile is 8x8 pixels.  It contains 3 64byte idct blocks
     one for Y, Cb and Cr. No upsampling is required.

  2) each tile is 16x16 pixels.  It contains 6 64byte idct blocks
     four Y and one Cb and one Cr.  Upsampling is required.

  3) each tile is 16x8 pixels.  It contains 4 64byte idct blocks.
     two Y and one Cb, Cr.  Simple upsampling required.

  The vast majority of jpegs on the web fall into the above
  categories (sans grayscale ones, often found in PDFs).

  A jpeg can be progressive or not. This decoder does not support
  progressive jpegs yet.

  This implementation is suitable for viewing jpegs on a computer screen
  where minor (tiny) differences are impossible to notice.  For photographic
  image processing, jpeglib should be preferred, although in this case
  one would better use a lossless alternative.

  For that, maximum speed routines are used.  The output should be the
  same as with djpeg -fast.  Right now there is a small error which we
  try to keep less than 2%.

  A "highest quality" mode will require:
	- proper idct (not idctfast)
	- fancy upsampling

  This code also supports downscaling at 1/8, for fast thumbnail generation.

  The special macro "TRANSPOSED".  If defined, it means that the jpeg zigzag
  is altered to provide values which are ordered suitably for the specific
  IDCT routine.  Note that the quantization tables must also be properly
  transposed when passed to this library.

 *****************************************************************************/

/******************************************************************************
 * YCbCr to RGB conversion _and_ upsampling
 ******************************************************************************/

static const short int CrToR [] =
{
	 -179, -178, -177, -175, -174, -172, -171, -170, -168, -167, -165, -164, 
	 -163, -161, -160, -158, -157, -156, -154, -153, -151, -150, -149, -147, 
	 -146, -144, -143, -142, -140, -139, -137, -136, -135, -133, -132, -130, 
	 -129, -128, -126, -125, -123, -122, -121, -119, -118, -116, -115, -114, 
	 -112, -111, -109, -108, -107, -105, -104, -102, -101, -100, -98, -97, 
	 -95, -94, -93, -91, -90, -88, -87, -86, -84, -83, -81, -80, 
	 -79, -77, -76, -74, -73, -72, -70, -69, -67, -66, -64, -63, 
	 -62, -60, -59, -57, -56, -55, -53, -52, -50, -49, -48, -46, 
	 -45, -43, -42, -41, -39, -38, -36, -35, -34, -32, -31, -29, 
	 -28, -27, -25, -24, -22, -21, -20, -18, -17, -15, -14, -13, 
	 -11, -10, -8, -7, -6, -4, -3, -1, 0, 1, 3, 4, 
	 6, 7, 8, 10, 11, 13, 14, 15, 17, 18, 20, 21, 
	 22, 24, 25, 27, 28, 29, 31, 32, 34, 35, 36, 38, 
	 39, 41, 42, 43, 45, 46, 48, 49, 50, 52, 53, 55, 
	 56, 57, 59, 60, 62, 63, 64, 66, 67, 69, 70, 72, 
	 73, 74, 76, 77, 79, 80, 81, 83, 84, 86, 87, 88, 
	 90, 91, 93, 94, 95, 97, 98, 100, 101, 102, 104, 105, 
	 107, 108, 109, 111, 112, 114, 115, 116, 118, 119, 121, 122, 
	 123, 125, 126, 128, 129, 130, 132, 133, 135, 136, 137, 139, 
	 140, 142, 143, 144, 146, 147, 149, 150, 151, 153, 154, 156, 
	 157, 158, 160, 161, 163, 164, 165, 167, 168, 170, 171, 172, 
	 174, 175, 177, 178, 
};

static const short int CbToB [] =
{
	 -227, -225, -223, -222, -220, -218, -216, -214, -213, -211, -209, -207, 
	 -206, -204, -202, -200, -198, -197, -195, -193, -191, -190, -188, -186, 
	 -184, -183, -181, -179, -177, -175, -174, -172, -170, -168, -167, -165, 
	 -163, -161, -159, -158, -156, -154, -152, -151, -149, -147, -145, -144, 
	 -142, -140, -138, -136, -135, -133, -131, -129, -128, -126, -124, -122, 
	 -120, -119, -117, -115, -113, -112, -110, -108, -106, -105, -103, -101, 
	 -99, -97, -96, -94, -92, -90, -89, -87, -85, -83, -82, -80, 
	 -78, -76, -74, -73, -71, -69, -67, -66, -64, -62, -60, -58, 
	 -57, -55, -53, -51, -50, -48, -46, -44, -43, -41, -39, -37, 
	 -35, -34, -32, -30, -28, -27, -25, -23, -21, -19, -18, -16, 
	 -14, -12, -11, -9, -7, -5, -4, -2, 0, 2, 4, 5, 
	 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 25, 27, 
	 28, 30, 32, 34, 35, 37, 39, 41, 43, 44, 46, 48, 
	 50, 51, 53, 55, 57, 58, 60, 62, 64, 66, 67, 69, 
	 71, 73, 74, 76, 78, 80, 82, 83, 85, 87, 89, 90, 
	 92, 94, 96, 97, 99, 101, 103, 105, 106, 108, 110, 112, 
	 113, 115, 117, 119, 120, 122, 124, 126, 128, 129, 131, 133, 
	 135, 136, 138, 140, 142, 144, 145, 147, 149, 151, 152, 154, 
	 156, 158, 159, 161, 163, 165, 167, 168, 170, 172, 174, 175, 
	 177, 179, 181, 183, 184, 186, 188, 190, 191, 193, 195, 197, 
	 198, 200, 202, 204, 206, 207, 209, 211, 213, 214, 216, 218, 
	 220, 222, 223, 225, 
};

static const int CrToG [] =
{
	 5990656, 5943854, 5897052, 5850250, 5803448, 5756646, 5709844, 5663042, 
	 5616240, 5569438, 5522636, 5475834, 5429032, 5382230, 5335428, 5288626, 
	 5241824, 5195022, 5148220, 5101418, 5054616, 5007814, 4961012, 4914210, 
	 4867408, 4820606, 4773804, 4727002, 4680200, 4633398, 4586596, 4539794, 
	 4492992, 4446190, 4399388, 4352586, 4305784, 4258982, 4212180, 4165378, 
	 4118576, 4071774, 4024972, 3978170, 3931368, 3884566, 3837764, 3790962, 
	 3744160, 3697358, 3650556, 3603754, 3556952, 3510150, 3463348, 3416546, 
	 3369744, 3322942, 3276140, 3229338, 3182536, 3135734, 3088932, 3042130, 
	 2995328, 2948526, 2901724, 2854922, 2808120, 2761318, 2714516, 2667714, 
	 2620912, 2574110, 2527308, 2480506, 2433704, 2386902, 2340100, 2293298, 
	 2246496, 2199694, 2152892, 2106090, 2059288, 2012486, 1965684, 1918882, 
	 1872080, 1825278, 1778476, 1731674, 1684872, 1638070, 1591268, 1544466, 
	 1497664, 1450862, 1404060, 1357258, 1310456, 1263654, 1216852, 1170050, 
	 1123248, 1076446, 1029644, 982842, 936040, 889238, 842436, 795634, 
	 748832, 702030, 655228, 608426, 561624, 514822, 468020, 421218, 
	 374416, 327614, 280812, 234010, 187208, 140406, 93604, 46802, 
	 0, -46802, -93604, -140406, -187208, -234010, -280812, -327614, 
	 -374416, -421218, -468020, -514822, -561624, -608426, -655228, -702030, 
	 -748832, -795634, -842436, -889238, -936040, -982842, -1029644, -1076446, 
	 -1123248, -1170050, -1216852, -1263654, -1310456, -1357258, -1404060, -1450862, 
	 -1497664, -1544466, -1591268, -1638070, -1684872, -1731674, -1778476, -1825278, 
	 -1872080, -1918882, -1965684, -2012486, -2059288, -2106090, -2152892, -2199694, 
	 -2246496, -2293298, -2340100, -2386902, -2433704, -2480506, -2527308, -2574110, 
	 -2620912, -2667714, -2714516, -2761318, -2808120, -2854922, -2901724, -2948526, 
	 -2995328, -3042130, -3088932, -3135734, -3182536, -3229338, -3276140, -3322942, 
	 -3369744, -3416546, -3463348, -3510150, -3556952, -3603754, -3650556, -3697358, 
	 -3744160, -3790962, -3837764, -3884566, -3931368, -3978170, -4024972, -4071774, 
	 -4118576, -4165378, -4212180, -4258982, -4305784, -4352586, -4399388, -4446190, 
	 -4492992, -4539794, -4586596, -4633398, -4680200, -4727002, -4773804, -4820606, 
	 -4867408, -4914210, -4961012, -5007814, -5054616, -5101418, -5148220, -5195022, 
	 -5241824, -5288626, -5335428, -5382230, -5429032, -5475834, -5522636, -5569438, 
	 -5616240, -5663042, -5709844, -5756646, -5803448, -5850250, -5897052, -5943854, 
};

static const int CbToG [] =
{
	 2919680, 2897126, 2874572, 2852018, 2829464, 2806910, 2784356, 2761802, 
	 2739248, 2716694, 2694140, 2671586, 2649032, 2626478, 2603924, 2581370, 
	 2558816, 2536262, 2513708, 2491154, 2468600, 2446046, 2423492, 2400938, 
	 2378384, 2355830, 2333276, 2310722, 2288168, 2265614, 2243060, 2220506, 
	 2197952, 2175398, 2152844, 2130290, 2107736, 2085182, 2062628, 2040074, 
	 2017520, 1994966, 1972412, 1949858, 1927304, 1904750, 1882196, 1859642, 
	 1837088, 1814534, 1791980, 1769426, 1746872, 1724318, 1701764, 1679210, 
	 1656656, 1634102, 1611548, 1588994, 1566440, 1543886, 1521332, 1498778, 
	 1476224, 1453670, 1431116, 1408562, 1386008, 1363454, 1340900, 1318346, 
	 1295792, 1273238, 1250684, 1228130, 1205576, 1183022, 1160468, 1137914, 
	 1115360, 1092806, 1070252, 1047698, 1025144, 1002590, 980036, 957482, 
	 934928, 912374, 889820, 867266, 844712, 822158, 799604, 777050, 
	 754496, 731942, 709388, 686834, 664280, 641726, 619172, 596618, 
	 574064, 551510, 528956, 506402, 483848, 461294, 438740, 416186, 
	 393632, 371078, 348524, 325970, 303416, 280862, 258308, 235754, 
	 213200, 190646, 168092, 145538, 122984, 100430, 77876, 55322, 
	 32768, 10214, -12340, -34894, -57448, -80002, -102556, -125110, 
	 -147664, -170218, -192772, -215326, -237880, -260434, -282988, -305542, 
	 -328096, -350650, -373204, -395758, -418312, -440866, -463420, -485974, 
	 -508528, -531082, -553636, -576190, -598744, -621298, -643852, -666406, 
	 -688960, -711514, -734068, -756622, -779176, -801730, -824284, -846838, 
	 -869392, -891946, -914500, -937054, -959608, -982162, -1004716, -1027270, 
	 -1049824, -1072378, -1094932, -1117486, -1140040, -1162594, -1185148, -1207702, 
	 -1230256, -1252810, -1275364, -1297918, -1320472, -1343026, -1365580, -1388134, 
	 -1410688, -1433242, -1455796, -1478350, -1500904, -1523458, -1546012, -1568566, 
	 -1591120, -1613674, -1636228, -1658782, -1681336, -1703890, -1726444, -1748998, 
	 -1771552, -1794106, -1816660, -1839214, -1861768, -1884322, -1906876, -1929430, 
	 -1951984, -1974538, -1997092, -2019646, -2042200, -2064754, -2087308, -2109862, 
	 -2132416, -2154970, -2177524, -2200078, -2222632, -2245186, -2267740, -2290294, 
	 -2312848, -2335402, -2357956, -2380510, -2403064, -2425618, -2448172, -2470726, 
	 -2493280, -2515834, -2538388, -2560942, -2583496, -2606050, -2628604, -2651158, 
	 -2673712, -2696266, -2718820, -2741374, -2763928, -2786482, -2809036, -2831590, 
};

static const unsigned char rgb_limit [] =
{
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 
	 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 
	 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 
	 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 
	 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 
	 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 
	 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 
	 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 
	 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 
	 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 
	 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 
	 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 
	 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 
	 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 
	 250, 251, 252, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	 255, 255, 255, 255, 255, 255, 255, 255 
};

struct rgb {
	unsigned char r, g, b;
};

typedef struct
{
	const unsigned char *YCbCr;
	unsigned int mcuw, mcuh;
	unsigned int blocks_in_mcu;
	union {
		struct rgb *out;
		unsigned short *out16;
	};
} yctx;

const int sizeof_yctx = sizeof (yctx);

void init_yctx (yctx *c, unsigned int mcuw, unsigned int mcuh, unsigned int blocks_in_mcu)
{
	c->mcuw = mcuw;
	c->mcuh = mcuh;
	c->blocks_in_mcu = blocks_in_mcu;
}

static inline unsigned char LIM (int x)
{
	return rgb_limit [x + 128];
}

/* table for up-sampling in the 16x16 case */
const static struct h2v2s {
	unsigned int outp, yp;
} h2v2 [] = {
	    {0,0}, {2,2}, {4,4}, {6,6}, {8,64}, {10,66}, {12,68}, {14,70}, {32,16}, {34,18},
	  {36,20}, {38,22}, {40,80}, {42,82}, {44,84}, {46,86}, {64,32}, {66,34}, {68,36},
	  {70,38}, {72,96}, {74,98}, {76,100}, {78,102}, {96,48}, {98,50}, {100,52}, {102,54},
	{104,112}, {106,114}, {108,116}, {110,118}, {128,128}, {130,130}, {132,132}, {134,134},
	{136,192}, {138,194}, {140,196}, {142,198}, {160,144}, {162,146}, {164,148}, {166,150},
	{168,208}, {170,210}, {172,212}, {174,214}, {192,160}, {194,162}, {196,164}, {198,166},
	{200,224}, {202,226}, {204,228}, {206,230}, {224,176}, {226,178}, {228,180}, {230,182},
	{232,240}, {234,242}, {236,244}, {238,246}
};

static const unsigned int h2v1 [] =
{
	0, 1, 2, 3, 4, 5, 6, 7, 64, 65, 66, 67, 68, 69, 70, 71, 8, 9, 10, 11, 12, 13, 14, 15, 
	72, 73, 74, 75, 76, 77, 78, 79, 16, 17, 18, 19, 20, 21, 22, 23, 80, 81, 82, 83, 84, 
	85, 86, 87, 24, 25, 26, 27, 28, 29, 30, 31, 88, 89, 90, 91, 92, 93, 94, 95, 32, 33, 
	34, 35, 36, 37, 38, 39, 96, 97, 98, 99, 100, 101, 102, 103, 40, 41, 42, 43, 44, 45, 
	46, 47, 104, 105, 106, 107, 108, 109, 110, 111, 48, 49, 50, 51, 52, 53, 54, 55, 112, 
	113, 114, 115, 116, 117, 118, 119, 56, 57, 58, 59, 60, 61, 62, 63, 120, 121, 122, 123, 
	124, 125, 126, 127 
};

// 8x8 3 blocks
static void YCbCr2RGBEx_b3 (const yctx *c)
{
	const unsigned char *YCbCr = c->YCbCr;
	struct rgb *out = c->out;

	unsigned int j;
	unsigned char y, cb, cr;
	unsigned int cboffs = 64;
	unsigned int croffs = cboffs + 64;
	int GG;

	for (j = 0; j < 64; j++) {
		y = YCbCr [j];
		cb = YCbCr [cboffs++];
		cr = YCbCr [croffs++];
		GG = (CbToG [cb] + CrToG [cr]) >> 16;
		out->r = LIM (y + CrToR [cr]);
		out->b = LIM (y + CbToB [cb]);
		out++->g = LIM (y + GG);
	}
}

// 16x8 4 blocks
static void YCbCr2RGBEx_b4 (const yctx *c)
{
	const unsigned char *YCbCr = c->YCbCr;
	struct rgb *out = c->out;

	unsigned char y, cb, cr;
	unsigned int cboffs = 128;
	unsigned int croffs = cboffs + 64;
	int GG;

	unsigned int i, ii;
	for (ii = i = 0; ii < 64; ii++) {
		cb = YCbCr [cboffs + ii];
		cr = YCbCr [croffs + ii];
		GG = (CbToG [cb] + CrToG [cr]) >> 16;
		y = YCbCr [h2v1 [i++]];
		out->r = LIM (y + CrToR [cr]);
		out->b = LIM (y + CbToB [cb]);
		out++->g = LIM (y + GG);
		y = YCbCr [h2v1 [i++]];
		out->r = LIM (y + CrToR [cr]);
		out->b = LIM (y + CbToB [cb]);
		out++->g = LIM (y + GG);
	}
}

// 16x16 6 blocks
static void YCbCr2RGBEx_b6 (const yctx *c)
{
	const unsigned char *YCbCr = c->YCbCr;
	struct rgb *out = c->out;

	unsigned int bn;
	unsigned char y, cb, cr;
	unsigned int cboffs = 64 * 4;
	unsigned int croffs = cboffs + 64;
	int GG;
	// blocks in mcu 6, mcusize=16.  No "fancy" upsampling
	unsigned int oo, yp;
	const struct h2v2s *hp = h2v2;
	short int Cb, Cr;
	for (bn = 0; bn < 64; bn++, hp++) {
		cb = YCbCr [cboffs++];
		cr = YCbCr [croffs++];
		GG = (CbToG [cb] + CrToG [cr]) >> 16;
		Cb = CbToB [cb];
		Cr = CrToR [cr];
		oo = hp->outp;
		yp = hp->yp;

		y = YCbCr [yp];
		out [oo].r = LIM (y + Cr);
		out [oo].b = LIM (y + Cb);
		out [oo].g = LIM (y + GG);
		++oo;
		y = YCbCr [yp + 1];
		out [oo].r = LIM (y + Cr);
		out [oo].b = LIM (y + Cb);
		out [oo].g = LIM (y + GG);
		yp += 8;
		oo += 15;
		y = YCbCr [yp];
		out [oo].r = LIM (y + Cr);
		out [oo].b = LIM (y + Cb);
		out [oo].g = LIM (y + GG);
		++oo;
		y = YCbCr [yp + 1];
		out [oo].r = LIM (y + Cr);
		out [oo].b = LIM (y + Cb);
		out [oo].g = LIM (y + GG);
	}
}

/******************************************************************************
 * IDCT
 *  (this is the method in jidctfst.c)
 ******************************************************************************/

static const unsigned char idct_range_limit [] =
{
	128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
	144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158,
	159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173,
	174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188,
	189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203,
	204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218,
	219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233,
	234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248,
	249, 250, 251, 252, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4,
	5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
	35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
	50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64,
	65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
	80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94,
	95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
	110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124,
	125, 126, 127
};

static inline unsigned char OUTV (int x)
{
	return idct_range_limit [(x >> 5) & 1023];
}

static inline int MUL (int var, int con)
{
	return var * con >> 8;
}

typedef struct
{
	const short *coeff;
	unsigned char *out;
	unsigned short *qtblY, *qtblCbCr;
	unsigned int blocks_in_mcu;
} idctctx;

const int sizeof_idct = sizeof (idctctx);

void init_idct (idctctx *c, int blocks_in_mcu, unsigned short *qtblY, unsigned short *qtblCbCr)
{
	c->blocks_in_mcu = blocks_in_mcu;
	c->qtblY = qtblY;
	c->qtblCbCr = qtblCbCr;
}

static void idct (const short *coeff, unsigned char *out, const unsigned short *quant)
{
#define FIX141 362
#define FIX184 473
#define FIX108 277
#define FIX261 669
	unsigned int ctr;
	unsigned int ip, wp, qp, op;
short
	int workspace [64];
short
	int tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp10, tmp11, tmp12, tmp13;
short
	int z10, z11, z12, z13, z5;

	wp = ip = qp = 0;
	for (ctr = 8; ctr; --ctr) {
#ifndef TRANSPOSED
		if (coeff [ip + 8] || coeff [ip + 16] || coeff [ip + 24] || coeff [ip + 32] ||
		    coeff [ip + 40] || coeff [ip + 48] || coeff [ip + 56]) goto slow;
#else
		unsigned int *testz = (unsigned int*) &coeff [ip + 2];
		if (testz [0] || testz [1] || testz [2] || coeff [ip + 1])
			goto slow;
#endif

		workspace [wp + 0 * 8] = workspace [wp + 1 * 8] =
		workspace [wp + 2 * 8] = workspace [wp + 3 * 8] =
		workspace [wp + 4 * 8] = workspace [wp + 5 * 8] =
		workspace [wp + 6 * 8] = workspace [wp + 7 * 8] = coeff [ip] * quant [qp];

#ifndef TRANSPOSED
		++ip;
		++qp;
#else
		ip += 8;
		qp += 8;
#endif
		++wp;
		continue;
	slow:
		// even
#ifndef TRANSPOSED
		tmp0 = coeff [ip] * quant [qp];
		tmp1 = coeff [ip + 2 * 8] * quant [qp + 2 * 8];
		tmp2 = coeff [ip + 4 * 8] * quant [qp + 4 * 8];
		tmp3 = coeff [ip + 6 * 8] * quant [qp + 6 * 8];
#else
		tmp0 = coeff [ip] * quant [qp];
		tmp1 = coeff [ip + 2] * quant [qp + 2];
		tmp2 = coeff [ip + 4] * quant [qp + 4];
		tmp3 = coeff [ip + 6] * quant [qp + 6];
#endif
		tmp10 = tmp0 + tmp2;
		tmp11 = tmp0 - tmp2;
		tmp13 = tmp1 + tmp3;
		tmp12 = MUL (tmp1 - tmp3, FIX141) - tmp13;

		tmp0 = tmp10 + tmp13;
		tmp3 = tmp10 - tmp13;
		tmp1 = tmp11 + tmp12;
		tmp2 = tmp11 - tmp12;

		// odd
#ifndef TRANSPOSED
		tmp4 = coeff [ip + 1 * 8] * quant [qp + 1 * 8];
		tmp5 = coeff [ip + 3 * 8] * quant [qp + 3 * 8];
		tmp6 = coeff [ip + 5 * 8] * quant [qp + 5 * 8];
		tmp7 = coeff [ip + 7 * 8] * quant [qp + 7 * 8];
#else
		tmp4 = coeff [ip + 1] * quant [qp + 1];
		tmp5 = coeff [ip + 3] * quant [qp + 3];
		tmp6 = coeff [ip + 5] * quant [qp + 5];
		tmp7 = coeff [ip + 7] * quant [qp + 7];
#endif
		z13 = tmp6 + tmp5;
		z10 = tmp6 - tmp5;
		z11 = tmp4 + tmp7;
		z12 = tmp4 - tmp7;
		tmp7 = z11 + z13;
		tmp11 = MUL (z11 - z13, FIX141);
		z5 = MUL (z10 + z12, FIX184);
		tmp10 = MUL (z12, FIX108) - z5;
		tmp12 = MUL (z10, -FIX261) + z5;
		tmp6 = tmp12 - tmp7;
		tmp5 = tmp11 - tmp6;
		tmp4 = tmp10 + tmp5;

		workspace [wp + 0 * 8] = tmp0 + tmp7;
		workspace [wp + 7 * 8] = tmp0 - tmp7;
		workspace [wp + 1 * 8] = tmp1 + tmp6;
		workspace [wp + 6 * 8] = tmp1 - tmp6;
		workspace [wp + 2 * 8] = tmp2 + tmp5;
		workspace [wp + 5 * 8] = tmp2 - tmp5;
		workspace [wp + 4 * 8] = tmp3 + tmp4;
		workspace [wp + 3 * 8] = tmp3 - tmp4;

#ifndef TRANSPOSED
		++ip;
		++qp;
#else
		ip += 8;
		qp += 8;
#endif
		++wp;
	}

	wp = 0;
	for (op = 0; op < 64; op += 8) {
		unsigned int *testz = (unsigned int*) &workspace [wp + 2];
		if (testz [0] || testz [1] || testz [2] || workspace [wp + 1])
			goto slow2;

		out [op + 0] = out [op + 1] =
		out [op + 2] = out [op + 3] =
		out [op + 4] = out [op + 5] =
		out [op + 6] = out [op + 7] = OUTV (workspace [wp]);

		wp += 8;
		continue;
	slow2:

		// even
		tmp10 = workspace [wp] + workspace [wp + 4];
		tmp11 = workspace [wp] - workspace [wp + 4];
		tmp13 = workspace [wp + 2] + workspace [wp + 6];
		tmp12 = MUL (workspace [wp + 2] - workspace [wp + 6], FIX141) - tmp13;
		tmp0 = tmp10 + tmp13;
		tmp3 = tmp10 - tmp13;
		tmp1 = tmp11 + tmp12;
		tmp2 = tmp11 - tmp12;

		// odd
		z13 = workspace [wp + 5] + workspace [wp + 3];
		z10 = workspace [wp + 5] - workspace [wp + 3];
		z11 = workspace [wp + 1] + workspace [wp + 7];
		z12 = workspace [wp + 1] - workspace [wp + 7];
		tmp7 = z11 + z13;
		tmp11 = MUL (z11 - z13, FIX141);
		z5 = MUL (z10 + z12, FIX184);
		tmp10 = MUL (z12, FIX108) - z5;
		tmp12 = MUL (z10, -FIX261) + z5;
		tmp6 = tmp12 - tmp7;
		tmp5 = tmp11 - tmp6;
		tmp4 = tmp10 + tmp5;

		out [op + 0] = OUTV (tmp0 + tmp7);
		out [op + 7] = OUTV (tmp0 - tmp7);
		out [op + 1] = OUTV (tmp1 + tmp6);
		out [op + 6] = OUTV (tmp1 - tmp6);
		out [op + 2] = OUTV (tmp2 + tmp5);
		out [op + 5] = OUTV (tmp2 - tmp5);
		out [op + 4] = OUTV (tmp3 + tmp4);
		out [op + 3] = OUTV (tmp3 - tmp4);

		wp += 8;
	}
}

static void idct_mcu (const idctctx *c)
{
	unsigned int i;
	for (i = 0; i < c->blocks_in_mcu; i++) {
		const short *coeff = c->coeff + i * 64;
		unsigned char *out = c->out + i * 64;
		const unsigned short *quant = i < c->blocks_in_mcu - 2 ? c->qtblY : c->qtblCbCr;
		idct (coeff, out, quant);
	}
}

/******************************************************************************
 * Huffman Decoder
 ******************************************************************************/

static const int half [] = {
	0, 0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
	0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000
};

static inline int value_from_category (int cate, int offset)
{
	if (offset < half [cate])
		offset += (-1 << cate) + 1;
	return offset;
}

/* huffman tables */

typedef unsigned int *uptr;
typedef int *iptr;

typedef struct
{
	uptr valptr, bits, huffval, look_nbits, look_sym;
	iptr mincode, maxcode;
} hufftable;

const int sizeof_hufftable = sizeof (hufftable);

void init_hufftable (hufftable *h, iptr mincode, iptr maxcode, uptr valptr,
		     uptr bits, uptr huffval, uptr look_nbits, uptr look_sym)
{
	h->mincode	= mincode;
	h->maxcode	= maxcode;
	h->valptr	= valptr;
	h->huffval	= huffval;
	h->bits		= bits;
	h->look_nbits	= look_nbits;
	h->look_sym	= look_sym;
}

/* bit reader / decoder state */

typedef struct
{
	unsigned char *ptr;
	unsigned int left;
	unsigned int bitbuf;
	unsigned int bitbuflen;
	unsigned int restart_interval, restarts_to_go;
	int dcY, dcCb, dcCr;
	int blocks_in_mcu;
	int unread_marker;
	int restart_num, next_restart_num;
	hufftable *YDC, *YAC, *CbCrDC, *CbCrAC;
	int eof;
	/* output buffer */
	short int *coeff;
} zstate;

const int sizeof_zstate = sizeof (zstate);

void init_zstate (zstate *z, unsigned char *ptr, unsigned int offset, unsigned int left,
		  hufftable *YDC, hufftable *YAC, hufftable *CbCrDC, hufftable *CbCrAC,
		  unsigned int restart_interval, unsigned int restarts_to_go, int blocks_in_mcu)
{
	z->ptr = ptr + offset;
	z->left = left;
	z->bitbuf = z->bitbuflen = 0;
	z->unread_marker = 0;
	z->eof = 0;
	z->restart_interval = restart_interval;
	z->restarts_to_go = restarts_to_go;
	z->dcY = z->dcCr = z->dcCb = 0;
	z->blocks_in_mcu = blocks_in_mcu;
	z->restart_num = z->next_restart_num = 0;
	z->YDC = YDC;
	z->YAC = YAC;
	z->CbCrDC = CbCrDC;
	z->CbCrAC = CbCrAC;
}

static void fill_bitbuf (zstate *z)
{
	while (z->bitbuflen < 25)
		if (z->left) {
			if (z->unread_marker && z->bitbuflen)
				break;
			unsigned char uc = *z->ptr++;
			--z->left;
			if (unlikely (uc == 0xff)) {
				while (uc == 0xff) {
					uc = *z->ptr++;
					if (!z->left--)
						goto eof;
				}
				if (!uc) uc = 0xff;
				else {
					z->unread_marker = uc;
					if (z->bitbuflen)
						break;
				}
			}
			z->bitbuf = (z->bitbuf << 8) | uc;
			z->bitbuflen += 8;
		} else {
		eof:
			z->bitbuf = 0;
			z->bitbuflen = 32;
			z->left = 0;
			++z->eof;
		}
}

static inline unsigned int get_bits (zstate *z, unsigned int n)
{
	if (z->bitbuflen >= n) {
	up:
		z->bitbuflen -= n;
		return (z->bitbuf >> z->bitbuflen) & ((1 << n) - 1);
	}
	fill_bitbuf (z);
	goto up;
}

static int special_decode (zstate *z, hufftable *htbl, unsigned int n)
{
	int code = get_bits (z, n);
	while (code > htbl->maxcode [n]) {
		code <<= 1;
		code |= get_bits (z, 1);
		++n;
	}

	if (n > 16)
		return 0;

	return htbl->huffval [htbl->valptr [n] + (code - htbl->mincode [n])];
}

static int get_category (zstate *z, hufftable *htbl)
{
	if (unlikely (z->bitbuflen < 8)) {
		fill_bitbuf (z);

		if (unlikely (z->bitbuflen < 8))
			return special_decode (z, htbl, 1);
	}

	unsigned int look = (z->bitbuf >> (z->bitbuflen - 8)) & 0xff;
	unsigned int nb = htbl->look_nbits [look];

	if (nb) {
		z->bitbuflen -= nb;
		return htbl->look_sym [look];
	}

	return special_decode (z, htbl, 9);
}

static const unsigned int zigzag [] =
{
#ifndef TRANSPOSED
	 0,
	 1,  8,
	16,  9,  2,
	 3, 10, 17, 24,
	32, 25, 18, 11, 4,
	 5, 12, 19, 26, 33, 40,
	48, 41, 34, 27, 20, 13,  6,
	 7, 14, 21, 28, 35, 42, 49, 56,
	57, 50, 43, 36, 29, 22, 15,
	23, 30, 37, 44, 51, 58,
	59, 52, 45, 38, 31,
	39, 46, 53, 60,
	61, 54, 47,
	55, 62,
	63,
#else
	0, 8, 1, 2, 9, 16, 24, 17, 10, 3, 4, 11, 18, 25, 32, 40, 33,
	26, 19, 12, 5, 6, 13, 20, 27, 34, 41, 48, 56, 49, 42, 35, 28, 21,
	14, 7, 15, 22, 29, 36, 43, 50, 57, 58, 51, 44, 37, 30, 23, 31, 38,
	45, 52, 59, 60, 53, 46, 39, 47, 54, 61, 62, 55, 63,
#endif
	/* safety */
	63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63
};

static void huffman_decode (zstate *z, unsigned int i)
{
	short int *coeff = z->coeff + i * 64;
	unsigned int *p = (unsigned int*) coeff;
	unsigned int k;

	for (k = 0; k < 8; k++) {
		*p++ = 0;
		*p++ = 0;
		*p++ = 0;
		*p++ = 0;
	}

	if (unlikely (z->eof))
		return;

	hufftable *dctbl, *actbl;
	int *dc;

	if (i < z->blocks_in_mcu - 2) {
		dctbl = z->YDC;
		actbl = z->YAC;
		dc = &z->dcY;
	} else {
		dctbl = z->CbCrDC;
		actbl = z->CbCrAC;
		dc = i == z->blocks_in_mcu - 2 ? &z->dcCb : &z->dcCr;
	}

	int r, s = get_category (z, dctbl);

	if (s) {
		r = get_bits (z, s);
		s = value_from_category (s, r);
	}

	s += *dc;
	*dc = s;

	// dc coefficient
	coeff [0] = s;

	k = 1;
	while (k < 64) {
		s = get_category (z, actbl);
		r = s >> 4;
		s &= 15;

		if (s) {
			k += r;
			r = get_bits (z, s);
			s = value_from_category (s, r);
			coeff [zigzag [k]] = s;
		} else {
			if (r != 15)
				break;
			k += 15;
		}
		++k;
	}
}

static int prepare_decode (zstate*);

static void huffman_decode_mcu (zstate *z)
{
	unsigned int i;

	prepare_decode (z);
	for (i = 0; i < z->blocks_in_mcu; i++)
		huffman_decode (z, i);
	--z->restarts_to_go;
}

#define SOF0 0xc0
#define RST0 0xd0
#define RST7 0xd7

static inline unsigned char rbyte (zstate *z)
{
	if (z->left) {
		--z->left;
		return *z->ptr++;
	}
	return 0xff;
}

static int read_marker (zstate *z)
{
	return rbyte (z) == 0xff ? rbyte (z) : -1;
}

static int next_marker (zstate *z)
{
	for (;;) {
		unsigned char c = rbyte (z);
		while (c != 0xff)
			c = rbyte (z);
		do c = rbyte (z);
		while (c == 0xff && z->left);
		if (!z->left) return 0;
		if (!c) return c;
	}
}

static int prepare_decode (zstate *z)
{
	if (unlikely (z->restart_interval && !z->restarts_to_go)) {
		z->bitbuflen = 0;
		z->dcY = z->dcCr = z->dcCb = 0;
		/* read restart marker */
		if (!z->unread_marker)
			if ((z->unread_marker = read_marker (z)) == -1)
				return 3;
		if (z->unread_marker == RST0 + z->restart_num)
			z->unread_marker = 0;
		else for (;;) {
			int desired = z->next_restart_num;
			int marker = z->unread_marker;
			if (marker < SOF0) return 1;
			if (marker < RST0 || marker > RST7) return 2;

			if (marker == RST0 + ((desired + 1) & 7)
			||  marker == RST0 + ((desired + 1) & 7))
				break;

			if (marker == RST0 + ((desired - 1) & 7)
			||  marker == RST0 + ((desired - 1) & 7)) {
				z->unread_marker = next_marker (z);
				continue;
			}

			z->unread_marker = 0;
			break;
		}
		z->next_restart_num = (z->next_restart_num + 1) & 7;
		//
		z->restarts_to_go = z->restart_interval;
	}
	return 0;
}

/******************************************************************************
 * Blit tile to output
 *  It would be possible to merge the blitting into the YCbCr2RGBEx
 *  function to avoid the intermediate copy.  It'll definitely save
 *  the extra copy,  but it might be confusing for the cache and it
 *  will complicate the edge test which can be inlined and factored
 *  out at compile-time...
 ******************************************************************************/

typedef struct
{
	unsigned char *dest;
	const unsigned char *src;
	unsigned int mcuw, mcuh, width, height;
} blitctx;

const int sizeof_blit = sizeof (blitctx);

void init_blit (blitctx *b, unsigned char *dest, unsigned int mcuw,
		unsigned int mcuh, unsigned int width, unsigned int height)
{
	b->dest = dest;
	b->mcuw = mcuw;
	b->mcuh = mcuh;
	b->width = width;
	b->height = height;
}

static inline void blit (const blitctx *b, unsigned int x, unsigned int y, int edge)
{
	unsigned int mcuw = b->mcuw;
	unsigned int mcuh = b->mcuh;
	unsigned int xp = x * mcuw;
	unsigned int yp = y * mcuh;
	unsigned int nr = mcuh;
	unsigned int nc = mcuw;
	unsigned int xx, yy;
	unsigned int w = b->width, h = b->height;
	unsigned int ds = 0;

	if (edge) {
		if (nr + yp > h) nr = h - yp;
		if (nc + xp > w) nc = w - xp;
		ds = 3 * (mcuw - nc);
	}

	unsigned int nc3 = 3 * nc;
	unsigned char *dest = b->dest + 3 * (w * yp + xp);
	const unsigned char *src = b->src;

	w *= 3;
	for (yy = 0; yy < nr; yy++) {
		for (xx = 0; xx < nc; xx++) {
			*dest++ = *src++;
			*dest++ = *src++;
			*dest++ = *src++;
		}
		dest += w - nc3;
		src += ds;
	}
}

#ifdef BPP16
static inline void blit16 (const blitctx *b, unsigned int x, unsigned int y, int edge)
{
	unsigned int mcuw = b->mcuw;
	unsigned int mcuh = b->mcuh;
	unsigned int xp = x * mcuw;
	unsigned int yp = y * mcuh;
	unsigned int nr = mcuh;
	unsigned int nc = mcuw;
	unsigned int xx, yy;
	unsigned int w = b->width, h = b->height;
	unsigned int ds = 0;

	if (edge) {
		if (nr + yp > h) nr = h - yp;
		if (nc + xp > w) nc = w - xp;
		ds = 3 * (mcuw - nc);
	}

	ushort *dest = (ushort*) b->dest + (w * yp + xp);
	const unsigned char *src = b->src;

	for (yy = 0; yy < nr; yy++, dest += w, src += ds)
		for (xx = 0; xx < nc; xx++, src += 3)
			put_rgb_ushort (&dest [xx], src [0], src [1], src [2]);
}
#endif

/******************************************************************************
 * Run main jpeg decompression loop
 ******************************************************************************/

//#define TIMEIDCT

#ifdef TIMEIDCT
static long long get_ticks ()
{
	long long val;
	asm volatile ("rdtsc" : "=A" (val));
	return val;
}
#endif

void decompress_image (zstate *z, idctctx *i, yctx *c, blitctx *b)
{
	unsigned int x, y;
	unsigned int cxTile, cyTile;
	void (*YCbCr2RGBEx)(const yctx*) = c->blocks_in_mcu == 3 ? YCbCr2RGBEx_b3 :
				 c->blocks_in_mcu == 4 ? YCbCr2RGBEx_b4 : YCbCr2RGBEx_b6;

#ifdef TIMEIDCT
	long long t0, dt, tn;
	dt = tn = 0;
#endif
	/* max space for the 16x16 case */
	short int *zcoeff = __builtin_alloca (z->blocks_in_mcu * 64 * sizeof *zcoeff);
	unsigned char *iout = __builtin_alloca (z->blocks_in_mcu * 64);
	struct rgb rout [16*16];
	i->coeff = (void*) (z->coeff = zcoeff);
	i->out = (void*) (c->YCbCr = iout);
	b->src = (void*) (c->out = rout);

	cxTile = (b->width + b->mcuw - 1) / b->mcuw;
	cyTile = (b->height + b->mcuh - 1) / b->mcuh;

	for (y = 0; y < cyTile - 1; y++) {
		for (x = 0; x < cxTile - 1; x++) {
			huffman_decode_mcu (z);
#ifdef TIMEIDCT
			t0 = get_ticks ();
#endif
			idct_mcu (i);
#ifdef TIMEIDCT
			dt += get_ticks () - t0;
			++tn;
#endif
			YCbCr2RGBEx (c);
			blit (b, x, y, 0);
		}
		huffman_decode_mcu (z);
		idct_mcu (i);
		YCbCr2RGBEx (c);
		blit (b, x, y, 1);
	}
	for (x = 0; x < cxTile; x++) {
		huffman_decode_mcu (z);
		idct_mcu (i);
		YCbCr2RGBEx (c);
		blit (b, x, y, 1);
	}
#ifdef TIMEIDCT
	fprintf (stderr, "Idct avg ticks: %llu\n", dt / tn);
#endif
}

void decompress_image_bg (zstate *z, idctctx *i, yctx *c, blitctx *b)
{
	decompress_image (z, i, c, b);
}

/*#####################################
 * decoder for 16 bits per pixel output
 ######################################*/

#ifdef BPP16
void decompress_image16 (zstate *z, idctctx *i, yctx *c, blitctx *b)
{
	unsigned int x, y;
	unsigned int cxTile, cyTile;
	void (*YCbCr2RGBEx)(const yctx*) = c->blocks_in_mcu == 3 ? YCbCr2RGBEx_b3 :
				 c->blocks_in_mcu == 4 ? YCbCr2RGBEx_b4 : YCbCr2RGBEx_b6;

	/* max space for the 16x16 case */
	short int *zcoeff = __builtin_alloca (z->blocks_in_mcu * 64 * sizeof *zcoeff);
	unsigned char *iout = __builtin_alloca (z->blocks_in_mcu * 64);
	struct rgb rout [16*16];
	i->coeff = (void*) (z->coeff = zcoeff);
	i->out = (void*) (c->YCbCr = iout);
	b->src = (void*) (c->out = rout);

	cxTile = (b->width + b->mcuw - 1) / b->mcuw;
	cyTile = (b->height + b->mcuh - 1) / b->mcuh;

	for (y = 0; y < cyTile - 1; y++) {
		for (x = 0; x < cxTile - 1; x++) {
			huffman_decode_mcu (z);
			idct_mcu (i);
			YCbCr2RGBEx (c);
			blit16 (b, x, y, 0);
		}
		huffman_decode_mcu (z);
		idct_mcu (i);
		YCbCr2RGBEx (c);
		blit16 (b, x, y, 1);
	}
	for (x = 0; x < cxTile; x++) {
		huffman_decode_mcu (z);
		idct_mcu (i);
		YCbCr2RGBEx (c);
		blit16 (b, x, y, 1);
	}
}

void decompress_image16_bg (zstate *z, idctctx *i, yctx *c, blitctx *b)
{
	decompress_image16 (z, i, c, b);
}
#endif

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *  Special case. Produce a thumbnail of the image scaled down 8 times.
 *  The idct is just the first value (DC coefficient).
 *  Huffman is simplified to store only the DC.
 *  This gives about 100 thumbnails per second!
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

static inline void YCbCr2RGBEx1x1 (const yctx *c, int blocks_in_mcu, const unsigned char *YCbCr,
				   struct rgb *out)
{
	unsigned char y, cb, cr, i;
	int GG;

	if (blocks_in_mcu == 3) {
		y = YCbCr [0];
		cb = YCbCr [1];
		cr = YCbCr [2];
		GG = (CbToG [cb] + CrToG [cr]) >> 16;
		out->r = LIM (y + CrToR [cr]);
		out->b = LIM (y + CbToB [cb]);
		out->g = LIM (y + GG);
	} else if (blocks_in_mcu == 4) {
		cb = YCbCr [2];
		cr = YCbCr [3];
		GG = (CbToG [cb] + CrToG [cr]) >> 16;
		for (i = 0; i < 2; i++) {
			y = YCbCr [i];
			out->r = LIM (y + CrToR [cr]);
			out->b = LIM (y + CbToB [cb]);
			out++->g = LIM (y + GG);
		}
	} else {
		cb = YCbCr [4];
		cr = YCbCr [5];
		GG = (CbToG [cb] + CrToG [cr]) >> 16;
		for (i = 0; i < 4; i++) {
			y = YCbCr [i];
			out->r = LIM (y + CrToR [cr]);
			out->b = LIM (y + CbToB [cb]);
			out++->g = LIM (y + GG);
		}
	}
}

static inline void idct1x1 (const idctctx *ctx, unsigned int nb, unsigned int bn,
			    const short int *coeff, unsigned char *iout)
{
	const unsigned short *quant = nb < bn - 2 ? ctx->qtblY : ctx->qtblCbCr;
	iout [nb] = OUTV (coeff [nb] * quant [0]);
}

static inline void idct1x1_mcu (const idctctx *c, unsigned int bn, short int *coeff, unsigned char *iout)
{
	unsigned int i;
	for (i = 0; i < bn; i++)
		idct1x1 (c, i, bn, coeff, iout);
}

static inline void huffman_decode1x1 (zstate *z, unsigned int i, short int *coeff)
{
	hufftable *dctbl, *actbl;
	int *dc;

	if (i < z->blocks_in_mcu - 2) {
		dctbl = z->YDC;
		actbl = z->YAC;
		dc = &z->dcY;
	} else {
		dctbl = z->CbCrDC;
		actbl = z->CbCrAC;
		dc = i == z->blocks_in_mcu - 2 ? &z->dcCb : &z->dcCr;
	}

	int k, r, s = get_category (z, dctbl);

	if (s) {
		r = get_bits (z, s);
		s = value_from_category (s, r);
	}

	s += *dc;
	*dc = s;

	/* DC */
	coeff [i] = s;

	/* SKIP ACs */
	for (k = 1; k < 64; k++) {
		s = get_category (z, actbl);
		r = s >> 4;
		if (s & 15) {
			k += r;
			get_bits (z, s & 15);
		} else {
			if (r != 15)
				break;
			k += 15;
		}
	}
}

static inline void huffman_decode_mcu1x1 (zstate *z, short int *coeff)
{
	unsigned int i;

	prepare_decode (z);
	for (i = 0; i < z->blocks_in_mcu; i++)
		huffman_decode1x1 (z, i, coeff);
	--z->restarts_to_go;
}

static inline int fdiv (int a, int b)
{
	int r = a / b;
	if (a % b) r++;
	return r;
}

void decompress_image_1x1 (zstate *z, idctctx *i, yctx *c, blitctx *b)
{
	unsigned int x, y, nr, nc;
	unsigned int cxTile, cyTile;
	unsigned int mcuw = b->mcuw / 8;
	unsigned int mcuh = b->mcuh / 8;
	const unsigned char *src;
	unsigned char *dest;

	/* maximums */
	short int zcoeff [6];
	unsigned char iout [6];
	struct rgb rout [4];
	i->coeff = z->coeff = zcoeff;
	i->out = (void*) (c->YCbCr = iout);
	b->src = (void*) (c->out = rout);

	cxTile = (b->width + b->mcuw - 1) / b->mcuw;
	cyTile = (b->height + b->mcuh - 1) / b->mcuh;
	unsigned int w = fdiv (b->width, 8), h = fdiv (b->height, 8);

	if (mcuw == 1 && mcuh == 1) {
		for (y = 0; y < cyTile; y++)
			for (x = 0; x < cxTile; x++) {
				huffman_decode_mcu1x1 (z, zcoeff);
				idct1x1_mcu (i, 3, zcoeff, iout);
				YCbCr2RGBEx1x1 (c, 3, iout, rout);
				dest = b->dest + 3 * (w * y + x);
				src = (void*) rout;
				*dest++ = *src++;
				*dest++ = *src++;
				*dest++ = *src++;
			}
	} else if (mcuw == 2 && mcuh == 1) {
		cxTile *= 2;
		for (y = 0; y < cyTile; y++)
			for (x = 0; x < cxTile; x += 2) {
				huffman_decode_mcu1x1 (z, zcoeff);
				idct1x1_mcu (i, 4, zcoeff, iout);
				YCbCr2RGBEx1x1 (c, 4, iout, rout);
				nc = 2;
				if (unlikely (nc + x > w)) nc = w - x;
				dest = b->dest + 3 * (w * y + x);
				unsigned int xx;
				src = (void*) rout;
				for (xx = 0; xx < nc; xx++) {
					*dest++ = *src++;
					*dest++ = *src++;
					*dest++ = *src++;
				}
			}
	} else {
		cxTile *= 2;
		cyTile *= 2;
		for (y = 0; y < cyTile; y += 2)
			for (x = 0; x < cxTile; x += 2) {
				huffman_decode_mcu1x1 (z, zcoeff);
				idct1x1_mcu (i, 6, zcoeff, iout);
				YCbCr2RGBEx1x1 (c, 6, iout, rout);
				nr = nc = 2;
				if (unlikely (nr + y > h)) nr = h - y;
				if (unlikely (nc + x > w)) nc = w - x;
				dest = b->dest + 3 * (w * y + x);
				unsigned int xx, yy;
				src = (void*) rout;
				for (yy = 0; yy < nr; yy++) {
					for (xx = 0; xx < nc; xx++) {
						*dest++ = *src++;
						*dest++ = *src++;
						*dest++ = *src++;
					}
					dest += 3 * w - 3 * nc;
				}
			}
	}
}

/* Binary Interface */
/* this interface may seem strange. It's goal was to be able to
 * invoke different stages of the decoding (idct, rgb) from pyvm
 * and run the main-loop in pyvm.  Now the main loop is in C, so
 * the API may be revised.  */

const char ABI [] =
"- init_yctx=			siii		\n"
"- init_idct=			sip16p16	\n"
"- init_hufftable=		siiiiiii	\n"
"- init_zstate=			ssiissssiii	\n"
"- init_blit=			sp8iiii		\n"
"- decompress_image		ssss		\n"
"- decompress_image_bg!		ssss		\n"
"- decompress_image16		ssss		\n"
"- decompress_image16_bg!	ssss		\n"
"- decompress_image_1x1		ssss		\n"
;

// Todo:
//	progressive jpeg
//	arithmetic coding (probably NOT worth it)
//	grayscale jpeg, different number of components
//	optimize with MMX
//	scale down 2x2, 4x4
