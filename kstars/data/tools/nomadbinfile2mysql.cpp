/*
    SPDX-FileCopyrightText: 2011 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * NOTE: I modified nomadbinfiletester.c to do this -- Akarsh
 */

/*
 * TODO: VERY UGLY CODE. Please fix it some day. Preferably now.  This
 * file was created from a modified C file, and needs to be recast
 * into the C++ way of writing stuff, i.e. with classes etc.
 */

#include "nomadbinfile2mysql.h"
#include "binfile.h"
#include "angconversion.h"
#include "MeshIterator.h"
#include <iostream>
#include <string.h>
#include <stdio.h>

using namespace std;

NOMADStarDataWriter::NOMADStarDataWriter(FILE *f, int HTMLevel, MYSQL *link, char *_db_tbl)
{
    m_MySQLLink = link;
    DataFile    = f;
    // Create a new shiny HTMesh
    m_Mesh = new HTMesh(HTMLevel, HTMLevel);
    strcpy(db_tbl, _db_tbl);
    m_HeaderRead = false;
}

NOMADStarDataWriter::~NOMADStarDataWriter()
{
    delete m_Mesh;
}

void NOMADStarDataWriter::bswap_stardata(DeepStarData *stardata)
{
    stardata->RA   = bswap_32(stardata->RA);
    stardata->Dec  = bswap_32(stardata->Dec);
    stardata->dRA  = bswap_16(stardata->dRA);
    stardata->dDec = bswap_16(stardata->dDec);
    stardata->B    = bswap_16(stardata->B);
    stardata->V    = bswap_16(stardata->V);
}

/**
 *@short Create the table
 */
bool NOMADStarDataWriter::createTable()
{
    // TODO: This is not working. Investigate.
    char create_query[2048];
    sprintf(create_query,
            "CREATE TABLE IF NOT EXISTS `%s` (`Trixel` int(32) NOT NULL COMMENT 'Trixel Number', `RA` double NOT NULL "
            "COMMENT 'RA Hours', `Dec` double NOT NULL COMMENT 'Declination Degrees', `dRA` double NOT NULL COMMENT "
            "'Proper Motion along RA', `dDec` double NOT NULL COMMENT 'Proper Motion along Dec', `PM` double NOT NULL "
            "COMMENT 'Proper Motion (magnitude)', `V` float NOT NULL COMMENT 'Visual Magnitude', `B` float NOT NULL "
            "COMMENT 'Blue Magnitude', `Mag` float NOT NULL COMMENT 'Magnitude for sorting', `UID` int(64) NOT NULL "
            "auto_increment COMMENT 'Unique ID', `Copies` tinyint(8) NOT NULL COMMENT 'Number of Copies of the star', "
            "PRIMARY KEY  (`UID`), UNIQUE KEY `UID` (`UID`), KEY `Trixel` (`Trixel`,`PM`,`Mag`)) ENGINE=MyISAM DEFAULT "
            "CHARSET=latin1 AUTO_INCREMENT=1",
            db_tbl);

    if (mysql_query(m_MySQLLink, create_query))
    {
        cerr << "ERROR: Table creation failed!" << endl;
        return false;
    }
    return true;
}

/**
 *@short Calculate the final destination RA and Dec of a star with the
 *given initial RA, Dec and proper motion rates after 'years' number
 *of years
 */
void NOMADStarDataWriter::calculatePMCoords(double startRA, double startDec, double dRA, double dDec, double *endRA,
                                            double *endDec, float years)
{
    // (Translated from Perl)
    double theta0 = hour2rad(startRA);
    double lat0   = deg2rad(startDec);

    double PMperyear = sqrt(dRA * dRA + dDec * dDec);

    double dir0 = (years > 0.0) ? atan2(dRA, dDec) : atan2(-dRA, -dDec); // If years < 0, we're precessing backwards
    double PM   = PMperyear * fabs(years);

    double dst = deg2rad(arcsec2deg(PM / 1000.0)); // Milliarcsecond -> radian

    double phi0 = M_PI / 2.0 - lat0;

    double lat1   = asin(sin(lat0) * cos(dst) + cos(lat0) * sin(dst) * cos(dir0)); // Cosine rule on a sphere
    double dtheta = atan2(sin(dir0) * sin(dst) * cos(lat0), cos(dst) - sin(lat0) * sin(lat1));

    *endRA  = rad2hour(theta0 + dtheta);
    *endDec = rad2deg(lat1);
}

