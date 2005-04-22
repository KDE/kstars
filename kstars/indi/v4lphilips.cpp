/*
    Phlips webcam INDI driver
    Copyright (C) 2004 by Jasem Mutlaq

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
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <zlib.h>

#include "webcam/QCamV4L.h"
#include "webcam/philipsV4L.h"
#include "webcam/pwc-ioctl.h"
#include "indidevapi.h"
#include "indicom.h"
#include "fitsrw.h"
#include "eventloop.h"

void ISInit(void);
void ISPoll(void *);
void connectV4L(void);
void updateStream(void * p);
void getBasicData(void);
void uploadFile(const char* filename);
int  writeFITS(const char* filename, char errmsg[]);
int  grabImage(void);
int  checkPowerN(INumberVectorProperty *np);
int  checkPowerS(ISwitchVectorProperty *sp);
int  checkPowerT(ITextVectorProperty *tp);
FITS_HDU_LIST * create_fits_header (FITS_FILE *ofp, uint width, uint height, uint bpp);
void updateImageSettings();


extern char* me;
extern int errno;

#define mydev           "Philips Webcam"

#define COMM_GROUP	"Main Control"
#define IMAGE_GROUP	"Image Settings"
#define IMAGE_ADJUST    "Image Adjustments"
#define DATA_GROUP      "Data Channel"


#define MAX_PIXELS	4096		/* Max number of pixels in one dimension */
#define POLLMS		1000		/* Polling time (ms) */

#define FILENAMESIZ	2048
#define LIBVERSIZ 	1024
#define PREFIXSIZ	64
#define PIPEBUFSIZ	8192
#define FRAME_ILEN	64


typedef struct {
int  width;
int  height;
int  expose;
unsigned char  *Y;
unsigned char  *U;
unsigned char  *V;
unsigned char  *colorBuffer;
unsigned char  *compressedFrame;
} img_t;

typedef struct {
 int rp;				/* Read pipe */
 int wp;				/* Write pipe */
 bool streamReady;			/* Can we write to data stream? */
} client_t;

img_t * V4LFrame;			/* V4L frame */
static int streamTimerID;		/* Stream ID */

/*INDI controls */

