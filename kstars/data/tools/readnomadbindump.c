/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#define HTM_LEVEL 6 // Does not matter
#include "binfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <string.h>
#include "byteorder.h"

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
 * Does byteswapping for deepStarData structures
 */
void bswap_stardata(deepStarData *stardata)
{
    stardata->RA   = bswap_32(stardata->RA);
    stardata->Dec  = bswap_32(stardata->Dec);
    stardata->dRA  = bswap_16(stardata->dRA);
    stardata->dDec = bswap_16(stardata->dDec);
    stardata->B    = bswap_16(stardata->B);
    stardata->V    = bswap_16(stardata->V);
}

int main(int argc, char *argv[])
{
    FILE *f;
    deepStarData data;

    if (argc > 1)
    {
        f = fopen(argv[1], "rb");
        if (f == NULL)
        {
            fprintf(stderr, "ERROR: Could not open file %s for binary read.\n", argv[1]);
            return 1;
        }
    }
    else
        f = stdin;

    int id = 0;
    while (!feof(f))
    {
        if (!fread(&data, sizeof(deepStarData), 1, f) && !feof(f))
        {
            fprintf(stderr, "WARNING: Looks like we encountered a premature end\n");
            break;
        }
        //        if( byteswap ) bswap_stardata( &data );
        fprintf(stdout, "*** Entry #%d ***\n", id);
        fprintf(stdout, "\tRA = %f\n", data.RA / 1000000.0);
        fprintf(stdout, "\tDec = %f\n", data.Dec / 100000.0);
        fprintf(stdout, "\tdRA/dt = %f\n", data.dRA / 1000.0);
        fprintf(stdout, "\tdDec/dt = %f\n", data.dDec / 1000.0);
        fprintf(stdout, "\tB = %f\n", data.B / 1000.0);
        fprintf(stdout, "\tV = %f\n", data.V / 1000.0);
        ++id;
    }

    fclose(f);

    return 0;
}
