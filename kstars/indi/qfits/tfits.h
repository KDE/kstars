/*----------------------------------------------------------------------------*/
/**
   @file	tfits.h
   @author	Yves Jung
   @date	July 1999
   @version	$Revision$
   @brief	FITS table handling
*/
/*----------------------------------------------------------------------------*/

/*
	$Id$
	$Author$
	$Date$
	$Revision$
*/

#ifndef TFITS_H
#define TFITS_H

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------------------------
   								Includes
 -----------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
	
#include "fits_h.h"
#include "static_sz.h"

/* <dox> */
/*-----------------------------------------------------------------------------
   								Defines
 -----------------------------------------------------------------------------*/

/* The following defines the maximum acceptable size for a FITS value */
#define FITSVALSZ					60

#define QFITS_INVALIDTABLE			0
#define QFITS_BINTABLE				1
#define QFITS_ASCIITABLE			2

/*-----------------------------------------------------------------------------
   								New types
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief    Column data type
 */ 
/*----------------------------------------------------------------------------*/
typedef enum _TFITS_DATA_TYPE_ {
	TFITS_ASCII_TYPE_A,
	TFITS_ASCII_TYPE_D,
	TFITS_ASCII_TYPE_E,
	TFITS_ASCII_TYPE_F,
	TFITS_ASCII_TYPE_I,
	TFITS_BIN_TYPE_A,
	TFITS_BIN_TYPE_B,
	TFITS_BIN_TYPE_C,
	TFITS_BIN_TYPE_D,
	TFITS_BIN_TYPE_E,
	TFITS_BIN_TYPE_I,
	TFITS_BIN_TYPE_J,
	TFITS_BIN_TYPE_L,
	TFITS_BIN_TYPE_M,
	TFITS_BIN_TYPE_P,
	TFITS_BIN_TYPE_X,
	TFITS_BIN_TYPE_UNKNOWN
} tfits_type ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Column object

  This structure contains all information needed to read a column in a table.
  These informations come from the header. 
  The qfits_table object contains a list of qfits_col objects.

  This structure has to be created from scratch and filled if one want to 
  generate a FITS table.
 */
/*----------------------------------------------------------------------------*/
typedef struct qfits_col
{
	/** 
	  Number of atoms in one field.
	 In ASCII tables, it is the number of characters in the field as defined
	 in TFORM%d keyword.
	 In BIN tables, it is the number of atoms in each field. For type 'A', 
	 it is the number of characters. A field with two complex object will
	 have atom_nb = 4.
	*/
	int			atom_nb ;

    /**
     Number of decimals in a ASCII field. 
     This value is always 0 for BIN tables
    */
    int         atom_dec_nb ;

	/** 
	  Size of one element in bytes. In ASCII tables, atom_size is the size
	  of the element once it has been converted in its 'destination' type.
	  For example, if "123" is contained in an ASCII table in a column 
	  defined as I type, atom_nb=3, atom_size=4.
	  In ASCII tables:
	   - type 'A' : atom_size = atom_nb = number of chars
	   - type 'I', 'F' or 'E' : atom_size = 4
	   - type 'D' : atom_size = 8
	  In BIN tables :
	   - type 'A', 'L', 'X', 'B': atom_size = 1 
	   - type 'I' : atom_size = 2
	   - type 'E', 'J', 'C', 'P' : atom_size = 4
	   - type 'D', 'M' : atom_size = 8
	  In ASCII table, there is one element per field. The size in bytes and 
	  in number of characters is atom_nb, and the size in bytes after 
	  conversion of the field is atom_size.
	  In BIN tables, the size in bytes of a field is always atom_nb*atom_size.
	 */
	int			atom_size ;	
	
	/** 
	  Type of data in the column as specified in TFORM keyword 
	  In ASCII tables : TFITS_ASCII_TYPE_* with *=A, I, F, E or D 
	  In BIN tables : TFITS_BIN_TYPE_* with *=L, X, B, I, J, A, E, D, C, M or P 
	*/
	tfits_type	atom_type ;

	/** Label of the column */
	char    	tlabel[FITSVALSZ] ;

	/** Unit of the data */
	char    	tunit[FITSVALSZ] ;
	
	/** Null value */
	char		nullval[FITSVALSZ] ;

	/** Display format */
	char		tdisp[FITSVALSZ] ;
	
	/** 
	  zero and scale are used when the quantity in the field does not	 
	  represent a true physical quantity. Basically, thez should be used
	  when they are present: physical_value = zero + scale * field_value 
	  They are read from TZERO and TSCAL in the header
	 */
	int			zero_present ;	
	float		zero ;        
	int			scale_present ;
	float		scale ;   

	/** Offset between the beg. of the table and the beg. of the column.  */
    int			off_beg ;
	
	/** Flag to know if the column is readable. An empty col is not readable */
	int			readable ;
} qfits_col ;