/* Connect/Disconnect */
static ISwitch PowerS[]          	= {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
static ISwitchVectorProperty PowerSP	= { mydev, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, PowerS, NARRAY(PowerS), "", 0};

/* Ports */
static IText PortT[]			= {{"PORT", "Port", 0, 0, 0, 0}};
static ITextVectorProperty PortTP	= { mydev, "DEVICE_PORT", "Ports", COMM_GROUP, IP_RW, 0, IPS_IDLE, PortT, NARRAY(PortT), "", 0};

/* Camera Name */
static IText camNameT[]		        = {{"Model", "", 0, 0, 0, 0 }};
static ITextVectorProperty camNameTP    = { mydev, "Camera Model", "", COMM_GROUP, IP_RO, 0, IPS_IDLE, camNameT, NARRAY(camNameT), "" , 0};

/* Video Stream */
static ISwitch StreamS[]		= {{"ON", "", ISS_OFF, 0, 0}, {"OFF", "", ISS_ON, 0, 0} };
static ISwitchVectorProperty StreamSP   = { mydev, "VIDEO_STREAM", "Video Stream", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, StreamS, NARRAY(StreamS), "", 0 };

/* Frame Rate */
static INumber FrameRateN[]		= {{"RATE", "Rate", "%0.f", 1., 15., 1., 15., 0, 0, 0}};
static INumberVectorProperty FrameRateNP= { mydev, "FRAME_RATE", "Frame Rate", COMM_GROUP, IP_RW, 60, IPS_IDLE, FrameRateN, NARRAY(FrameRateN), "", 0};

static INumber ShutterSpeedN[]		= {{"Speed", "", "%0.f", 0., 65535., 100., 0., 0, 0, 0}};
static INumberVectorProperty ShutterSpeedNP={ mydev, "Shutter Speed", "", COMM_GROUP, IP_RW, 0, IPS_IDLE, ShutterSpeedN, NARRAY(ShutterSpeedN), "", 0};

/* Exposure time */
static INumber ExposeTimeN[]    = {{ "EXPOSE_S", "Duration (s)", "%5.2f", 1., 1., .0, 1., 0, 0, 0}};
static INumberVectorProperty ExposeTimeNP = { mydev, "EXPOSE_DURATION", "Expose", COMM_GROUP, IP_RW, 60, IPS_IDLE, ExposeTimeN, NARRAY(ExposeTimeN), "", 0};

/* Image color */
static ISwitch ImageTypeS[]		= {{ "Grey", "", ISS_ON, 0, 0}, { "Color", "", ISS_OFF, 0, 0 }};
static ISwitchVectorProperty ImageTypeSP= { mydev, "Image Type", "", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, ImageTypeS, NARRAY(ImageTypeS), "", 0};

/* Frame dimension */
static INumber ImageSizeN[]		= {{"WIDTH", "Width", "%0.f", 0., 0., 10., 0., 0, 0, 0},
					   {"HEIGHT", "Height", "%0.f", 0., 0., 10., 0., 0, 0, 0}};
static INumberVectorProperty ImageSizeNP = { mydev, "IMAGE_SIZE", "Image Size", IMAGE_GROUP, IP_RW, 60, IPS_IDLE, ImageSizeN, NARRAY(ImageSizeN), "", 0};

static ISwitch BackLightS[]	= {{"ON", "", ISS_OFF, 0, 0}, {"OFF", "", ISS_ON, 0, 0} };
static ISwitchVectorProperty BackLightSP = { mydev, "Back Light", "", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, BackLightS, NARRAY(BackLightS), "", 0};

static ISwitch AntiFlickerS[]	= {{"ON", "", ISS_OFF, 0, 0}, {"OFF", "", ISS_ON, 0, 0} };
static ISwitchVectorProperty AntiFlickerSP = { mydev, "Anti Flicker", "", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, AntiFlickerS, NARRAY(AntiFlickerS), "", 0};

static ISwitch NoiseReductionS[] = { {"None", "", ISS_ON, 0, 0},
				     {"Low", "", ISS_OFF, 0, 0},
                                     {"Medium", "", ISS_OFF, 0, 0},
				     {"High", "", ISS_OFF, 0, 0}};
				     
static ISwitchVectorProperty NoiseReductionSP = { mydev, "Noise Reduction", "", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, NoiseReductionS, NARRAY(NoiseReductionS), "", 0};

static ISwitch CamSettingS[]  = { {"Save", "", ISS_OFF, 0, 0 },
				  { "Restore", "", ISS_OFF, 0, 0},
				  { "Factory", "", ISS_OFF, 0, 0}};
				  
static ISwitchVectorProperty CamSettingSP = { mydev, "Settings", "", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, CamSettingS, NARRAY(CamSettingS), "", 0};
  
static INumber ImageAdjustN[] = {  {"Contrast", "", "%0.f", 0., 256., 1., 0., 0, 0, 0 }, 
                                   {"Brightness", "", "%0.f", 0., 256., 1., 0., 0 ,0 ,0}, 
				   {"Color", "", "%0.f", 0., 256., 1., 0., 0 , 0 ,0},
				   {"Sharpness", "", "%0.f", -1., 256., 1., -1., 0 , 0 ,0},
				   {"Gain", "", "%0.f", 0., 256., 1., 0., 0 , 0 ,0},
				   {"Gamma", "", "%0.f", 0., 256., 1., 0., 0 , 0 ,0}};
				   
static INumberVectorProperty ImageAdjustNP = {mydev, "Image Adjustments", "", IMAGE_ADJUST, IP_RW, 0, IPS_IDLE, ImageAdjustN, NARRAY(ImageAdjustN), "", 0 };

static INumber WhiteBalanceN[] = { {"Manual Red", "", "%0.f", 0., 256., 1., 0., 0, 0, 0 }, 
				   {"Manual Blue", "", "%0.f", 0., 256., 1., 0., 0, 0, 0 }};
				   
static INumberVectorProperty WhiteBalanceNP = { mydev, "White Balance", "", IMAGE_ADJUST, IP_RW, 0, IPS_IDLE, WhiteBalanceN, NARRAY(WhiteBalanceN), "", 0 };

static ISwitch WhiteBalanceModeS[] = {{ "Auto", "", ISS_ON, 0, 0 },
                                     { "Manual", "", ISS_OFF, 0, 0 },
				     { "Indoor", "", ISS_OFF, 0, 0},
				     { "Outdoor", "", ISS_OFF, 0, 0},
				     { "Fluorescent", "", ISS_OFF, 0, 0}};
				     
static ISwitchVectorProperty WhiteBalanceModeSP = { mydev, "White Balance Mode", "", IMAGE_ADJUST, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, WhiteBalanceModeS, NARRAY(WhiteBalanceModeS), "", 0};
				     
/* BLOB for sending image */
static IBLOB imageB = {"CCD1", "Feed", "", 0, 0, 0, 0, 0, 0, 0};
static IBLOBVectorProperty imageBP = {mydev, "Video", "Video", COMM_GROUP,
  IP_RO, 0, IPS_IDLE, &imageB, 1, "", 0};

/* send client definitions of all properties */
void ISInit()
{
  static int isInit=0;

 if (isInit)
  return;
 
 V4LFrame = new img_t;
 
 if (V4LFrame == NULL)
 {
   IDMessage(mydev, "Error: unable to initialize driver. Low memory.");
   IDLog("Error: unable to initialize driver. Low memory.");
   return;
 }
 
 streamTimerID = -1;
 
 PortT[0].text     = strcpy(new char[32], "/dev/video0");
 camNameT[0].text  = new char[MAXINDILABEL];
 
 isInit = 1;

}

void ISGetProperties (const char *dev)
{ 

   ISInit();
  
  if (dev && strcmp (mydev, dev))
    return;
    
  /* COMM_GROUP */
  IDDefSwitch(&PowerSP, NULL);
  IDDefText(&PortTP, NULL);
  IDDefText(&camNameTP, NULL);
  IDDefSwitch(&StreamSP, NULL);
  IDDefNumber(&FrameRateNP, NULL);
  IDDefNumber(&ExposeTimeNP, NULL);
  IDDefNumber(&ShutterSpeedNP, NULL);
  IDDefBLOB(&imageBP, NULL);
  
  /* Image properties */
  IDDefSwitch(&ImageTypeSP, NULL);
  IDDefNumber(&ImageSizeNP, NULL);
  IDDefSwitch(&BackLightSP, NULL);
  IDDefSwitch(&AntiFlickerSP, NULL);
  IDDefSwitch(&NoiseReductionSP, NULL);
  IDDefSwitch(&CamSettingSP, NULL);
  
  /* Image Adjustments */
  IDDefSwitch(&WhiteBalanceModeSP, NULL);
  IDDefNumber(&WhiteBalanceNP, NULL);
  IDDefNumber(&ImageAdjustNP, NULL);
  
}
  
void ISNewBLOB (const char */*dev*/, const char */*name*/, int */*sizes[]*/, char **/*blobs[]*/, char **/*formats[]*/, char **/*names[]*/, int /*n*/)
{}

void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	char errmsg[ERRMSG_SIZE];
	int index=0;
	
	/* ignore if not ours */
     if (dev && strcmp (dev, mydev))
	 return;
	    
	ISInit();
     
     /* Connection */
     if (!strcmp (name, PowerSP.name))
     {
          IUResetSwitches(&PowerSP);
	  IUUpdateSwitches(&PowerSP, states, names, n);
   	  connectV4L();
	  return;
     }
    
     /* Image Type */
     if (!strcmp(name, ImageTypeSP.name))
     {
       IUResetSwitches(&ImageTypeSP);
       IUUpdateSwitches(&ImageTypeSP, states, names, n);
       ImageTypeSP.s = IPS_OK;
       
       IDSetSwitch(&ImageTypeSP, NULL);
       return;
     }
     
     /* Video Stream */
     if (!strcmp(name, StreamSP.name))
     {
     
      if (checkPowerS(&StreamSP))
         return;
       
       IUResetSwitches(&StreamSP);
       IUUpdateSwitches(&StreamSP, states, names, n);
       StreamSP.s = IPS_IDLE;
       
       // Refresh Size
       IDSetNumber(&ImageSizeNP, NULL);
          
       if (StreamS[0].s == ISS_ON && streamTimerID == -1)
       {
         IDLog("Starting the video stream.\n");
         streamTimerID = addTimer(1000 / (int) FrameRateN[0].value, updateStream, NULL);
	 StreamSP.s  = IPS_BUSY; 
       }
       else
       {
         IDLog("The video stream has been disabled.\n");
	 //rmTimer(streamTimerID);
	 streamTimerID = -1;
	 

       }
       
       IDSetSwitch(&StreamSP, NULL);
       return;
     }
     
    if (!strcmp (AntiFlickerSP.name, name))
    {
       if (checkPowerS(&AntiFlickerSP))
         return;
	 
       AntiFlickerSP.s = IPS_IDLE;
       
       IUResetSwitches(&AntiFlickerSP);
       IUUpdateSwitches(&AntiFlickerSP, states, names, n);
       
       if (AntiFlickerS[0].s == ISS_ON)
       {
         if (setFlicker(true, errmsg) < 0)
	 {
	   AntiFlickerS[0].s = ISS_OFF;
	   AntiFlickerS[1].s = ISS_ON;
	   IDSetSwitch(&AntiFlickerSP, "%s", errmsg);
	   return;
	 }
	 
	 AntiFlickerSP.s = IPS_OK;
	 IDSetSwitch(&AntiFlickerSP, NULL);
       }
       else
       {
         if (setFlicker(false, errmsg) < 0)
	 {
	   AntiFlickerS[0].s = ISS_ON;
	   AntiFlickerS[1].s = ISS_OFF;
	   IDSetSwitch(&AntiFlickerSP, "%s", errmsg);
	   return;
	 }
	 
	 IDSetSwitch(&AntiFlickerSP, NULL);
       }
       
       return;
    }
    
    if (!strcmp (BackLightSP.name, name))
    {
       if (checkPowerS(&BackLightSP))
         return;
	 
       BackLightSP.s = IPS_IDLE;
       
       IUResetSwitches(&BackLightSP);
       IUUpdateSwitches(&BackLightSP, states, names, n);
       
       if (BackLightS[0].s == ISS_ON)
       {
         if (setBackLight(true, errmsg) < 0)
	 {
	   BackLightS[0].s = ISS_OFF;
	   BackLightS[1].s = ISS_ON;
	   IDSetSwitch(&BackLightSP, "%s", errmsg);
	   return;
	 }
	 
	 BackLightSP.s = IPS_OK;
	 IDSetSwitch(&BackLightSP, NULL);
       }
       else
       {
         if (setBackLight(false, errmsg) < 0)
	 {
	   BackLightS[0].s = ISS_ON;
	   BackLightS[1].s = ISS_OFF;
	   IDSetSwitch(&BackLightSP, "%s", errmsg);
	   return;
	 }
	 
	 IDSetSwitch(&BackLightSP, NULL);
       }
       
       return;
    }
	 
    if (!strcmp (NoiseReductionSP.name, name))
    {
       if (checkPowerS(&NoiseReductionSP))
         return;
	 
       NoiseReductionSP.s = IPS_IDLE;
       
       IUResetSwitches(&NoiseReductionSP);
       IUUpdateSwitches(&NoiseReductionSP, states, names, n);
       
       for (int i=0; i < 4; i++)
        if (NoiseReductionS[i].s == ISS_ON)
	{
	   index = i;
	   break;
	}
	
       if (setNoiseRemoval(index, errmsg) < 0)
       {
         IUResetSwitches(&NoiseReductionSP);
	 NoiseReductionS[0].s = ISS_ON;
	 IDSetSwitch(&NoiseReductionSP, "%s", errmsg);
	 return;
       }
       
       NoiseReductionSP.s = IPS_OK;
       
       IDSetSwitch(&NoiseReductionSP, NULL);
       return;
    }
    
    if (!strcmp (WhiteBalanceModeSP.name, name))
    {
       if (checkPowerS(&WhiteBalanceModeSP))
         return;
	 
       WhiteBalanceModeSP.s = IPS_IDLE;
       
       IUResetSwitches(&WhiteBalanceModeSP);
       IUUpdateSwitches(&WhiteBalanceModeSP, states, names, n);
       
       for (int i=0; i < 5; i++)
        if (WhiteBalanceModeS[i].s == ISS_ON)
	{
	   index = i;
	   break;
	}
	
	switch (index)
	{
	  // Auto
	  case 0:
	   if (setWhiteBalanceMode(PWC_WB_AUTO, errmsg) < 0)
	   {
	     IUResetSwitches(&WhiteBalanceModeSP),
	     WhiteBalanceModeS[0].s = ISS_ON;
	     IDSetSwitch(&WhiteBalanceModeSP, "%s", errmsg);
	     return;
	   }
	   break;
	   
	 // Manual
	 case 1:
	  if (setWhiteBalanceMode(PWC_WB_MANUAL, errmsg) < 0)
	   {
	     IUResetSwitches(&WhiteBalanceModeSP),
	     WhiteBalanceModeS[0].s = ISS_ON;
	     IDSetSwitch(&WhiteBalanceModeSP, "%s", errmsg);
	     return;
	   }
	   break;
	   
	 // Indoor
	 case 2:
	  if (setWhiteBalanceMode(PWC_WB_INDOOR, errmsg) < 0)
	   {
	     IUResetSwitches(&WhiteBalanceModeSP),
	     WhiteBalanceModeS[0].s = ISS_ON;
	     IDSetSwitch(&WhiteBalanceModeSP, "%s", errmsg);
	     return;
	   }
	   break;
	   
	 // Outdoor
	 case 3:
	  if (setWhiteBalanceMode(PWC_WB_OUTDOOR, errmsg) < 0)
	   {
	     IUResetSwitches(&WhiteBalanceModeSP),
	     WhiteBalanceModeS[0].s = ISS_ON;
	     IDSetSwitch(&WhiteBalanceModeSP, "%s", errmsg);
	     return;
	   }
	   break;
	   
	 // Flurescent
	 case 4:
	  if (setWhiteBalanceMode(PWC_WB_FL, errmsg) < 0)
	   {
	     IUResetSwitches(&WhiteBalanceModeSP),
	     WhiteBalanceModeS[0].s = ISS_ON;
	     IDSetSwitch(&WhiteBalanceModeSP, "%s", errmsg);
	     return;
	   }
	   break;
	   
	}
	     
	WhiteBalanceModeSP.s = IPS_OK;
	IDSetSwitch(&WhiteBalanceModeSP, NULL);
	return;
	
     }
	
    if (!strcmp (CamSettingSP.name, name))
    {
       
       if (checkPowerS(&CamSettingSP))
         return;
    
	CamSettingSP.s = IPS_IDLE;
	
	IUResetSwitches(&CamSettingSP);
	IUUpdateSwitches(&CamSettingSP, states, names, n);
	
	if (CamSettingS[0].s == ISS_ON)
	{
	  if (saveSettings(errmsg) < 0)
	  {
	    IUResetSwitches(&CamSettingSP);
	    IDSetSwitch(&CamSettingSP, "%s", errmsg);
	    return;
	  }
	  
	  CamSettingSP.s = IPS_OK;
	  IDSetSwitch(&CamSettingSP, "Settings saved.");
	  return;
	}
	
	if (CamSettingS[1].s == ISS_ON)
	{
	   restoreSettings();
	   IUResetSwitches(&CamSettingSP);
	   CamSettingSP.s = IPS_OK;
	   IDSetSwitch(&CamSettingSP, "Settings restored.");
           updateImageSettings();
	   return;
	}
	
	if (CamSettingS[2].s == ISS_ON)
	{
	  restoreFactorySettings();
	  IUResetSwitches(&CamSettingSP);
	  CamSettingSP.s = IPS_OK;
	  IDSetSwitch(&CamSettingSP, "Factory settings restored.");
          updateImageSettings();
	  return;
	}
     }
    
}

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	IText *tp;
	
	ISInit();
 
       /* ignore if not ours */ 
       if (dev && strcmp (mydev, dev))
         return;

	/* suppress warning */
	n=n;
	
	if (!strcmp(name, PortTP.name) )
	{
	  PortTP.s = IPS_OK;
	  tp = IUFindText( &PortTP, names[0] );
	  if (!tp)
	   return;

	  tp->text = new char[strlen(texts[0])+1];
	  strcpy(tp->text, texts[0]);
	  IDSetText (&PortTP, NULL);
	  return;
	}
	
}


