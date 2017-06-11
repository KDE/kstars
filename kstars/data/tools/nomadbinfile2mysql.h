/***************************************************************************
       nomadbinfile2mysql.h -- Declarations for nomadbinfile2mysql.cpp
                             -------------------
    begin                : Sat Jul 2 2011
    copyright            : (C) 2011 by Akarsh Simha
    email                : akarshsimha@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef NOMADBINFILE2MYSQL_H
#define NOMADBINFILE2MYSQL_H

#define pow2(x)          (1 << (x))
#define HTM_LEVEL        6
#define INDEX_ENTRY_SIZE 12
#define NTRIXELS         32768 // TODO: Change if HTM Level Changes
#define PM_MILLENIA      10    // This is not good. Needs to be stored in the data file

#include "HTMesh.h"

#include <mysql/mysql.h>
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
    NOMADStarDataWriter(FILE *f, int HTMLevel, MYSQL *link, char *_db_tbl);

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
    MYSQL *m_MySQLLink;
    char db_tbl[20];
    bool byteswap;
    bool m_HeaderRead;
    long m_IndexOffset;
    u_int16_t ntrixels;
    FILE *DataFile;
};

#endif
