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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

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
#include "lx200driver.h"

/* LX200 definitions */
int fd;
int read_ret, write_ret;

int LX200readOut(int timeout);
int openPort(const char *portID);
int portReadT(char *buf, int nbytes, int timeout);
int portWrite(const char * buf);
int portRead(char *buf, int nbytes);
int Connect(const char* device);
int testTelescope(void);
void Disconnect(void);
char ACK(void);
void checkLX200Format(void);
double getCommand(const char *cmd);
double getTrackFreq(void);
int getCommandStr(char *data, const char* cmd);
int getTimeFormat(void);
int getCalenderDate(char *date);
int getUCTOffset(void);
int getSiteName(char *siteName, int siteNum);
int getSiteLatitude(int *dd, int *mm);
int getSiteLongitude(int *ddd, int *mm);
int getNumberOfBars(void);
int getHomeSearchStatus(void);
double getOTATemp(void);
int setCommand(double data, const char *cmd);
void setCommandInt(int data, const char *cmd);
int setCommandXYZ( int x, int y, int z, const char *cmd);
int setStandardProcedure(char * writeData);
void setSlewMode(int slewMode);
void setAlignmentMode(unsigned int alignMode);
int setObjectRA(double h, double m, double s);
int setObjectDEC(double d, double m, double s);
int setCalenderDate(int dd, int mm, int yy);
int setUTCOffset(int hours);
int setTrackFreq(double trackF);
int setSiteLongitude(int degrees, int minutes);
int setSiteLatitude(int degrees, int minutes, int seconds);
int setObjAz(int degrees, int minutes);
int setObjAlt(int degrees, int minutes);
int setSiteName(char * siteName, int siteNum);
void setFocuserMotion(int motionType);
void setFocuserSpeedMode (int speedMode);
int Slew(void);
void Sync(char *matchedObject);
void abortSlew(void);
void MoveTo(int direction);
void HaltMovement(int direction);
void selectSite(int siteNum);
void selectCatalogObject(int catalog, int NNNN);
void selectTrackingMode(int trackMode);
int selectSubCatalog(int catalog, int subCatalog);
int setMinElevationLimit(int min);
int setMaxElevationLimit(int max);
int getMaxElevationLimit(void);
int getMinElevationLimit(void);


/**********************************************************************
* BASIC
**********************************************************************/

int Connect(const char *device)
{
 fprintf(stderr, "connecting to device %s\n", device);
 if (openPort(device) < 0)
  return -1;
 else
  return 0;
}

void Disconnect()
{

close(fd);

}

int testTelescope()
{
  char ack[1] = { (char) 0x06 };
  char MountAlign[2];

  write(fd, ack, 1);
  return portRead(MountAlign, 1);
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

  if ( !(read_ret = portRead(MountAlign, 1)))
   return MountAlign[0];
  else
   return -1;

}

double getCommand(const char * cmd)
{
  char tempString[16];
  double value;

  portWrite(cmd);

    read_ret = portRead(tempString, -1);
    tempString[read_ret - 1] = '\0';

  if (getSex(tempString, &value))
   return -1000;
 else
   return value;
}

int getCommandStr(char *data, const char* cmd)
{
    char * term;
    portWrite(cmd);

    read_ret = portRead(data, -1);
    if (read_ret < 1)
     return -1;

    term = strchr (data, '#');
    if (term)
      *term = '\0';

    fprintf(stderr, "Request data: %s\n", data);

    return 0;
}

int getCalenderDate(char *date)
{

 float dd, mm, yy;

 if (getCommandStr(date, "#:GC#"))
  return (-1);

 /* Meade format is MM/DD/YY */
 if (validateSex(date, &mm, &dd, &yy))
  return (-1);

 /* We need to have in in YYYY/MM/DD format */
 sprintf(date, "20%02d/%02d/%02d", (int) yy, (int) mm, (int) dd);

 return (0);

}

