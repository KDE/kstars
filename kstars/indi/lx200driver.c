#if 0
    LX200 Driver
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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>

#include "indicom.h"
#include "indidevapi.h"
#include "lx200driver.h"

#define LX200_TIMEOUT	5		/* FD timeout in seconds */

int fd;
int read_ret, write_ret;

/**************************************************************************
 Basic I/O
**************************************************************************/
int openPort(const char *portID);
int portRead(char *buf, int nbytes, int timeout);
int portWrite(const char * buf);
int LX200readOut(int timeout);

int Connect(const char* device);
void Disconnect(void);

/**************************************************************************
 Diagnostics
 **************************************************************************/
char ACK(void);
int testTelescope(void);
int testAP(void);

/**************************************************************************
 Get Commands: store data in the supplied buffer. Return 0 on success or -1 on failure 
 **************************************************************************/
 
/* Get Double from Sexagisemal */
int getCommandSexa(double *value, const char *cmd);
/* Get String */
int getCommandString(char *data, const char* cmd);
/* Get Int */
int getCommandInt(int *value, const char* cmd);
/* Get tracking frequency */
int getTrackFreq(double * value);
/* Get site Latitude */
int getSiteLatitude(int *dd, int *mm);
/* Get site Longitude */
int getSiteLongitude(int *ddd, int *mm);
/* Get Calender data */
int getCalenderDate(char *date);
/* Get site Name */
int getSiteName(char *siteName, int siteNum);
/* Get Number of Bars */
int getNumberOfBars(int *value);
/* Get Home Search Status */
int getHomeSearchStatus(int *status);
/* Get OTA Temperature */
int getOTATemp(double * value);
/* Get time format: 12 or 24 */
int getTimeFormat(int *format);


/**************************************************************************
 Set Commands
 **************************************************************************/

/* Set Int */
int setCommandInt(int data, const char *cmd);
/* Set Sexigesimal */
int setCommandXYZ( int x, int y, int z, const char *cmd);
/* Common routine for Set commands */
int setStandardProcedure(char * writeData);
/* Set Slew Mode */
int setSlewMode(int slewMode);
/* Set Alignment mode */
int setAlignmentMode(unsigned int alignMode);
/* Set Object RA */
int setObjectRA(double ra);
/* set Object DEC */
int setObjectDEC(double dec);
/* Set Calender date */
int setCalenderDate(int dd, int mm, int yy);
/* Set UTC offset */
int setUTCOffset(double hours);
/* Set Track Freq */
int setTrackFreq(double trackF);
/* Set current site longitude */
int setSiteLongitude(double Long);
/* Set current site latitude */
int setSiteLatitude(double Lat);
/* Set Object Azimuth */
int setObjAz(double az);
/* Set Object Altitude */
int setObjAlt(double alt);
/* Set site name */
int setSiteName(char * siteName, int siteNum);
/* Set maximum slew rate */
int setMaxSlewRate(int slewRate);
/* Set focuser motion */
int setFocuserMotion(int motionType);
/* Set focuser speed mode */
int setFocuserSpeedMode (int speedMode);
/* Set minimum elevation limit */
int setMinElevationLimit(int min);
/* Set maximum elevation limit */
int setMaxElevationLimit(int max);

/**************************************************************************
 Motion Commands
 **************************************************************************/
/* Slew to the selected coordinates */
int Slew(void);
/* Synchronize to the selected coordinates and return the matching object if any */
int Sync(char *matchedObject);
/* Abort slew in all axes */
int abortSlew(void);
/* Move into one direction, two valid directions can be stacked */
int MoveTo(int direction);
/* Half movement in a particular direction */
int HaltMovement(int direction);
/* Select the tracking mode */
int selectTrackingMode(int trackMode);
/* Select Astro-Physics tracking mode */
int selectAPTrackingMode(int trackMode);

/**************************************************************************
 Other Commands
 **************************************************************************/
 /* Ensures LX200 RA/DEC format is long */
int checkLX200Format(void);
/* Select a site from the LX200 controller */
int selectSite(int siteNum);
/* Select a catalog object */
int selectCatalogObject(int catalog, int NNNN);
/* Select a sub catalog */
int selectSubCatalog(int catalog, int subCatalog);

/**********************************************************************
* BASIC
**********************************************************************/

int Connect(const char *device)
{
 fprintf(stderr, "Connecting to device %s\n", device);
 
 if (openPort(device) < 0)
  return -1;
 else
  return 0;
}