void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
        char errmsg[ERRMSG_SIZE];
	
	/* ignore if not ours */
	if (dev && strcmp (dev, mydev))
	    return;
	    
	ISInit();
    
    /* Frame Size */
    if (!strcmp (ImageSizeNP.name, name))
    {
      if (checkPowerN(&ImageSizeNP))
        return;
	
       int oldW = (int) ImageSizeN[0].value;
       int oldH = (int) ImageSizeN[1].value;
	
      ImageSizeNP.s = IPS_OK;
      
      if (IUUpdateNumbers(&ImageSizeNP, values, names, n) < 0)
       return;
      
      if (setPhilipsSize( (int) ImageSizeN[0].value, (int) ImageSizeN[1].value))
      {
         ImageSizeN[0].value = getWidth();
	 ImageSizeN[1].value = getHeight();
         IDSetNumber(&ImageSizeNP, NULL);
      }
      else
      {
        ImageSizeN[0].value = oldW;
	ImageSizeN[1].value = oldH;
        ImageSizeNP.s = IPS_IDLE;
	IDSetNumber(&ImageSizeNP, "Failed to set a new image size.");
      }
      
      return;
   }
   
   /* Frame rate */
   if (!strcmp (FrameRateNP.name, name))
   {
     if (checkPowerN(&FrameRateNP))
      return;
      
     FrameRateNP.s = IPS_IDLE;
     
     int oldFP = (int) FrameRateN[0].value; 
     
     if (IUUpdateNumbers(&FrameRateNP, values, names, n) < 0)
       return;
       
     if (setFrameRate( (int) FrameRateN[0].value, errmsg) < 0)
     {
       FrameRateN[0].value = oldFP;
       IDSetNumber(&FrameRateNP, "%s", errmsg);
       return;
     }
       
     FrameRateNP.s = IPS_OK;
     IDSetNumber(&FrameRateNP, NULL);
     return;
   }
   
   if (!strcmp (ImageAdjustNP.name, name))
   {
     if (checkPowerN(&ImageAdjustNP))
       return;
       
     ImageAdjustNP.s = IPS_IDLE;
     
     double oldImgPar[6], shrValue;
     for (int i=0; i < 6; i++)
       oldImgPar[i] = ImageAdjustN[i].value;
     
     if (IUUpdateNumbers(&ImageAdjustNP, values, names, n) < 0)
       return;
     
     if ( ImageAdjustN[0].value != oldImgPar[0])
     {
     	        IDLog("Setting contrast %g\n", (ImageAdjustN[0].value * 256));
     		setContrast( (int) (ImageAdjustN[0].value * 256));
		ImageAdjustN[0].value = getContrast() / 256.;
     }
		
     if ( ImageAdjustN[1].value != oldImgPar[1])
     {
     		IDLog("Setting brighness %g\n", (ImageAdjustN[1].value * 256));
		setBrightness( (int) (ImageAdjustN[1].value * 256));
		ImageAdjustN[1].value = getBrightness() / 256.;
     }
		
     if ( ImageAdjustN[2].value != oldImgPar[2])
     {
           IDLog("Setting color %g\n" , (ImageAdjustN[2].value * 256));
     	   setColor( (int) (ImageAdjustN[2].value * 256));
	   ImageAdjustN[2].value = getColor() / 256.;
     }
		
     if (ImageAdjustN[3].value < 0)
       shrValue = -1;
     else
       shrValue = (ImageAdjustN[3].value * 256);
       
     if ( ImageAdjustN[3].value != oldImgPar[3])
     	if (setSharpness( (int) shrValue , errmsg) < 0)
     	{ 
       		for (int i=0; i < 6; i++)
         	ImageAdjustN[i].value = oldImgPar[i];
	 
       		IDSetNumber(&ImageAdjustNP, "%s", errmsg);
       		return;
     	}
	else
	{
	   IDLog("Setting sharpness %g\n", shrValue);
	   if (shrValue == -1)
	      ImageAdjustN[3].value = -1;
	   else
	      ImageAdjustN[3].value = getSharpness() / 256.;
	}
	
     if ( ImageAdjustN[4].value != oldImgPar[4])
     	if (setGain( (int) (ImageAdjustN[4].value * 256), errmsg) < 0)
     	{
       		for (int i=0; i < 6; i++)
         		ImageAdjustN[i].value = oldImgPar[i];
	 
       		IDSetNumber(&ImageAdjustNP, "%s", errmsg);
       		return;
     	}
	else
	{
	   IDLog("Setting gain %g\n", (ImageAdjustN[4].value * 256));
	   ImageAdjustN[4].value = getGain() / 256.;
	}
	
     if ( ImageAdjustN[5].value != oldImgPar[5])
     {
       IDLog("Setting gamma %g\n",  (ImageAdjustN[5].value * 256));
       setGama ( (int) (ImageAdjustN[5].value * 256));
       ImageAdjustN[5].value = getGama() / 256.;
     }
     
     ImageAdjustNP.s = IPS_OK;
     IDSetNumber(&ImageAdjustNP, NULL);
     return;
   }
   
   if (!strcmp (ShutterSpeedNP.name, name))
   {
     if (checkPowerN(&ShutterSpeedNP))
       return;
       
     ShutterSpeedNP.s = IPS_IDLE;
     
     if (setExposure( (int) values[0], errmsg) < 0)
     {
       IDSetNumber(&ShutterSpeedNP, "%s", errmsg);
       return;
     }
     
     ShutterSpeedN[0].value = values[0];
     ShutterSpeedNP.s = IPS_OK;
     IDSetNumber(&ShutterSpeedNP, NULL);
     return;
  }
  
  if (!strcmp (WhiteBalanceNP.name, name))
  {
     if (checkPowerN(&WhiteBalanceNP))
       return;
       
     WhiteBalanceNP.s = IPS_IDLE;
     
     int oldBalance[2];
     oldBalance[0] = (int) WhiteBalanceN[0].value;
     oldBalance[1] = (int) WhiteBalanceN[1].value;
     
     if (IUUpdateNumbers(&WhiteBalanceNP, values, names, n) < 0)
       return;
     
     if (setWhiteBalanceRed( (int) WhiteBalanceN[0].value * 256, errmsg))
     {
       WhiteBalanceN[0].value = oldBalance[0];
       WhiteBalanceN[1].value = oldBalance[1];
       IDSetNumber(&WhiteBalanceNP, "%s", errmsg);
       return;
     }
     if (setWhiteBalanceBlue( (int) WhiteBalanceN[1].value * 256, errmsg))
     {
       WhiteBalanceN[0].value = oldBalance[0];
       WhiteBalanceN[1].value = oldBalance[1];
       IDSetNumber(&WhiteBalanceNP, "%s", errmsg);
       return;
     }
     
     IUResetSwitches(&WhiteBalanceModeSP);
     WhiteBalanceModeS[1].s = ISS_ON;
     WhiteBalanceModeSP.s   = IPS_OK;
     WhiteBalanceNP.s = IPS_OK;
     IDSetSwitch(&WhiteBalanceModeSP, NULL);
     IDSetNumber(&WhiteBalanceNP, NULL);
     return;
   }
   
   /* Exposure */
    if (!strcmp (ExposeTimeNP.name, name))
    {
       
       if (checkPowerN(&ExposeTimeNP))
         return;
    
	streamTimerID = -1;
	StreamS[0].s  = ISS_OFF;
	StreamS[1].s  = ISS_ON;
	StreamSP.s    = IPS_IDLE;
	IDSetSwitch(&StreamSP, NULL);
	
	V4LFrame->expose = 1000;
	V4LFrame->Y      = getY();
	V4LFrame->U      = getU();
	V4LFrame->V      = getV();
	
        if (grabImage() < 0)
	{
	 ExposeTimeNP.s   = IPS_IDLE;
	 IDSetNumber(&ExposeTimeNP, NULL);
	}
	  
	
     return;
    } 
  
  	
}

