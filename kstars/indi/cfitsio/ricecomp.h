/*
Copyright (Unpublished-all rights reserved under the copyright laws of the United States), U.S. Government as represented by the Administrator of the National Aeronautics and Space Administration. No copyright is claimed in the United States under Title 17, U.S. Code.

Permission to freely use, copy, modify, and distribute this software and its documentation without fee is hereby granted, provided that this copyright notice and disclaimer of warranty appears in all copies. (However, see the restriction on the use of the gzip compression code, below).

e-mail: pence@tetra.gsfc.nasa.gov

DISCLAIMER:

THE SOFTWARE IS PROVIDED 'AS IS' WITHOUT ANY WARRANTY OF ANY KIND, EITHER EXPRESSED, IMPLIED, OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, ANY WARRANTY THAT THE SOFTWARE WILL CONFORM TO SPECIFICATIONS, ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND FREEDOM FROM INFRINGEMENT, AND ANY WARRANTY THAT THE DOCUMENTATION WILL CONFORM TO THE SOFTWARE, OR ANY WARRANTY THAT THE SOFTWARE WILL BE ERROR FREE. IN NO EVENT SHALL NASA BE LIABLE FOR ANY DAMAGES, INCLUDING, BUT NOT LIMITED TO, DIRECT, INDIRECT, SPECIAL OR CONSEQUENTIAL DAMAGES, ARISING OUT OF, RESULTING FROM, OR IN ANY WAY CONNECTED WITH THIS SOFTWARE, WHETHER OR NOT BASED UPON WARRANTY, CONTRACT, TORT , OR OTHERWISE, WHETHER OR NOT INJURY WAS SUSTAINED BY PERSONS OR PROPERTY OR OTHERWISE, AND WHETHER OR NOT LOSS WAS SUSTAINED FROM, OR AROSE OUT OF THE RESULTS OF, OR USE OF, THE SOFTWARE OR SERVICES PROVIDED HEREUNDER."

The file compress.c contains (slightly modified) source code that originally came from gzip-1.2.4, copyright (C) 1992-1993 by Jean-loup Gailly. This gzip code is distributed under the GNU General Public License and thus requires that any software that uses the CFITSIO library (which in turn uses the gzip code) must conform to the provisions in the GNU General Public License. A copy of the GNU license is included at the beginning of compress.c file.

Similarly, the file wcsutil.c contains 2 slightly modified routines from the Classic AIPS package that are also distributed under the GNU General Public License.

Alternate versions of the compress.c and wcsutil.c files (called compress_alternate.c and wcsutil_alternate.c) are provided for users who want to use the CFITSIO library but are unwilling or unable to publicly release their software under the terms of the GNU General Public License. These alternate versions contains non-functional stubs for the file compression and uncompression routines and the world coordinate transformation routines used by CFITSIO. Replace the file `compress.c' with `compress_alternate.c' and 'wcsutil.c' with 'wcsutil_alternate.c before compiling the CFITSIO library. This will produce a version of CFITSIO which does not support reading or writing compressed FITS files, or doing image coordinate transformations, but is otherwise identical to the standard version. 

*/

/* @(#) buffer.h 1.1 98/07/21 12:34:27 */
/* buffer.h: structure for compression to buffer rather than to a file, including
 *   bit I/O buffer
 *
 * R. White, 19 June 1998
 */


typedef unsigned char Buffer_t;

typedef struct {
	int bitbuffer;		/* bit buffer					*/
	int bits_to_go;		/* bits to go in buffer			*/
	Buffer_t *start;	/* start of buffer				*/
	Buffer_t *current;	/* current position in buffer	*/
	Buffer_t *end;		/* end of buffer				*/
} Buffer;

#define buffree(mf)		(free(mf->start), free(mf))
#define bufused(mf)		(mf->current - mf->start)
#define bufreset(mf)	(mf->current = mf->start)

/*
 * getcbuf, putcbuf macros for character IO to buffer
 * putcbuf returns EOF on end of buffer, else returns 0
 */
#define getcbuf(mf) ((mf->current >= mf->end) ? EOF : *(mf->current)++)
#define putcbuf(c,mf) \
	((mf->current >= mf->end) ? \
	EOF :\
	((*(mf->current)++ = c), 0))

/*
 * bufalloc sets up buffer of length n
 */

/*  not needed by CFITSIO

static Buffer *bufalloc(int n)
{
Buffer *mf;

	mf = (Buffer *) malloc(sizeof(Buffer));
	if (mf == (Buffer *)NULL) return((Buffer *)NULL);

	mf->start = (Buffer_t *) malloc(n*sizeof(Buffer_t));
	if (mf->start == (Buffer_t *)NULL) {
		free(mf);
		return((Buffer *)NULL);
	}
	mf->bits_to_go = 8;
	mf->end = mf->start + n;
	mf->current = mf->start;
	return(mf);
}
*/

/*
 * bufrealloc extends buffer (or truncates it) by
 * reallocating memory
 */

/* not needed by CFITSIO 
static int bufrealloc(Buffer *mf, int n)
{
int len;

	len = mf->current - mf->start;

	* silently throw away data if buffer is already longer than n *
	if (len>n) len = n;
	if (len<0) len = 0;

	mf->start = (Buffer_t *) realloc(mf->start, n*sizeof(Buffer_t));
	if (mf->start == (Buffer_t *)NULL) return(0);

	mf->end = mf->start + n;
	mf->current = mf->start + len;
	return(n);
}
*/

/*
 * bufdump dumps contents of buffer to outfile and resets
 * it to be empty.  Returns number of bytes written.
 *
 * Note we don't write out the bit buffer -- you must call
 * done_outputing_bits() first to ensure that the bit buffer
 * is written out.  I do it this way to allow incremental
 * buffer dumps while bit IO is still going on.
 */

/*  not needed by CFITSIO

static int bufdump(FILE *outfile, Buffer *buffer)
{
int ndump;

	ndump = bufused(buffer);
	if (fwrite(buffer->start, 1, ndump, outfile) != ndump) {
		fprintf(stderr, "bufdump: error in write\n");
		exit(1);
    }
	bufreset(buffer);
	return(ndump);
}
*/
