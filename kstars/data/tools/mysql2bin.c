/* 
 * Program to convert star data stored in a MySQL database into the binary file format used by KStars
 * Start Date: 26th May 2008 ; Author: Akarsh Simha <akarshsimha@gmail.com>
 * License: GPL
 */

#include <mysql/mysql.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define TRIXEL_NAME_SIZE 8
#define DB_TBL "allstars"
#define DB_DB "stardb"
#define VERBOSE 0
#define LONG_NAME_LIMIT 32
#define BAYER_LIMIT 8
#define HTM_LEVEL 3
#define NTRIXELS 512
#define INDEX_ENTRY_SIZE 8
#define GLOBAL_MAG_LIMIT 8.00
#define MYSQL_STARS_PER_QUERY 400

/*
 * struct to store star data, to be written in this format, into the binary file.
 */

typedef struct starData {
  int32_t RA;
  int32_t Dec;
  int32_t dRA;
  int32_t dDec;
  int32_t parallax;
  int32_t HD;
  int16_t mag;
  int16_t bv_index;
  char spec_type[2];
  char flags;
  char unused;
} starData;

/*
 * struct to store star name data, to be written in this format, into the star name binary file.
 */

typedef struct starName {
  char bayerName[BAYER_LIMIT];
  char longName[LONG_NAME_LIMIT];
} starName;

/*
 * enum listing out various possible data types
 */

enum dataType {
  DT_CHAR,              /* Character */
  DT_INT8,              /* 8-bit Integer */
  DT_UINT8,             /* 8-bit Unsigned Integer */
  DT_INT16,             /* 16-bit Integer */
  DT_UINT16,            /* 16-bit Unsigned Integer */
  DT_INT32,             /* 32-bit Integer */
  DT_UINT32,            /* 32-bit Unsigned Integer */
  DT_CHARV,             /* Fixed-length array of characters */
  DT_STR,               /* Variable length array of characters, either terminated by NULL or by the limit on field size */
  DT_SPCL = 128         /* Flag indicating that the field requires special treatment (eg: Different bits may mean different things) */
};

/*
 * struct to store the description of a field / data element in the binary files
 */

typedef struct dataElement {
  char name[10];
  int8_t size;
  u_int8_t type;
  int32_t scale;
} dataElement;


/*
 * Convert a string to an int32_t with a double as an intermediate
 * i : A pointer to the target int32_t
 * str : A pointer to the string that carries the data
 * ndec : Number of decimal places to truncate to
 */

int str2int32(int32_t *i, const char *str, int ndec) {

  double dbl;

  if(i == NULL)
    return 0;

  dbl = atof(str);

  *i = (int32_t)(round(dbl * pow(10, ndec)));

  return 1;

}

/*
 * Convert a string to an int16_t with a double as an intermediate
 * i : A pointer to the target int16_t
 * str : The string that carries the data
 * ndec : Number of decimal places to truncate to
 */

int str2int16(int16_t *i, const char *str, int ndec) {

  double dbl;

  if(i == NULL || str == NULL)
    return 0;

  dbl = atof(str);

  *i = (int16_t)(round(dbl * pow(10, ndec)));

  return 1;
}

/*
 * Convert a string into a character array for n characters
 * a : The target array
 * str : The string that carries the data
 * n : Number of characters to convert
 */

int str2charv(char *a, const char *str, int n) {

  int i, ret;

  if(a == NULL || str == NULL)
    return 0;

  ret = 1;

  for(i = 0; i < n; ++i) {
    a[i] = ((ret < 0)? '\0' : str[i]);
    if(str[i] == '\0')                /* We can do this safely because we aren't storing binary data in the DB */
      ret = -1;
  }

  return ret;
}

/*
 * Check whether the string passed is blank
 * str : String to check
 * returns 1 if the string is blank, 0 otherwise.
 */

int isblank(char *str) {

  if(str == NULL)
    return 1;

  while(*str != '\0') {
    if(*str != ' ' && *str != '\n' && *str != '\r' && *str != '\t')
      return 0;
    ++str;
  }

  return 1;
}