/* Downloads the image from the CCD row by row and store them
   in a raw file.
 N.B. No processing is done on the image */
int grabImage()
{
   int err, fd;
   char errmsg[1024];
   char filename[] = "/tmp/fitsXXXXXX";
   
   if ((fd = mkstemp(filename)) < 0)
   { 
    IDMessage(mydev, "Error making temporary filename.");
    IDLog("Error making temporary filename.\n");
    return -1;
   }
   close(fd);
  
   err = writeFITS(filename, errmsg);
   if (err)
   {
       IDMessage(mydev, errmsg, NULL);
       return -1;
   }
   
  return 0;
}

int writeFITS(const char* filename, char errmsg[])
{
  FITS_FILE* ofp;
  int i, bpp, bpsl, width, height;
  long nbytes;
  FITS_HDU_LIST *hdu;
  
  ofp = fits_open (filename, "w");
  if (!ofp)
  {
    strcpy(errmsg, "Error: cannot open file for writing.");
    return (-1);
  }
  
  width  = getWidth();
  height = getHeight();
  bpp    = 1;                      /* Bytes per Pixel */
  bpsl   = bpp * width;    	   /* Bytes per Line */
  nbytes = 0;
  
  hdu = create_fits_header (ofp, width, height, bpp);
  if (hdu == NULL)
  {
     strcpy(errmsg, "Error: creating FITS header failed.");
     return (-1);
  }
  if (fits_write_header (ofp, hdu) < 0)
  {
    strcpy(errmsg, "Error: writing to FITS header failed.");
    return (-1);
  }
  
  for (i= height - 1; i >=0 ; i--) 
  {
    fwrite(V4LFrame->Y + (i * width), 1, width, ofp->fp);
    nbytes += bpsl;
  }
  
  nbytes = nbytes % FITS_RECORD_SIZE;
  if (nbytes)
  {
    while (nbytes++ < FITS_RECORD_SIZE)
      putc (0, ofp->fp);
  }
  
  if (ferror (ofp->fp))
  {
    strcpy(errmsg, "Error: write error occured");
    return (-1);
  }
 
 fits_close (ofp);      
 
  /* Success */
 ExposeTimeNP.s = IPS_OK;
 /*IDSetSwitch(&ExposeSP, "FITS image written to %s", filename);
 IDLog("FITS image written to '%s'\n", filename);*/
 //IDSetNumber(&ExposeTimeNP, "Loading FITS image...");
 IDSetNumber(&ExposeTimeNP, NULL);
 IDLog("Loading FITS image...\n");
 
 uploadFile(filename);
  
 return 0;

}