void Disconnect()
{
fprintf(stderr, "Disconnected.\n");
close(fd);
}

int testTelescope()
{  
  int i=0;
  char ack[1] = { (char) 0x06 };
  char MountAlign[64];
  fprintf(stderr, "Testing telescope's connection...\n");

  for (i=0; i < 2; i++)
  {
    write(fd, ack, 1);
    read_ret = portRead(MountAlign, 1, LX200_TIMEOUT);
    if (read_ret == 1)
     return 0;
    usleep(50000);
  }
  
  return -1;
}

int testAP()
{
   int i=0;
   char currentDate[64];

   fprintf(stderr, "Testing telescope's connection...\n");

  /* We need to test if the telescope is responding
  / We're going to request the calander date */
  for (i=0; i < 2; i++)
  {
    if (!getCalenderDate(currentDate))
     return 0;

    usleep(50000);
  }
  
  return -1;

}


/**********************************************************************
* GET
**********************************************************************/

char ACK()
{
  char ack[1] = { (char) 0x06 };
  char MountAlign[2];

  write_ret = write(fd, ack, 1);

  if (write_ret < 0)
    return -1;
 
  read_ret = portRead(MountAlign, 1, LX200_TIMEOUT);
  
  if (read_ret == 1)
    return MountAlign[0];
  else
    return read_ret;

}

int getCommandSexa(double *value, const char * cmd)
{
  char tempString[16];
  
  tcflush(fd, TCIFLUSH);

  if (portWrite(cmd) < 0)
   return -1;
  
  if ( (read_ret = portRead(tempString, -1, LX200_TIMEOUT)) < 1)
     return read_ret;
 
  tempString[read_ret - 1] = '\0';
  
  if (f_scansexa(tempString, value))
  {
   fprintf(stderr, "unable to process [%s]\n", tempString);
   return -1;
  }
 
   return 0;
}

int getCommandString(char *data, const char* cmd)
{
    char * term;
    
    if (portWrite(cmd) < 0)
      return -1;

    read_ret = portRead(data, -1, LX200_TIMEOUT);
    
    if (read_ret < 1)
     return read_ret;

    term = strchr (data, '#');
    if (term)
      *term = '\0';

    fprintf(stderr, "Requested data: %s\n", data);

    return 0;
}

int getCalenderDate(char *date)
{

 int dd, mm, yy;
 int err;

 if ( (err = getCommandString(date, "#:GC#")) )
   return err;

 /* Meade format is MM/DD/YY */

  read_ret = sscanf(date, "%d%*c%d%*c%d", &mm, &dd, &yy);
  if (read_ret < 3)
   return -1;

 /* We need to have in in YYYY/MM/DD format */
 sprintf(date, "20%02d/%02d/%02d", yy, mm, dd);

 return (0);

}

int getTimeFormat(int *format)
{
  char tempString[16];
  int tMode;

  if (portWrite("#:Gc#") < 0)
    return -1;

  read_ret = portRead(tempString, -1, LX200_TIMEOUT);
  
  if (read_ret < 1)
   return read_ret;
   
  tempString[read_ret-1] = '\0';

  read_ret = sscanf(tempString, "(%d)", &tMode);

  if (read_ret < 1)
   return -1;
  else
   *format = tMode;
   
  return 0;

}

/*int getUTCOffset()
{
    char tempString[4];
    int offSet;

    portWrite("#:GG#");

    read_ret = portRead(tempString, 4);
    if (read_ret)
     return -1;

    tempString[3] = '\0';

    sscanf(tempString, "%d", &offSet);

    fprintf(stderr, "UTC Offset: %d\n", offSet);

    return offSet;
}

int getMaxElevationLimit()
{
    char tempString[16];
    int limit;

    portWrite("#:Go#");

    read_ret = portRead(tempString, -1, LX200_TIMEOUT);
    if (read_ret < 1)
     return -1;

    tempString[read_ret-1] = '\0';

    sscanf(tempString, "%d", &limit);

    fprintf(stderr, "Max elevation limit string is %s\n", tempString);

    return limit;
}

int getMinElevationLimit()
{
    char tempString[16];
    int limit;

    portWrite("#:Gh#");

    read_ret = portRead(tempString, -1, LX200_TIMEOUT);
    if (read_ret < 1)
     return -1;

    tempString[read_ret-1] = '\0';

    sscanf(tempString, "%d", &limit);

    fprintf(stderr, "Min elevation limit string is %s\n", tempString);

    return limit;

}
*/

