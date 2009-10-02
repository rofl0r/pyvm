/*
 * The glyph grayscale bitmap structure.
 * Created by ft2 and mkglyph and used by render
 */
struct rglyph
{
	int advance;		/* 64*advance */
	int px, py;		/* move pen position for the top, left pixel */
	int rows, width, pitch;	/* bitmap size */
	int sp;			/* is space? */
	/* more bytes actually allocated for the bitmap itself */
	unsigned char bitmap [4];
};
