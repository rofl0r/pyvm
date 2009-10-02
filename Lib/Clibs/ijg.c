/*
 * Memory-to-memory jpeg decoding using IJG libjpeg
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3 of the License.
 *
 */

#include <stdio.h>
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>

typedef struct {
	struct jpeg_source_mgr pub;
	JOCTET * buffer;
	unsigned int len;
} my_source_mgr;

typedef my_source_mgr * my_src_ptr;

static void init_source (j_decompress_ptr cinfo)
{
	my_src_ptr src = (my_src_ptr) cinfo->src;
}

static int fill_input_buffer (j_decompress_ptr cinfo)
{
	static char EOI [] = { 0xff, JPEG_EOI };
	my_src_ptr src = (my_src_ptr) cinfo->src;
	src->pub.next_input_byte = EOI;
	src->pub.bytes_in_buffer = 2;
	return TRUE;
}


static void skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
	my_src_ptr src = (my_src_ptr) cinfo->src;

	src->pub.next_input_byte += (size_t) num_bytes;
	src->pub.bytes_in_buffer -= (size_t) num_bytes;
}

static void term_source (j_decompress_ptr cinfo)
{ }

static void jpeg_mem_src (j_decompress_ptr cinfo, const void *mem, int len)
{
	my_src_ptr src;

	if (!cinfo->src) {
		cinfo->src = (struct jpeg_source_mgr *)
			(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				sizeof(my_source_mgr));
		src = (my_src_ptr) cinfo->src;
	}

	src = (my_src_ptr) cinfo->src;
	src->buffer = (JOCTET *) mem;
	src->pub.init_source = init_source;
	src->pub.fill_input_buffer = fill_input_buffer;
	src->pub.skip_input_data = skip_input_data;
	src->pub.resync_to_restart = jpeg_resync_to_restart;
	src->pub.term_source = term_source;
	src->pub.bytes_in_buffer = len;
	src->pub.next_input_byte = mem;
}

//////////////////////////////////////////////////////////////////////////////

struct my_error_mgr {
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
};

typedef struct my_error_mgr *my_error_ptr;

static void my_error_exit (j_common_ptr cinfo)
{
	my_error_ptr myerr = (my_error_ptr) cinfo->err;
	longjmp(myerr->setjmp_buffer, 1);
}

typedef struct { unsigned char r, g, b; } rgb;

int jpeg2rgb (const void *data, int len, int downscale, int fast, unsigned char *bu)
{
	struct jpeg_decompress_struct cinfo;
	struct my_error_mgr jerr;
	JSAMPARRAY buffer;
	int row_stride;
	int i;
	unsigned char *b1;

fast=1;
	cinfo.err = jpeg_std_error (&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	if (setjmp (jerr.setjmp_buffer)) {
		fprintf (stderr, "JPEG: ");
		cinfo.err->output_message ((void*)&cinfo);
		jpeg_destroy_decompress (&cinfo);
		return 0;
	}
	jpeg_create_decompress (&cinfo);
	jpeg_mem_src (&cinfo, data, len);
	jpeg_read_header (&cinfo, TRUE);
	cinfo.scale_num = 1;
	cinfo.scale_denom = downscale;
	cinfo.out_color_space = JCS_RGB;
	if (fast) {
		cinfo.do_fancy_upsampling = FALSE;
		cinfo.do_block_smoothing = FALSE;
		cinfo.dct_method = JDCT_FASTEST;
	}
	jpeg_start_decompress (&cinfo);
	row_stride = cinfo.output_width * cinfo.output_components;
	buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

	while (cinfo.output_scanline < cinfo.output_height) {
		jpeg_read_scanlines (&cinfo, buffer, 1);
		memcpy (bu, buffer [0], row_stride);
		bu += row_stride;
	}

	jpeg_finish_decompress (&cinfo);
	jpeg_destroy_decompress (&cinfo);
	/* expand grayscale to rgb */
//	if (cinfo.output_components == 1) {
//		unsigned int N = cinfo.output_width * cinfo.output_height;
//		rgb *rb = ((rgb*)b0) + N - 1;
//		bu = b0 + N;
//		while (N--) {
//			rb->r = rb->g = rb->b = *--bu;
//			--rb;
//		}
//	}
	return 1;
}

/*
int jpegsize (const unsigned char *mem, int *w, int *h)
{
	int pos;
	int m, l;

	if (mem [0] != 255 || mem [1] != 0xd8)
		return 1;
	pos = 2;
	while (1) {
//printf ("xXXX: %i\n", mem [pos]);
		if (mem [pos++] != 255)
			return 1;
//printf ("ooX: %x\n", mem [pos]);
		m = mem [pos++];
		if (m < 192)
			return 2;
		if (m != 0xc0 && m !=0xc2) {
			pos += (mem [pos] << 8) + mem [pos + 1];
			continue;
		}
		pos += 3;
		*h = (mem [pos] << 8) + mem [pos + 1];
		pos += 2;
		*w = (mem [pos] << 8) + mem [pos + 1];
		return 0;
	}
}
*/

#if 0
char inbuf [500 * 1024], *out;
int main (int argc, char **argv)
{
	FILE *F = fopen (argv [1], "r");
	int n = fread (inbuf, 1, 500 * 1024, F);
	fclose (F);
	int w, h;
	if (get_dimensions (inbuf, &w, &h)) {
		printf ("Can't get dims!\n");
		return 1;
	}
	printf ("%ix%i\n", w, h);
	out = malloc (w * h * 3);
	decode (inbuf, n, 1, 1, out);
	F = fopen ("out.ppm", "w");
	fprintf (F, "P6\n%i %i\n255\n", w, h);
	fwrite (out, 3*w*h, 1, F);
	fclose (F);
}
#endif

const char ABI [] =
"i jpeg2rgb siiis	\n"
;