int getSiteName(char *siteName, int siteNum)
{
  char * term;

  switch (siteNum)
  {
    case 1:
     if (portWrite("#:GM#") < 0)
      return -1;
     break;
    case 2:
     if (portWrite("#:GN#") < 0)
      return -1;
     break;
    case 3:
     if (portWrite("#:GO#") < 0)
       return -1;
     break;
    case 4:
     if (portWrite("#:GP#") < 0)
      return -1;
     break;
    default:
     return -1;
   }

   read_ret = portRead(siteName, -1, LX200_TIMEOUT);
   if (read_ret < 1)
     return read_ret;

   siteName[read_ret - 1] = '\0';

   term = strchr (siteName, ' ');
    if (term)
      *term = '\0';

    term = strchr (siteName, '<');
    if (term)
      strcpy(siteName, "unused site");

   fprintf(stderr, "Requested site name: %s\n", siteName);

    return 0;
}

int getSiteLatitude(int *dd, int *mm)
{
  char tempString[16];
  
  if (portWrite("#:Gt#") < 0)
    return -1;

  read_ret = portRead(tempString, -1, LX200_TIMEOUT);
  
   if (read_ret < 1) 
   return read_ret;
   
  tempString[read_ret -1] = '\0';

  if (sscanf (tempString, "%d%*c%d", dd, mm) < 2)
   return -1;

  fprintf(stderr, "Requested site latitude in String %s\n", tempString);
  fprintf(stderr, "Requested site latitude %d:%d\n", *dd, *mm);

  return 0;
}

int getSiteLongitude(int *ddd, int *mm)
{
  char tempString[16];

  if (portWrite("#:Gg#") < 0)
   return -1;

  read_ret = portRead(tempString, -1, LX200_TIMEOUT);
  
  if (read_ret < 1)
    return read_ret;
    
  tempString[read_ret -1] = '\0';

  if (sscanf (tempString, "%d%*c%d", ddd, mm) < 2)
   return -1;

  fprintf(stderr, "Requested site longitude in String %s\n", tempString);
  fprintf(stderr, "Requested site longitude %d:%d\n", *ddd, *mm);

  return 0;
}

int getTrackFreq(double *value)
{
    float Freq;
    char tempString[16];
    
    if (portWrite("#:GT#") < 0)
      return -1;

    read_ret = portRead(tempString, -1, LX200_TIMEOUT);
    
    if (read_ret < 1)
     return read_ret;

    tempString[read_ret] = '\0';
    
    /*fprintf(stderr, "Telescope tracking freq str: %s\n", tempString);*/
    
    if (sscanf(tempString, "%f#", &Freq) < 1)
     return -1;
   
    *value = (double) Freq;
    
    /*fprintf(stderr, "Tracking frequency value is %f\n", Freq);*/
    
    return 0;
}

int getNumberOfBars(int *value)
{
   char tempString[128];

   if (portWrite("#:D#") < 0)
     return -1;

   read_ret = portRead(tempString, -1, LX200_TIMEOUT);
   
   if (read_ret < 0)
    return read_ret;

   *value = read_ret -1;
   
   return 0;
}

int getHomeSearchStatus(int *status)
{
  char tempString[16];

  if (portWrite("#:h?#") < 0)
   return -1;

  read_ret = portRead(tempString, 1, LX200_TIMEOUT);
  
  if (read_ret < 1)
   return read_ret;
   
  tempString[1] = '\0';

  if (tempString[0] == '0')
    *status = 0;
  else if (tempString[0] == '1')
    *status = 1;
  else if (tempString[0] == '2')
    *status = 1;
  
  return 0;
}

int getOTATemp(double *value)
{

  char tempString[16];
  float temp;
  
  if (portWrite("#:fT#") < 0)
   return -1;

  read_ret = portRead(tempString, -1, LX200_TIMEOUT);
  
  if (read_ret < 1)
   return read_ret;
   
  tempString[read_ret - 1] = '\0';

  if (sscanf(tempString, "%f", &temp) < 1)
   return -1;
   
   *value = (double) temp;

  return 0;

}

