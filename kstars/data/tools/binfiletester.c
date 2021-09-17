/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <string.h>
#include "byteorder.h"

#define HTM_LEVEL        3
#define INDEX_ENTRY_SIZE 12

#include "binfile.h"

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

dataElement de[100];
u_int16_t nfields;
long index_offset, data_offset;
char byteswap;
u_int32_t ntrixels;

/*
 * Does byteswapping for starData structures
 */
void bswap_stardata(starData *stardata)
{
    stardata->RA       = bswap_32(stardata->RA);
    stardata->Dec      = bswap_32(stardata->Dec);
    stardata->dRA      = bswap_32(stardata->dRA);
    stardata->dDec     = bswap_32(stardata->dDec);
    stardata->parallax = bswap_32(stardata->parallax);
    stardata->HD       = bswap_32(stardata->HD);
    stardata->mag      = bswap_16(stardata->mag);
    stardata->bv_index = bswap_16(stardata->bv_index);
}

int verifyIndexValidity(FILE *f)
{
    int i;
    u_int32_t trixel;
    u_int32_t offset;
    u_int32_t prev_offset;
    u_int32_t nrecs;
    u_int32_t prev_nrecs;
    unsigned int nerr;

    fprintf(stdout, "Performing Index Table Validity Check...\n");
    fprintf(stdout, "Assuming that index starts at %X\n", ftell(f));
    index_offset = ftell(f);

    prev_offset = 0;
    prev_nrecs  = 0;
    nerr        = 0;

    for (i = 0; i < ntrixels; ++i)
    {
        if (!fread(&trixel, 4, 1, f))
        {
            fprintf(stderr, "Table truncated before expected! Read i = %d records so far\n", i);
            +nerr;
            break;
        }
        if (byteswap)
            trixel = bswap_32(trixel);
        if (trixel >= ntrixels)
        {
            fprintf(stderr, "Trixel number %u is greater than the expected number of trixels %u\n", trixel, ntrixels);
            ++nerr;
        }
        if (trixel != i)
        {
            fprintf(stderr, "Found trixel = %d, while I expected number = %d\n", trixel, i);
            ++nerr;
        }
        fread(&offset, 4, 1, f);
        if (byteswap)
            offset = bswap_32(offset);
        fread(&nrecs, 4, 1, f);
        if (byteswap)
            nrecs = bswap_32(nrecs);
        if (prev_offset != 0 && prev_nrecs != (-prev_offset + offset) / sizeof(starData))
        {
            fprintf(stderr, "Expected %u  = (%X - %x) / %d records, but found %u, in trixel %d\n",
                    (offset - prev_offset) / sizeof(starData), offset, prev_offset, sizeof(starData), nrecs, trixel);
            ++nerr;
        }
        prev_offset = offset;
        prev_nrecs  = nrecs;
    }

    data_offset = ftell(f);

    if (nerr)
    {
        fprintf(stderr, "ERROR ;-): The index seems to have %u errors\n", nerr);
    }
    else
    {
        fprintf(stdout, "Index verified. PASSED.\n");
    }
}

/**
 *This method ensures that the data part of the file is sane. It ensures that there are no jumps in magnitude etc.
 */
