/******************************************************************************/
/*                      Peter Kirchgessner                                    */
/*                      e-mail: pkirchg@aol.com                               */
/*                      WWW   : http://members.aol.com/pkirchg                */
/******************************************************************************/
/*  #BEG-HDR                                                                  */
/*                                                                            */
/*  Package       : FITS reading/writing library                              */
/*  Modul-Name    : fitsrw.h                                                  */
/*  Description   : Include file for FITS-r/w-library                         */
/*  Function(s)   :                                                           */
/*  Author        : P. Kirchgessner                                           */
/*  Date of Gen.  : 12-Apr-97                                                 */
/*  Last modified : 17-May-97                                                 */
/*  Version       : 0.10                                                      */
/*  Compiler Opt. :                                                           */
/*  Changes       :                                                           */
/*                                                                            */
/*  #END-HDR                                                                  */
/******************************************************************************/

/** \file fitsrw.h
    \brief FITS reading/writing library.
    \author Peter Kirchgessner
*/

#ifndef FITS_MAX_AXIS

#include <stdio.h>

#define FITS_CARD_SIZE      80
#define FITS_RECORD_SIZE  2880
#define FITS_MAX_AXIS      999

#define FITS_NADD_CARDS    128

#ifdef __cplusplus
extern "C" {
#endif

/* Data representations */
/** \typedef FITS_BITPIX8
    \brief One byte declared as unsigned char.
*/
typedef unsigned char FITS_BITPIX8;

/** \typedef FITS_BITPIX16
    \brief Two bytes declared as short.
*/
typedef short         FITS_BITPIX16;

/** \typedef FITS_BITPIX32
    \brief Four bytes declared as long.
*/
typedef long          FITS_BITPIX32;

/** \typedef FITS_BITPIXM32
    \brief IEEE -32 FITS format declared as 4-byte float.
*/
typedef float         FITS_BITPIXM32;

/** \typedef FITS_BITPIXM64
    \brief IEEE -64 FITS format declared as 8-byte double.
*/
typedef double        FITS_BITPIXM64;

typedef int           FITS_BOOL;
typedef long          FITS_LONG;
typedef double        FITS_DOUBLE;
typedef char          FITS_STRING[FITS_CARD_SIZE];

typedef enum {
 typ_bitpix8, typ_bitpix16, typ_bitpix32, typ_bitpixm32, typ_bitpixm64,
 typ_fbool, typ_flong, typ_fdouble, typ_fstring
} FITS_DATA_TYPES;

/** \struct FITS_PIX_TRANSFORM
    \brief A struct that describes how to transform FITS pixel values.
*
* The pixel values represent the lower and upper boundaries of the pixel data from the FITS file. The data values represent the range of the final pixel values. For example, to transform FITS pixel values to grey scale, the data min is 0 and data max is 255. The transformation is linear.
*/
typedef struct {
 double pixmin, pixmax;    /** The pixel values [pixmin,pixmax] that should be mapped */
 double datamin, datamax;  /** The data values [datamin,datamax] that the pixel values should be mapped to*/
 double replacement;       /** datavalue to use for blank or NaN pixels */
 char dsttyp;              /** Destination typ ('c' = char) */
} FITS_PIX_TRANSFORM;

typedef union {
 FITS_BITPIX8   bitpix8;
 FITS_BITPIX16  bitpix16;
 FITS_BITPIX32  bitpix32;
 FITS_BITPIXM32 bitpixm32;
 FITS_BITPIXM64 bitpixm64;

 FITS_BOOL   fbool;
 FITS_LONG   flong;
 FITS_DOUBLE fdouble;
 FITS_STRING fstring;
} FITS_DATA;

/** \struct FITS_RECORD_LIST
    \brief Record list.
*/
typedef struct fits_record_list { 
 unsigned char data[FITS_RECORD_SIZE];
 struct fits_record_list *next_record;
} FITS_RECORD_LIST;

/** \struct FITS_HDU_LIST
    \brief Header and Data Unit List.
    
* The structure hold header and data unit lists. The \p used struct contains flags specifying if some cards are used.
*/
typedef struct fits_hdu_list {    
 long header_offset;              /** Offset of header in the file */
 long data_offset;                /** Offset of data in the file */
 long data_size;                  /** Size of data in the HDU (including pad)*/
 long udata_size;                 /** Size of used data in the HDU (excl. pad) */
 int  bpp;                        /** Bytes per pixel */
 int numpic;                      /** Number of interpretable images in HDU */
 int naddcards;                   /** Number of additional cards */
 char addcards[FITS_NADD_CARDS][FITS_CARD_SIZE];
 struct {
   char nan_value;                /** NaN's found in data ? */
   char blank_value;              /** Blanks found in data ? */
                                  
   char blank;                    
   char datamin;                  
   char datamax;
   char simple;                   /** This indicates a simple HDU */
   char xtension;                 /** This indicates an extension */
   char gcount;
   char pcount;
   char bzero;
   char bscale;
   char groups;
   char extend;
 } used;
 double pixmin, pixmax;           /** Minimum/Maximum pixel values */
                                  /* Some decoded data of the HDU: */
 int naxis;                       /** Number of axes */
 int naxisn[FITS_MAX_AXIS];       /** Sizes of axes (NAXIS1 --> naxisn[0]) */
 int bitpix;                      /** Data representation (8,16,32,-16,-32) */
                                  /* When using the following data, */
                                  /* the used-flags must be checked before. */
 long blank;                      /** Blank value. Check the \p used struct to verify if this is a valid value*/
 double datamin, datamax;         /** Minimum/Maximum physical data values. Check the \p used struct to verify if this is a valid value*/
 char xtension[FITS_CARD_SIZE];   /** Type of extension. Check the \p used struct to verify if this is a valid value*/ 
 long gcount, pcount;             /** Used by XTENSION. Check the \p used struct to verify if this is a valid value*/ 
 double bzero, bscale;            /** Transformation values. Check the \p used struct to verify if this is a valid value*/ 
 int groups;                      /** Random groups indicator. Check the \p used struct to verify if this is a valid value*/ 
 int extend;                      /** Extend flag. Check the \p used struct to verify if this is a valid value*/ 

 FITS_RECORD_LIST *header_record_list; /** Header records read in */
 struct fits_hdu_list *next_hdu;
} FITS_HDU_LIST;

/** \struct FITS_FILE
    \brief Structure to hold FITS file information and pointers.
*/
typedef struct {
 FILE *fp;                    /** File pointer to fits file */
 char openmode;               /** Mode the file was opened (0, 'r', 'w') */

 int n_hdu;                   /** Number of HDUs in file */
 int n_pic;                   /** Total number of interpretable pictures */
 int nan_used;                /** NaN's used in the file ? */
 int blank_used;              /** Blank's used in the file ? */

 FITS_HDU_LIST *hdu_list;     /** Header and Data Unit List */
} FITS_FILE;


/* User callable functions of the FITS-library */
/**
 * \defgroup fitsFunctions User callable functions of the FITS-library.
 */
/*@{*/

/** \brief open a FITS file.
    \param filename name of file to open
    \param openmode mode to open the file ("r", "w")
    \return On success, a FITS_FILE-pointer is returned. On failure, a NULL-pointer is returned. The functions scans through the file loading each header and analyzing them.
*/
FITS_FILE     *fits_open (const char *filename, const char *openmode);

/** \brief close a FITS file.
    \param ff FITS file pointer.
*/
void           fits_close (FITS_FILE *ff);

/** \brief add a HDU to the file.
    \param ff FITS file pointer.
    \return Adds a new HDU to the list kept in ff. A pointer to the new HDU is returned. On failure, a NULL-pointer is returned.
*/
FITS_HDU_LIST *fits_add_hdu (FITS_FILE *ff);

/** \brief add a card to the HDU.
    The card must follow the standards of FITS. The card must not use a 
    keyword that is written using *hdulist itself.
    \param hdulist HDU listr.
    \param card card to add.
    \return On success 0 is returned. On failure -1 is returned.
*/
int            fits_add_card (FITS_HDU_LIST *hdulist, char *card);

/** \brief print the internal representation of a single header.
    \param hdr pointer to the header
*/
void           fits_print_header (FITS_HDU_LIST *hdr);

/** \brief write a FITS header to the file.
    \param ff FITS-file pointer.
    \param hdulist pointer to header.
    \return On success, 0 is returned. On failure, -1 is returned.
*/
int            fits_write_header (FITS_FILE *ff, FITS_HDU_LIST *hdulist);

/** \brief get information about an image.
    \param ff FITS-file pointer.
    \param picind Index of picture in file (1,2,...)
    \param hdupicind Index of picture in HDU (1,2,...)
    \return The function returns on success a pointer to a FITS_HDU_LIST. hdupicind then gives the index of the image within the HDU. On failure, NULL is returned.
*/
FITS_HDU_LIST *fits_image_info (FITS_FILE *ff, int picind, int *hdupicind);

/** \brief position to a specific image.
    The function positions the file pointer to a specified image.
    \param ff FITS-file pointer.
    \param picind Index of picture to seek (1,2,...)
    \return The function returns on success a pointer to a FITS_HDU_LIST. This pointer must also be used when reading data from the image. On failure, NULL is returned.
*/
FITS_HDU_LIST *fits_seek_image (FITS_FILE *ff, int picind);

/** \brief decode a card
    Decodes a card and returns a pointer to the union, keeping the data. 
    \param card pointer to card image.
    \param data_type datatype to decode.
    \return If card is NULL or on failure, a NULL-pointer is returned. If the card does not have the value indicator, an error is generated, but its tried to decode the card. The data is only valid up to the next call of the function.
*/
FITS_DATA     *fits_decode_card (const char *card, FITS_DATA_TYPES data_type);

/** \brief search a card in the record list.
    A card is searched in the reord list. Only the first eight characters of keyword are significant. If keyword is less than 8 characters, its filled with blanks.
    \param rl record list to search.
    \param keyword keyword identifying the card.
    \return If the card is found, a pointer to the card is returned. The pointer does not point to a null-terminated string. Only the next 80 bytes are allowed to be read. On failure a NULL-pointer is returned.
*/
char          *fits_search_card (FITS_RECORD_LIST *rl, const char *keyword);

/** \brief read pixel values from a file
    The function reads npix pixel values from the file, transforms them checking for blank/NaN pixels and stores the transformed values in buf. hdulist must be a pointer returned by fits_seek_image(). Before starting to read an image, fits_seek_image() must be called. Even for successive images.
    \param ff FITS file structure.
    \param hdulist pointer to hdulist that describes image.
    \param npix number of pixel values to read.
    \param trans pixel transformation.
    \param buf buffer where to place transformed pixels.
    \return The number of transformed pixels is returned. If the returned value is less than npix (or even -1), an error has occured.
*/
int            fits_read_pixel (FITS_FILE *ff, FITS_HDU_LIST *hdulist,
                                int npix, FITS_PIX_TRANSFORM *trans, void *buf);

/** \brief get an error message.
    \return If an error message has been set, a pointer to the message is returned. Otherwise a NULL pointer is returned. An inquired error message is removed from the error FIFO.
*/
char           *fits_get_error (void);

/*@}*/

int 	       fits_nan_32 (unsigned char *v);
int            fits_nan_64 (unsigned char *v);

/* Demo functions */
#define FITS_NO_DEMO
int fits_to_pgmraw (char *fitsfile, char *pgmfile);
int pgmraw_to_fits (char *pgmfile, char *fitsfile);

#ifdef __cplusplus
}
#endif

#endif
