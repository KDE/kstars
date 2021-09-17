/*
    SPDX-FileCopyrightText: 1993-2011 Emmanuel Bertin -- IAP /CNRS/UPMC
    SPDX-FileCopyrightText: 2014 SEP developers

    SPDX-License-Identifier: LGPL-3.0-or-later
*/

#define	RETURN_OK           0  /* must be zero */
#define MEMORY_ALLOC_ERROR  1
#define PIXSTACK_FULL       2
#define ILLEGAL_DTYPE       3
#define ILLEGAL_SUBPIX      4
#define NON_ELLIPSE_PARAMS  5
#define ILLEGAL_APER_PARAMS 6
#define DEBLEND_OVERFLOW    7
#define LINE_NOT_IN_BUF     8
#define RELTHRESH_NO_NOISE  9
#define UNKNOWN_NOISE_TYPE  10

#define	BIG 1e+30  /* a huge number (< biggest value a float can store) */
#define	PI  3.1415926535898
#define	DEG (PI/180.0)	    /* 1 deg in radians */

typedef	int	      LONG;
typedef	unsigned int  ULONG;
typedef	unsigned char BYTE;    /* a byte */

/* keep these synchronized */
typedef float         PIXTYPE;    /* type used inside of functions */
#define PIXDTYPE      SEP_TFLOAT  /* dtype code corresponding to PIXTYPE */


/* signature of converters */
typedef PIXTYPE (*converter)(void *ptr);
typedef void (*array_converter)(void *ptr, int n, PIXTYPE *target);
typedef void (*array_writer)(float *ptr, int n, void *target);

#define	QCALLOC(ptr, typ, nel, status)				     	\
  {if (!(ptr = (typ *)calloc((size_t)(nel),sizeof(typ))))		\
      {									\
	char errtext[160];						\
	sprintf(errtext, #ptr " (" #nel "=%lu elements) "		\
		"at line %d in module " __FILE__ " !",			\
		(size_t)(nel)*sizeof(typ), __LINE__);			\
	put_errdetail(errtext);						\
	status = MEMORY_ALLOC_ERROR;					\
	goto exit;							\
      };								\
  }

#define	QMALLOC(ptr, typ, nel, status)					\
  {if (!(ptr = (typ *)malloc((size_t)(nel)*sizeof(typ))))		\
      {									\
	char errtext[160];						\
	sprintf(errtext, #ptr " (" #nel "=%lu elements) "		\
		"at line %d in module " __FILE__ " !",			\
		(size_t)(nel)*sizeof(typ), __LINE__);			\
	put_errdetail(errtext);						\
	status = MEMORY_ALLOC_ERROR;					\
	goto exit;							\
      };								\
  }

float fqmedian(float *ra, int n);
void put_errdetail(char *errtext);

int get_converter(int dtype, converter *f, int *size);
int get_array_converter(int dtype, array_converter *f, int *size);
int get_array_writer(int dtype, array_writer *f, int *size);
int get_array_subtractor(int dtype, array_writer *f, int *size);