/*
 * Write one data element description into a binary file header.
 *
 * f    : Handle of file to write into
 * name : Name of the field, as a string. Max as specified in struct dataElement
 * size : Size (in bytes) of the field
 * type : Type of the field, as specified in enum dataType
 * scale : Scale factor used for conversion of fixed-point reals to integers. N/A to DT_CHARV, DT_STR and DT_CHAR
 */

int writeDataElementDescription(FILE *f, char *name, int8_t size, enum dataType type, int32_t scale) {
  struct dataElement de;
  if(f == NULL || name == NULL)
    return 0;
  str2charv(de.name, name, 10);
  de.size = size;
  de.type = type;
  de.scale = scale;
  fwrite(&de, sizeof(struct dataElement), 1, f);
  return 1;
}

/*
 * Dump the data file header.
 *
 * WARNING: Must edit everytime the definition of the starData structures changes
 *
 * f : Data file handle
 */

int writeDataFileHeader(FILE *f) {

  char ASCII_text[124];
  u_int16_t nfields;
  u_int16_t nindexes;
  int16_t endian_id=0x4B53;

  if(f == NULL)
    return 0;

  nfields = 11;

  str2charv(ASCII_text, "KStars Star Data v1.0. To be read using the 32-bit starData structure only.", 124);
  fwrite(ASCII_text, 124, 1, f);
  fwrite(&endian_id, 2, 1, f);
  fwrite(&nfields, sizeof(u_int16_t), 1, f);

  writeDataElementDescription(f, "RA", 4, DT_INT32, 1000000);
  writeDataElementDescription(f, "Dec", 4, DT_INT32, 100000);
  writeDataElementDescription(f, "dRA", 4, DT_INT32, 10);
  writeDataElementDescription(f, "dDec", 4, DT_INT32, 10);
  writeDataElementDescription(f, "parallax", 4, DT_INT32, 10);
  writeDataElementDescription(f, "HD", 4, DT_INT32, 10);
  writeDataElementDescription(f, "mag", 2, DT_INT16, 100);
  writeDataElementDescription(f, "bv_index", 2, DT_INT16, 100);
  writeDataElementDescription(f, "spec_type", 2, DT_CHARV, 0);
  writeDataElementDescription(f, "flags", 1, DT_CHAR, 0);
  writeDataElementDescription(f, "unused", 1, DT_CHAR, 100);

  nindexes = NTRIXELS;
  fwrite(&nindexes, sizeof(u_int16_t), 1, f);

  return 1;
}

/*
 * Dump the name file header.
 *
 * WARNING: Must edit everytime the definition of the starName structures changes
 *
 * nf : Name file handle
 */

int writeNameFileHeader(FILE *nf) {

  char ASCII_text[124];
  u_int16_t nfields;
  u_int16_t nindexes;
  int16_t endian_id=0x4B53;

  if(nf == NULL)
    return 0;

  nfields = 2;

  str2charv(ASCII_text, "KStars Star Name Data v1.0. Refer associated documentation for format description.", 124);
  fwrite(ASCII_text, 124, 1, nf);
  fwrite(&endian_id, 2, 1, nf);
  fwrite(&nfields, sizeof(u_int16_t), 1, nf);

  writeDataElementDescription(nf, "bayerName", BAYER_LIMIT, DT_STR, 0);
  writeDataElementDescription(nf, "longName", LONG_NAME_LIMIT, DT_STR, 0);

  nindexes = 0;
  fwrite(&nindexes, sizeof(u_int16_t), 1, nf);

  return 1;
}

/*
 * Write an index entry
 *
 * hf : File to write index entry into
 * name : Name of the trixel
 * offset : File offset for this trixel
 * nrec : Number of stars under this trixel
 */

