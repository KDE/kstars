/*
    INDI Lib
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <sys/time.h>

#include "indicom.h"

const char * Direction[] = { "North", "West", "East", "South", "All"};
const char * SolarSystem[] = { "Mercury", "Venus", "Moon", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune", "Pluto"};

int validateSex(const char * str, float *x, float *y, float *z)
{
  int ret;

  ret = sscanf(str, "%f%*c%f%*c%f", x, y, z);
  if (ret < 1)
   return -1;

  if (ret == 1)
  {
   *y = 0;
   *z = 0;
  }
  else if (ret == 2)
   *z = 0;


  return (0);

}

int getSex(const char *str, double * value)
{
  float x,y,z;

  if (validateSex(str, &x, &y, &z))
   return (-1);

  if (x > 0)
   *value = x + (y / 60.0) + (z / (3600.00));
  else
   *value = x - (y / 60.0) - (z / (3600.00));

   return (0);
}

/* Mode 0   XX:YY:ZZ
   Mode 1 +-XX:YY:ZZ
   Mode 2   XX:YY
   Mode 3 +-XX:YY
   Mode 3   XX:YY.Z
   Mode 4   XXX:YY */
void formatSex(double number, char * str, int mode)
{
  int x;
  double y, z;

  x = (int) number;

  y = ((number - x) * 60.00);

  if (y < 0) y *= -1;

  z = (y - (int) y) * 60.00;

  if (z < 0) z *= -1;

  switch (mode)
  {
    case XXYYZZ:
      	sprintf(str, "%02d:%02d:%02.0f", x, (int) y, z);
	break;
    case SXXYYZZ:
        sprintf(str, "%+03d:%02d:%02.0f", x, (int) y, z);
	break;
    case XXYY:
        sprintf(str, "%02d:%02d", x, (int) y);
	break;
    case SXXYY:
    sprintf(str, "%+03d:%02d", x, (int) y);
	break;
    case XXYYZ:
        sprintf(str, "%02d:%02.1f", x, (float) y);
	break;
    case XXXYY:
    	sprintf(str, "%03d:%02d", x, (int) y);
	break;
  }

}

int extractDate(char *inDate, int *dd, int *mm, int *yy)
{
 char buf[16];
 int dates[3];
 unsigned int i,j;
 int slashCount = 0;

 fprintf(stderr, "date %s\n", inDate);
 fprintf(stderr, "length %d\n", strlen(inDate));


 for (i =0, j=0; i <= strlen(inDate); i++)
 {
   if (i == strlen(inDate) || inDate[i] == '/' || inDate[i] == '-' || inDate[i] == ':')
   {
     slashCount++;

     if (slashCount > 3)
     {
      return -1;
      fprintf(stderr, "error: too many fields\n");
     }

      /* +3 digits field, get only last two */
     if (j > 2)
     {
       /* if it it not the years field, return an error */
       if (slashCount > 1)
       {
   	 return -1;
	 fprintf(stderr, "error: field has more than two digits\n");
       }
     }

     switch (j)
     {
      case 1:
      buf[1] = '\0';
      break;

      case 2:
      buf[2] = '\0';
      break;

      case 3:
       fprintf(stderr, "error: field has 3 digits\n");
       return -1;

      case 4:
      buf[0] = buf[2];
      buf[1] = buf[3];
      buf[2] = '\0';
      break;

      default:
       return -1;
     }

     dates[slashCount-1] = atoi(buf);
     j=0;
   }

   else
     buf[j++] = inDate[i];
 }

 *yy = dates[0];
 *mm = dates[1];
 *dd = dates[2];

 if (*mm < 1 || *mm > 12 || *dd < 1 || *yy < 0)
 {
  fprintf(stderr, "date range error\n");
  return -1;
 }

 return 0;

}

int extractTime(char *inTime, int *h, int *m, int *s)
{
 char buf[8];
 int times[3];
 unsigned int i,j;
 int colonCount = 0;

 fprintf(stderr, "time %s\n", inTime);
 fprintf(stderr, "length %d\n", strlen(inTime));


 for (i =0, j=0; i <= strlen(inTime); i++)
 {
   if (i == strlen(inTime) || inTime[i] == ':')
   {
     colonCount++;

     if (colonCount > 3)
     {
      return -1;
      fprintf(stderr, "error: too many fields\n");
     }

     switch (j)
     {
      case 1:
      buf[1] = '\0';
      break;

      case 2:
      buf[2] = '\0';
      break;

     default:
     buf[j] = '\0';
      break;

     }

     times[colonCount-1] = atoi(buf);
     j=0;
   }

   else
     buf[j++] = inTime[i];
 }

 *h = times[0];
 *m = times[1];
 *s = times[2];

 if (*h < 0 || *h > 24 || *m < 0 || *m > 60 || *s < 0 || *s > 60)
 {
  fprintf(stderr, "time range error\n");
  return -1;
 }

 return 0;

}

/* Seperates date form time fields in the ISO time format */
int extractDateTime(char *datetime, char *inTime, char *date)
{
 uint searchT;

 searchT = strcspn(datetime, "T");

 if (searchT == strlen(datetime))
  return (-1);

 strncpy(date, datetime, searchT);
 date[searchT] = '\0';

 strcpy(inTime, datetime + searchT + 1);
 inTime[strlen(datetime)] = '\0';

 return (0);
}

/* Forms YYYY-MM-DDTHH:MM:SS */
void formatDateTime(char *datetime, char *inTime, char *date)
{
 sprintf(datetime, "%sT%s", date, inTime);
}

