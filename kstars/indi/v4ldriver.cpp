#if 0
    V4L INDI Driver
    INDI Interface for V4L devices
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
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "webcam/QCamV4L.h"
#include "indidevapi.h"
#include "indicom.h"
#include "fitsrw.h"
#include "eventloop.h"

void ISInit();
void ISPoll(void *);
void connectV4L();
void initDataChannel();
void waitForData();
void updateDataChannel(void *p);
void updateStream(void * p);
int  writeFITS(char *filename, char errmsg[]);
int  writeRAW (char *filename, char errmsg[]);
int  grabImage();
int  checkPowerN(INumberVectorProperty *np);
int  checkPowerS(ISwitchVectorProperty *sp);
int  checkPowerT(ITextVectorProperty *tp);
int  findPort();
FITS_HDU_LIST * create_fits_header (FITS_FILE *ofp, uint width, uint height, uint bpp);


extern char* me;
extern int errno;

#define mydev           "V4L Device"

#define COMM_GROUP	"Communication"
#define EXPOSE_GROUP	"Expose"
#define IMAGE_GROUP	"Image Settings"


#define MAX_PIXELS	4096		/* Max number of pixels in one dimension */
#define POLLMS		1000		/* Polling time (ms) */

#define FILENAMESIZ	2048
#define LIBVERSIZ 	1024
#define PREFIXSIZ	64
#define BUFSIZ		8192
//#define BUFSIZ		101376

typedef struct {
int  width;
int  height;
int  expose;
unsigned char  *Y;
unsigned char  *U;
unsigned char  *V;
unsigned char  *colorBuffer;
} img_t;

img_t * V4LFrame;			/* V4L frame */
FILE *wfp;				/* File buffer */
int wp[2], rp[2];			/* Pipes */
int DataPort;				/* Data Port */
bool streamReady;			/* Can we write to data stream? */
int streamTimerID;

/*INDI controls */

/* Connect/Disconnect */
static ISwitch PowerS[]          	= {{"CONNECT" , "Connect" , ISS_OFF},{"DISCONNECT", "Disconnect", ISS_ON}};
static ISwitchVectorProperty PowerSP	= { mydev, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, PowerS, NARRAY(PowerS)};

/* Ports */
static IText PortT[]			= {{"PORT", "Port", "/dev/ttyS0"}};
static ITextVectorProperty PortTP	= { mydev, "DEVICE_PORT", "Ports", COMM_GROUP, IP_RW, 0, IPS_IDLE, PortT, NARRAY(PortT)};

/* Camera Name */
static IText camNameT[]		        = {{"Model", "", "" }};
static ITextVectorProperty camNameTP    = { mydev, "Camera Model", "", COMM_GROUP, IP_RO, 0, IPS_IDLE, camNameT, NARRAY(camNameT)};

/* Data channel */
static INumber DataChannelN[]		= {{"CHANNEL", "Channel", "%0.f", 1024., 20000., 1., 0.}};
static INumberVectorProperty DataChannelNP={ mydev, "DATA_CHANNEL", "Data Channel", COMM_GROUP, IP_RO, 0, IPS_IDLE, DataChannelN, NARRAY(DataChannelN)};

/* Video Stream */
static ISwitch StreamS[]		= {{"ON", "", ISS_OFF}, {"OFF", "", ISS_ON}};
static ISwitchVectorProperty StreamSP   = { mydev, "VIDEO_STREAM", "Video Stream", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, StreamS, NARRAY(StreamS) };

/* Frame Rate */
static INumber FrameRateN[]		= {{"RATE", "Rate", "%0.f", 1., 50., 1., 10.}};
static INumberVectorProperty FrameRateNP= { mydev, "FRAME_RATE", "Frame Rate", COMM_GROUP, IP_RW, 60, IPS_IDLE, FrameRateN, NARRAY(FrameRateN)};

