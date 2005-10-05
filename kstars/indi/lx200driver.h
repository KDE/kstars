/*
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

*/

#ifndef LX200DRIVER_H
#define LX200DRIVER_H

  /* Slew speeds */
enum TSlew { LX200_SLEW_MAX, LX200_SLEW_FIND, LX200_SLEW_CENTER, LX200_SLEW_GUIDE};
  /* Alignment modes */
enum TAlign {  LX200_ALIGN_POLAR, LX200_ALIGN_ALTAZ, LX200_ALIGN_LAND };
  /* Directions */
enum TDirection { LX200_NORTH, LX200_WEST, LX200_EAST, LX200_SOUTH, LX200_ALL};
  /* Formats of Right ascention and Declenation */
enum TFormat { LX200_FORMAT_SHORT, LX200_FORMAT_LONG};
  /* Time Format */
enum TTimeFormat { LX200_24, LX200_AM, LX200_PM};
  /* Focus operation */
enum TFocusMotion { LX200_FOCUSIN, LX200_FOCUSOUT };
enum TFocusSpeed  { LX200_HALTFOCUS = 0, LX200_FOCUSFAST, LX200_FOCUSMEDIUM, LX200_FOCUSSLOW };
  /* Library catalogs */
enum TCatalog { LX200_STAR_C, LX200_DEEPSKY_C};
  /* Frequency mode */
enum StarCatalog { LX200_STAR, LX200_SAO, LX200_GCVS };
  /* Deep Sky Catalogs */
enum DeepSkyCatalog { LX200_NGC, LX200_IC, LX200_UGC, LX200_CALDWELL, LX200_ARP, LX200_ABELL, LX200_MESSIER_C};
  /* Mount tracking frequency, in Hz */
enum TFreq { LX200_TRACK_DEFAULT, LX200_TRACK_LUNAR, LX200_TRACK_MANUAL};

#define MaxReticleDutyCycle		15
#define MaxFocuserSpeed			4

/* GET formatted sexagisemal value from device, return as double */
#define getLX200RA(x)				getCommandSexa(x, "#:GR#")
#define getLX200DEC(x)				getCommandSexa(x, "#:GD#")
#define getObjectRA(x)				getCommandSexa(x, "#:Gr#")
#define getObjectDEC(x)				getCommandSexa(x, "#:Gd#")
#define getLocalTime12(x)			getCommandSexa(x, "#:Ga#")
#define getLocalTime24(x)			getCommandSexa(x, "#:GL#")
#define getSDTime(x)				getCommandSexa(x, "#:GS#")
#define getLX200Alt(x)				getCommandSexa(x, "#:GA#")
#define getLX200Az(x)				getCommandSexa(x, "#:GZ#")

/* GET String from device and store in supplied buffer x */
#define getObjectInfo(x)			getCommandString(x, "#:LI#")
#define getVersionDate(x)			getCommandString(x, "#:GVD#")
#define getVersionTime(x)			getCommandString(x, "#:GVT#")
#define getFullVersion(x)			getCommandString(x, "#:GVF#")
#define getVersionNumber(x)			getCommandString(x, "#:GVN#")
#define getProductName(x)			getCommandString(x, "#:GVP#")
#define turnGPS_StreamOn()			getCommandString(x, "#:gps#")

/* GET Int from device and store in supplied pointer to integer x */
#define getUTCOffset(x)				getCommandInt(x, "#:GG#")
#define getMaxElevationLimit(x)			getCommandInt(x, "#:Go#")
#define getMinElevationLimit(x)			getCommandInt(x, "#:Gh#")

/* Generic set, x is an integer */
#define setReticleDutyFlashCycle(x)		setCommandInt(x, "#:BD")
#define setReticleFlashRate(x)			setCommandInt(x, "#:B")
#define setFocuserSpeed(x)			setCommandInt(x, "#:F")
#define setSlewSpeed(x)				setCommandInt(x, "#:Sw")

/* Set X:Y:Z */
#define setLocalTime(x,y,z)			setCommandXYZ(x,y,z, "#:SL")
#define setSDTime(x,y,z)			setCommandXYZ(x,y,z, "#:SS")

/* GPS Specefic */
#define turnGPSOn()				portWrite("#:g+#")
#define turnGPSOff()				portWrite("#:g-#")
#define alignGPSScope()				portWrite("#:Aa#")
#define gpsSleep()				portWrite("#:hN#")
#define gpsWakeUp()				portWrite("#:hW#")
#define gpsRestart()				portWrite("#:I#")
#define updateGPS_System()			setStandardProcedure("#:gT#")
#define enableDecAltPec()			portWrite("#:QA+#")
#define disableDecAltPec()			portWrite("#:QA-#")
#define enableRaAzPec()				portWrite("#:QZ+#")
#define disableRaAzPec()			portWrite("#:QZ-#")
#define activateAltDecAntiBackSlash()		portWrite("#$BAdd#")
#define activateAzRaAntiBackSlash()		portWrite("#$BZdd#")
#define SelenographicSync()			portWrite("#:CL#")

#define slewToAltAz()				setStandardProcedure("#:MA#")
#define toggleTimeFormat()			portWrite("#:H#")
#define increaseReticleBrightness()		portWrite("#:B+#")
#define decreaseReticleBrightness()		portWrite("#:B-#")
#define turnFanOn()				portWrite("#:f+#")
#define turnFanOff()				portWrite("#:f-#")
#define seekHomeAndSave()			portWrite("#:hS#")
#define seekHomeAndSet()			portWrite("#:hF#")
#define turnFieldDeRotatorOn()			portWrite("#:r+#")
#define turnFieldDeRotatorOff()			portWrite("#:r-#")
#define slewToPark()				portWrite("#:hP#")

/* Astro-Physics specific */
#define APPark()				portWrite("#:KA#")
#define APUnpark()				portWrite("#:PO#");

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************
 Basic I/O
**************************************************************************/
int openPort(const char *portID);
int portRead(char *buf, int nbytes, int timeout);
int portWrite(const char * buf);
int LX200readOut(int timeout);

int Connect(const char* device);
void Disconnect();

/**************************************************************************
 Diagnostics
 **************************************************************************/
char ACK();
int testTelescope();
int testAP();

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
/* Get RA, DEC from Sky Commander controller */
int updateSkyCommanderCoord(double *ra, double *dec);
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
int Slew();
/* Synchronize to the selected coordinates and return the matching object if any */
int Sync(char *matchedObject);
/* Abort slew in all axes */
int abortSlew();
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
int checkLX200Format();
/* Select a site from the LX200 controller */
int selectSite(int siteNum);
/* Select a catalog object */
int selectCatalogObject(int catalog, int NNNN);
/* Select a sub catalog */
int selectSubCatalog(int catalog, int subCatalog);

#ifdef __cplusplus
}
#endif

#endif