int updateSkyCommanderCoord(double *ra, double *dec)
{
  char coords[16];
  char CR[1] = { (char) 0x0D };
  float RA=0.0, DEC=0.0;

  write(fd, CR, 1);

  read_ret = portRead(coords, 16, LX200_TIMEOUT);

  read_ret = sscanf(coords, " %g %g", &RA, &DEC);

  if (read_ret < 2)
  {
   fprintf(stderr, "Error in Sky commander number format [%s], exiting.\n", coords);
   return -1;
  }

  *ra  = RA;
  *dec = DEC;

  return 0;

}


/**********************************************************************
* SET
**********************************************************************/

int setStandardProcedure(char * data)
{
 char boolRet[2];
 
 if (portWrite(data) < 0)
  return -1;
 
 read_ret = portRead(boolRet, 1, LX200_TIMEOUT);
 
 if (read_ret < 1)
   return read_ret;

 if (boolRet[0] == '0')
 {
     fprintf(stderr, "%s Failed.\n", data);
     return -1;
 }

 fprintf(stderr, "%s Successful\n", data);
 return 0;


}

int setCommandInt(int data, const char *cmd)
{

  char tempString[16];

  snprintf(tempString, sizeof( tempString ), "%s%d#", cmd, data);

  if (portWrite(tempString) < 0)
    return -1;

  return 0;
}

int setMinElevationLimit(int min)
{
 char tempString[16];

 snprintf(tempString, sizeof( tempString ), "#:Sh%02d#", min);

 return (setStandardProcedure(tempString));
}

int setMaxElevationLimit(int max)
{
 char tempString[16];

 snprintf(tempString, sizeof( tempString ), "#:So%02d*#", max);

 return (setStandardProcedure(tempString));

}

int setMaxSlewRate(int slewRate)
{

   char tempString[16];

   if (slewRate < 2 || slewRate > 8)
    return -1;

   snprintf(tempString, sizeof( tempString ), "#:Sw%d#", slewRate);

   return (setStandardProcedure(tempString));

}


int setObjectRA(double ra)
{

 int h, m, s;
 char tempString[16];

 getSexComponents(ra, &h, &m, &s);

 snprintf(tempString, sizeof( tempString ), "#:Sr %02d:%02d:%02d#", h, m, s);
 IDLog("Set Object RA String %s\n", tempString);
  return (setStandardProcedure(tempString));
}


int setObjectDEC(double dec)
{
  int d, m, s;
  char tempString[16];

  getSexComponents(dec, &d, &m, &s);

  /* case with negative zero */
  if (!d && dec < 0)
    snprintf(tempString, sizeof( tempString ), "#:Sd -%02d:%02d:%02d#", d, m, s);
  else
    snprintf(tempString, sizeof( tempString ), "#:Sd %+03d:%02d:%02d#", d, m, s);

  IDLog("Set Object DEC String %s\n", tempString);
  
   return (setStandardProcedure(tempString));

}

int setCommandXYZ(int x, int y, int z, const char *cmd)
{
  char tempString[16];

  snprintf(tempString, sizeof( tempString ), "%s %02d:%02d:%02d#", cmd, x, y, z);

  return (setStandardProcedure(tempString));
}

int setAlignmentMode(unsigned int alignMode)
{
  fprintf(stderr , "Set alignment mode %d\n", alignMode);

  switch (alignMode)
   {
     case LX200_ALIGN_POLAR:
      if (portWrite("#:AP#") < 0)
        return -1;
      break;
     case LX200_ALIGN_ALTAZ:
      if (portWrite("#:AA#") < 0)
        return -1;
      break;
     case LX200_ALIGN_LAND:
       if (portWrite("#:AL#") < 0)
        return -1;
       break;
   }
   
   return 0;
}

int setCalenderDate(int dd, int mm, int yy)
{
   char tempString[32];
   char dumpPlanetaryUpdateString[64];
   char boolRet[2];
   yy = yy % 100;

   snprintf(tempString, sizeof( tempString ), "#:SC %02d/%02d/%02d#", mm, dd, yy);

   if (portWrite(tempString) < 0)
    return -1;

   read_ret = portRead(boolRet, 1, LX200_TIMEOUT);
   
   if (read_ret < 1)
    return read_ret;
    
   boolRet[1] = '\0';

   if (boolRet[0] == '0')
     return -1;
     
    /* Read dumped data */
    portRead(dumpPlanetaryUpdateString, -1, LX200_TIMEOUT);
    portRead(dumpPlanetaryUpdateString, -1, 5);

   return 0;
}