/* Retrieves basic data from the device upon connection.*/
void getBasicData()
{

  char errmsg[1024];
  bool result;
  int xmax, ymax, xmin, ymin, index;
  
  getMaxMinSize(xmax, ymax, xmin, ymin);
  
  /* Width */
  ImageSizeN[0].value = getWidth();
  ImageSizeN[0].min = xmin;
  ImageSizeN[0].max = xmax;
  
  /* Height */
  ImageSizeN[1].value = getHeight();
  ImageSizeN[1].min = ymin;
  ImageSizeN[1].max = ymax;

  IDSetNumber(&ImageSizeNP, NULL);
  IUUpdateMinMax(&ImageSizeNP);
  
  strncpy(camNameT[0].text, getDeviceName(), MAXINDILABEL);
  IDSetText(&camNameTP, NULL);
  
  IDLog("Raw values\n Contrast: %d \n Brightness %d \n Color %d \n Sharpness %d \n Gain %d \n Gamma %d \n", getContrast(), getBrightness(), getColor(), getSharpness(), getGain(), getGama());
  
  updateImageSettings();
  
  if (setFrameRate( (int) FrameRateN[0].value, errmsg) < 0)
  {
    FrameRateNP.s = IPS_ALERT;
    IDSetNumber(&FrameRateNP, "%s", errmsg);
  }
  else
  {
    FrameRateNP.s = IPS_OK;
    IDSetNumber(&FrameRateNP, NULL);
  }
  
  result = getBackLight();
  if (result)
  {
   BackLightS[0].s = ISS_ON;
   BackLightS[1].s = ISS_OFF;
  }
  else
  {
   BackLightS[0].s = ISS_OFF;
   BackLightS[1].s = ISS_ON;
  }
  IDSetSwitch(&BackLightSP, NULL);
  
  result = getFlicker();
  if (result)
  {
    AntiFlickerS[0].s = ISS_ON;
    AntiFlickerS[1].s = ISS_OFF;
  }
  else
  {
    AntiFlickerS[0].s = ISS_OFF;
    AntiFlickerS[1].s = ISS_ON;
  }
  IDSetSwitch(&AntiFlickerSP, NULL);
  
  index = getNoiseRemoval();
  IUResetSwitches(&NoiseReductionSP);
  NoiseReductionS[index].s = ISS_ON;
  IDSetSwitch(&NoiseReductionSP, NULL);
  
  index = getWhiteBalance();
  IUResetSwitches(&WhiteBalanceModeSP);
  switch (index)
  {
    case PWC_WB_AUTO:
     WhiteBalanceModeS[0].s = ISS_ON;
     break;
    case PWC_WB_MANUAL:
     WhiteBalanceModeS[1].s = ISS_ON;
     break;
    case PWC_WB_INDOOR:
     WhiteBalanceModeS[2].s = ISS_ON;
     break;
    case PWC_WB_OUTDOOR:
     WhiteBalanceModeS[3].s = ISS_ON;
     break;
    case PWC_WB_FL:
     WhiteBalanceModeS[3].s = ISS_ON;
     break;
  }
  IDSetSwitch(&WhiteBalanceModeSP, NULL);    
  
}