/**
 *@short Do some calculations and insert the star data into the database
 */
bool NOMADStarDataWriter::insertStarData(unsigned int trixel, const DeepStarData *const data)
{
    char query[2048];
    float mag;
    float B, V, RA, Dec, dRA, dDec;

    // Rescale the data from the structure
    B    = ((double)data->B) / 1000.0;
    V    = ((double)data->V) / 1000.0;
    RA   = ((double)data->RA) / 1000000.0;
    Dec  = ((double)data->Dec) / 100000.0;
    dRA  = ((double)data->dRA) / 1000.0;
    dDec = ((double)data->dDec) / 1000.0;

    // Check if the supplied trixel is really the trixel in which the
    // star is in according to its RA and Dec. If that's not the case,
    // this star is a duplicate and must be ignored
    unsigned int originalTrixelID = m_Mesh->index(RA, Dec);
    if (trixel != originalTrixelID)
        return true; // Ignore this star.

    // Magnitude for sorting.
    if (V == 30.0 && B != 30.0)
    {
        mag = B - 1.6;
    }
    else
    {
        mag = V;
    }

    // Compute the proper motion
    double RA1, Dec1, RA2, Dec2;
    double PM; // Magnitude of the proper motion in milliarcseconds per year

    PM = sqrt(dRA * dRA + dDec * dDec);

    calculatePMCoords(RA, Dec, dRA, dDec, &RA1, &Dec1, PM_MILLENIA * -1000.);
    calculatePMCoords(RA, Dec, dRA, dDec, &RA2, &Dec2, PM_MILLENIA * 1000.);

    unsigned int TrixelList[900];
    int ntrixels = 0;

    double separation = sqrt(hour2deg(RA1 - RA2) * hour2deg(RA1 - RA2) +
                             (Dec1 - Dec2) * (Dec1 - Dec2)); // Separation in degrees // ugly.
    if (separation > 50.0 / 60.0)                            // 50 arcminutes
    {
        m_Mesh->intersect(RA1, Dec1, RA2, Dec2);
        MeshIterator trixels(m_Mesh);
        while (trixels.hasNext())
        {
            TrixelList[ntrixels] = trixels.next();
            ntrixels++;
        }
    }
    else
    {
        TrixelList[0] = originalTrixelID;
        ntrixels      = 1;
    }

    if (ntrixels == 0)
    {
        cerr << "Ntrixels is zero in trixel " << originalTrixelID;
        return false;
    }

    for (int i = 0; i < ntrixels; ++i)
    {
        sprintf(query,
                "INSERT INTO `%s` (`Trixel`, `RA`, `Dec`, `dRA`, `dDec`, `B`, `V`, `mag`, `PM`, `Copies`) VALUES "
                "(\'%d\', \'%lf\', \'%lf\', \'%lf\', \'%lf\', \'%lf\', \'%lf\', \'%lf\', \'%lf\', \'%u\')",
                db_tbl, TrixelList[i], RA, Dec, dRA, dDec, B, V, mag, PM,
                ((TrixelList[i] == originalTrixelID) ?
                     ntrixels :
                     0) // Duplicates get a 'Copies' value of 0. The real star gets the actual value.
        );
        if (mysql_query(m_MySQLLink, query))
        {
            cerr << "MySQL INSERT INTO failed! Query was: " << endl << query << endl;
            return false;
        }
    }
    return true;
}

bool NOMADStarDataWriter::truncateTable()
{
    // Truncate table. TODO: Issue warning etc
    char query[60];
    sprintf(query, "TRUNCATE TABLE `%s`", db_tbl);
    if (mysql_query(m_MySQLLink, query))
    {
        cerr << "Truncate table query \"" << query << "\" failed!" << endl;
        return false;
    }
    return true;
}

/**
 *@short Write star data to the database
 */