int verifyStarData(FILE *f)
{
    int16_t faintMag;
    int8_t HTM_Level;
    u_int16_t MSpT;
    int16_t realFaintMag = -500;
    u_int16_t realMSpT;
    u_int32_t nstars;
    u_int32_t offset;

    int trixel, i;
    int nerr_trixel;
    int nerr;

    starData data;
    int16_t mag;

    fprintf(stdout, "Assuming that the data starts at %ld\n", ftell(f));
    fread(&faintMag, 2, 1, f);
    fprintf(stdout, "Faint Magnitude Limit: %f\n", faintMag / 100.0);
    fread(&HTM_Level, 1, 1, f);
    fprintf(stdout, "HTMesh Level: %d\n", HTM_Level);
    if (HTM_Level != HTM_LEVEL)
    {
        fprintf(stderr,
                "ERROR: HTMesh Level in file (%d) and HTM_LEVEL in program (%d) differ. Please set the define "
                "directive for HTM_LEVEL correctly and rebuild\n.",
                HTM_Level, HTM_LEVEL);
        return 0;
    }
    fread(&MSpT, 2, 1, f);

    mag  = -500;
    nerr = 0;

    // Scan the file for magnitude jumps, etc.
    realMSpT = 0;
    for (trixel = 0; trixel < ntrixels; ++trixel)
    {
        mag         = -500;
        nerr_trixel = 0;
        fprintf(stdout, "Checking trixel #%d: ", trixel);
        fseek(f, index_offset + trixel * INDEX_ENTRY_SIZE + 4, SEEK_SET);
        fread(&offset, 4, 1, f);
        fread(&nstars, 4, 1, f);
        if (nstars > realMSpT)
            realMSpT = nstars;
        fseek(f, offset, SEEK_SET);
        for (i = 0; i < nstars; ++i)
        {
            fread(&data, sizeof(starData), 1, f);
            if (byteswap)
                bswap_stardata(&data);
            if (mag != -500 && ((data.mag - mag) > 20 && mag < 1250) || data.mag < mag)
            { // TODO: Make sensible magnitude limit (1250) user specifiable
                // TODO: Enable byteswapping
                fprintf(stderr, "\n\tEncountered jump of %f at star #%d in trixel %d from %f to %f.",
                        (data.mag - mag) / 100.0, i, trixel, mag / 100.0, data.mag / 100.0);
                ++nerr_trixel;
            }
            mag = data.mag;
            if (mag > realFaintMag)
            {
                realFaintMag = mag;
            }
            if (mag > 1500)
                fprintf(stderr, "Magnitude > 15.00 ( = %f ) in trixel %d\n", mag / 100.0, trixel);
        }
        if (nerr_trixel > 0)
            fprintf(stderr, "\n * Encountered %d magnitude jumps in trixel %d\n", nerr_trixel, trixel);
        else
            fprintf(stdout, "Successful\n");
        nerr += nerr_trixel;
    }
    if (MSpT != realMSpT)
    {
        fprintf(stderr, "ERROR: MSpT according to file = %d, but turned out to be %d\n", MSpT, realMSpT);
        nerr++;
    }
    if (realFaintMag != faintMag)
    {
        fprintf(stderr, "ERROR: Faint Magnitude according to file = %f, but turned out to be %f\n", faintMag / 100.0,
                realFaintMag / 100.0);
        nerr++;
    }
    if (nerr > 0)
    {
        fprintf(stderr, "ERROR: Exiting with %d errors\n", nerr);
        return 0;
    }
    fprintf(stdout, "Data validation success!\n");
    return 1;
}