int checkPowerS(ISwitchVectorProperty *sp)
{
  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(sp->label, ""))
        IDMessage (mydev, "Cannot change property %s while the camera is offline.", sp->name);
    else
    	IDMessage (mydev, "Cannot change property %s while the camera is offline.", sp->label);
	
    sp->s = IPS_IDLE;
    IDSetSwitch(sp, NULL);
    return -1;
  }

  return 0;
}

int checkPowerN(INumberVectorProperty *np)
{
  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(np->label, ""))
        IDMessage (mydev, "Cannot change property %s while the camera is offline.", np->name);
    else
    	IDMessage (mydev, "Cannot change property %s while the camera is offline.", np->label);
	
    np->s = IPS_IDLE;
    IDSetNumber(np, NULL);
    return -1;
  }

  return 0;
}

int checkPowerT(ITextVectorProperty *tp)
{
  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(tp->label, ""))
        IDMessage (mydev, "Cannot change property %s while the camera is offline.", tp->name);
    else
    	IDMessage (mydev, "Cannot change property %s while the camera is offline.", tp->label);
    
    tp->s = IPS_IDLE;
    IDSetText(tp, NULL);
    return -1;
  }

  return 0;

}

void connectV4L()
{
  char errmsg[1024];
    
  switch (PowerS[0].s)
  {
     case ISS_ON:
      if (connectPhilips(PortT[0].text, errmsg))
      {
	  PowerSP.s = IPS_IDLE;
	  PowerS[0].s = ISS_OFF;
	  PowerS[1].s = ISS_ON;
	  IDSetSwitch(&PowerSP, "Error: %s", errmsg);
	  IDLog("Error: %s\n", errmsg);
	  return;
      }
      
      /* Sucess! */
      
      PowerS[0].s = ISS_ON;
      PowerS[1].s = ISS_OFF;
      PowerSP.s = IPS_OK;
      IDSetSwitch(&PowerSP, "Video4Linux Generic Device is online. Retrieving basic data.");
      
      V4LFrame->compressedFrame = (unsigned char *) malloc (sizeof(unsigned char) * 1);
      
      IDLog("V4L Device is online. Retrieving basic data.\n");
      getBasicData();
      
      break;
      
    case ISS_OFF:
      PowerS[0].s = ISS_OFF;
      PowerS[1].s = ISS_ON;
      PowerSP.s = IPS_IDLE;
      
      // Disable stream if running
      if (streamTimerID != -1)
      {
      	rmTimer(streamTimerID);
      	streamTimerID = -1;
      }
      
      
      free(V4LFrame->compressedFrame);
      V4LFrame->compressedFrame = NULL;
      disconnectCam();      
      
      IDSetSwitch(&PowerSP, "Video4Linux Generic Device is offline.");
      
      break;
     }
}