/* Image color */
static ISwitch ImageTypeS[]		= {{ "GREY", "Grey", ISS_ON}, { "COLOR", "Color", ISS_OFF }};
static ISwitchVectorProperty ImageTypeSP= { mydev, "IMAGE_TYPE", "Image Type", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, ImageTypeS, NARRAY(ImageTypeS)};

/* Images Type */
static ISwitch ImageFormatS[]		= {{ "FITS", "", ISS_ON}, { "RAW", "", ISS_OFF }};
static ISwitchVectorProperty ImageFormatSP= { mydev, "IMAGE_FORMAT", "Image Format", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, ImageFormatS, NARRAY(ImageFormatS)};

/* Frame dimension */
static INumber ImageSizeN[]		= {{"WIDTH", "Width", "%0.f", 0., 0., 10., 0.},
					   {"HEIGHT", "Height", "%0.f", 0., 0., 10., 0.}};
static INumberVectorProperty ImageSizeNP = { mydev, "IMAGE_SIZE", "Image Size", IMAGE_GROUP, IP_RW, 60, IPS_IDLE, ImageSizeN, NARRAY(ImageSizeN)};


/* File Name */
static IText FileNameT[]		= {{ "FILE", "File" }};
static ITextVectorProperty FileNameTP	= { mydev, "FILE_NAME", "File name", EXPOSE_GROUP, IP_RW, 0, IPS_IDLE, FileNameT, NARRAY(FileNameT)};

/* Exposure */
  static ISwitch ExposeS[]    = {{ "Capture Image", "", ISS_OFF}};
  static ISwitchVectorProperty ExposeSP = { mydev, "Capture", "", EXPOSE_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, ExposeS, NARRAY(ExposeS)};
 
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
 streamReady   = false;
 FileNameT[0].text = strcpy(new char[FILENAMESIZ], "image1.fits");
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
  IDDefNumber(&DataChannelNP, NULL);
  IDDefNumber(&FrameRateNP, NULL);
  
  /* Expose */
  IDDefSwitch(&ExposeSP, NULL);
  IDDefText(&FileNameTP, NULL);
  
  /* Image properties */
  IDDefSwitch(&ImageTypeSP, NULL);
  IDDefSwitch(&ImageFormatSP, NULL);
  IDDefNumber(&ImageSizeNP, NULL);
  
  
  DataPort = findPort();
 
  if (DataPort > 0)
 	initDataChannel(); 
  
  //IEAddTimer (POLLMS, ISPoll, NULL);
  
}
  
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
        char errmsg[2048];
	long err;
	ISwitch *sp;
	
	/* ignore if not ours */
	if (dev && strcmp (dev, mydev))
	    return;
	    
	ISInit();
	
	IDLog("In new Switch\n");
	
     /* Connection */
     if (!strcmp (name, PowerSP.name))
     {
          IUResetSwitches(&PowerSP);
	  IUUpdateSwitches(&PowerSP, states, names, n);
   	  connectV4L();
	  return;
     }


     /* Image Format */	
     if (!strcmp(name, ImageFormatSP.name))
     {
        IUResetSwitches(&ImageFormatSP);
       	IUUpdateSwitches(&ImageFormatSP, states, names, n);
	ImageFormatSP.s = IPS_OK;
	IDSetSwitch(&ImageFormatSP, NULL);
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
       
       if (StreamS[0].s == ISS_ON && streamTimerID == -1)
       {
         fprintf(stderr, "We are starting the stream.\n");
         streamTimerID = addTimer(1000 / (int) FrameRateN[0].value, updateStream, NULL);
	 //streamTimerID = addTimer(10000, updateStream, NULL);
	 StreamSP.s  = IPS_BUSY;
       }
       else
       {
         StreamSP.s = IPS_IDLE;
	 rmTimer(streamTimerID);
	 streamTimerID = -1;
       }
       
       IDSetSwitch(&StreamSP, NULL);
       return;
     }
     
     /* Exposure */
    if (!strcmp (ExposeSP.name, name))
    {
       
       if (checkPowerS(&ExposeSP))
         return;
    
        ExposeS[0].s = ISS_OFF;
       
	V4LFrame->expose = 1000;
	V4LFrame->Y      = getY();
	V4LFrame->U      = getU();
	V4LFrame->V      = getV();
	
        grabImage();
	
     return;
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
	
	if (!strcmp(name, FileNameTP.name))
	{
	  tp = IUFindText(&FileNameTP, names[0]);
	  FileNameTP.s = IPS_IDLE;
	  
	  if (!tp)
	  {
	    IDSetText(&FileNameTP, "Error: %s is not a member of %s property.", names[0], name);
	    IDLog("Error: %s is not a member of %s property.", names[0], name);
	    return;
	  }
	  
	  strcpy(tp->text, texts[0]);
	  FileNameTP.s = IPS_OK;
	  IDSetText(&FileNameTP, NULL);
	  return;
	  
	}	  
	      	
}