void readStarList(FILE *f, u_int32_t trixel, FILE *names)
{
    long offset;
    long n;
    int i;
    u_int32_t nrecs;
    u_int32_t trix;
    char bayerName[8];
    char longName[32];
    starData data;

    printf("Reading star list for trixel %d\n", trixel);
    rewind(f);
    offset = index_offset +
             trixel * INDEX_ENTRY_SIZE; // CAUTION: Change if the size of each entry in the index table changes
    fseek(f, offset, SEEK_SET);
    fread(&trix, 4, 1, f);
    if (byteswap)
        trix = bswap_32(trix);
    if (trix != trixel)
    {
        fprintf(
            stderr,
            "ERROR: Something fishy in the index. I guessed that %d would be here, but instead I find %d. Aborting.\n",
            trixel, trix);
        return;
    }
    fread(&offset, 4, 1, f);
    if (byteswap)
        offset = bswap_32(offset);
    fread(&nrecs, 4, 1, f);
    if (byteswap)
        nrecs = bswap_32(nrecs);

    if (fseek(f, offset, SEEK_SET))
    {
        fprintf(stderr,
                "ERROR: Could not seek to position %X in the file. The file is either truncated or the indexes are "
                "bogus.\n",
                offset);
        return;
    }
    printf("Data for trixel %d starts at offset 0x%X and has %d records\n", trixel, offset, nrecs);

    for (i = 0; i < nrecs; ++i)
    {
        offset = ftell(f);
        n      = (offset - data_offset) / sizeof(starData);
        fread(&data, sizeof(starData), 1, f);
        if (byteswap)
            bswap_stardata(&data);
        printf("Entry #%d\n", i);
        printf("\tRA = %f\n", data.RA / 1000000.0);
        printf("\tDec = %f\n", data.Dec / 100000.0);
        printf("\tdRA/dt = %f\n", data.dRA / 10.0);
        printf("\tdDec/dt = %f\n", data.dDec / 10.0);
        printf("\tParallax = %f\n", data.parallax / 10.0);
        printf("\tHD Catalog # = %lu\n", data.HD);
        printf("\tMagnitude = %f\n", data.mag / 100.0);
        printf("\tB-V Index = %f\n", data.bv_index / 100.0);
        printf("\tSpectral Type = %c%c\n", data.spec_type[0], data.spec_type[1]);
        printf("\tHas a name? %s\n", ((data.flags & 0x01) ? "Yes" : "No"));
        /*
          if(data.flags & 0x01 && names) {
          fseek(names, n * (32 + 8) + 0xA0, SEEK_SET);
          fread(bayerName, 8, 1, names);
          fread(longName, 32, 1, names);
          printf("\t\tBayer Designation = %s\n", bayerName);
          printf("\t\tLong Name = %s\n", longName);
          }
        */
        printf("\tMultiple Star? %s\n", ((data.flags & 0x02) ? "Yes" : "No"));
        printf("\tVariable Star? %s\n", ((data.flags & 0x04) ? "Yes" : "No"));
    }
}

/**
 *@short  Read the KStars binary file header and display its contents
 *@param f  Binary file to read from
 *@returns  non-zero if successful, zero if not
 */

int readFileHeader(FILE *f)
{
    int i;
    int16_t endian_id;
    char ASCII_text[125];
    u_int8_t version_no;

    if (f == NULL)
        return 0;

    fread(ASCII_text, 124, 1, f);
    ASCII_text[124] = '\0';
    printf("%s", ASCII_text);

    fread(&endian_id, 2, 1, f);
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

    fread(&version_no, 1, 1, f);
    fprintf(stdout, "File version number: %d\n", version_no);
    fread(&nfields, 2, 1, f);
    if (byteswap)
        nfields = bswap_16(nfields);
    fprintf(stdout, "%d fields reported\n", nfields);

    for (i = 0; i < nfields; ++i)
    {
        fread(&(de[i]), sizeof(struct dataElement), 1, f);
        if (byteswap)
            de->scale = bswap_32(de->scale);
        displayDataElementDescription(&(de[i]));
    }

    fread(&ntrixels, 4, 1, f);
    if (byteswap)
        ntrixels = bswap_32(ntrixels);
    fprintf(stdout, "Number of trixels reported = %d\n", ntrixels);

    return 1;
}

int main(int argc, char *argv[])
{
    FILE *f, *names;
    int16_t maglim = -500;
    names          = NULL;
    if (argc <= 1)
    {
        fprintf(stderr, "USAGE: %s filename [trixel]\n", argv[0]);
        fprintf(stderr, "Designed for use only with KStars star data files\n");
        return 1;
    }

    f = fopen(argv[1], "rb");

    if (f == NULL)
    {
        fprintf(stderr, "ERROR: Could not open file %s for binary read.\n", argv[1]);
        return 1;
    }

    readFileHeader(f);

    verifyIndexValidity(f);
    verifyStarData(f);

    //    fread(&maglim, 2, 1, f);
    //    fprintf(stdout, "Limiting Magnitude of Catalog File: %f\n", maglim / 100.0);

    if (argc > 2)
    {
        /*
          if(argc > 3)
          names = fopen(argv[3], "rb");
          else
          names = NULL;
        
          fprintf(stderr, "Names = %s\n", ((names)?"Yes":"No"));
        */
        rewind(f);
        readStarList(f, atoi(argv[2]), names);
    }

    fclose(f);
    if (names)
        fclose(names);

    return 0;
}
