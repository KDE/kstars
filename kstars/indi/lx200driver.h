/*
    LX200 Driver
    Copyright (C) 2003 Jasem Mutlaq

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

#ifndef LX200DRIVER_H
#define LX200DRIVER_H

enum TSlew { LX200_SLEW_MAX, LX200_SLEW_FIND, LX200_SLEW_CENTER, LX200_SLEW_GUIDE};
  /* Alignment modes */
enum TAlign {  LX200_ALIGN_POLAR, LX200_ALIGN_ALTAZ, LX200_ALIGN_LAND };
  /* Directions */
enum TDirection { LX200_NORTH, LX200_WEST, LX200_EAST, LX200_SOUTH, LX200_ALL};
  /* Formats of Right ascention and Declenation */
enum TFormat { LX200_FORMAT_SHORT, LX200_FORMAT_LONG};

enum TTimeFormat { LX200_24, LX200_AM, LX200_PM};
  /* Focus operation */
enum TFocusMotion { LX200_FOCUSIN, LX200_FOCUSOUT };
enum TFocusSpeed  { LX200_HALTFOCUS, LX200_FOCUSFAST, LX200_FOCUSSLOW };
  /* Library catalogs */
enum TCatalog { LX200_STAR_C, LX200_DEEPSKY_C};
  /* Frequency mode */
enum StarCatalog { LX200_STAR, LX200_SAO, LX200_GCVS };

enum DeepSkyCatalog { LX200_NGC, LX200_IC, LX200_UGC, LX200_CALDWELL, LX200_ARP, LX200_ABELL, LX200_MESSIER_C};

enum TFreq { LX200_TRACK_DEFAULT, LX200_TRACK_LUNAR, LX200_TRACK_MANUAL};

enum TPorts { SERIAL_PORT, USB_PORT};

extern const char * LX200Direction[];
extern const char * SolarSystem[];
extern const char* ttyPort[];
extern const char* USBPort[];

#define OPENPORT_ERROR			-1000		/* port open failed */
#define READOUT_ERROR			-1001		/* timeout on fd */
#define READ_ERROR			-1002		/* reading from fd failed */
#define WRITE_ERROR			-1003		/* writing to fd failed */

#define MaxReticleDutyCycle		15
#define MaxFocuserSpeed			4

// GET formatted coordinates/time, returns double
#define getLX200RA()			getCommand("#:GR#")
#define getLX200DEC()			getCommand("#:GD#")
#define getObjectRA()			getCommand("#:Gr#")
#define getObjectDEC()			getCommand("#:Gd#")
#define getLocalTime12()		getCommand("#:Ga#")
#define getLocalTime24()		getCommand("#:GL#")
#define getSDTime()			getCommand("#:GS#")

// Get String, pass x as string
#define getCalenderDate(x)		getCommandStr(x, "#:GC#")
#define getObjectInfo(x)		getCommandStr(x, "#:LI#")
#define getVersionDate(x)		getCommandStr(x, "#:GVD#")
#define getVersionTime(x)		getCommandStr(x, "#:GVT#")
#define getFullVersion(x)		getCommandStr(x, "#:GVF#")
#define getVersionNumber(x)		getCommandStr(x, "#:GVN#")
#define getProductName(x)		getCommandStr(x, "#:GVP#")
#define turnGPS_StreamOn		getCommandStr(x, "#:gps#")

// Generic set, x is a double to set
#define setObjectRA(x)			setCommand(x, "#:Sr")

// Generic set, x is an integer
#define setReticleDutyFlashCycle(x)	setCommandInt(x, "#:BD")
#define setReticleFlashRate(x)		setCommandInt(x, "#:B")
#define setFocuserSpeed(x)		setCommandInt(x, "#:F")
#define setSlewSpeed(x)			setCommandInt(x, "#:Sw")


// Set X:Y:Z
#define setLocalTime(x,y,z)		setCommandXYZ(x,y,z, "#:SL")
#define setSDTime(x,y,z)		setCommandXYZ(x,y,z, "#:SS")

