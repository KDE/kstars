/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Program to convert star data stored in a MySQL database into
// the binary file format used by KStars

#define DB_TBL                "tycho2"
#define DB_DB                 "stardb"
#define VERBOSE               0
#define LONG_NAME_LIMIT       32
#define BAYER_LIMIT           8
#define HTM_LEVEL             3
#define NTRIXELS              512 // TODO: Change if HTM Level Changes
#define INDEX_ENTRY_SIZE      12
#define GLOBAL_MAG_LIMIT      8.00
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

typedef struct starData
{
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

typedef struct starName
{
    char bayerName[BAYER_LIMIT];
    char longName[LONG_NAME_LIMIT];
} starName;

/*
 * Dump the data file header.
 *
 * WARNING: Must edit every time the definition of the starData structures changes
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

    nfields = 11;

    str2charv(ASCII_text, "KStars Star Data v1.0. To be read using the 32-bit starData structure only.", 124);
    fwrite(ASCII_text, 124, 1, f);
    fwrite(&endian_id, 2, 1, f);
    fwrite(&version_no, 1, 1, f);
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
    fwrite(&nindexes, sizeof(u_int32_t), 1, f);

    return 1;
}

/*
 * Dump the name file header.
 *
 * WARNING: Must edit every time the definition of the starName structures changes
 *
 * nf : Name file handle
 */

int writeNameFileHeader(FILE *nf)
{
    char ASCII_text[124];
    u_int16_t nfields;
    u_int32_t nindexes;
    int16_t endian_id     = 0x4B53;
    u_int32_t data_offset = 0;
    u_int8_t version_no   = 1;

    if (nf == NULL)
        return 0;

    nfields = 2;

    str2charv(ASCII_text, "KStars Star Name Data v1.0. Refer associated documentation for format description.", 124);
    fwrite(ASCII_text, 124, 1, nf);
    fwrite(&endian_id, 2, 1, nf);
    fwrite(&version_no, 1, 1, nf);
    fwrite(&nfields, sizeof(u_int16_t), 1, nf);

    writeDataElementDescription(nf, "bayerName", BAYER_LIMIT, DT_STR, 0);
    writeDataElementDescription(nf, "longName", LONG_NAME_LIMIT, DT_STR, 0);

    nindexes = 1;
    fwrite(&nindexes, sizeof(u_int32_t), 1, nf);

    return 1;
}

int main(int argc, char *argv[])
{
    /* === Declarations === */

    int ret, i, exitflag;
    long int lim;
    char named, shallow;

    unsigned long ns_header_offset, us_header_offset, ds_header_offset;
    unsigned long nsf_trix_begin, usf_trix_begin, dsf_trix_begin;
    unsigned long nsf_trix_count, usf_trix_count, dsf_trix_count;
    unsigned long ntrixels;
    unsigned long nf_header_offset;
    unsigned int names_count;
    int16_t maglim;
    u_int8_t htm_level;
    u_int16_t MSpT_named, MSpT_unnamed, MSpT_deep;

    char query[512];
    u_int32_t current_trixel, new_trixel;

    /* File streams */
    FILE *f;                           /* Pointer to "current" file */
    FILE *nsf, *usf, *dsf;             /* Handles of named and unnamed star data files */
    FILE *nsfhead, *usfhead, *dsfhead; /* Handles of named and unnamed star header files */
    FILE *namefile;                    /* Pointer to the name file */
    FILE *hdindex;                     /* Pointer to the HD Index file */

    /* starData and starName structures */
    starData data;
    starName name;

    /* MySQL structures */
    MYSQL link;
    MYSQL_RES *result;
    MYSQL_ROW row;

    /* Check the number of arguments */
    if (argc <= 9)
    {
        fprintf(stderr,
                "USAGE %s DBUserName DBPassword DBName UnnamedStarDataFile UnnamedStarHeaderFile DeepStarDataFile "
                "DeepStarHeaderFile ShallowStarDataFile ShallowStarHeaderFile StarNameFile HDIndex [TableName]\n",
                argv[0]);
        fprintf(stderr, "The magnitude limit for a Shallow Star is set in the program source using GLOBAL_MAG_LIMIT\n");
        fprintf(stderr, "The database used is a MySQL DB on localhost. The default table name is `allstars`\n");
        fprintf(stderr, "HDIndex must contain 360000 blank records of 32 bits each. You can use dd if=/dev/zero of=... "
                        "... to create it\n");
    }

    /* == Open all file streams required == */
    /* Unnamed Star Handling */
    usf = fopen(argv[4], "wb");
    if (usf == NULL)
    {
        fprintf(stderr, "ERROR: Could not open %s [Deep Star Data File] for binary write.\n", argv[4]);
        return 1;
    }

    usfhead = fopen(argv[5], "wb");
    if (usfhead == NULL)
    {
        fprintf(stderr, "ERROR: Could not open %s [Deep Star Header File] for binary write.\n", argv[5]);
        fcloseall();
        return 1;
    }

    dsf = fopen(argv[6], "wb");
    if (usfhead == NULL)
    {
        fprintf(stderr, "ERROR: Could not open %s [Deep Star Header File] for binary write.\n", argv[6]);
        fcloseall();
        return 1;
    }

    dsfhead = fopen(argv[7], "wb");
    if (usfhead == NULL)
    {
        fprintf(stderr, "ERROR: Could not open %s [Deep Star Header File] for binary write.\n", argv[7]);
        fcloseall();
        return 1;
    }

    /* Bright / Named Star Handling */
    nsf = fopen(argv[8], "wb");
    if (nsf == NULL)
    {
        fprintf(stderr, "ERROR: Could not open %s [Shallow Star Data File] for binary write.\n", argv[8]);
        fcloseall();
        return 1;
    }

    nsfhead = fopen(argv[9], "wb");
    if (nsfhead == NULL)
    {
        fprintf(stderr, "ERROR: Could not open %s [Shallow Star Header File] for binary write.\n", argv[9]);
        fcloseall();
        return 1;
    }

    namefile = fopen(argv[10], "wb");
    if (namefile == NULL)
    {
        fprintf(stderr, "ERROR: Could not open %s [Star Name File] for binary write.\n", argv[7]);
        fcloseall();
        return 1;
    }

    hdindex = fopen(argv[11], "wb");
    if (hdindex == NULL)
    {
        fprintf(stderr, "ERROR: Could not open %s [HD Index] for binary read/write.\n", argv[9]);
        fcloseall();
        return 1;
    }
    u_int32_t foo = 0;
    for (i = 0; i < 360000; ++i)
        fwrite(&foo, sizeof(u_int32_t), 1, hdindex);

    /* MySQL connect */
    if (mysql_init(&link) == NULL)
    {
        fprintf(stderr, "ERROR: Failed to initialize MySQL connection!\n");
        return 1;
    }

    ret = mysql_real_connect(&link, "localhost", argv[1], argv[2], argv[3], 0, NULL, 0);

    if (!ret)
    {
        fprintf(stderr, "ERROR: MySQL connect failed for the following reason: %s\n", mysql_error(&link));
        fcloseall();
        return 1;
    }

    if (mysql_select_db(&link, argv[3]))
    {
        fprintf(stderr, "ERROR: Could not select MySQL database %s. MySQL said: %s", argv[3], mysql_error(&link));
        fcloseall();
        mysql_close(&link);
        return 1;
    }

    /* Write file headers */
    writeNameFileHeader(namefile);
    writeDataFileHeader(nsfhead);
    writeDataFileHeader(usfhead);
    writeDataFileHeader(dsfhead);
    ns_header_offset = ftell(nsfhead);
    us_header_offset = ftell(usfhead);
    nf_header_offset = ftell(namefile);
    ds_header_offset = ftell(dsfhead);

    /* Write a bogus index entry into the namefile, to be filled with correct data later */
    writeIndexEntry(namefile, 0, ftell(namefile) + INDEX_ENTRY_SIZE, 0);

    /* Leave space for / write a deep magnitude limit specification in the data files */
    maglim = GLOBAL_MAG_LIMIT * 100;
    fwrite(&maglim, 2, 1, nsf); // This is also a bogus entry, because it will be overwritten later
    maglim = GLOBAL_MAG_LIMIT * 100;
    fwrite(&maglim, 2, 1, dsf); // This is also a bogus entry, because it will be overwritten later
    maglim = (int)(-5.0 * 100);
    fwrite(&maglim, 2, 1, usf); // Bogus entry

    /* Write a HTM level specification in the data file */
    htm_level = HTM_LEVEL;
    fwrite(&htm_level, 1, 1, nsf);
    fwrite(&htm_level, 1, 1, usf);
    fwrite(&htm_level, 1, 1, dsf);

    /* Leave space for a specification of MSpT (Maximum Stars per Trixel) in the data files */
    MSpT_named = MSpT_unnamed = 0;
    fwrite(&MSpT_named, 2, 1, nsf);   // Bogus entry
    fwrite(&MSpT_deep, 2, 1, dsf);    // Bogus entry
    fwrite(&MSpT_unnamed, 2, 1, usf); // Bogus entry

    /* Initialize some variables */
    lim            = 0;
    exitflag       = 0;
    current_trixel = 0;
    nsf_trix_count = usf_trix_count = dsf_trix_count = 0;
    nsf_trix_begin = usf_trix_begin = dsf_trix_begin =
        2 + 1 +
        2; // The 2+1+2 is to leave space for deep magnitude limit, HTM Level and MSpT specification. TODO: Change this if things change.
    ntrixels    = 0;
    names_count = 0;

    /* Recurse over every MYSQL_STARS_PER_QUERY DB entries */
    while (!exitflag)
    {
        /* Build MySQL query for next MYSQL_STARS_PER_QUERY stars */
        sprintf(query,
                "SELECT `Trixel`, `RA`, `Dec`, `dRA`, `dDec`, `Parallax`, `Mag`, `BV_Index`, `Spec_Type`, `Mult`, "
                "`Var`, `HD`, `UID`, `Name`, `GName` FROM `%s` ORDER BY `trixel`, `mag` ASC LIMIT %ld, %d",
                (argc >= 13) ? argv[12] : DB_TBL, lim, MYSQL_STARS_PER_QUERY);

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
            new_trixel = atoi(row[0]);
            if (new_trixel != current_trixel)
            {
                if (VERBOSE)
                {
                    fprintf(stderr, "Trixel Changed from %d to %d!\n", current_trixel, new_trixel);
                }
                while (new_trixel != current_trixel)
                {
                    writeIndexEntry(nsfhead, current_trixel,
                                    ns_header_offset + NTRIXELS * INDEX_ENTRY_SIZE + nsf_trix_begin, nsf_trix_count);
                    writeIndexEntry(dsfhead, current_trixel,
                                    ds_header_offset + NTRIXELS * INDEX_ENTRY_SIZE + dsf_trix_begin, dsf_trix_count);
                    writeIndexEntry(usfhead, current_trixel,
                                    us_header_offset + NTRIXELS * INDEX_ENTRY_SIZE + usf_trix_begin, usf_trix_count);
                    nsf_trix_begin = ftell(nsf);
                    dsf_trix_begin = ftell(dsf);
                    usf_trix_begin = ftell(usf);
                    if (nsf_trix_count > MSpT_named)
                        MSpT_named = nsf_trix_count;
                    if (dsf_trix_count > MSpT_deep)
                        MSpT_deep = dsf_trix_count;
                    if (usf_trix_count > MSpT_unnamed)
                        MSpT_unnamed = usf_trix_count;
                    nsf_trix_count = dsf_trix_count = usf_trix_count = 0;
                    current_trixel++;
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
            if (!isblank(row[13]) || !isblank(row[14]))
            {
                named = 1;

                /* Print out messages */
                if (VERBOSE)
                    fprintf(stderr, "Named Star!\n");
                if (VERBOSE > 1)
                    fprintf(stderr, "Bayer Name = %s, Long Name = %s\n", row[14], row[13]);

                /* Check for overflows */
                if (strlen(row[13]) > LONG_NAME_LIMIT)
                    fprintf(stderr, "ERROR: Long Name %s with length %d exceeds LONG_NAME_LIMIT = %d\n", row[13],
                            strlen(row[13]), LONG_NAME_LIMIT);
                if (strlen(row[14]) > BAYER_LIMIT)
                    fprintf(stderr, "ERROR: Bayer designation %s with length %d exceeds BAYER_LIMIT = %d\n", row[14],
                            strlen(row[14]), BAYER_LIMIT);

                /* Set up the starName structure */
                str2charv(name.bayerName, row[14], BAYER_LIMIT);
                str2charv(name.longName, row[13], LONG_NAME_LIMIT);
                data.flags = data.flags | 0x01; /* Switch on the 'named' bit */
            }

            /* Are we looking at a 'global' star [always in memory] or dynamically loaded star? */
            if (named)
            {
                f = nsf;
                nsf_trix_count++;
                shallow = 1;
            }
            else if ((data.mag / 100.0) <= GLOBAL_MAG_LIMIT)
            {
                f = usf;
                usf_trix_count++;
                shallow = 1;
            }
            else
            {
                dsf_trix_count++;
                f = dsf;
                if (maglim < data.mag)
                    maglim = data.mag;
                shallow = 0;
            }

            /* Convert various fields and make entries into the starData structure */
            str2int32(&data.RA, row[1], 6);
            str2int32(&data.Dec, row[2], 5);
            str2int32(&data.dRA, row[3], 1);
            str2int32(&data.dDec, row[4], 1);
            str2int32(&data.parallax, row[5], 1);
            str2int32(&data.HD, row[11], 0);
            str2int16(&data.bv_index, row[7], 2);
            if (str2charv(data.spec_type, row[8], 2) < 0)
                fprintf(stderr, "Spectral type entry %s in DB is possibly invalid for UID = %s\n", row[8], row[12]);
            if (row[9][0] != '0' && row[9][0] != '\0')
                data.flags = data.flags | 0x02;
            if (row[10][0] != '0' && row[10][0] != '\0')
                data.flags = data.flags | 0x04;

            if (data.HD != 0 && !shallow)
            {
                // Use this information to generate the HD index we want to have.
                int32_t off;
                if (fseek(hdindex, (data.HD - 1) * sizeof(int32_t), SEEK_SET))
                {
                    fprintf(stderr, "Invalid seek ( to %X ) on HD Index file. Index will be corrupt.\n",
                            (data.HD - 1) * sizeof(u_int32_t));
                }
                off = ftell(f) + ds_header_offset + NTRIXELS * INDEX_ENTRY_SIZE;
                fprintf(stderr, "DEBUG: HD %d at %X. Writing at %X\n", data.HD, off, ftell(hdindex));
                if (!fwrite(&off, sizeof(int32_t), 1, hdindex))
                    fprintf(stderr, "fwrite() on HD Index file failed. Index will be empty or incomplete.\n");
            }

            /* Write the data into the appropriate data file and any names into the name file */
            if (VERBOSE)
                fprintf(stderr, "Writing UID = %s...", row[12]);
            fwrite(&data, sizeof(starData), 1, f);
            if (named)
            {
                fwrite(&name.bayerName, BAYER_LIMIT, 1, namefile);
                fwrite(&name.longName, LONG_NAME_LIMIT, 1, namefile);
                names_count++;
                if (VERBOSE)
                    fprintf(stderr, "Named star count = %ul", names_count);
            }
            if (VERBOSE)
                fprintf(stderr, "Done.\n");
        }
        mysql_free_result(result);
        lim += MYSQL_STARS_PER_QUERY;
    }

    do
    {
        writeIndexEntry(nsfhead, current_trixel, ns_header_offset + NTRIXELS * INDEX_ENTRY_SIZE + nsf_trix_begin,
                        nsf_trix_count);
        writeIndexEntry(dsfhead, current_trixel, ds_header_offset + NTRIXELS * INDEX_ENTRY_SIZE + dsf_trix_begin,
                        dsf_trix_count);
        writeIndexEntry(usfhead, current_trixel, us_header_offset + NTRIXELS * INDEX_ENTRY_SIZE + usf_trix_begin,
                        usf_trix_count);
        nsf_trix_begin = ftell(nsf);
        dsf_trix_begin = ftell(dsf);
        usf_trix_begin = ftell(usf);
        nsf_trix_count = usf_trix_count = dsf_trix_count = 0;
        current_trixel++;
        ntrixels++;
    } while (current_trixel != NTRIXELS);

    fseek(namefile, nf_header_offset, SEEK_SET);
    writeIndexEntry(namefile, 0, nf_header_offset + INDEX_ENTRY_SIZE, names_count);

    if (ntrixels != NTRIXELS)
    {
        fprintf(stderr,
                "ERROR: Expected %u trixels, but found %u instead. Please redefine NTRIXELS in this program, or check "
                "the source database for bogus trixels\n",
                NTRIXELS, ntrixels);
    }

    rewind(usf);
    rewind(dsf);
    rewind(nsf);
    fwrite(&maglim, 2, 1, dsf);
    maglim = GLOBAL_MAG_LIMIT * 100;
    fwrite(&maglim, 2, 1, nsf);
    fwrite(&maglim, 2, 1, usf);
    fwrite(&htm_level, 1, 1, usf);
    fwrite(&htm_level, 1, 1, nsf);
    fwrite(&htm_level, 1, 1, dsf);
    fwrite(&MSpT_unnamed, 2, 1, usf);
    fwrite(&MSpT_deep, 2, 1, dsf);
    fwrite(&MSpT_named, 2, 1, nsf);

    fcloseall();
    mysql_close(&link);

    return 0;
}