int writeIndexEntry(FILE *hf, char *name, u_int32_t offset, u_int16_t nrec) {

  u_int16_t trixel_id;

  if(hf == NULL)
    return 0;

  if(name == NULL)
    return 0;

  trixel_id = trixel2number(name);
  fwrite(&trixel_id, sizeof(u_int16_t), 1, hf);
  fwrite(&offset, 4, 1, hf);
  fwrite(&nrec, 2, 1, hf);

  /* Put this just for safety, in case we change our mind - we should avoid screwing things up */
  if(2 + 4 + 2 != INDEX_ENTRY_SIZE) {
    fprintf(stderr, "CODE ERROR: 2 + 4 + 2 != INDEX_ENTRY_SIZE\n");
  }

  return 1;
}

/*
 * "Increments" a trixel
 *
 * str : String to hold the incremented trixel
 */

void nextTrixel(char *trixel) {

  char *ptr = trixel + HTM_LEVEL + 1;
  while(ptr > trixel) {
    *ptr = *ptr + 1;
    if(*ptr != '4')
      break;
    *ptr = '0';
    ptr--;
  }
  if(*ptr == 'N')
    *ptr = 'S';
  else if(*ptr == 'S')
    *ptr = '0';
}

/*
 * Finds the ID of a given trixel
 * WARNING: Implemented only for a Level 3 HTMesh
 * NOTE: This function does not test whether the given input is sane
 *
 * trixel : String representation of the trixel to be converted
 */

inline int trixel2number(char *trixel) {
  return (trixel[4] - '0') + (trixel[3] - '0') * 4 + (trixel[2] - '0') * 16 + (trixel[1] - '0') * 64 + ((trixel[0] == 'N')?0:256);
}