void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
      long err;
      INumber *np;
      char errmsg[1024];

	/* ignore if not ours */
	if (dev && strcmp (dev, mydev))
	    return;
	    
	ISInit();
    
    /* Frame Size */
    if (!strcmp (ImageSizeNP.name, name))
    {
      if (checkPowerN(&ImageSizeNP))
        return;
	
      ImageSizeNP.s = IPS_OK;
      
      if (IUUpdateNumbers(&ImageSizeNP, values, names, n) < 0)
       return;
      
      if (setSize( (int) ImageSizeN[0].value, (int) ImageSizeN[1].value))
      {
         IDSetNumber(&ImageSizeNP, NULL);
	 return;
      }
      else
      {
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
     
     if (IUUpdateNumbers(&FrameRateNP, values, names, n) < 0)
       return;
       
     setFPS( (int) FrameRateN[0].value );
     
     FrameRateNP.s = IPS_OK;
     IDSetNumber(&FrameRateNP, NULL);
     return;
   }
      
  
  	
}

void ISPoll(void *p)
{
    #if 0
	long err;
	long timeleft;
	double ccdTemp;
	
	if (!isCCDConnected())
	{
	 IEAddTimer (POLLMS, ISPoll, NULL);
	 return;
	}
	 
	
	switch (ExposeTimeNP.s)
	{
	  case IPS_IDLE:
	    break;
	    
	  case IPS_OK:
	    break;
	    
	  case IPS_BUSY:
	    if ( (err = FLIGetExposureStatus(fli_dev, &timeleft)))
	    { 
	      ExposeTimeNP.s = IPS_IDLE; 
	      ExposeTimeN[0].value = 0;
	      
	      IDSetNumber(&ExposeTimeNP, "FLIGetExposureStatus() failed. %s.", strerror((int)-err));
	      IDLog("FLIGetExposureStatus() failed. %s.\n", strerror((int)-err));
	      break;
	    }
	    
	    /*ExposeProgressN[0].value = (timeleft / 1000.);*/
	    
	    if (timeleft > 0)
	    {
	      ExposeTimeN[0].value = timeleft / 1000.;
	      IDSetNumber(&ExposeTimeNP, NULL); 
	      break;
	    }
	    /*{
	      IDSetNumber(&ExposeProgressNP, NULL);
	      break;
	    }*/
	    
	    /* We're done exposing */
	    ExposeTimeNP.s = IPS_IDLE; 
	    ExposeTimeN[0].value = 0;
	    /*ExposeProgressNP.s = IPS_IDLE;*/
	    IDSetNumber(&ExposeTimeNP, "Exposure done, downloading image...");
	    IDLog("Exposure done, downloading image...\n");
	    /*IDSetNumber(&ExposeProgressNP, NULL);*/
	    
	    /* grab and save image */
	     if (grabImage())
	      break;
	    
	    /* Multiple image exposure 
	    if ( imagesLeft > 0)
	    { 
	      IDMessage(mydev, "Image #%d will be taken in %0.f seconds.", imageCount+1, DelayN[0].value);
	      IDLog("Image #%d will be taken in %0.f seconds.", imageCount+1, DelayN[0].value);
	      IEAddTimer (DelayN[0].value * 1000., handleExposure, NULL);
	    }*/
	    break;
	    
	  case IPS_ALERT:
	    break;
	 }
	 
	 switch (ExposeProgressNP.s)
	 {
	   case IPS_IDLE:
	   case IPS_OK:
	   	break;
           case IPS_BUSY:
	      ExposeProgressN[0].value--;
	      
	      if (ExposeProgressN[0].value > 0)
	          IDSetNumber(&ExposeProgressNP, NULL);
	      else
	      {
	        ExposeProgressNP.s = IPS_IDLE;
		IDSetNumber(&ExposeProgressNP, NULL);
	      }
	      break;
	      
	  case IPS_ALERT:
	     break;
	  }
		
	 
	 switch (TemperatureNP.s)
	 {
	   case IPS_IDLE:
	   case IPS_OK:
	     if ( (err = FLIGetTemperature(fli_dev, &ccdTemp)))
	     {
	       TemperatureNP.s = IPS_IDLE;
	       IDSetNumber(&TemperatureNP, "FLIGetTemperature() failed. %s.", strerror((int)-err));
	       IDLog("FLIGetTemperature() failed. %s.", strerror((int)-err));
	       return;
	     }
	     
	     if (fabs(TemperatureN[0].value - ccdTemp) >= TEMP_THRESHOLD)
	     {
	       TemperatureN[0].value = ccdTemp;
	       IDSetNumber(&TemperatureNP, NULL);
	     }
	     break;
	     
	   case IPS_BUSY:
	   if ((err = FLIGetTemperature(fli_dev, &ccdTemp)))
	     {
	       TemperatureNP.s = IPS_ALERT;
	       IDSetNumber(&TemperatureNP, "FLIGetTemperature() failed. %s.", strerror((int)-err));
	       IDLog("FLIGetTemperature() failed. %s.", strerror((int)-err));
	       return;
	     }
	     
	     if (fabs(FLICam->temperature - ccdTemp) <= TEMP_THRESHOLD)
	       TemperatureNP.s = IPS_OK;
	     	       
              TemperatureN[0].value = ccdTemp;
	      IDSetNumber(&TemperatureNP, NULL);
	      break;
	      
	    case IPS_ALERT:
	     break;
	  }
  	 
	 p=p; 
	 
	 #endif
 	
	 IEAddTimer (POLLMS, ISPoll, NULL);
}