FITS_HDU_LIST * create_fits_header (FITS_FILE *ofp, uint width, uint height, uint bpp)
{
 FITS_HDU_LIST *hdulist;
 char expose_s[80];
 char obsDate[80];
 char instrumentName[80];
 char ts[32];
	
 struct tm *tp;
 time_t t;
 time (&t);
 tp = gmtime (&t);
 strftime (ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tp);
 
 snprintf(instrumentName, 80, "INSTRUME= '%s'", getDeviceName());
 snprintf(obsDate, 80, "DATE-OBS= '%s' /Observation Date UTC", ts);

 hdulist = fits_add_hdu (ofp);
 if (hdulist == NULL) return (NULL);

 hdulist->used.simple = 1;
 hdulist->bitpix = 8;
 hdulist->naxis = 2;
 hdulist->naxisn[0] = width;
 hdulist->naxisn[1] = height;
 hdulist->naxisn[2] = bpp;
 hdulist->used.datamin = 0;
 hdulist->used.datamax = 0;
 hdulist->used.bzero = 1;
 hdulist->bzero = 0.0;
 hdulist->used.bscale = 1;
 hdulist->bscale = 1.0;
 
 sprintf(expose_s, "EXPOSURE= %d / milliseconds", V4LFrame->expose);
 
 fits_add_card (hdulist, expose_s);
 fits_add_card (hdulist, instrumentName);
 fits_add_card (hdulist, obsDate);
 
 return (hdulist);
}