int main(int argc, char *argv[]) {

  /* === Declarations === */

  int ret, i, exitflag;
  long int lim;
  char named;

  unsigned long ns_header_offset, us_header_offset;
  unsigned long nsf_trix_begin, usf_trix_begin;
  unsigned long nsf_trix_count, usf_trix_count;
  unsigned long ntrixels;

  char query[512];
  char current_trixel[HTM_LEVEL + 2 + 1];

  /* File streams */
  FILE *f;                 /* Pointer to "current" file */
  FILE *nsf, *usf;         /* Handles of named and unnamed star data files */
  FILE *nsfhead, *usfhead; /* Handles of named and unnamed star header files */
  FILE *namefile;          /* Pointer to the name file */

  /* starData and starName structures */
  starData data;
  starName name;

  /* MySQL structures */
  MYSQL link;
  MYSQL_RES *result;
  MYSQL_ROW row;

  /* Check the number of arguments */
  if(argc <= 8) {
    fprintf(stderr, 
	    "USAGE %s DBUserName DBPassword DeepStarDataFile DeepStarHeaderFile ShallowStarDataFile ShallowStarHeaderFile StarNameFile DBName [TableName]\n", 
	    argv[0]);
    fprintf(stderr, "The magnitude limit for a Shallow Star is set in the program source using GLOBAL_MAG_LIMIT\n");
    fprintf(stderr, "The database used is a MySQL DB on localhost. The default table name is `allstars`\n");
  }

  /* == Open all file streams required == */
  /* Unnamed Star Handling */
  usf = fopen(argv[3], "wb");
  if(usf == NULL) {
    fprintf(stderr, "ERROR: Could not open %s [Deep Star Data File] for binary write.\n", argv[3]);
    return 1;
  }

  usfhead = fopen(argv[4], "wb");
  if(usfhead == NULL) {
    fprintf(stderr, "ERROR: Could not open %s [Deep Star Header File] for binary write.\n", argv[4]);
    fcloseall();
    return 1;
  }

  /* Bright / Named Star Handling */
  nsf = fopen(argv[5], "wb");
  if(nsf == NULL) {
    fprintf(stderr, "ERROR: Could not open %s [Shallow Star Data File] for binary write.\n", argv[5]);
    fcloseall();
    return 1;
  }

  nsfhead = fopen(argv[6], "wb");
  if(nsfhead == NULL) {
    fprintf(stderr, "ERROR: Could not open %s [Shallow Star Header File] for binary write.\n", argv[6]);
    fcloseall();
    return 1;
  }

  namefile = fopen(argv[7], "wb");
  if(namefile == NULL) {
    fprintf(stderr, "ERROR: Could not open %s [Star Name File] for binary write.\n", argv[7]);
    fcloseall();
    return 1;
  }


  /* MySQL connect */
  if(mysql_init(&link) == NULL) {
    fprintf(stderr, "ERROR: Failed to initialize MySQL connection!\n");
    return 1;
  }

  ret = mysql_real_connect(&link, "localhost", argv[1], argv[2], argv[8], 0, NULL, 0);

  if(!ret) {
    fprintf(stderr, "ERROR: MySQL connect failed for the following reason: %s\n", mysql_error(&link));
    fcloseall();
    return 1;
  }

  if(mysql_select_db(&link, argv[8])) {
    fprintf(stderr, "ERROR: Could not select MySQL database %s. MySQL said: %s", argv[8], mysql_error(&link));
    fcloseall();
    mysql_close(&link);
    return 1;
  }

  /* Write file headers */
  writeNameFileHeader(namefile);
  writeDataFileHeader(nsfhead);
  writeDataFileHeader(usfhead);
  ns_header_offset = ftell(nsfhead);
  us_header_offset = ftell(usfhead);

  /* Initialize some variables */
  lim = 0;
  exitflag = 0;
  strcpy(current_trixel, "N0000");
  nsf_trix_count = usf_trix_count = 0;
  nsf_trix_begin = usf_trix_begin = 0;
  ntrixels = 0;

  /* Recurse over every MYSQL_STARS_PER_QUERY DB entries */
  while(!exitflag) {

    /* Build MySQL query for next MYSQL_STARS_PER_QUERY stars */
      sprintf(query, 
	      "SELECT `trixel`, `ra`, `dec`, `dra`, `ddec`, `parallax`, `mag`, `bv_index`, `spec_type`, `mult`, `var_range`, `var_period`, `UID`, `name`, `gname` FROM `%s` ORDER BY `trixel`, `mag` ASC LIMIT %ld, %d", 
	      (argc >= 10) ? argv[9] : DB_TBL, lim, MYSQL_STARS_PER_QUERY);

    if(VERBOSE) { fprintf(stderr, "SQL Query: %s\n", query); }
    
    /* Execute MySQL query and get the result */
    mysql_real_query(&link, query, (unsigned int)strlen(query)); 
    result = mysql_use_result(&link);
    
    if(!result) {
      fprintf(stderr, "MySQL returned NULL result: %s\n", mysql_error(&link));
      fcloseall();
      mysql_close(&link);
      return 1;
    }

    exitflag = -1;

    /* Process the result row by row */
    while(row = mysql_fetch_row(result)) {
      
      /* Very verbose details */
      if(VERBOSE > 1) {
	fprintf(stderr, "UID = %s\n", row[12]);
	for(i = 0; i <= 14; ++i)
	  fprintf(stderr, "\tField #%d = %s\n", i, row[i]);
      }

      if(exitflag == -1)
	exitflag = 0;

      /* Write index entries if we've changed trixel */
      if(strcmp(row[0], current_trixel)) { 
	if(VERBOSE) { fprintf(stderr, "Trixel Changed from %s to %s!\n", current_trixel, row[0]); }
	while(strcmp(row[0],current_trixel)) {
	  writeIndexEntry(nsfhead, current_trixel, ns_header_offset + NTRIXELS * INDEX_ENTRY_SIZE + nsf_trix_begin, nsf_trix_count);
	  writeIndexEntry(usfhead, current_trixel, us_header_offset + NTRIXELS * INDEX_ENTRY_SIZE + usf_trix_begin, usf_trix_count);
	  nsf_trix_begin = ftell(nsf);
	  usf_trix_begin = ftell(usf);
	  nsf_trix_count = usf_trix_count = 0;
	  nextTrixel(current_trixel);
	  ntrixels++;
	}
      }

      /* ==== Set up the starData structure ==== */

      /* Initializations */
      data.flags = 0;

      /* This data is required early! */
      str2int16(&data.mag, row[6], 2);

      /* Are we looking at a named star */
      named = 0;
      if(!isblank(row[13]) || !isblank(row[14])) {
	named = 1;

	/* Print out messages */
	if(VERBOSE) 
	  fprintf(stderr, "Named Star!\n");
	if(VERBOSE > 1)
	  fprintf(stderr, "Bayer Name = %s, Long Name = %s\n", row[14], row[13]);

	/* Check for overflows */
	if(strlen(row[13]) > LONG_NAME_LIMIT)
	  fprintf(stderr,
		  "ERROR: Long Name %s with length %d exceeds LONG_NAME_LIMIT = %d\n", 
		  row[13], strlen(row[13]), LONG_NAME_LIMIT);
	if(strlen(row[14]) > BAYER_LIMIT)
	  fprintf(stderr, 
		  "ERROR: Bayer designation %s with length %d exceeds BAYER_LIMIT = %d\n",
		  row[14], strlen(row[14]), BAYER_LIMIT);
	
	/* Set up the starName structure */
	str2charv(name.bayerName, row[14], BAYER_LIMIT);
	str2charv(name.longName, row[13], LONG_NAME_LIMIT);
	data.flags = data.flags | 0x01; /* Switch on the 'named' bit */
      }

      /* Are we looking at a 'global' star [always in memory] or dynamically loaded star? */
      if(named || (data.mag/100.0) <= GLOBAL_MAG_LIMIT) {
	f = nsf;
	nsf_trix_count++;
      }
      else {
	usf_trix_count++;
	f = usf;
      }

      /* Convert various fields and make entries into the starData structure */
      str2int32(&data.RA, row[1], 6);
      str2int32(&data.Dec, row[2], 5);
      str2int32(&data.dRA, row[3], 1);
      str2int32(&data.dDec, row[4], 1);
      str2int32(&data.parallax, row[5], 1);
      str2int32(&data.HD, "", 0);                  /* TODO: Put HD data into MySQL DB */
      str2int16(&data.bv_index, row[7], 2);
      if(str2charv(data.spec_type, row[8], 2) < 0)
	fprintf(stderr, "Spectral type entry %s in DB is possibly invalid for UID = %s\n", row[8], row[12]); 
      if(row[9][0] != '0' && row[9][0] != '\0')
	data.flags = data.flags | 0x02;
      if(!isblank(row[10]) || !isblank(row[11]))
	data.flags = data.flags | 0x04;

      /* Write the data into the appropriate data file and any names into the name file */
      if(VERBOSE) 
	fprintf(stderr, "Writing UID = %s...", row[12]);
      fwrite(&data, sizeof(starData), 1, f);
      if(named) {
	fwrite(&name.bayerName, BAYER_LIMIT, 1, namefile);
	fwrite(&name.longName, LONG_NAME_LIMIT, 1, namefile);
      }
      if(VERBOSE) 
	fprintf(stderr, "Done.\n");

    }
    
    mysql_free_result(result);
    lim += MYSQL_STARS_PER_QUERY;
  }

  do {
    writeIndexEntry(nsfhead, current_trixel, ns_header_offset + NTRIXELS * INDEX_ENTRY_SIZE + nsf_trix_begin, nsf_trix_count);
    writeIndexEntry(usfhead, current_trixel, us_header_offset + NTRIXELS * INDEX_ENTRY_SIZE + usf_trix_begin, usf_trix_count);
    nsf_trix_begin = ftell(nsf);
    usf_trix_begin = ftell(usf);
    nsf_trix_count = usf_trix_count = 0;
    nextTrixel(current_trixel);
    ntrixels++;
  }while(strcmp(current_trixel,"00000"));

  if(ntrixels != NTRIXELS) {
    fprintf(stderr, "ERROR: Expected %u trixels, but found %u instead. Please redefine NTRIXELS in this program, or check the source database for bogus trixels\n", NTRIXELS, ntrixels);
  }

  fcloseall();
  mysql_close(&link);
    
  return 0;
}