/* Downloads the image from the CCD row by row and store them
   in a raw file.
 N.B. No processing is done on the image */
int grabImage()
{
   int err;
   char errmsg[1024];
   
   err = (ImageFormatS[0].s == ISS_ON) ? writeFITS(FileNameT[0].text, errmsg) : writeRAW(FileNameT[0].text, errmsg);
   if (err)
   {
       IDMessage(mydev, errmsg, NULL);
       return -1;
   }
   
  return 0;
}

int writeRAW (char *filename, char errmsg[])
{

int fd, img_size;

img_size = getWidth() * getHeight() * sizeof(unsigned char);

if ((fd = open(filename, O_WRONLY | O_CREAT  | O_EXCL,
		   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH )) == -1)
   {
      IDMessage(mydev, "Error: open of %s failed.", filename);
      IDLog("Error: open of '%s' failed.\n", filename);
      return -1;
   }
   
 if (write(fd, V4LFrame->Y, img_size) != img_size)
 {
      IDMessage(mydev, "Error: write to %s failed.", filename);
      IDLog("Error: write to '%s' failed.\n", filename);
      return -1;
 }
   
 close(fd);
   
 /* Success */
 ExposeSP.s = IPS_OK;
 IDSetSwitch(&ExposeSP, "Raw image written to %s", filename);
 IDLog("Raw image written to '%s'\n", filename);

 return 0;
 
}