int getTimeFormat()
{
  char tempString[16];
  int tMode;

  portWrite("#:Gc#");

  read_ret = portRead(tempString, -1);
  tempString[read_ret-1] = '\0';

  read_ret = sscanf(tempString, "(%d)", &tMode);

  if (read_ret < 1)
   return -1;
  else
  return tMode;

}

int getUCTOffset()
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

    read_ret = portRead(tempString, -1);
    if (read_ret)
     return -1;

    tempString[read_ret-1] = '\0';

    sscanf(tempString, "%d", &limit);

    return limit;
}

int getMinElevationLimit()
{
    char tempString[16];
    int limit;

    portWrite("#:Gh#");

    read_ret = portRead(tempString, -1);
    if (read_ret)
     return -1;

    tempString[read_ret-1] = '\0';

    sscanf(tempString, "%d", &limit);

    return limit;

}

int getSiteName(char *siteName, int siteNum)
{
  char * term;

  switch (siteNum)
  {
    case 1:
     portWrite("#:GM#");
     break;
    case 2:
     portWrite("#:GN#");
     break;
    case 3:
     portWrite("#:GO#");
     break;
    case 4:
     portWrite("#:GP#");
     break;
    default:
     return -1;
   }

   read_ret = portRead(siteName, -1);
   if (read_ret < 1)
     return -1;

   siteName[read_ret - 1] = '\0';
   term = strchr (siteName, ' ');
    if (term)
      *term = '\0';

   fprintf(stderr, "Requested site name: %s\n", siteName);

    return 0;

}

int getSiteLatitude(int *dd, int *mm)
{
  char tempString[16];

  portWrite("#:Gt#");

  read_ret = portRead(tempString, -1);
  tempString[read_ret -1] = '\0';

  if (!sscanf (tempString, "%d%*c%d", dd, mm))
   return -1000;

  fprintf(stderr, "Requested site latitude in String %s\n", tempString);
  fprintf(stderr, "Requested site latitude %d:%d\n", *dd, *mm);

  return 0;
}

int getSiteLongitude(int *ddd, int *mm)
{
  char tempString[16];

  portWrite("#:Gg#");

  read_ret = portRead(tempString, -1);
  tempString[read_ret -1] = '\0';

  if (!sscanf (tempString, "%d%*c%d", ddd, mm))
   return -1000;

  fprintf(stderr, "Requested site longitude in String %s\n", tempString);
  fprintf(stderr, "Requested site longitude %d:%d\n", *ddd, *mm);

  return 0;
}

double getTrackFreq()
{
    float Freq = 0;
    char tempString[16];
    portWrite("#:GT#");

    read_ret = portRead(tempString, -1);
    if (read_ret < 1)
     return -1;

    tempString[read_ret] = '\0';
    sscanf(tempString, "%f#", &Freq);

    return (double) Freq;
}

int getNumberOfBars()
{
   char tempString[128];

   portWrite("#:D#");

   read_ret = portRead(tempString, -1);

   return (read_ret - 1);

}

int getHomeSearchStatus()
{
  char tempString[16];

  portWrite("#:h?#");

  portRead(tempString, 1);
  tempString[1] = '\0';

  if (tempString[0] == '0')
    return 0;
  else if (tempString[0] == '1')
    return 1;
  else if (tempString[0] == '2')
    return 2;
  else
    return -1;
}

double getOTATemp()
{

  char tempString[16];
  float temp;
  portWrite("#:fT#");

  read_ret = portRead(tempString, -1);
  if (read_ret < 1)
   return READ_ERROR;
  tempString[read_ret - 1] = '\0';

  if (!sscanf(tempString, "%f", &temp))
   return READ_ERROR;

  return (double) temp;

}

/**********************************************************************
* SET
**********************************************************************/

int setStandardProcedure(char * data)
{
 char boolRet[2];
 portWrite(data);
 read_ret = portRead(boolRet, 1);

   if (boolRet[0] == '0')
   {
     fprintf(stderr, "Failure\n");
     return -1;
   }

   fprintf(stderr, "Success\n");
   return 0;


}

