/*----------------------------------------------------------------------------*/
/**
   @file    pafs.c
   @author  Nicolas Devillard
   @date    February 1999
   @version	$Revision$
   @brief   paf format I/O
*/
/*----------------------------------------------------------------------------*/

/*
	$Id$
	$Author$
	$Date$
	$Revision$
*/

/*-----------------------------------------------------------------------------
   								Includes
 -----------------------------------------------------------------------------*/

#include "qerror.h"
#include "pafs.h"
#include "t_iso8601.h"

/*-----------------------------------------------------------------------------
   								Defines
 -----------------------------------------------------------------------------*/

#define PAF_MAGIC       "PAF.HDR.START"
#define PAF_MAGIC_SZ    13

/*-----------------------------------------------------------------------------
   							Function prototypes
 -----------------------------------------------------------------------------*/
static char * qfits_strcrop(char * s) ;

/*-----------------------------------------------------------------------------
  							Function codes
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief	Open a new PAF file, output a default header.
  @param	filename	Name of the file to create.
  @param	paf_id		PAF identificator.
  @param	paf_desc	PAF description.
  @param    login_name  Login name
  @param    datetime    Date
  @return	Opened file pointer.

  This function creates a new PAF file with the requested file name.
  If another file already exists with the same name, it will be
  overwritten (if the file access rights allow it).

  A default header is produced according to the VLT DICB standard. You
  need to provide an identificator (paf_id) of the producer of the
  file. Typically, something like "ISAAC/zero_point".

  The PAF description (paf_desc) is meant for humans. Typically,
  something like "Zero point computation results".

  This function returns an opened file pointer, ready to receive more
  data through fprintf's. The caller is responsible for fclose()ing
  the file.
 */
/*----------------------------------------------------------------------------*/
FILE * qfits_paf_print_header(
	    char    *   filename,
	    char    *  	paf_id,
	    char    *	paf_desc,
        char    *   login_name,
        char    *   datetime)
{
	FILE * paf ;
    
    if ((paf=fopen(filename, "w"))==NULL) {
		qfits_error("cannot create PAF file [%s]", filename);
		return NULL ;
	}
	fprintf(paf, "PAF.HDR.START         ;# start of header\n");
	fprintf(paf, "PAF.TYPE              \"pipeline product\" ;\n");
	fprintf(paf, "PAF.ID                \"%s\"\n", paf_id);
	fprintf(paf, "PAF.NAME              \"%s\"\n", filename);
	fprintf(paf, "PAF.DESC              \"%s\"\n", paf_desc);
	fprintf(paf, "PAF.CRTE.NAME         \"%s\"\n", login_name) ;
	fprintf(paf, "PAF.CRTE.DAYTIM       \"%s\"\n", datetime) ;
	fprintf(paf, "PAF.LCHG.NAME         \"%s\"\n", login_name) ;
	fprintf(paf, "PAF.LCHG.DAYTIM       \"%s\"\n", datetime) ;
	fprintf(paf, "PAF.CHCK.CHECKSUM     \"\"\n");
	fprintf(paf, "PAF.HDR.END           ;# end of header\n");
	fprintf(paf, "\n");
	return paf ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief	Query a PAF file for a value.
  @param	filename	Name of the PAF to query.
  @param	key			Name of the key to query.
  @return	1 pointer to statically allocated string, or NULL.

  This function parses a PAF file and returns the value associated to a
  given key, as a pointer to an internal statically allocated string.
  Do not try to free or modify the contents of the returned string!

  If the key is not found, this function returns NULL.
 */
/*----------------------------------------------------------------------------*/
char * qfits_paf_query(
        char    *   filename, 
        char    *   key)
{
	static char value[ASCIILINESZ];
	FILE	*	paf ;
	char		line[ASCIILINESZ+1];
	char		val[ASCIILINESZ+1];
	char		head[ASCIILINESZ+1];
	int			found ;
	int			len ;

	/* Check inputs */
	if (filename==NULL || key==NULL) return NULL ;

	/* Check PAF validity */
	if (qfits_is_paf_file(filename)!=1) {
		qfits_error("not a PAF file: [%s]", filename);
		return NULL ;
	}

	/* Open file and read it */
	paf = fopen(filename, "r");
	if (paf==NULL) {
		qfits_error("opening [%s]", filename);
		return NULL ;
	}
	
	found = 0 ;
	while (fgets(line, ASCIILINESZ, paf)!=NULL) {
		sscanf(line, "%[^ ]", head);
		if (!strcmp(head, key)) {
			/* Get value */
			sscanf(line, "%*[^ ] %[^;]", value);
			found ++ ;
			break ;
		}
	}
	if (!found) return NULL ;
	
	/* Remove trailing blanks */
	strcpy(val, qfits_strcrop(value));
	/* Get rid of possible quotes */
	len = strlen(val);
	if (val[0]=='\"' && val[len-1]=='\"') {
		strncpy(value, val+1, len-2);
		value[len-2]=(char)0;
	} else {
		strcpy(value, val);
	}
	return value ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief	returns 1 if file is in PAF format, 0 else
  @param	filename name of the file to check
  @return	int 0, 1, or -1
  Returns 1 if the file name corresponds to a valid PAF file. Returns
  0 else. If the file does not exist, returns -1. Validity of the PAF file 
  is checked with the presence of PAF.HDR.START at the beginning
 */
/*----------------------------------------------------------------------------*/
int qfits_is_paf_file(char * filename)
{
	FILE	*	fp ;
	int			is_paf ;
	char		line[ASCIILINESZ] ;
	
	if (filename==NULL) return -1 ;

	/* Initialize is_paf */
	is_paf = 0 ;

	/* Open file */
	if ((fp = fopen(filename, "r"))==NULL) {
		qfits_error("cannot open file [%s]", filename) ;
		return -1 ;
	}

	/* Parse file */
	while (fgets(line, ASCIILINESZ, fp) != NULL) {
		if (line[0] != '#') {
			if (!strncmp(line, PAF_MAGIC, PAF_MAGIC_SZ)) is_paf = 1 ;
			(void)fclose(fp) ;
			return is_paf ;
		}
	}

	(void)fclose(fp) ;
	return is_paf ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Remove blanks at the end of a string.
  @param    s   String to parse.
  @return   ptr to statically allocated string.

  This function returns a pointer to a statically allocated string,
  which is identical to the input string, except that all blank
  characters at the end of the string have been removed.
  Do not free or modify the returned string! Since the returned string
  is statically allocated, it will be modified at each function call
  (not re-entrant).
 */
/*----------------------------------------------------------------------------*/
static char * qfits_strcrop(char * s)
{
    static char l[ASCIILINESZ+1];
    char * last ;

    if (s==NULL) return NULL ;
    memset(l, 0, ASCIILINESZ+1);
    strcpy(l, s);
    last = l + strlen(l);
    while (last > l) {
        if (!isspace((int)*(last-1)))
            break ;
        last -- ;
    }
    *last = (char)0;
    return l ;
}

/* vim: set ts=4 et sw=4 tw=75 */