int writeFITS(char *filename, char errmsg[])
{
  FITS_FILE* ofp;
  int i, bpp, bpsl, width, height;
  long nbytes;
  FITS_HDU_LIST *hdu;
  
  ofp = fits_open (filename, "w");
  if (!ofp)
  {
    sprintf(errmsg, "Error: can't open file for writing.");
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
     sprintf(errmsg, "Error: creating FITS header failed.");
     return (-1);
  }
  if (fits_write_header (ofp, hdu) < 0)
  {
    sprintf(errmsg, "Error: writing to FITS header failed.");
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
    sprintf(errmsg, "Error: write error occured");
    return (-1);
  }
 
 fits_close (ofp);      
 
  /* Success */
 ExposeSP.s = IPS_OK;
 IDSetSwitch(&ExposeSP, "FITS image written to %s", filename);
 IDLog("FITS image written to '%s'\n", filename);
  
  return 0;

}


/* Retrieves basic data from the device upon connection.*/
void getBasicData()
{

  int xmax, ymax, xmin, ymin;
  
  IDLog("In getBasicData()\n");
  
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
  
  strncpy(camNameT[0].text, getDeviceName(), MAXINDILABEL);
  IDSetText(&camNameTP, NULL);
  
  IDLog("Exiting getBasicData()\n");
}

#if 0
int manageDefaults(char errmsg[])
{
  long err;
  int exposeTimeMS;
  
  exposeTimeMS = (int) (ExposeTimeN[0].value * 1000.);
  
  IDLog("Setting default exposure time of %d ms.\n", exposeTimeMS);
  if ( (err = FLISetExposureTime(fli_dev, exposeTimeMS) ))
  {
    sprintf(errmsg, "FLISetExposureTime() failed. %s.\n", strerror((int)-err));
    IDLog(errmsg, NULL);
    return -1;
  }
  
  /* Default frame type is NORMAL */
  if ( (err = FLISetFrameType(fli_dev, FLI_FRAME_TYPE_NORMAL) ))
  {
    sprintf(errmsg, "FLISetFrameType() failed. %s.\n", strerror((int)-err));
    IDLog(errmsg, NULL);
    return -1;
  }
  
  /* X horizontal binning */
  if ( (err = FLISetHBin(fli_dev, BinningN[0].value) ))
  {
    sprintf(errmsg, "FLISetBin() failed. %s.\n", strerror((int)-err));
    IDLog(errmsg, NULL);
    return -1;
  }
  
  /* Y vertical binning */
  if ( (err = FLISetVBin(fli_dev, BinningN[1].value) ))
  {
    sprintf(errmsg, "FLISetVBin() failed. %s.\n", strerror((int)-err));
    IDLog(errmsg, NULL);
    return -1;
  }
  
  IDLog("Setting default binning %f x %f.\n", BinningN[0].value, BinningN[1].value);
  
  FLISetNFlushes(fli_dev, NFLUSHES);
  
  /* Set image area */
  if (setImageArea(errmsg))
    return -1;
  
  /* Success */
  return 0;
    
}


int getOnSwitch(ISState * states, int n)
{
 int i;
 
 for (i=0; i < n ; i++)
     if (states[i] == ISS_ON)
      return i;

 return -1;
}

#endif

int checkPowerS(ISwitchVectorProperty *sp)
{
  if (PowerSP.s != IPS_OK)
  {
    IDMessage (mydev, "Cannot change a property while the camera is offline.");
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
    IDMessage (mydev, "Cannot change a property while the camera is offline");
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
    IDMessage (mydev, "Cannot change a property while the camera is offline");
    tp->s = IPS_IDLE;
    IDSetText(tp, NULL);
    return -1;
  }

  return 0;

}

