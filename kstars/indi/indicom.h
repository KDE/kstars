/*
    INDI LIB
    Common routines used by all drivers
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#ifndef INDICOM_H
#define INDICOM_H

enum SexFormat { XXYYZZ = 0 , SXXYYZZ, XXYY, SXXYY, XXYYZ, XXXYY };

extern const char * Direction[];
extern const char * SolarSystem[];

#ifdef __cplusplus
extern "C" {
#endif

int validateSex(const char * str, float *x, float *y, float *z);
void formatSex(double number, char * str, int mode);
int getSex(const char *str, double * value);

int extractDate(char *inDate, int *dd, int *mm, int *yy);
int extractTime(char *inTime, int *h, int *m, int *s);
int extractDateTime(char *datetime, char *inTime, char *date);
void formatDateTime(char *datetime, char *inTime, char *date);

#ifdef __cplusplus
}
#endif

#endif