void setCommandInt(int data, const char *cmd)
{

  char tempString[16];

  sprintf(tempString, "%s%d#", cmd, data);

  portWrite(tempString);

}

/*int setObjectRA(double RA)
{
  char tempString[16];
  int hours = (int) data;
  int minutes = (int) ((data - hours) * 60.0);
  int seconds = (int) ( (data - hours) * 60.0 - minutes);

  sprintf(tempString, "#:Sr %02d:%02d:%02d#", hours, minutes, seconds);
  return (setStandardProcedure(tempString));
}*/

int setMinElevationLimit(int min)
{
 char tempString[16];

 sprintf(tempString, "#:Sh%02d#", min);

 return (setStandardProcedure(tempString));
}

int setMaxElevationLimit(int max)
{
 char tempString[16];

 sprintf(tempString, "#:So%02d#", max);

 return (setStandardProcedure(tempString));

}


int setObjectRA(double h, double m, double s)
{

 char tempString[16];
 sprintf(tempString, "#:Sr %02d:%02d:%02d#", (int) h, (int) m, (int) s);
  return (setStandardProcedure(tempString));
}


int setObjectDEC(double d, double m, double s)
{
  char tempString[16];
   sprintf(tempString, "#:Sd %+03d:%02d:%02d#", (int) d, (int) m, (int) s);

   return (setStandardProcedure(tempString));

}

/*int setObjectDEC(double DEC)
{
  char tempString[16];
  int degrees = (int) DEC;
  int minutes = (int) ((DEC - degrees) * 60.0);
  int seconds = (int) ( (DEC - degrees) * 60.0 - minutes);

  if (minutes < 0)
    minutes *= -1;
  if (seconds < 0)
    seconds *= -1;

   sprintf(tempString, "#:Sd %+03d:%02d:%02d#", degrees, minutes, seconds);

   return (setStandardProcedure(tempString));
}
*/


int setCommandXYZ(int x, int y, int z, const char *cmd)
{
  char tempString[16];

  sprintf(tempString, "%s %02d:%02d:%02d#", cmd, x, y, z);

  return (setStandardProcedure(tempString));
}

void setAlignmentMode(unsigned int alignMode)
{
  fprintf(stderr , "In set alignment mode\n");

  switch (alignMode)
   {
     case LX200_ALIGN_POLAR:
      portWrite("#:AP#");
      break;
     case LX200_ALIGN_ALTAZ:
      portWrite("#:AA#");
      break;
     case LX200_ALIGN_LAND:
       portWrite("#:AL#");
       break;
   }

}

int setCalenderDate(int dd, int mm, int yy)
{
   char tempString[32];
   char dumpPlanetaryUpdateString[64];
   char boolRet[2];
   int result = 0;
   yy = yy % 100;

   sprintf(tempString, "#:SC %02d/%02d/%02d#", mm, dd, yy);

   portWrite(tempString);

   portRead(boolRet, 1);
   boolRet[1] = '\0';

   if (boolRet[0] == '0')
    result = -1;
   else
   {
    result = 0;

    portRead(dumpPlanetaryUpdateString, -1);

    portReadT(dumpPlanetaryUpdateString, -1, 5);

   }

   return result;
}

int setUTCOffset(int hours)
{
   char tempString[16];

   sprintf(tempString, "#:SG %+03d#", hours);

   return (setStandardProcedure(tempString));

}

int setSiteLongitude(int degrees, int minutes)
{
   char tempString[32];

   sprintf(tempString, "#:Sg%03d:%02d#", degrees, minutes);

   return (setStandardProcedure(tempString));
}

int setSiteLatitude(int degrees, int minutes, int seconds)
{
   char tempString[32];

   sprintf(tempString, "#:St%+03d:%02d:%02d#", degrees, minutes, seconds);

   return (setStandardProcedure(tempString));
}

int setObjAz(int degrees, int minutes)
{

   char tempString[16];

   sprintf(tempString, "#:Sz%03d:%02d#", degrees, minutes);

   return (setStandardProcedure(tempString));

}

