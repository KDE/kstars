/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Program to convert star data stored in a MySQL database into
// the binary file format used by KStars

#define DB_TBL                "nomad"
#define DB_DB                 "stardb"
#define VERBOSE               1
#define HTM_LEVEL             6
#define NTRIXELS              32768 // TODO: Change if HTM Level Changes
#define INDEX_ENTRY_SIZE      12
#define MYSQL_STARS_PER_QUERY 1000000

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

    str2charv(ASCII_text, "KStars Star Data v1.0. To be read using the 32-bit starData structure only.", 124);
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
        fprintf(stderr, "USAGE %s DBUserName DBPassword DeepStarDataFile DeepStarHeaderFile DBName [TableName]\n",
                argv[0]);
        fprintf(stderr, "The database used is a MySQL DB on localhost. The default table name is `nomad`\n");
    }

    /* == Open all file streams required == */
    /* Unnamed Star Handling */
    usf = fopen(argv[3], "wb");
    if (usf == NULL)
    {
        fprintf(stderr, "ERROR: Could not open %s [Deep Star Data File] for binary write.\n", argv[3]);
        return 1;
    }

    usfhead = fopen(argv[4], "wb");
    if (usfhead == NULL)
    {
        fprintf(stderr, "ERROR: Could not open %s [Deep Star Header File] for binary write.\n", argv[4]);
        fcloseall();
        return 1;
    }

    /* MySQL connect */
    if (mysql_init(&link) == NULL)
    {
        fprintf(stderr, "ERROR: Failed to initialize MySQL connection!\n");
        return 1;
    }

    ret = mysql_real_connect(&link, "localhost", argv[1], argv[2], argv[5], 0, NULL, 0);

    if (!ret)
    {
        fprintf(stderr, "ERROR: MySQL connect failed for the following reason: %s\n", mysql_error(&link));
        fcloseall();
        return 1;
    }

    if (mysql_select_db(&link, argv[5]))
    {
        fprintf(stderr, "ERROR: Could not select MySQL database %s. MySQL said: %s", argv[5], mysql_error(&link));
        fcloseall();
        mysql_close(&link);
        return 1;
    }

    if (VERBOSE)
        fprintf(stdout, "Size of deepStarData structure: %d\n", sizeof(deepStarData));

    /* Write file headers */
    writeDataFileHeader(usfhead);
    us_header_offset = ftell(usfhead);

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
    lim            = 0;
    exitflag       = 0;
    current_trixel = 0;
    usf_trix_count = 0;
    usf_trix_begin =
        2 + 1 +
        2; // The 2 + 1 + 2 is to leave space for deep magnitude limit, HTM Level and MSpT specification. TODO: Change this if things change.
    ntrixels = 0;

    /* Recurse over every MYSQL_STARS_PER_QUERY DB entries */
    while (!exitflag)
    {
        /* Build MySQL query for next MYSQL_STARS_PER_QUERY stars */
        sprintf(query,
                "SELECT `Trixel`, `RA`, `Dec`, `dRA`, `dDec`, `B`, `V`, `UID` FROM `%s` ORDER BY `trixel`, `Mag` ASC "
                "LIMIT %ld, %d",
                (argc >= 7) ? argv[6] : DB_TBL, lim, MYSQL_STARS_PER_QUERY);

        if (VERBOSE)
        {
            fprintf(stderr, "SQL Query: %s\n", query);
        }

        /* Execute MySQL query and get the result */
        mysql_real_query(&link, query, (unsigned int)strlen(query));
        result = mysql_use_result(&link);

        if (!result)
        {
            fprintf(stderr, "MySQL returned NULL result: %s\n", mysql_error(&link));
            fcloseall();
            mysql_close(&link);
            return 1;
        }

        exitflag = -1;

        /* Process the result row by row */
        while (row = mysql_fetch_row(result))
        {
            /* Very verbose details */
            if (VERBOSE > 1)
            {
                fprintf(stderr, "UID = %s\n", row[12]);
                for (i = 0; i <= 14; ++i)
                    fprintf(stderr, "\tField #%d = %s\n", i, row[i]);
            }

            if (exitflag == -1)
                exitflag = 0;

            /* Write index entries if we've changed trixel */
            u_int32_t new_trixel = atoi(row[0]);
            if (current_trixel != new_trixel)
            {
                if (VERBOSE)
                {
                    fprintf(stderr, "Trixel Changed from %d to %s = %d!\n", current_trixel, row[0], new_trixel);
                }
                while (new_trixel != current_trixel)
                {
                    writeIndexEntry(usfhead, current_trixel,
                                    us_header_offset + NTRIXELS * INDEX_ENTRY_SIZE + usf_trix_begin, usf_trix_count);
                    usf_trix_begin = ftell(usf);
                    if (usf_trix_count > MSpT_unnamed)
                        MSpT_unnamed = usf_trix_count;
                    current_trixel++;
                    ntrixels++;
                    usf_trix_count = 0;
                    if (VERBOSE)
                    {
                        fprintf(stderr, "%d %d\n", current_trixel - 1, ntrixels);
                    }
                }
                if (VERBOSE)
                {
                    fprintf(stderr, "Done writing trixel entry\n");
                }
            }

            /* Initializations */

            usf_trix_count++;
            f = usf;

            /* Convert various fields and make entries into the deepStarData structure */
            str2int32(&data.RA, row[1], 6);
            str2int32(&data.Dec, row[2], 5);
            str2int16(&data.dRA, row[3], 3);
            str2int16(&data.dDec, row[4], 3);
            str2int16(&data.B, row[5], 3);
            str2int16(&data.V, row[6], 3);

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

            /* Write the data into the appropriate data file and any names into the name file */
            if (VERBOSE)
                fprintf(stderr, "Writing UID = %s...", row[7]);
            fwrite(&data, sizeof(deepStarData), 1, f);
            if (VERBOSE)
                fprintf(stderr, "Done.\n");
        }
        mysql_free_result(result);
        lim += MYSQL_STARS_PER_QUERY;
    }

    do
    {
        writeIndexEntry(usfhead, current_trixel, us_header_offset + NTRIXELS * INDEX_ENTRY_SIZE + usf_trix_begin,
                        usf_trix_count);
        usf_trix_begin = ftell(usf);
        usf_trix_count = 0;
        current_trixel++;
        ntrixels++;
    } while (current_trixel != NTRIXELS);

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
    mysql_close(&link);

    return 0;
}