/*----------------------------------------------------------------------------*/
/**
  @brief    Table object

  This structure contains all information needed to read a FITS table.
  These information come from the header. The object is created by 
  qfits_open().
 
  To read a FITS table, here is a code example:
  @code
  int main(int argc, char* argv[])
  {
  	qfits_table     *   table ;
 	int					n_ext ;
	int					i ;

	// Query the number of extensions
	n_ext = qfits_query_n_ext(argv[1]) ;
	
	// For each extension
	for (i=0 ; i<n_ext ; i++) {
		// Read all the infos about the current table 
		table = qfits_table_open(argv[1], i+1) ;
		// Display the current table 
		dump_extension(table, stdout, '|', 1, 1) ;
	}
	return ;
  }
  @endcode
 */
/*----------------------------------------------------------------------------*/
typedef struct qfits_table
{
	/**
		Name of the file the table comes from or it is intended to end to
	 */
	char			filename[FILENAMESZ] ;
	/** 
		Table type. 
        Possible values: QFITS_INVALIDTABLE, QFITS_BINTABLE, QFITS_ASCIITABLE
	 */
	int				tab_t ;
	/** Width in bytes of the table */
	int				tab_w ;			
	/** Number of columns */
	int				nc ;			
	/** Number of raws */
	int			    nr ;
	/** Array of qfits_col objects */
	qfits_col	*	col ;			
} qfits_table ;

/*-----------------------------------------------------------------------------
   							Function prototypes
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief    Identify a file as containing a FITS table in extension.
  @param    filename    Name of the FITS file to examine.
  @param    xtnum       Extension number to check (starting from 1).
  @return   int 1 if the extension contains a table, 0 else.

  Examines the requested extension and identifies the presence of a
  FITS table.
 */
/*----------------------------------------------------------------------------*/
int qfits_is_table(char * filename, int xtnum) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Generate a default primary header to store tables   
  @return   the header object   
 */
/*----------------------------------------------------------------------------*/
qfits_header * qfits_table_prim_header_default(void) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Generate a default extension header to store tables   
  @return   the header object   
 */
/*----------------------------------------------------------------------------*/
qfits_header * qfits_table_ext_header_default(qfits_table *) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Table object constructor
  @param    filename    Name of the FITS file associated to the table
  @param    table_type  Type of the table (QFITS_ASCIITABLE or QFITS_BINTABLE)
  @param    table_width Width in bytes of the table
  @param    nb_cols     Number of columns
  @param    nb_raws     Number of raws
  @return   The table object
  The columns are also allocated. The object has to be freed with 
  qfits_table_close()
 */
/*----------------------------------------------------------------------------*/
qfits_table * qfits_table_new(
        char    *   filename,
        int         table_type,
        int         table_width,
        int         nb_cols,
        int         nb_raws) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Fill a column object with some provided informations
  @param    qc      Pointer to the column that has to be filled
  @param    unit    Unit of the data 
  @param    label   Label of the column 
  @param    disp    Way to display the data 
  @param    nullval Null value
  @param    atom_nb Number of atoms per field. According to the type, an atom 
                    is a double, an int, a char, ... 
  @param    atom_dec_nb Number of decimals as specified in TFORM 
  @param    atom_size   Size in bytes of the field for ASCII tables, and of 
                        an atom for BIN tables. ASCII tables only contain 1 
                        atom per field (except for A type where you can of
                        course have more than one char per field)
  @param    atom_type   Type of data (11 types for BIN, 5 for ASCII)
  @param    zero_present    Flag to use or not zero
  @param    zero            Zero value
  @param    scale_present   Flag to use or not scale
  @param    scale           Scale value
  @param    offset_beg  Gives the position of the column
  @return   -1 in error case, 0 otherwise
 */