int setObjAlt(int degrees, int minutes)
{
    char tempString[16];

   sprintf(tempString, "#:Sa%+02d*%02d#", degrees, minutes);

   return (setStandardProcedure(tempString));
}


int setSiteName(char * siteName, int siteNum)
{

   char tempString[16];

   switch (siteNum)
   {
     case 1:
      sprintf(tempString, "#:SM %s#", siteName);
      break;
     case 2:
      sprintf(tempString, "#:SN %s#", siteName);
      break;
     case 3:
      sprintf(tempString, "#:SO %s#", siteName);
      break;
    case 4:
      sprintf(tempString, "#:SP %s#", siteName);
      break;
    default:
      return -1;
    }

   return (setStandardProcedure(tempString));
}

void setSlewMode(int slewMode)
{

  switch (slewMode)
  {
    case LX200_SLEW_MAX:
     portWrite("#:RS#");
     break;
    case LX200_SLEW_FIND:
     portWrite("#:RM#");
     break;
    case LX200_SLEW_CENTER:
     portWrite("#:RC#");
     break;
    case LX200_SLEW_GUIDE:
     portWrite("#:RG#");
     break;
    default:
     break;
   }

}

/* ask about what this actually *accomplish* before writing it to INDI*/
void setFocuserMotion(int motionType)
{

  switch (motionType)
  {
    case LX200_FOCUSIN:
    portWrite("#:F+#");
      break;
    case LX200_FOCUSOUT:
     portWrite("#:F-#");
     break;
  }

}

void setFocuserSpeedMode (int speedMode)
{

 switch (speedMode)
 {
    case LX200_HALTFOCUS:
     portWrite("#:FQ#");
     break;
    case LX200_FOCUSFAST:
     portWrite("#:FF#");
     break;
    case LX200_FOCUSSLOW:
      portWrite("#:FS#");
      break;
 }

}

int setTrackFreq(double trackF)
{
  char tempString[16];

  sprintf(tempString, "#:ST %04.1f#", trackF);

  return (setStandardProcedure(tempString));

}

/**********************************************************************
* Misc
*********************************************************************/

int Slew()
{
    char slewNum[2];
    char errorMsg[64];

    portWrite("#:MS#");

    portRead(slewNum, 1);
    slewNum[1] = '\0';

    if (slewNum[0] == '0')
     return 0;
    else if (slewNum[0] == '1')
    {
      portRead(errorMsg, -1);
      return 1;
    }
    else
    {
     portRead(errorMsg, -1);
     return 2;
    }

}

void MoveTo(int direction)
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

}

void HaltMovement(int direction)
{

switch (direction)
  {
    case LX200_NORTH:
     portWrite("#:Qn#");
     break;
    case LX200_WEST:
     portWrite("#:Qw#");
     break;
    case LX200_EAST:
     portWrite("#:Qe#");
     break;
    case LX200_SOUTH:
     portWrite("#:Qs#");
    break;
    case LX200_ALL:
     portWrite("#:Q#");
     break;
    default:
    break;
  }

}

void abortSlew()
{

 portWrite("#:Q#");

}

void Sync(char *matchedObject)
{
  portWrite("#:CM#");

  read_ret = portRead(matchedObject, -1);
  matchedObject[read_ret-1] = '\0';
}

void selectSite(int siteNum)
{

  switch (siteNum)
  {
    case 1:
      portWrite("#:W1#");
      break;
    case 2:
      portWrite("#:W2#");
      break;
    case 3:
      portWrite("#:W3#");
      break;
    case 4:
      portWrite("#:W4#");
      break;
    default:
    break;
  }

}

void selectCatalogObject(int catalog, int NNNN)
{
 char tempString[16];
 switch (catalog)
 {
   case LX200_STAR_C:
    sprintf(tempString, "#:LS%d#", NNNN);
    break;
   case LX200_DEEPSKY_C:
    sprintf(tempString, "#:LC%d#", NNNN);
    break;
   case LX200_MESSIER_C:
    sprintf(tempString, "#:LM%d#", NNNN);
    break;
   default:
    return;
  }

  portWrite(tempString);

}

