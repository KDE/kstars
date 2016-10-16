/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Library General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

Copyright (C) 2015 Liam Girdwood <lgirdwood@gmail.com>
*/

/* Build ELP C data structures from ELP82 data files */
/* ftp://ftp.imcce.fr/pub/ephem/moon/elp82b/ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>

#define HEADER	"#include \"lunar-priv.h\"\n"

struct elp {
	int i;
	FILE *fdi, *fdo, *fdh;
	int size;
};

/* elp 1 - 3 */
static void main_parse(struct elp *elp, const char *prefix)
{
	char line[1024], out[256], hdr[256];
	double d[11];
	int match;

	/* ignore first line of inpput */
	fgets(line, 1024, elp->fdi);

	/* write header */
	fputs(HEADER, elp->fdo);
	sprintf(hdr, "const struct %s elp%d[] = {\n", prefix, elp->i);
	fputs(hdr, elp->fdo);

	elp->size = 0;

	/* read input line by line for coefficients */
	match = fscanf(elp->fdi, "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
			&d[0], &d[1], &d[2], &d[3], &d[4],
			&d[5], &d[6], &d[7], &d[8], &d[9], &d[10]);
	while (match == 11) {
		sprintf(out, "{{%f, %f, %f, %f}, %f, {%f, %f, %f, %f, %f, %f}},\n",
			d[0], d[1], d[2], d[3], d[4], d[5], d[6],
			d[7], d[8], d[9], d[10]);
		fputs(out, elp->fdo);
		elp->size++;

		match = fscanf(elp->fdi, "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
			&d[0], &d[1], &d[2], &d[3], &d[4],
			&d[5], &d[6], &d[7], &d[8], &d[9], &d[10]);
	}

	fputs("};\n", elp->fdo);
}

/* elp 4 - 9 and the rest*/
static void earth_parse(struct elp *elp, const char *prefix)
{
	char line[1024], out[256], hdr[256];
	double d[8];
	int match;

	/* ignore first line of inpput */
	fgets(line, 1024, elp->fdi);

	/* write header */
	fputs(HEADER, elp->fdo);
	sprintf(hdr, "const struct %s elp%d[] = {\n", prefix, elp->i);
	fputs(hdr, elp->fdo);

	elp->size = 0;

	match = fscanf(elp->fdi, "%lf %lf %lf %lf %lf %lf %lf %lf",
			&d[0], &d[1], &d[2], &d[3], &d[4],
			&d[5], &d[6], &d[7]);

	/* read input line by line for coefficients */
	while (match == 8) {

		sprintf(out, "{%f, {%f, %f, %f, %f}, %f, %f, %f},\n",
			d[0], d[1], d[2], d[3], d[4], d[5], d[6],
			d[7]);
		fputs(out, elp->fdo);
		elp->size++;

		match = fscanf(elp->fdi, "%lf %lf %lf %lf %lf %lf %lf %lf",
			&d[0], &d[1], &d[2], &d[3], &d[4],
			&d[5], &d[6], &d[7]);
	}

	fputs("};\n", elp->fdo);
}

/* used for elp 10 - 21 */
static void planet_parse(struct elp *elp, const char *prefix)
{
	char line[1024], out[256], hdr[256];
	double d[14];
	int match;

	/* ignore first line of inpput */
	fgets(line, 1024, elp->fdi);

	/* write header */
	fputs(HEADER, elp->fdo);
	sprintf(hdr, "const struct %s elp%d[] = {\n", prefix, elp->i);
	fputs(hdr, elp->fdo);

	elp->size = 0;

	match = fscanf(elp->fdi,
			"%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
			&d[0], &d[1], &d[2], &d[3], &d[4],
			&d[5], &d[6], &d[7], &d[8], &d[9],
			&d[10], &d[11], &d[12], &d[13]);

	/* read input line by line for coefficients */
	while (match == 14) {

		sprintf(out,
			"{{%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f}, %f, %f, %f},\n",
			d[0], d[1], d[2], d[3], d[4], d[5], d[6],
			d[7], d[8], d[9], d[10], d[11], d[12], d[13]);
		fputs(out, elp->fdo);
		elp->size++;

		match = fscanf(elp->fdi,
			"%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
			&d[0], &d[1], &d[2], &d[3], &d[4],
			&d[5], &d[6], &d[7], &d[8], &d[9],
			&d[10], &d[11], &d[12], &d[13]);
	}

	fputs("};\n", elp->fdo);
}


int main(int argc, char *argv[])
{
	struct elp elp;
	char file[256];

	memset(&elp, 0, sizeof(elp));

	/* open output header file */
	elp.fdh = fopen("elp.h", "w");
	if (elp.fdo < 0) {
		fprintf(stderr, "error: cannot open ouput file ELP.h\n");
		fprintf(stderr, "error: please download ELP data from ftp://ftp.imcce.fr/pub/ephem/moon/elp82b/\n");
		exit(-errno);
	}
	fprintf(stdout, "opened output header ELP.h\n");

	for (elp.i = 1; elp.i <= 36; elp.i++) {

		/* open input file */
		sprintf(file, "ELP%d", elp.i); 
		elp.fdi = fopen(file, "r");
		if (elp.fdi < 0) {
			fprintf(stderr, "error: cannot open input file %s\n", file);
			exit(-errno);
		}
		fprintf(stdout, "opened input %s\n", file);

		/* open output file */
		sprintf(file, "elp%d.c", elp.i);
		elp.fdo = fopen(file, "w");
		if (elp.fdo < 0) {
			fprintf(stderr, "error: cannot open ouput file %s\n", file);
			exit(-errno);
		}
		fprintf(stdout, "opened output %s\n", file);

		switch (elp.i) {
		case 1 ... 3:
			main_parse(&elp, "main_problem ");
			break;
		case 4 ... 9:
			earth_parse(&elp, "earth_pert ");
			break;
		case 10 ... 21:
			planet_parse(&elp, "planet_pert ");
			break;
		case 22 ... 27:
			earth_parse(&elp, "earth_pert ");
			break;
		case 28 ... 30:
			earth_parse(&elp, "earth_pert ");
			break;
		case 31 ... 33:
			earth_parse(&elp, "earth_pert ");
			break;
		case 34 ... 36:
			earth_parse(&elp, "earth_pert ");
			break;
		default:
			exit(-EINVAL);
			break;
		}


		fclose(elp.fdo);
		fclose(elp.fdi);

		/* write header */
		sprintf(file, "#define ELP%d_SIZE	%d\n", elp.i, elp.size); 
		fputs(file, elp.fdh);
	}

	fclose(elp.fdh);
	return 0;
}