void connectV4L()
{
  long err;
  char errmsg[1024];
 
  IDLog ("In ConnectV4L\n");
    
  switch (PowerS[0].s)
  {
     case ISS_ON:
      if (connectCam(PortT[0].text))
      {
	  PowerSP.s = IPS_IDLE;
	  PowerS[0].s = ISS_OFF;
	  PowerS[1].s = ISS_ON;
	  IDSetSwitch(&PowerSP, "Error: no cameras were detected.");
	  IDLog("Error: no cameras were detected.\n");
	  return;
      }
      
      /* Sucess! */
      PowerS[0].s = ISS_ON;
      PowerS[1].s = ISS_OFF;
      PowerSP.s = IPS_OK;
      IDSetSwitch(&PowerSP, "V4LDevice is online. Retrieving basic data.");
      IDLog("V4L Device is online. Retrieving basic data.\n");
      getBasicData();
      /*if (manageDefaults(errmsg))
      {
        IDMessage(mydev, errmsg, NULL);
	IDLog(errmsg);
	return;
      }*/
      
      break;
      
    case ISS_OFF:
      PowerS[0].s = ISS_OFF;
      PowerS[1].s = ISS_ON;
      PowerSP.s = IPS_IDLE;
      disconnectCam();
      IDSetSwitch(&PowerSP, "V4L Device is offline.");
      
      break;
     }
}

