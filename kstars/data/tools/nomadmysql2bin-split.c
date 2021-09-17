/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Program to convert star data stored in a MySQL database into
// the binary file format used by KStars

// This version generates several split up data files with no headers
// The headers / index table must be generated separately and the files
// must be concatenated appropriately to generate the data file.

#define DB_TBL                "nomad"
#define DB_DB                 "stardb"
#define VERBOSE               0
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

int main(int argc, char *argv[])
{
    /* === Declarations === */

    int ret, i;

    double Mag;

    u_int8_t htm_level;
    u_int16_t MSpT_unnamed;

    u_int32_t trixel_number;

    char query[512];

    /* File streams */
    FILE *f;   /* Pointer to "current" file */
    FILE *usf; /* Handle to star data file */

    /* deepStarData structure */
    deepStarData data;

    /* MySQL structures */
    MYSQL link;
    MYSQL_RES *result;
    MYSQL_ROW row;

    /* Check the number of arguments */
    if (argc <= 6)
    {
        fprintf(stderr, "USAGE %s DBUserName DBPassword DeepStarDataFile TrixelNumber DBName TableName\n", argv[0]);
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

    trixel_number = atoi(argv[4]);

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

    /* Build MySQL query for stars of the trixel */
    sprintf(query,
            "SELECT `Trixel`, `RA`, `Dec`, `dRA`, `dDec`, `B`, `V`, `UID` FROM `%s` WHERE `Trixel`=\'%d\' ORDER BY "
            "`Mag` ASC",
            (argc >= 7) ? argv[6] : DB_TBL, trixel_number);

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

        f = usf;

        /* Convert various fields and make entries into the deepStarData structure */
        str2int32(&data.RA, row[1], 6);
        str2int32(&data.Dec, row[2], 5);
        str2int16(&data.dRA, row[3], 3);
        str2int16(&data.dDec, row[4], 3);
        str2int16(&data.B, row[5], 3);
        str2int16(&data.V, row[6], 3);

        /* Write the data into the appropriate data file and any names into the name file */
        if (VERBOSE)
            fprintf(stderr, "Writing UID = %s...", row[7]);
        fwrite(&data, sizeof(deepStarData), 1, f);
        if (VERBOSE)
            fprintf(stderr, "Done.\n");
    }
    mysql_free_result(result);

    fcloseall();
    mysql_close(&link);

    return 0;
}