#define turnGPSOn()			portWrite("#:g+#")
#define turnGPSOff()			portWrite("#:g-#")
#define alignGPSScope()			portWrite("#:Aa#")
#define gpsSleep()			portWrite("#:hN#")
#define gpsWakeUp()			portWrite("#:hW#")
#define gpsRestart()			portWrite("#:I#")
#define updateGPS_System()		setStandardProcedure("#:gT#")
#define enableDecAltPec()		portWrite("#:QA+#")
#define disableDecAltPec()		portWrite("#:QA-#")
#define enableRaAzPec()			portWrite("#:QZ+#")
#define disableRaAzPec()		portWrite("#:QZ-#")
#define activateAltDecAntiBackSlash()	portWrite("#$BAdd#")
#define activateAzRaAntiBackSlash()	portWrite("#$BZdd#")
#define SelenographicSync()		portWrite("#:CL#")



#define slewToAltAz()			setStandardProcedure("#:MA#")
#define toggleTimeFormat()		portWrite("#:H#")
#define increaseReticleBrightness()	portWrite("#:B+#")
#define decreaseReticleBrightness()	portWrite("#:B-#")
#define turnFanOn()			portWrite("#:f+#")
#define turnFanOff()			portWrite("#:f-#")
#define seekHomeAndSave()		portWrite("#:hS#")
#define seekHomeAndSet()		portWrite("#:hF#")
#define turnFieldDeRotatorOn()		portWrite("#:r+#")
#define turnFieldDeRotatorOff()		portWrite("#:r-#")
#define slewToPark()			portWrite("#:hP#")


#ifdef __cplusplus
extern "C" {
#endif

int validateFormat(char * str, double * result);
void formatHMS(double number, char * str);
void formatDMS(double number, char * str);

int LX200readOut(int timeout);
int openPort(const char *portID);
int portReadT(char *buf, int nbytes, int timeout);
int portWrite(const char * buf);
int portRead(char *buf, int nbytes);

int Connect(const char* device);
int testTelescope();
void Disconnect();

char ACK();
   
// Get commands
void checkLX200Format();
double getCommand(char *cmd);
double getTrackFreq();

int getCommandStr(char *data, char* cmd);
int getTimeFormat();

int getUCTOffset();
int getSiteName(char *siteName, int siteNum);
int getSiteLatitude(int *dd, int *mm);
int getSiteLongitude(int *ddd, int *mm);
int getNumberOfBars();
int getHomeSearchStatus();


// GPS
double getOTATemp();


// Set Commands
int setCommand(double data, char *cmd);
void setCommandInt(int data, char *cmd);

int setCommandXYZ( int x, int y, int z, char *cmd);
int setStandardProcedure(char * writeData);

void setSlewMode(int slewMode);
void setAlignmentMode(unsigned int alignMode);
int setObjectDEC(double DEC);
int setCalenderDate(int dd, int mm, int yy);
int setUTCOffset(int hours);
int setTrackFreq(double trackF);

int setSiteLongitude(int degrees, int minutes);
int setSiteLatitude(int degrees, int minutes);
int setObjAz(int degrees, int minutes);
int setObjAlt(int degrees, int minutes);

int setSiteName(char * siteName, int siteNum);

void setFocuserMotion(int motionType);
void setFocuserSpeedMode (int speedMode);

// Other
int Slew();
void Sync(char *matchedObject);
void abortSlew();
void MoveTo(int direction);
void HaltMovement(int direction);
void selectSite(int siteNum);
void selectCatalogObject(int catalog, int NNNN);
void selectTrackingMode(int trackMode);
int selectSubCatalog(int catalog, int subCatalog);
int extractDate(char *date, int *dd, int *mm, int *yy);
int extractTime(char *time, int *h, int *m, int *s);


#ifdef __cplusplus
}
#endif

#endif