/*----------------------------------------------------------------------------*/
int qfits_col_fill(
        qfits_col   *   qc,
        int             atom_nb,
        int             atom_dec_nb,
        int             atom_size,
        tfits_type      atom_type,
        char        *   label,
        char        *   unit,
        char        *   nullval,
        char        *   disp,
        int             zero_present,
        float           zero,
        int             scale_present,
        float           scale,
        int             offset_beg) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Read a FITS extension.
  @param    filename    Name of the FITS file to examine.
  @param    xtnum       Extension number to read (starting from 1).
  @return   Pointer to newly allocated qfits_table structure.

  Read a FITS table from a given file name and extension, and return a
  newly allocated qfits_table structure. 
 */
/*----------------------------------------------------------------------------*/
qfits_table * qfits_table_open(
        char    *   filename,
        int         xtnum) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Free a FITS table and associated pointers
  @param    t qfits_table to free
  @return   void
  Frees all memory associated to a qfits_table structure.
 */
/*----------------------------------------------------------------------------*/
void qfits_table_close(qfits_table * t) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Extract data from a column in a FITS table
  @param    th      Allocated qfits_table
  @param    colnum  Number of the column to extract (from 0 to colnum-1)
  @param    selection  boolean array to define the selected rows
  @return   unsigned char array

  If selection is NULL, select the complete column.
  
  Extract a column from a FITS table and return the data as a bytes 
  array. The returned array type and size are determined by the
  column object in the qfits_table and by the selection parameter.

  Returned array size in bytes is:
  nbselected * col->natoms * col->atom_size

  Numeric types are correctly understood and byte-swapped if needed,
  to be converted to the local machine type.

  NULL values have to be handled by the caller.

  The returned buffer has been allocated using one of the special memory
  operators present in xmemory.c. To deallocate the buffer, you must call
  the version of free() offered by xmemory, not the usual system free(). It
  is enough to include "xmemory.h" in your code before you make calls to 
  the pixel loader here.
 */
/*----------------------------------------------------------------------------*/
unsigned char * qfits_query_column(
        qfits_table     *   th,
        int                 colnum,
        int             *   selection) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Extract consequtive values from a column in a FITS table
  @param    th      Allocated qfits_table
  @param    colnum  Number of the column to extract (from 0 to colnum-1)
  @param    start_ind   Index of the first row (0 for the first)
  @param    nb_rows     Number of rows to extract
  @return   unsigned char array
  Does the same as qfits_query_column() but on a consequtive sequence of rows
  Spares the overhead of the selection object allocation
 */
/*----------------------------------------------------------------------------*/
unsigned char * qfits_query_column_seq(
        qfits_table     *   th,
        int                 colnum,
        int                 start_ind,
        int                 nb_rows) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Compute the table width in bytes from the columns infos 
  @param    th      Allocated qfits_table
  @return   the width (-1 in error case)
 */
/*----------------------------------------------------------------------------*/
int qfits_compute_table_width(qfits_table * th) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Extract binary data from a column in a FITS table
  @param    th      Allocated qfits_table
  @param    colnum  Number of the column to extract (from 0 to colnum-1)
  @param    selection  bollean array to identify selected rows
  @param    null_value  Value to return when a NULL value comes
  @return   Pointer to void *

  Extract a column from a FITS table and return the data as a generic
  void* array. The returned array type and size are determined by the
  column object in the qfits_table.
    
  Returned array size in bytes is:
  nb_selected * col->atom_nb * col->atom_size
  
  NULL values are recognized and replaced by the specified value.

  The returned buffer has been allocated using one of the special memory
  operators present in xmemory.c. To deallocate the buffer, you must call
  the version of free() offered by xmemory, not the usual system free(). It
  is enough to include "xmemory.h" in your code before you make calls to 
  the pixel loader here.
 */
