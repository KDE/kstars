/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Use the nomadmysql2bin-split program / the shell script to create binary
// dumps of star data by trixel. Thereafter use this program to merge the
// pieces together and create a well-defined binary file in the binary file
// format described in ../README.binfileformat

#define VERBOSE          1
#define HTM_LEVEL        6
#define NTRIXELS         32768 // TODO: Change if HTM Level Changes
#define INDEX_ENTRY_SIZE 12

#include "binfile.h"

#include <mysql/mysql.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

/*
 * struct to store star data, to be written in this format, into the binary file.
 */

typedef struct deepStarData
{
    int32_t RA;
    int32_t Dec;
    int16_t dRA;
    int16_t dDec;
    int16_t B;
    int16_t V;
} deepStarData;

/*
 * Dump the data file header.
 *
 * WARNING: Must edit every time the definition of the deepStarData structures changes
 *
 * f : Data file handle
 */

int writeDataFileHeader(FILE *f)
{
    char ASCII_text[124];
    u_int16_t nfields;
    u_int32_t nindexes;
    int16_t endian_id   = 0x4B53;
    u_int8_t version_no = 1;

    if (f == NULL)
        return 0;

    nfields = 6;

    str2charv(ASCII_text, "KStars Star Data v1.0. To be read using the 16-bit deepStarData structure only.", 124);
    fwrite(ASCII_text, 124, 1, f);
    fwrite(&endian_id, 2, 1, f);
    fwrite(&version_no, 1, 1, f);
    fwrite(&nfields, sizeof(u_int16_t), 1, f);

    writeDataElementDescription(f, "RA", 4, DT_INT32, 1000000);
    writeDataElementDescription(f, "Dec", 4, DT_INT32, 100000);
    writeDataElementDescription(f, "dRA", 2, DT_INT16, 10);
    writeDataElementDescription(f, "dDec", 2, DT_INT16, 10);
    writeDataElementDescription(f, "B", 2, DT_INT16, 100);
    writeDataElementDescription(f, "V", 2, DT_INT16, 100);

    nindexes = NTRIXELS;
    fwrite(&nindexes, sizeof(u_int32_t), 1, f);

    return 1;
}

int main(int argc, char *argv[])
{
    /* === Declarations === */

    int ret, i, exitflag;
    long int lim;

    double Mag;

    unsigned long us_header_offset;
    unsigned long usf_trix_begin;
    unsigned long usf_trix_count;
    unsigned long ntrixels;
    int16_t maglim;
    u_int8_t htm_level;
    u_int16_t MSpT_unnamed;

    char query[512];
    u_int32_t current_trixel;

    /* File streams */
    FILE *f;       /* Pointer to "current" file */
    FILE *usf;     /* Handle to star data file */
    FILE *usfhead; /* Handle to star header file */

    /* deepStarData structure */
    deepStarData data;

    /* MySQL structures */
    MYSQL link;
    MYSQL_RES *result;
    MYSQL_ROW row;

    /* Check the number of arguments */
    if (argc <= 6)
    {
        fprintf(stderr, "USAGE %s DataFile HeaderFile InputDataFilePrefix\n", argv[0]);
        fprintf(stderr, "The database used is a MySQL DB on localhost. The default table name is `nomad`\n");
    }

    /* == Open all file streams required == */
    /* Unnamed Star Handling */
    usf = fopen(argv[1], "wb");
    if (usf == NULL)
    {
        fprintf(stderr, "ERROR: Could not open %s [Data File] for binary write.\n", argv[1]);
        return 1;
    }

    usfhead = fopen(argv[2], "wb");
    if (usfhead == NULL)
    {
        fprintf(stderr, "ERROR: Could not open %s [Header File] for binary write.\n", argv[2]);
        fcloseall();
        return 1;
    }

    if (VERBOSE)
        fprintf(stdout, "Size of deepStarData structure: %d\n", sizeof(deepStarData));

    /* Write file headers */
    writeDataFileHeader(usfhead);
    us_header_offset = ftell(usfhead);

    /* Leave space in the data file for certain catalog information */
    /* Leave space for / write a deep magnitude limit specification in the data files */
    maglim = (int)(-5.0 * 100);
    fwrite(&maglim, 2, 1, usf); // Bogus entry

    /* Write a HTM level specification in the data file */
    htm_level = HTM_LEVEL;
    fwrite(&htm_level, 1, 1, usf);

    /* Leave space for a specification of MSpT (Maximum Stars per Trixel) in the data files */
    MSpT_unnamed = 0;
    fwrite(&MSpT_unnamed, 2, 1, usf); // Bogus entry

    /* Initialize some variables */
    usf_trix_begin =
        2 + 1 +
        2; // The 2 + 1 + 2 is to leave space for deep magnitude limit, HTM Level and MSpT specification. TODO: Change this if things change.
    ntrixels = 0;

    for (current_trixel = 0; current_trixel < NTRIXELS; ++current_trixel)
    {
        char fname[256];
        sprintf(fname, "%s%d", argv[3], current_trixel);
        FILE *trixdump;
        trixdump = fopen(fname, "rb");
        if (trixdump == NULL)
        {
            fprintf(stderr, "ERROR: Could not open %s for binary read! Exiting with INCOMPLETE data files.\n", fname);
            fcloseall();
            return 1;
        }
        usf_trix_count = 0;
        while (!feof(trixdump))
        {
            if (!fread(&data, sizeof(deepStarData), 1, trixdump))
            {
                if (VERBOSE)
                    fprintf(stdout, "Finished transferring trixel %d (%d records)\n", current_trixel, usf_trix_count);
                break;
            }
            usf_trix_count++;

            int16_t B = data.B;
            int16_t V = data.V;

            if (V == 30000)
            {
                if (B - 1600 > maglim && B != 30000)
                {
                    maglim = B - 1600;
                }
            }
            else
            {
                if (V > maglim)
                {
                    maglim = V;
                }
            }
            fwrite(&data, sizeof(deepStarData), 1, usf);
        }

        fclose(trixdump);

        /* Write index entries if we've changed trixel */
        if (VERBOSE)
        {
            fprintf(stderr, "Done with trixel %d!\n", current_trixel);
        }
        writeIndexEntry(usfhead, current_trixel, us_header_offset + NTRIXELS * INDEX_ENTRY_SIZE + usf_trix_begin,
                        usf_trix_count);
        usf_trix_begin += usf_trix_count * sizeof(deepStarData);
        if (usf_trix_count > MSpT_unnamed)
            MSpT_unnamed = usf_trix_count;
        ntrixels++;
        usf_trix_count = 0;
    }

    if (ntrixels != NTRIXELS)
    {
        fprintf(stderr,
                "ERROR: Expected %u trixels, but found %u instead. Please redefine NTRIXELS in this program, or check "
                "the source database for bogus trixels\n",
                NTRIXELS, ntrixels);
    }

    rewind(usf);
    fwrite(&maglim, 2, 1, usf);
    fwrite(&htm_level, 1, 1, usf);
    fwrite(&MSpT_unnamed, 2, 1, usf);

    fcloseall();

    return 0;
}