int selectSubCatalog(int catalog, int subCatalog)
{
  char tempString[16];
  switch (catalog)
  {
    case LX200_STAR_C:
    sprintf(tempString, "#:LsD%d#", subCatalog);
    break;
    case LX200_DEEPSKY_C:
    sprintf(tempString, "#:LoD%d#", subCatalog);
    break;
    case LX200_MESSIER_C:
     return 1;
    default:
     return 0;
  }

   return (setStandardProcedure(tempString));
}

void checkLX200Format()
{

  char tempString[16];

    portWrite("#:GR#");

    read_ret = portRead(tempString, -1);
    tempString[read_ret - 1] = '\0';

    /* Short */
    if (tempString[5] == '.')
     portWrite("#:U#");

}

void selectTrackingMode(int trackMode)
{

   switch (trackMode)
   {
    case LX200_TRACK_DEFAULT:
     portWrite("#:TQ#");
     break;
    case LX200_TRACK_LUNAR:
     portWrite("#:TL#");
     break;
   case LX200_TRACK_MANUAL:
     portWrite("#:TM#");
     break;
   default:
    break;
   }

}

/**********************************************************************
* Comm
**********************************************************************/

int openPort(const char *portID)
{
  struct termios ttyOptions;

  fd = open(portID, O_RDWR);

  if (fd == -1)
     return OPENPORT_ERROR;

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
   ttyOptions.c_ispeed = B9600;
   ttyOptions.c_ospeed = B9600;

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
  int nbytes = strlen(buf);

  int bytesWritten = 0;

  while (nbytes > 0)
  {
    bytesWritten = write(fd, buf, nbytes);

    if (bytesWritten < 0)
     return WRITE_ERROR;
   

    buf += bytesWritten;
    nbytes -= bytesWritten;
  }

  /* if return is 0 then everything is written, otherwise, some bytes were not written */
  return (nbytes);
}

/* I hate C !!!! */
int portReadT(char *buf, int nbytes, int timeout)
{

int bytesRead = 0;

  if (nbytes == -1)
  {
     int totalBytes = 0;
     for (;;)
     {
         if (LX200readOut(timeout))
	  return READOUT_ERROR;

         bytesRead = read(fd, buf, 1);

         if (bytesRead < 0 )
            return READ_ERROR;
            
        if (bytesRead)
          totalBytes++;

        if (*buf == '#')
	   return totalBytes;
	
        buf += bytesRead;
     }
  }

  while (nbytes > 0)
  {
     if (LX200readOut(timeout))
      return READOUT_ERROR;

     bytesRead = read(fd, buf, nbytes);

     if (bytesRead < 0 )
      return bytesRead;
     
     buf += bytesRead;
     nbytes -= bytesRead;
  }

  return (nbytes);
}

int portRead(char *buf, int nbytes)
{
  int bytesRead = 0;
  
  /* if nbytes is -1 then read till encountering terminating # */
  if (nbytes == -1)
  {
     int totalBytes = 0;
     for (;;)
     {
         if (LX200readOut(2))
	  return READOUT_ERROR;

         bytesRead = read(fd, buf, 1);

         if (bytesRead < 0 )
            return READ_ERROR;

        if (bytesRead)
          totalBytes++;

        if (*buf == '#')
	{
	   return totalBytes;
	}
	
        buf += bytesRead;
     }
  }

  /* Read nbytes */
  while (nbytes > 0)
  {
     if (LX200readOut(2))
      return READOUT_ERROR;

     bytesRead = read(fd, buf, nbytes);

     if (bytesRead < 0 )
      return bytesRead;
     
     buf += bytesRead;
     nbytes -= bytesRead;
  }

  return (nbytes);
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

  if (retval > 0)
   return 0;
  else
   return 
   	READOUT_ERROR;
     

}
 