int setUTCOffset(double hours)
{
   char tempString[16];

   /*TODO add fractions*/
   snprintf(tempString, sizeof( tempString ), "#:SG %+03d#", (int) hours);

   fprintf(stderr, "UTC string is %s\n", tempString);

   return (setStandardProcedure(tempString));

}

int setSiteLongitude(double Long)
{
   int d, m, s;
   char tempString[32];

   getSexComponents(Long, &d, &m, &s);

   snprintf(tempString, sizeof( tempString ), "#:Sg%03d:%02d#", d, m);

   return (setStandardProcedure(tempString));
}

int setSiteLatitude(double Lat)
{
   int d, m, s;
   char tempString[32];

   getSexComponents(Lat, &d, &m, &s);

   snprintf(tempString, sizeof( tempString ), "#:St%+03d:%02d:%02d#", d, m, s);

   return (setStandardProcedure(tempString));
}

int setObjAz(double az)
{
   int d,m,s;
   char tempString[16];

   getSexComponents(az, &d, &m, &s);

   snprintf(tempString, sizeof( tempString ), "#:Sz%03d:%02d#", d, m);

   return (setStandardProcedure(tempString));

}

int setObjAlt(double alt)
{
    int d, m, s;
    char tempString[16];

   getSexComponents(alt, &d, &m, &s);

   snprintf(tempString, sizeof( tempString ), "#:Sa%+02d*%02d#", d, m);

   return (setStandardProcedure(tempString));
}


int setSiteName(char * siteName, int siteNum)
{

   char tempString[16];

   switch (siteNum)
   {
     case 1:
      snprintf(tempString, sizeof( tempString ), "#:SM %s#", siteName);
      break;
     case 2:
      snprintf(tempString, sizeof( tempString ), "#:SN %s#", siteName);
      break;
     case 3:
      snprintf(tempString, sizeof( tempString ), "#:SO %s#", siteName);
      break;
    case 4:
      snprintf(tempString, sizeof( tempString ), "#:SP %s#", siteName);
      break;
    default:
      return -1;
    }

   return (setStandardProcedure(tempString));
}

int setSlewMode(int slewMode)
{

  switch (slewMode)
  {
    case LX200_SLEW_MAX:
     if (portWrite("#:RS#") < 0)
      return -1;
     break;
    case LX200_SLEW_FIND:
     if (portWrite("#:RM#") < 0)
      return -1;
     break;
    case LX200_SLEW_CENTER:
     if (portWrite("#:RC#") < 0)
      return -1;
     break;
    case LX200_SLEW_GUIDE:
     if (portWrite("#:RG#") < 0)
      return -1;
     break;
    default:
     break;
   }
   
   return 0;

}

int setFocuserMotion(int motionType)
{

  switch (motionType)
  {
    case LX200_FOCUSIN:
    if (portWrite("#:F+#") < 0)
      return -1;
      break;
    case LX200_FOCUSOUT:
     if (portWrite("#:F-#") < 0)
       return -1;
     break;
  }

  return 0;
}

int setFocuserSpeedMode (int speedMode)
{

 switch (speedMode)
 {
    case LX200_HALTFOCUS:
     if (portWrite("#:FQ#") < 0)
      return -1;
     break;
   case LX200_FOCUSSLOW:
      if (portWrite("#:FS#") < 0)
       return -1;
      break;
    case LX200_FOCUSMEDIUM:
     if (portWrite("#:F3#") < 0)
      return -1;
      break;
    case LX200_FOCUSFAST:
     if (portWrite("#:FF#") < 0)
      return -1;
     break;
 }
 
 return 0;

}

int setTrackFreq(double trackF)
{
  char tempString[16];

  snprintf(tempString, sizeof( tempString ), "#:ST %04.1f#", trackF);

  return (setStandardProcedure(tempString));

}

/**********************************************************************
* Misc
*********************************************************************/

int Slew()
{
    char slewNum[2];
    char errorMsg[128];

    if (portWrite("#:MS#") < 0)
      return -1;

    read_ret = portRead(slewNum, 1, LX200_TIMEOUT);
    
    if (read_ret < 1)
      return read_ret;
      
    slewNum[1] = '\0';

    if (slewNum[0] == '0')
     return 0;
   
   read_ret = portRead(errorMsg, -1, LX200_TIMEOUT);
   
   if (read_ret < 1)
    return read_ret;
    
    if  (slewNum[0] == '1')
      return 1;
    else return 2;

}