bool NOMADStarDataWriter::writeStarDataToDB()
{
    int8_t HTM_Level;
    u_int16_t MSpT;
    u_int32_t nstars;
    u_int32_t offset;
    unsigned int trixel;
    DeepStarData data;
    int16_t mag;

    /*
      // TODO: FIX THIS // FIXME
    // We must at least check if the HTM level matches
    fseek( DataFile, m_IndexOffset - 3, SEEK_SET );
    fread( &HTM_Level, 1, 1, DataFile );
    fprintf( stdout, "HTMesh Level: %d\n", HTM_Level );
    if( HTM_Level != m_Mesh->level() ) {
        cerr << "ERROR: HTMesh Level in file (" << HTM_Level << ") and HTM_LEVEL in program (" << m_Mesh->level() << ") differ." << endl
             << "Please set the define directive for HTM_LEVEL in the header file correctly and rebuild."
             << endl;
        return false;
    }
    */

    for (trixel = 0; trixel < ntrixels; ++trixel)
    {
        fseek(DataFile, m_IndexOffset + trixel * INDEX_ENTRY_SIZE + 4, SEEK_SET);
        fread(&offset, 4, 1, DataFile);
        fread(&nstars, 4, 1, DataFile);

        /* If offset > 2^31 - 1, do the fseek in two steps */
        if (offset > (unsigned)(pow2(31) - 1))
        {
            fseek(DataFile, pow2(31) - 1, SEEK_SET);
            fseek(DataFile, offset - pow2(31) + 1, SEEK_CUR);
        }
        else
            fseek(DataFile, offset, SEEK_SET);

        for (int i = 0; i < nstars; ++i)
        {
            fread(&data, sizeof(DeepStarData), 1, DataFile);
            if (byteswap)
                bswap_stardata(&data);

            /** CODE FROM INSERTSTARDATA PASTED HERE FOR SPEED */
            {
                char query[2048];
                float mag;
                float B, V, RA, Dec, dRA, dDec;

                // Rescale the data from the structure
                B    = ((double)data.B) / 1000.0;
                V    = ((double)data.V) / 1000.0;
                RA   = ((double)data.RA) / 1000000.0;
                Dec  = ((double)data.Dec) / 100000.0;
                dRA  = ((double)data.dRA) / 1000.0;
                dDec = ((double)data.dDec) / 1000.0;

                // Check if the supplied trixel is really the trixel in which the
                // star is in according to its RA and Dec. If that's not the case,
                // this star is a duplicate and must be ignored
                unsigned int originalTrixelID = m_Mesh->index(hour2deg(RA), Dec);
                if (trixel != originalTrixelID)
                {
                    cout << "Trixel = " << trixel << ", but this is the original Trixel ID: " << originalTrixelID
                         << ". Skipping" << endl;
                    cout << "Skipped star has (RA, Dec) = " << RA << Dec << "; (dRA, dDec) = " << dRA << dDec
                         << "; and (B, V) = " << B << V << "." << endl;
                    cout << "This suspected duplicate is star " << i << "in trixel " << trixel;
                    continue;
                }

                // Magnitude for sorting.
                if (V == 30.0 && B != 30.0)
                {
                    mag = B - 1.6;
                }
                else
                {
                    mag = V;
                }

                // Compute the proper motion
                double RA1, Dec1, RA2, Dec2, RA1deg, RA2deg;
                double PM; // Magnitude of the proper motion in milliarcseconds per year

                PM = sqrt(dRA * dRA + dDec * dDec);

                calculatePMCoords(RA, Dec, dRA, dDec, &RA1, &Dec1, PM_MILLENIA * -1000.);
                calculatePMCoords(RA, Dec, dRA, dDec, &RA2, &Dec2, PM_MILLENIA * 1000.);
                RA1deg = hour2deg(RA1);
                RA2deg = hour2deg(RA2);

                unsigned int TrixelList[60];
                int nt = 0;

                double separationsqr = (RA1deg - RA2deg) * (RA1deg - RA2deg) +
                                       (Dec1 - Dec2) * (Dec1 - Dec2); // Separation in degrees // ugly.
                if (separationsqr >
                    0.69) // 50 arcminutes converted to degrees, squared and rounded below = 0.69. (This has nothing to do with sex positions.)
                {
                    m_Mesh->intersect(RA1deg, Dec1, RA2deg, Dec2);
                    MeshIterator trixels(m_Mesh);
                    while (trixels.hasNext())
                    {
                        TrixelList[nt] = trixels.next();
                        nt++;
                    }
                }
                else
                {
                    TrixelList[0] = originalTrixelID;
                    nt            = 1;
                }

                if (nt == 0)
                {
                    cerr << "# of trixels is zero in trixel " << originalTrixelID;
                    return false;
                }

                for (int i = 0; i < nt; ++i)
                {
                    sprintf(query,
                            "INSERT INTO `%s` (`Trixel`, `RA`, `Dec`, `dRA`, `dDec`, `B`, `V`, `mag`, `PM`, `Copies`) "
                            "VALUES (\'%d\', \'%lf\', \'%lf\', \'%lf\', \'%lf\', \'%lf\', \'%lf\', \'%lf\', \'%lf\', "
                            "\'%u\')",
                            db_tbl, TrixelList[i], RA, Dec, dRA, dDec, B, V, mag, PM,
                            ((TrixelList[i] == originalTrixelID) ?
                                 nt :
                                 0) // Duplicates get a 'Copies' value of 0. The real star gets the actual value.
                    );
                    if (mysql_query(m_MySQLLink, query))
                    {
                        cerr << "MySQL INSERT INTO failed! Query was: " << endl << query << endl;
                        return false;
                    }
                }
            }
        }
        if (trixel % 100 == 0)
            cout << "Finished trixel " << trixel << endl;
    }

    return true;
}