void updateStream(void * /*p*/)
{
   int width  = getWidth();
   int height = getHeight();
   uLongf compressedBytes = 0;
   uLong totalBytes;
   unsigned char *targetFrame;
   char frameSize[FRAME_ILEN];
   int nr=0, n=0,r, frameLen;
   
   if (PowerS[0].s == ISS_OFF || StreamS[0].s == ISS_OFF) return;
   
   V4LFrame->Y      		= getY();
   V4LFrame->U      		= getU();
   V4LFrame->V      		= getV();
   V4LFrame->colorBuffer 	= getColorBuffer();
  
   totalBytes  = ImageTypeS[0].s == ISS_ON ? width * height : width * height * 4;
   targetFrame = ImageTypeS[0].s == ISS_ON ? V4LFrame->Y : V4LFrame->colorBuffer;
   
   /* Compress frame */
   V4LFrame->compressedFrame = (unsigned char *) realloc (V4LFrame->compressedFrame, sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3);
   
   compressedBytes = sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3;
   
   //IDLog("Before compression\n");
   r = compress2(V4LFrame->compressedFrame, &compressedBytes, targetFrame, totalBytes, 4);
   if (r != Z_OK)
   {
 	/* this should NEVER happen */
 	IDLog("internal error - compression failed: %d\n", r);
	return;
   }
   
   /* #3 Send it */
   imageB.blob = V4LFrame->compressedFrame;
   imageB.bloblen = compressedBytes;
   imageB.size = totalBytes;
   strcpy(imageB.format, ".stream.z");
   imageBP.s = IPS_OK;
   IDSetBLOB (&imageBP, NULL);
   
  if (streamTimerID != -1)
    	streamTimerID = addTimer(1000/ (int) FrameRateN[0].value, updateStream, NULL);

}

void uploadFile(const char* filename)
{
   FILE * fitsFile;
   char frameSize[FRAME_ILEN];
   unsigned char *fitsData;
   int r=0;
   unsigned int frameLen =0 , nr = 0;
   uLongf compressedBytes = 0;
   uLong totalBytes;
   struct stat stat_p; 
 
   if ( -1 ==  stat (filename, &stat_p))
   { 
     IDLog(" Error occoured attempting to stat %s\n", filename); 
     return; 
   }
   
   totalBytes = stat_p.st_size;
   fitsData = new unsigned char[totalBytes];

   fitsFile = fopen(filename, "r");
   
   if (fitsFile == NULL)
    return;
   
   /* #1 Read file from disk */ 
   for (unsigned int i=0; i < totalBytes; i+= nr)
   {
      nr = fread(fitsData + i, 1, totalBytes - i, fitsFile);
     
     if (nr <= 0)
     {
        IDLog("Error reading temporary FITS file.\n");
        return;
     }
   }
   
   /* #2 Compress it */
   V4LFrame->compressedFrame = (unsigned char *) realloc (V4LFrame->compressedFrame, sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3);
   
   compressedBytes = sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3;
   r = compress2(V4LFrame->compressedFrame, &compressedBytes, fitsData, totalBytes, VIDEO_COMPRESSION_LEVEL);
   if (r != Z_OK)
   {
 	/* this should NEVER happen */
 	IDLog("internal error - compression failed: %d\n", r);
	return;
   }
  
   /* #3 Send it */
   imageB.blob = V4LFrame->compressedFrame;
   imageB.bloblen = compressedBytes;
   imageB.size = totalBytes;
   strcpy(imageB.format, ".fits.z");
   imageBP.s = IPS_OK;
   IDSetBLOB (&imageBP, NULL);
   
   
   delete (fitsData);
}

void updateImageSettings()
{
  int index =0;

  ImageAdjustN[0].value = getContrast() / 256.;
  ImageAdjustN[1].value = getBrightness() / 256.;
  ImageAdjustN[2].value = getColor() / 256.;
  index = getSharpness();
  if (index < 0)
  	ImageAdjustN[3].value = -1;
  else
    ImageAdjustN[3].value = getSharpness() / 256.;
    
  ImageAdjustN[4].value = getGain() / 256.;
  ImageAdjustN[5].value = getGama() / 256.;
       
  ImageAdjustNP.s = IPS_OK;
  IDSetNumber(&ImageAdjustNP, NULL);

}