int MoveTo(int direction)
{

  switch (direction)
  {
    case LX200_NORTH:
    portWrite("#:Mn#");
    break;
    case LX200_WEST:
    portWrite("#:Mw#");
    break;
    case LX200_EAST:
    portWrite("#:Me#");
    break;
    case LX200_SOUTH:
    portWrite("#:Ms#");
    break;
    default:
    break;
  }
  
  return 0;
}

int HaltMovement(int direction)
{

switch (direction)
  {
    case LX200_NORTH:
     if (portWrite("#:Qn#") < 0)
      return -1;
     break;
    case LX200_WEST:
     if (portWrite("#:Qw#") < 0)
      return -1;
     break;
    case LX200_EAST:
     if (portWrite("#:Qe#") < 0)
      return -1;
     break;
    case LX200_SOUTH:
     if (portWrite("#:Qs#") < 0)
       return -1;
    break;
    case LX200_ALL:
     if (portWrite("#:Q#") < 0)
       return -1;
     break;
    default:
     return -1;
    break;
  }
  
  return 0;

}

int abortSlew()
{
 if (portWrite("#:Q#") < 0)
  return -1;

 return 0;
}

int Sync(char *matchedObject)
{
  portWrite("#:CM#");

  read_ret = portRead(matchedObject, -1, LX200_TIMEOUT);
  
  if (read_ret < 1)
   return read_ret;
   
  matchedObject[read_ret-1] = '\0';
  
  /* Sleep 10ms before flushing. This solves some issues with LX200 compatible devices. */
  usleep(10000);
  
  tcflush(fd, TCIFLUSH);

  return 0;
}

int selectSite(int siteNum)
{

  switch (siteNum)
  {
    case 1:
      if (portWrite("#:W1#") < 0)
       return -1;
      break;
    case 2:
      if (portWrite("#:W2#") < 0)
       return -1;
      break;
    case 3:
      if (portWrite("#:W3#") < 0)
       return -1;
      break;
    case 4:
      if (portWrite("#:W4#") < 0)
       return -1;
      break;
    default:
    return -1;
    break;
  }
  
  return 0;

}

int selectCatalogObject(int catalog, int NNNN)
{
 char tempString[16];
 
 switch (catalog)
 {
   case LX200_STAR_C:
    snprintf(tempString, sizeof( tempString ), "#:LS%d#", NNNN);
    break;
   case LX200_DEEPSKY_C:
    snprintf(tempString, sizeof( tempString ), "#:LC%d#", NNNN);
    break;
   case LX200_MESSIER_C:
    snprintf(tempString, sizeof( tempString ), "#:LM%d#", NNNN);
    break;
   default:
    return -1;
  }

  if (portWrite(tempString) < 0)
   return -1;

  return 0;
}

int selectSubCatalog(int catalog, int subCatalog)
{
  char tempString[16];
  switch (catalog)
  {
    case LX200_STAR_C:
    snprintf(tempString, sizeof( tempString ), "#:LsD%d#", subCatalog);
    break;
    case LX200_DEEPSKY_C:
    snprintf(tempString, sizeof( tempString ), "#:LoD%d#", subCatalog);
    break;
    case LX200_MESSIER_C:
     return 1;
    default:
     return 0;
  }

  return (setStandardProcedure(tempString));
}

int checkLX200Format()
{

  char tempString[16];

  if (portWrite("#:GR#") < 0)
   return -1;

  read_ret = portRead(tempString, -1, LX200_TIMEOUT);
  
  if (read_ret < 1)
   return read_ret;
   
  tempString[read_ret - 1] = '\0';

  /* Short */
  if (tempString[5] == '.')
     if (portWrite("#:U#") < 0)
      return -1;
  
  return 0;
}

int  selectTrackingMode(int trackMode)
{

   switch (trackMode)
   {
    case LX200_TRACK_DEFAULT:
      fprintf(stderr, "Setting tracking mode to sidereal.\n");
     if (portWrite("#:TQ#") < 0)
       return -1;
     break;
    case LX200_TRACK_LUNAR:
      fprintf(stderr, "Setting tracking mode to LUNAR.\n");
     if (portWrite("#:TL#") < 0)
       return -1;
     break;
   case LX200_TRACK_MANUAL:
     fprintf(stderr, "Setting tracking mode to CUSTOM.\n");
     if (portWrite("#:TM#") < 0)
      return -1;
     break;
   default:
    return -1;
    break;
   }
   
   return 0;

}