bool NOMADStarDataWriter::readFileHeader()
{
    int i;
    int16_t endian_id;
    char ASCII_text[125];
    u_int8_t version_no;
    u_int16_t nfields;

    if (!DataFile)
        return false;

    fread(ASCII_text, 124, 1, DataFile);
    ASCII_text[124] = '\0';
    printf("%s", ASCII_text);

    fread(&endian_id, 2, 1, DataFile);
    if (endian_id != 0x4B53)
    {
        fprintf(stdout, "Byteswapping required\n");
        byteswap = 1;
    }
    else
    {
        fprintf(stdout, "Byteswapping not required\n");
        byteswap = 0;
    }

    fread(&version_no, 1, 1, DataFile);
    fprintf(stdout, "Version number: %d\n", version_no);

    fread(&nfields, 2, 1, DataFile);

    // Just to read those many bytes
    // TODO: Don't waste time and memory. fseek.
    dataElement de;
    for (i = 0; i < nfields; ++i)
        fread(&de, sizeof(struct dataElement), 1, DataFile);

    fread(&ntrixels, 4, 1, DataFile);
    if (byteswap)
        ntrixels = bswap_32(ntrixels);
    fprintf(stdout, "Number of trixels reported = %d\n", ntrixels);

    m_IndexOffset = ftell(DataFile);

    m_HeaderRead = true;

    return true;
}

bool NOMADStarDataWriter::write()
{
    if (!readFileHeader())
        return false;
    if (!createTable())
        return false;
    truncateTable();
    if (!writeStarDataToDB())
        return false;
}

int main(int argc, char *argv[])
{
    MYSQL link;
    FILE *f;
    char db_tbl[20];
    char db_name[20];

    if (argc <= 5)
    {
        fprintf(stderr, "USAGE: %s <NOMAD bin file> <MySQL DB User> <Password> <DB Name> <Table Name>\n", argv[0]);
        return 1;
    }

    strcpy(db_tbl, argv[5]);
    strcpy(db_name, argv[4]);

    f = fopen(argv[1], "r");

    if (f == nullptr)
    {
        fprintf(stderr, "ERROR: Could not open file %s for binary read.\n", argv[1]);
        return 1;
    }

    /* Open the Database */
    if (mysql_init(&link) == nullptr)
    {
        fprintf(stderr, "ERROR: Failed to initialize MySQL connection!\n");
        return 1;
    }
    MYSQL *ret;
    ret = mysql_real_connect(&link, "localhost", argv[2], argv[3], db_name, 0, nullptr, 0);

    if (!ret)
    {
        fprintf(stderr, "ERROR: MySQL connect failed for the following reason: %s\n", mysql_error(&link));
        fcloseall();
        return 1;
    }

    if (mysql_select_db(&link, db_name))
    {
        fprintf(stderr, "ERROR: Could not select MySQL database %s. MySQL said: %s", db_name, mysql_error(&link));
        fcloseall();
        mysql_close(&link);
        return 1;
    }

    NOMADStarDataWriter writer(f, HTM_LEVEL, &link, db_tbl);

    writer.write();

    fclose(f);
    mysql_close(&link);
    return 0;
}
