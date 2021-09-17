/*
    SPDX-FileCopyrightText: 2011 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef NOMADBINFILE2SQLITE_H
#define NOMADBINFILE2SQLITE_H

#define pow2(x)          (1 << (x))
#define HTM_LEVEL        6
#define INDEX_ENTRY_SIZE 12
#define NTRIXELS         pow2(2 * HTM_LEVEL) * 8
#define PM_MILLENIA      10 // This is not good. Needs to be stored in the data file

#include "HTMesh.h"

#include <sqlite3.h>
#include <sys/types.h>
#include <stdio.h>

/*
 * struct to store star data, to be written in this format, into the binary file.
 */

typedef struct DeepStarData
{
    int32_t RA;
    int32_t Dec;
    int16_t dRA;
    int16_t dDec;
    int16_t B;
    int16_t V;
} DeepStarData;

/**
 *@class NOMADStarDataWriter
 *@short Writes NOMAD star data
 *@note This is ugly code, not part of the main KStars program
 *@author Akarsh Simha <akarsh.simha@kdemail.net>
 */
class NOMADStarDataWriter
{
  public:
    /**
         *@short Constructor. Sets up the HTMesh, initializes various things.
         */
    NOMADStarDataWriter(FILE *f, int HTMLevel, sqlite3 *_db, char *_db_tbl);

    /**
         *@short Destructor. Deletes the HTMesh we created.
         */
    ~NOMADStarDataWriter();

    /**
         *@short Byteswaps the DeepStarData structure
         */
    static void bswap_stardata(DeepStarData *stardata);

    /**
         *@short Computes the (unprecessed) coordinates of a star after
         * accounting for proper motion
         */
    static void calculatePMCoords(double startRA, double startDec, double dRA, double dDec, double *endRA,
                                  double *endDec, float years);

    /**
         *@short Writes the star data into the DB by calling multiple functions
         *@return Whether the write was successful
         */
    bool write();

  private:
    /**
         *@short Creates the table to write data into
         */
    bool createTable();

    /**
         *@short Truncates the table
         */
    bool truncateTable();

    /**
         *@short Insert the star data into the database
         *@param trixel The trixel in which the star exists according to the data file
         *@param data The DeepStarData structure containing the star's information
         *@return true if inserted or star was a duplicate, false if an error occurred
         *@note This method takes care of duplicating the star and finding the number of copies
         */
    bool insertStarData(unsigned int trixel, const DeepStarData *const data);

    /**
         *@short Write star data into the DB
         *@note See README.binfileformat for more details
         */
    bool writeStarDataToDB();

    /**
         *@short Read the KStars binary file header and gets various
         * parameters required for further processing.
         *@returns true on success.
         */
    bool readFileHeader();

    HTMesh *m_Mesh;
    sqlite3 *db;
    char db_tbl[20];
    bool byteswap;
    bool m_HeaderRead;
    long m_IndexOffset;
    u_int16_t ntrixels;
    FILE *DataFile;
};

#endif