int selectAPTrackingMode(int trackMode)
{
    switch (trackMode)
   {
    /* Lunar */
    case 0:
      fprintf(stderr, "Setting tracking mode to lunar.\n");
     if (portWrite("#:RT0#") < 0)
       return -1;
     break;
    
     /* Solar */
     case 1:
      fprintf(stderr, "Setting tracking mode to solar.\n");
     if (portWrite("#:RT1#") < 0)
       return -1;
     break;

   /* Sidereal */
   case 2:
     fprintf(stderr, "Setting tracking mode to sidereal.\n");
     if (portWrite("#:RT2#") < 0)
      return -1;
     break;

   /* Zero */
   case 3:
     fprintf(stderr, "Setting tracking mode to zero.\n");
     if (portWrite("#:RT9#") < 0)
      return -1;
     break;
 
   default:
    return -1;
    break;
   }
   
   return 0;

}


/**********************************************************************
* Comm
**********************************************************************/

int openPort(const char *portID)
{
  struct termios ttyOptions;

  if ( (fd = open(portID, O_RDWR)) == -1)
    return -1;

  memset(&ttyOptions, 0, sizeof(ttyOptions));
  tcgetattr(fd, &ttyOptions);

   /* Control options
    charecter size */
   ttyOptions.c_cflag &= ~CSIZE;
   /* 8 bit, enable read */
   ttyOptions.c_cflag |= CREAD | CLOCAL | CS8;
   /* no parity */
   ttyOptions.c_cflag &= ~PARENB;

   /* set baud rate */
   cfsetispeed(&ttyOptions, B9600);
   cfsetospeed(&ttyOptions, B9600);

  /* set input/output flags */
  ttyOptions.c_iflag = IGNBRK;
  /* no software flow control */
  ttyOptions.c_iflag &= ~(IXON|IXOFF|IXANY);

  /* Read at least one byte */
  ttyOptions.c_cc[VMIN] = 1;
  ttyOptions.c_cc[VTIME] = 5;

  /* Misc. */
  ttyOptions.c_lflag = 0;
  ttyOptions.c_oflag = 0;

  /* set attributes */
  tcsetattr(fd, TCSANOW, &ttyOptions);

  /* flush the channel */
  tcflush(fd, TCIOFLUSH);
  return (fd);
}

int portWrite(const char * buf)
{
  int nbytes, totalBytesWritten;
  int bytesWritten = 0;   
   
  nbytes = totalBytesWritten = strlen(buf);

  while (nbytes > 0)
  {
    
    bytesWritten = write(fd, buf, nbytes);

    if (bytesWritten < 0)
     return -1;

    buf += bytesWritten;
    nbytes -= bytesWritten;
  }

  /* Returns the # of bytes written */
  return (totalBytesWritten);
}

int portRead(char *buf, int nbytes, int timeout)
{

int bytesRead = 0;
int totalBytesRead = 0;
int err;

  /* Loop until encountring the '#' char */
  if (nbytes == -1)
  {
     for (;;)
     {
         if ( (err = LX200readOut(timeout)) )
	   return err;

         bytesRead = read(fd, buf, 1);

         if (bytesRead < 0 )
            return -1;

        if (bytesRead)
          totalBytesRead++;

        if (*buf == '#')
	   return totalBytesRead;

        buf += bytesRead;
     }
  }
  
  while (nbytes > 0)
  {
     if ( (err = LX200readOut(timeout)) )
      return err;

     bytesRead = read(fd, buf, nbytes);

     if (bytesRead < 0 )
      return -1;

     buf += bytesRead;
     totalBytesRead++;
     nbytes -= bytesRead;
  }

  return totalBytesRead;
}

int LX200readOut(int timeout)
{
  struct timeval tv;
  fd_set readout;
  int retval;

  FD_ZERO(&readout);
  FD_SET(fd, &readout);

  /* wait for 'timeout' seconds */
  tv.tv_sec = timeout;
  tv.tv_usec = 0;

  /* Wait till we have a change in the fd status */
  retval = select (fd+1, &readout, NULL, NULL, &tv);

  /* Return 0 on successful fd change */
  if (retval > 0)
   return 0;
  /* Return -1 due to an error */
  else if (retval == -1)
   return retval;
  /* Return -2 if time expires before anything interesting happens */
  else 
    return -2;
  
}

