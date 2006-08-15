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

/*  This file, putcols.c, contains routines that write data elements to    */
/*  a FITS image or table, of type character string.                       */

/*  The FITSIO software was written by William Pence at the High Energy    */
/*  Astrophysic Science Archive Research Center (HEASARC) at the NASA      */
/*  Goddard Space Flight Center.                                           */


#include <string.h>
#include <stdlib.h>
#include "fitsio2.h"
/*--------------------------------------------------------------------------*/
int ffpcls( fitsfile *fptr,  /* I - FITS file pointer                       */
            int  colnum,     /* I - number of column to write (1 = 1st col) */
            LONGLONG  firstrow,  /* I - first row to write (1 = 1st row)        */
            LONGLONG  firstelem, /* I - first vector element to write (1 = 1st) */
            LONGLONG  nelem,     /* I - number of strings to write              */
            char  **array,   /* I - array of pointers to strings            */
            int  *status)    /* IO - error status                           */
/*
  Write an array of string values to a column in the current FITS HDU.
*/
{
    int tcode, maxelem, hdutype, nchar;
    long twidth, incre;
    long ii, jj, ntodo;
    LONGLONG repeat, startpos, elemnum, wrtptr, rowlen, rownum, remain, next, tnull;
    double scale, zero;
    char tform[20], *blanks;
    char message[FLEN_ERRMSG];
    char snull[20];   /*  the FITS null value  */
    tcolumn *colptr;

    double cbuff[DBUFFSIZE / sizeof(double)]; /* align cbuff on word boundary */
    char *buffer, *arrayptr;

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    /* reset position to the correct HDU if necessary */
    if (fptr->HDUposition != (fptr->Fptr)->curhdu)
    {
        ffmahd(fptr, (fptr->HDUposition) + 1, NULL, status);
    }
    else if ((fptr->Fptr)->datastart == DATA_UNDEFINED)
    {
        if ( ffrdef(fptr, status) > 0)               /* rescan header */
            return(*status);
    }

    /*---------------------------------------------------*/
    /*  Check input and get parameters about the column: */
    /*---------------------------------------------------*/
    if (colnum < 1 || colnum > (fptr->Fptr)->tfield)
    {
        sprintf(message, "Specified column number is out of range: %d",
                colnum);
        ffpmsg(message);
        return(*status = BAD_COL_NUM);
    }

    colptr  = (fptr->Fptr)->tableptr;   /* point to first column */
    colptr += (colnum - 1);     /* offset to correct column structure */
    tcode = colptr->tdatatype;

    if (tcode == -TSTRING) /* variable length column in a binary table? */
    {
      /* only write a single string; ignore value of firstelem */
      nchar = maxvalue(1,strlen(array[0])); /* will write at least 1 char */
                                          /* even if input string is null */

      if (ffgcprll( fptr, colnum, firstrow, 1, nchar, 1, &scale, &zero,
        tform, &twidth, &tcode, &maxelem, &startpos,  &elemnum, &incre,
        &repeat, &rowlen, &hdutype, &tnull, snull, status) > 0)
        return(*status);
	
      /* simply move to write position, then write the string */
      ffmbyt(fptr, startpos, IGNORE_EOF, status); 
      ffpbyt(fptr, nchar, array[0], status);

      if (*status > 0)  /* test for error during previous write operation */
      {
         sprintf(message,
          "Error writing to variable length string column (ffpcls).");
         ffpmsg(message);
      }

      return(*status);
    }
    else if (tcode == TSTRING)
    {
      if (ffgcprll( fptr, colnum, firstrow, firstelem, nelem, 1, &scale, &zero,
        tform, &twidth, &tcode, &maxelem, &startpos,  &elemnum, &incre,
        &repeat, &rowlen, &hdutype, &tnull, snull, status) > 0)
        return(*status);

      blanks = (char *) malloc(twidth); /* string for blank fill values */
      if (!blanks)
      {
        ffpmsg("Could not allocate memory for string (ffpcls)");
        return(*status = ARRAY_TOO_BIG);
      }

      for (ii = 0; ii < twidth; ii++)
          blanks[ii] = ' ';          /* fill string with blanks */

      remain = nelem;           /* remaining number of values to write  */
    }
    else 
      return(*status = NOT_ASCII_COL);
 
    /*-------------------------------------------------------*/
    /*  Now write the strings to the FITS column.            */
    /*-------------------------------------------------------*/

    next = 0;                 /* next element in array to be written  */
    rownum = 0;               /* row number, relative to firstrow     */

    while (remain)
    {
      /* limit the number of pixels to process at one time to the number that
         will fit in the buffer space or to the number of pixels that remain
         in the current vector, which ever is smaller.
      */
      ntodo = (long) minvalue(remain, maxelem);      
      ntodo = (long) minvalue(ntodo, (repeat - elemnum));

      wrtptr = startpos + (rownum * rowlen) + (elemnum * incre);
      ffmbyt(fptr, wrtptr, IGNORE_EOF, status);  /* move to write position */

      buffer = (char *) cbuff;

      /* copy the user's strings into the buffer */
      for (ii = 0; ii < ntodo; ii++)
      {
         arrayptr = array[next];

         for (jj = 0; jj < twidth; jj++)  /*  copy the string, char by char */
         {
            if (*arrayptr)
            {
              *buffer = *arrayptr;
              buffer++;
              arrayptr++;
            }
            else
              break;
         }

         for (;jj < twidth; jj++)    /* fill field with blanks, if needed */
         {
           *buffer = ' ';
           buffer++;
         }

         next++;
      }

      /* write the buffer full of strings to the FITS file */
      if (incre == twidth)
         ffpbyt(fptr, ntodo * twidth, cbuff, status);
      else
         ffpbytoff(fptr, twidth, ntodo, incre - twidth, cbuff, status);

      if (*status > 0)  /* test for error during previous write operation */
      {
         sprintf(message,
          "Error writing elements %.0f thru %.0f of input data array (ffpcls).",
             (double) (next+1), (double) (next+ntodo));
         ffpmsg(message);

         if (blanks)
           free(blanks);

         return(*status);
      }

      /*--------------------------------------------*/
      /*  increment the counters for the next loop  */
      /*--------------------------------------------*/
      remain -= ntodo;
      if (remain)
      {
          elemnum += ntodo;
          if (elemnum == repeat)  /* completed a row; start on next row */
          {
              elemnum = 0;
              rownum++;
          }
       }
    }  /*  End of main while Loop  */

    if (blanks)
      free(blanks);

    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffpcns( fitsfile *fptr,  /* I - FITS file pointer                       */
            int  colnum,     /* I - number of column to write (1 = 1st col) */
            LONGLONG  firstrow,  /* I - first row to write (1 = 1st row)        */
            LONGLONG  firstelem, /* I - first vector element to write (1 = 1st) */
            LONGLONG  nelem,     /* I - number of values to write               */
            char  **array,   /* I - array of values to write                */
            char  *nulvalue, /* I - string representing a null value        */
            int  *status)    /* IO - error status                           */
/*
  Write an array of elements to the specified column of a table.  Any input
  pixels flagged as null will be replaced by the appropriate
  null value in the output FITS file. 
*/
{
    long repeat, width, ngood = 0, nbad = 0, ii;
    LONGLONG first, fstelm, fstrow;

    if (*status > 0)
        return(*status);

    /* reset position to the correct HDU if necessary */
    if (fptr->HDUposition != (fptr->Fptr)->curhdu)
    {
        ffmahd(fptr, (fptr->HDUposition) + 1, NULL, status);
    }
    else if ((fptr->Fptr)->datastart == DATA_UNDEFINED)
    {
        if ( ffrdef(fptr, status) > 0)               /* rescan header */
            return(*status);
    }

    /* get the vector repeat length of the column */
    ffgtcl(fptr, colnum, NULL, &repeat, &width, status);

    if ((fptr->Fptr)->hdutype == BINARY_TBL)
        repeat = repeat / width;    /* convert from chars to unit strings */

    /* absolute element number in the column */
    first = (firstrow - 1) * repeat + firstelem;

    for (ii = 0; ii < nelem; ii++)
    {
      if (strcmp(nulvalue, array[ii]))  /* is this a good pixel? */
      {
         if (nbad)  /* write previous string of bad pixels */
         {
            fstelm = ii - nbad + first;  /* absolute element number */
            fstrow = (fstelm - 1) / repeat + 1;  /* starting row number */
            fstelm = fstelm - (fstrow - 1) * repeat;  /* relative number */

            if (ffpclu(fptr, colnum, fstrow, fstelm, nbad, status) > 0)
                return(*status);
            nbad=0;
         }

         ngood = ngood +1;  /* the consecutive number of good pixels */
      }
      else
      {
         if (ngood)  /* write previous string of good pixels */
         {
            fstelm = ii - ngood + first;  /* absolute element number */
            fstrow = (fstelm - 1) / repeat + 1;  /* starting row number */
            fstelm = fstelm - (fstrow - 1) * repeat;  /* relative number */

            if (ffpcls(fptr, colnum, fstrow, fstelm, ngood, &array[ii-ngood],
                status) > 0)
                return(*status);

            ngood=0;
         }

         nbad = nbad +1;  /* the consecutive number of bad pixels */
      }
    }

    /* finished loop;  now just write the last set of pixels */

    if (ngood)  /* write last string of good pixels */
    {
      fstelm = ii - ngood + first;  /* absolute element number */
      fstrow = (fstelm - 1) / repeat + 1;  /* starting row number */
      fstelm = fstelm - (fstrow - 1) * repeat;  /* relative number */

      ffpcls(fptr, colnum, fstrow, fstelm, ngood, &array[ii-ngood], status);
    }
    else if (nbad) /* write last string of bad pixels */
    {
      fstelm = ii - nbad + first;  /* absolute element number */
      fstrow = (fstelm - 1) / repeat + 1;  /* starting row number */
      fstelm = fstelm - (fstrow - 1) * repeat;  /* relative number */

      ffpclu(fptr, colnum, fstrow, fstelm, nbad, status);
    }

    return(*status);
}