/*----------------------------------------------------------------------------*/
void * qfits_query_column_data(
        qfits_table     *   th,
        int                 colnum,
        int             *   selection,
        void            *   null_value) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Extract binary data from a column in a FITS table
  @param    th      Allocated qfits_table
  @param    colnum  Number of the column to extract (from 0 to colnum-1)
  @param    start_ind   Index of the first row (0 for the first)
  @param    nb_rows     Number of rows to extract
  @param    null_value  Value to return when a NULL value comes
  @return   Pointer to void *
  Does the same as qfits_query_column_data() but on a consequtive sequence 
  of rows.  Spares the overhead of the selection object allocation
 */
/*----------------------------------------------------------------------------*/
void * qfits_query_column_seq_data(
        qfits_table     *   th,
        int                 colnum,
        int                 start_ind,
        int                 nb_rows,
        void            *   null_value) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Detect NULL values in a column
  @param    th      Allocated qfits_table
  @param    colnum  Number of the column to check (from 0 to colnum-1)
  @param    selection Array to identify selected rows
  @param    nb_vals Gives the size of the output array
  @param    nb_nulls Gives the number of detected null values
  @return   array with 1 for NULLs and 0 for non-NULLs  
 */
/*----------------------------------------------------------------------------*/
int * qfits_query_column_nulls(
        qfits_table     *   th,
        int                 colnum,
        int             *   selection,
        int             *   nb_vals,
        int             *   nb_nulls) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Save a table to a FITS file with a given FITS header.
  @param    array           Data array.
  @param    table           table
  @param    fh              FITS header to insert in the output file.
  @return   -1 in error case, 0 otherwise
 */
/*----------------------------------------------------------------------------*/
int qfits_save_table_hdrdump(
        void            **  array,
        qfits_table     *   table,
        qfits_header    *   fh) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Appends a std extension header + data to a FITS table file.
  @param    outfile     Pointer to (opened) file ready for writing.
  @param    t           Pointer to qfits_table
  @param    data        Table data to write
  @return   int 0 if Ok, -1 otherwise

  Dumps a FITS table to a file. The whole table described by qfits_table, and
  the data arrays contained in 'data' are dumped to the file. An extension
  header is produced with all keywords needed to describe the table, then the
  data is dumped to the file.
  The output is then padded to reach a multiple of 2880 bytes in size.
  Notice that no main header is produced, only the extension part.
 */
/*----------------------------------------------------------------------------*/
int qfits_table_append_xtension(
        FILE            *   outfile,
        qfits_table     *   t,
        void            **  data) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    Appends a specified extension header + data to a FITS table file.
  @param    outfile     Pointer to (opened) file ready for writing.
  @param    t           Pointer to qfits_table
  @param    data        Table data to write
  @param    hdr         Specified extension header
  @return   int 0 if Ok, -1 otherwise

  Dumps a FITS table to a file. The whole table described by qfits_table, and
  the data arrays contained in 'data' are dumped to the file following the
  specified fits header.
  The output is then padded to reach a multiple of 2880 bytes in size.
  Notice that no main header is produced, only the extension part.
 */
/*----------------------------------------------------------------------------*/
int qfits_table_append_xtension_hdr(
        FILE            *   outfile,
        qfits_table     *   t,
        void            **  data,
        qfits_header    *   hdr) ;

/*----------------------------------------------------------------------------*/
/**
  @brief    given a col and a row, find out the string to write for display
  @param    table   table structure
  @param    col_id  col id (0 -> nbcol-1)
  @param    row_id  row id (0 -> nrow-1)
  @param    use_zero_scale  Flag to use or not zero and scale
  @return   the string to write
 */
/*----------------------------------------------------------------------------*/
char * qfits_table_field_to_string(
        qfits_table     *   table,
        int                 col_id,
        int                 row_id,
        int                 use_zero_scale) ;

/* </dox> */
#ifdef __cplusplus
}
#endif

#endif
/* vim: set ts=4 et sw=4 tw=75 */