FITS_HDU_LIST * create_fits_header (FITS_FILE *ofp, uint width, uint height, uint bpp)
{
 FITS_HDU_LIST *hdulist;
 int print_ctype3 = 0;   /* The CTYPE3-card may not be FITS-conforming */
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

void updateStream(void *p)
{
   int width  = getWidth();
   int height = getHeight();
   int totalBytes = width * height;
   int nr=0, n=0;
   
   //int static writenow =0;
   
   V4LFrame->Y      		= getY();
   V4LFrame->U      		= getU();
   V4LFrame->V      		= getV();
   V4LFrame->colorBuffer 	= getColorBuffer();
   
   //fprintf(stderr, "Total bytes: %d\n", width * height);
   
  if (StreamS[0].s == ISS_ON && streamReady)
  {
    /* Grey */
    if (ImageTypeS[0].s == ISS_ON)
    {

	//V4LFrame->Y[0] = 111;
	//V4LFrame->Y[1] = 222;
	
	  for (nr=0; nr < totalBytes; nr+=n)
	  {
		n = write(wp[1], V4LFrame->Y + nr, totalBytes - nr );
		
		if (n <= 0)
		{
		        fprintf(stderr, "nr is %d\n", nr);
			
			if (n < 0)
		    		fprintf(stderr, "%s\n", strerror(errno));
			else
		    	        fprintf(stderr, "Short write of pixels.\n");
				
			
		    return;
		}		
		
		//fprintf(stderr, "We've written to the pipe %d\n", n);
          }
	
        
		
    }
    /* Color */
    else
    {
        //V4LFrame->colorBuffer[0] = 111;
	//V4LFrame->colorBuffer[1] = 222;
        //fprintf(wfp, "%s", (char *) getColorBuffer());
	for (nr=0; nr < totalBytes * 4; nr+=n)
	  {
		n = write(wp[1], V4LFrame->colorBuffer + nr, (totalBytes * 4) - nr );
		
		if (n <= 0)
		{
		        fprintf(stderr, "nr is %d\n", nr);
			
			if (n < 0)
		    		fprintf(stderr, "%s\n", strerror(errno));
			else
		    	        fprintf(stderr, "Short write of FITS pixels.\n");
				
			
		    return;
		}		
		
		//fprintf(stderr, "We've written to the pipe %d\n", n);
          }

	/*if (writenow == 0)
	{	
	fwrite(getColorBuffer(), 1, nr, wfp);
	writenow = 1;
        }*/
     }
    
    //if (ferror(wfp))
    if (nr <= 0)
    {
      fprintf(stderr, "Stream error: %s\n", strerror(errno));
      StreamS[0].s = ISS_OFF;
      StreamS[1].s = ISS_ON;
      StreamSP.s   = IPS_IDLE;
      IDSetSwitch(&StreamSP, "Stream error: %s.", strerror(errno));
      return;
    }
    
    if (!streamTimerID != -1)
    	streamTimerID = addTimer(1000/ (int) FrameRateN[0].value, updateStream, NULL);
	//streamTimerID = addTimer(4000, updateStream, NULL);
   
  }

}


void updateDataChannel(void *p)
{
  char buffer[1];
  fd_set rs;
  int nr;
  timeval tv;
  tv.tv_sec  = 0;
  tv.tv_usec = 0;
  
  FD_ZERO(&rs);
  FD_SET(rp[0], &rs);
  
  nr = select(rp[0]+1, &rs, NULL, NULL, &tv);
  fprintf(stderr , "In update data channel with nr sekect %d \n", nr);
  
  if (nr <= 0)
  {
    addTimer(500, updateDataChannel, NULL);
    return;
  }
  
  nr = read(rp[0], buffer, 1);
  if (nr > 0)
  {
    streamReady = true;
    DataChannelN[0].value = DataPort;
    DataChannelNP.s = IPS_OK;
    IDSetNumber(&DataChannelNP, NULL);
    //tcflush(rp[0], TCIFLUSH);
    return;
  }
  
  
}
    

void initDataChannel ()
{
	int pid;

	/* new pipes */
	if (pipe (rp) < 0) {
	    fprintf (stderr, "%s: read pipe: %s\n", me, strerror(errno));
	    exit(1); 
	}
	if (pipe (wp) < 0) {
	    fprintf (stderr, "%s: write pipe: %s\n", me, strerror(errno));
	    exit(1);
	}

	/* new process */
	pid = fork();
	if (pid < 0) {
	    fprintf (stderr, "%s: fork: %s\n", me, strerror(errno));
	    exit(1);
	}
	if (pid == 0) {
	    /* child: listen to driver */
	    /* Bind pipes to stdin/stdout */
	    //dup2 (wp[0], 0);
	    //dup2 (rp[1], 1);
	    
	    close(wp[1]);
	    close(rp[0]);
	    
	    waitForData();
	    
	    return;
	}

	//fcntl (rp[0], F_SETFL, O_NONBLOCK);
	//fcntl (wp[1], F_SETFL, O_NONBLOCK);
	//wfp = fopen ("/home/slovin/driver.txt", "w");
	//setbuf (wfp, NULL);
	close (wp[0]);
	close (rp[1]);
	
	fprintf(stderr, "Going to listen on fd %d, and write to fd %d\n", rp[0], wp[1]);
	updateDataChannel(NULL);

}

int findPort()
{
  struct sockaddr_in serv_socket;
  int sfd;
  int port = 8000;
  int i=0;
  
  /* make socket endpoint */
  if ((sfd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
	    fprintf (stderr, "%s: socket: %s", me, strerror(errno));
	    exit(1);
  }
	
  /* bind to given port for any IP address */
  memset (&serv_socket, 0, sizeof(serv_socket));
  serv_socket.sin_family = AF_INET;
  serv_socket.sin_addr.s_addr = htonl (INADDR_ANY);
  
  for (i=0; i < 10; i++)
  {
  	serv_socket.sin_port = htons ((unsigned short)port);
	if (bind(sfd,(struct sockaddr*)&serv_socket,sizeof(serv_socket)) < 0)
	{
	  	fprintf (stderr, "%s: bind: %s\n", me, strerror(errno));
		port +=10;
	}
	else break;
  }
  
  close(sfd);
  
  if (i == 10)
   return -1;
  
  return port;
	
}
	
	
  
void waitForData()
{
        struct sockaddr_in serv_socket;
	struct sockaddr_in cli_socket;
	int cli_len;
	int sfd, lsocket;
	int reuse = 1;
        fd_set rs;
	int maxfd;
	int i, s, nr, n;
	unsigned char buffer[BUFSIZ];
	char dummy[8];
	
	/* make socket endpoint */
	if ((sfd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
	    fprintf (stderr, "%s: socket: %s", me, strerror(errno));
	    exit(1);
	}
	
	/* bind to given port for any IP address */
	memset (&serv_socket, 0, sizeof(serv_socket));
	serv_socket.sin_family = AF_INET;
	serv_socket.sin_addr.s_addr = htonl (INADDR_ANY);
	
	serv_socket.sin_port = htons ((unsigned short)DataPort);
	if (setsockopt(sfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse)) < 0){
		fprintf (stderr, "%s: setsockopt: %s", me, strerror(errno));
	    	exit(1);
		}
	if (bind(sfd,(struct sockaddr*)&serv_socket,sizeof(serv_socket)) < 0){
	    	fprintf (stderr, "%s: bind: %s\n", me, strerror(errno));
		exit(1);
	    	}
	
        /* Tell driver we're ready to receive data */
	write(rp[1], dummy, 4);
	
	/* willing to accept connections with a backlog of 1 pending */
	if (listen (sfd, 1) < 0) {
	    fprintf (stderr, "%s: listen: %s", me, strerror(errno));
	    exit(1);
	}

	/* ok */
	lsocket = sfd;
        fprintf (stderr, "%s: listening to data port %d on fd %d\n",me,DataPort,sfd);

	/* start with public contact point */
	FD_ZERO(&rs);
	FD_SET(lsocket, &rs);
	maxfd = lsocket;

	/* wait for action */
	s = select (maxfd+1, &rs, NULL, NULL, NULL);
	if (s < 0) {
	    fprintf (stderr, "%s: select: %s\n", me, strerror(errno));
	    exit(1);
	}
 
	/* get a private connection to new client */
	cli_len = sizeof(cli_socket);
	s = accept (lsocket, (struct sockaddr *)&cli_socket, (socklen_t *)&cli_len);
	if(s < 0) {
	    fprintf (stderr, "%s: accept: %s", me, strerror(errno));
	    exit (1);
	}
	
	/* ok */
	//fcntl (s, F_SETFL, O_NONBLOCK);
	
	/*wfp = fopen ("/home/slovin/client.txt", "w");
	int static writenow=0;
	setbuf (wfp, NULL);*/
	
	FD_ZERO(&rs);
	FD_SET(wp[0], &rs);
	
	IDLog("We have a streaming client at %d\n", s);
	
	int bufcount=0;
	
	while (1)
	{
               // IDLog("Waiting for input from driver\n");	
	
		i = select(wp[0]+1, &rs, NULL, NULL, NULL);
		if (i < 0)
		{
	  		fprintf (stderr, "%s: select: %s\n", me, strerror(errno));
	  		exit(1);
		}
		
		nr = read(wp[0], buffer, BUFSIZ);
		if (nr < 0) {
	    	fprintf (stderr, "Client %d: %s\n", s, strerror(errno));
	    	//fclose(wfp);
	    	exit(1);
		}
		if (nr == 0) {
		fprintf (stderr, "Client %d: EOF\n", s);
		//fclose(wfp);
	    	exit(1);
		}
		
		
		
		//fprintf(wfp, "%s", (char *) buffer);
		//for (int i=0; i < BUFSIZ; i++)
		  //buffer[i] = 128;
	        
		for (i=0; i < nr; i+=n)
		{
		  n = write(s, buffer + i, nr - i);
		  
		  if (n <= 0)
		  {
		        fprintf(stderr, "nr is %d\n", nr);
			
			if (n < 0)
		    		fprintf(stderr, "%s\n", strerror(errno));
			else
		    	        fprintf(stderr, "Short write of FITS pixels.\n");
				
			
		       exit(1);
		  }		
		}
		
		//bufcount+= nr;
		
		//fprintf(stderr, "Receving stream from driver and we WROTE a total size of %d\n", i);
		
		//if (bufcount > 4096 && bufcount < 8000)
		  //fprintf(stderr, "Byte %d is %d\n", bufcount, buffer[0]);
		  
		//if (bufcount < 101376)
			//fwrite(buffer, 1, nr, wfp);
	
		
		/*if (ferror(wfp)) 
		{
			fprintf (stderr, "Client %d: %s\n", s, strerror(errno));
			exit(1);
		}*/
	}

}
