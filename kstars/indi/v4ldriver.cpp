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
void waitForData(int rp, int wp);
void updateDataChannel(void *p);
void updateStream(void * p);
void getBasicData();
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

#define mydev           "Video4Linux Generic Device"

#define COMM_GROUP	"Communication"
#define EXPOSE_GROUP	"Expose"
#define IMAGE_GROUP	"Image Settings"


#define MAX_PIXELS	4096		/* Max number of pixels in one dimension */
#define POLLMS		1000		/* Polling time (ms) */

#define FILENAMESIZ	2048
#define LIBVERSIZ 	1024
#define PREFIXSIZ	64
#define PIPEBUFSIZ	8192

typedef struct {
int  width;
int  height;
int  expose;
unsigned char  *Y;
unsigned char  *U;
unsigned char  *V;
unsigned char  *colorBuffer;
} img_t;

typedef struct {
 int rp;				/* Read pipe */
 int wp;				/* Write pipe */
 bool streamReady;			/* Can we write to data stream? */
} client_t;

client_t *INDIClients;			/* INDI clients */
img_t * V4LFrame;			/* V4L frame */
static int DataPort;			/* Data Port */
static int streamTimerID;		/* Stream ID */
static int nclients;			/* # of clients using the binary stream */

/*INDI controls */

/* Connect/Disconnect */
static ISwitch PowerS[]          	= {{"CONNECT" , "Connect" , ISS_OFF},{"DISCONNECT", "Disconnect", ISS_ON}};
static ISwitchVectorProperty PowerSP	= { mydev, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, PowerS, NARRAY(PowerS)};

/* Ports */
static IText PortT[]			= {{"PORT", "Port", "/dev/video0"}};
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
  
static INumber ImageAdjustN[] = {{"Contrast", "", "%0.f", 0., 256., 1., 0. }, 
                                   {"Brightness", "", "%0.f", 0., 256., 1., 0.}, 
				   {"Hue", "", "%0.f", 0., 256., 1., 0.}, 
				   {"Color", "", "%0.f", 0., 256., 1., 0.}, 
				   {"Whiteness", "", "%0.f", 0., 256., 1., 0.}};
				   
static INumberVectorProperty ImageAdjustNP = {mydev, "Image Adjustments", "", IMAGE_GROUP, IP_RW, 0, IPS_IDLE, ImageAdjustN, NARRAY(ImageAdjustN) };
 
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
 FileNameT[0].text = strcpy(new char[FILENAMESIZ], "image1.fits");
 camNameT[0].text  = new char[MAXINDILABEL];
 
 INDIClients = NULL;
 nclients    = 0;
 
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
  IDDefNumber(&ImageAdjustNP, NULL);
  
  INDIClients = INDIClients ? (client_t *) realloc(INDIClients, (nclients+1) * sizeof(client_t)) :
                              (client_t *) malloc (sizeof(client_t));
    
  if (INDIClients)
  {
        INDIClients[nclients].streamReady = false;
	INDIClients[nclients].rp	  = 0;
	INDIClients[nclients].wp          = 0;
  	DataPort = findPort();
        if (DataPort > 0)
 		initDataChannel();

        /* Send the basic data to the new client if the previous client(s) are already connected. */		
	if (PowerSP.s == IPS_OK)
	  getBasicData();
  }
  
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
       StreamSP.s = IPS_IDLE;
       
       if (StreamS[0].s == ISS_ON && streamTimerID == -1)
       {
         IDLog("Starting the video stream.\n");
         streamTimerID = addTimer(1000 / (int) FrameRateN[0].value, updateStream, NULL);
	 StreamSP.s  = IPS_BUSY;
       }
       else
       {
         IDLog("The video stream has been disabled.\n");
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
	
        if (grabImage() < 0)
	{
	 ExposeS[0].s = ISS_OFF;
	 ExposeSP.s   = IPS_IDLE;
	 IDSetSwitch(&ExposeSP, NULL);
	}
	  
	
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
         ImageSizeN[0].value = getWidth();
	 ImageSizeN[1].value = getHeight();
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
   
   if (!strcmp (ImageAdjustNP.name, name))
   {
     if (checkPowerN(&ImageAdjustNP))
       return;
       
     ImageAdjustNP.s = IPS_IDLE;
     
     if (IUUpdateNumbers(&ImageAdjustNP, values, names, n) < 0)
       return;
     
     setContrast(ImageAdjustN[0].value * 128);
     setBrightness(ImageAdjustN[1].value * 128);
     setHue(ImageAdjustN[2].value * 128);
     setColor(ImageAdjustN[3].value * 128);
     setWhiteness(ImageAdjustN[4].value * 128);
     
     ImageAdjustN[0].value = getContrast() / 128.;
     ImageAdjustN[1].value = getBrightness() / 128.;
     ImageAdjustN[2].value = getHue() / 128.;
     ImageAdjustN[3].value = getColor() / 128.;
     ImageAdjustN[4].value = getWhiteness() / 128.;
     
     ImageAdjustNP.s = IPS_OK;
     IDSetNumber(&ImageAdjustNP, NULL);
     return;
   }
      
  
  	
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
  
  IDLog("in writeFITS with filename %s\n", filename);
  
  ofp = fits_open (filename, "w");
  if (!ofp)
  {
    sprintf(errmsg, "Error: cannot open file for writing.");
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
  
  ImageAdjustN[0].value = getContrast() / 128.;
  ImageAdjustN[1].value = getBrightness() / 128.;
  ImageAdjustN[2].value = getHue() / 128.;
  ImageAdjustN[3].value = getColor() / 128.;
  ImageAdjustN[4].value = getWhiteness() / 128.;
     
  ImageAdjustNP.s = IPS_OK;
  IDSetNumber(&ImageAdjustNP, NULL);
  
  IDLog("Exiting getBasicData()\n");
}

int checkPowerS(ISwitchVectorProperty *sp)
{
  if (PowerSP.s != IPS_OK)
  {
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
    IDMessage (mydev, "Cannot change property %s while the camera is offline.", tp->label);
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
      IDSetSwitch(&PowerSP, "Video4Linux Generic Device is online. Retrieving basic data.");
      IDLog("V4L Device is online. Retrieving basic data.\n");
      getBasicData();
      
      break;
      
    case ISS_OFF:
      PowerS[0].s = ISS_OFF;
      PowerS[1].s = ISS_ON;
      PowerSP.s = IPS_IDLE;
      disconnectCam();
      IDSetSwitch(&PowerSP, "Video4Linux Generic Device is offline.");
      
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
   int totalBytes;
   unsigned char *targetFrame;
   int nr=0, n=0;
   
   V4LFrame->Y      		= getY();
   V4LFrame->U      		= getU();
   V4LFrame->V      		= getV();
   V4LFrame->colorBuffer 	= getColorBuffer();
  
   totalBytes  = ImageTypeS[0].s == ISS_ON ? width * height : width * height * 4;
   targetFrame = ImageTypeS[0].s == ISS_ON ? V4LFrame->Y : V4LFrame->colorBuffer;
   
  for (int i=0; i < nclients; i++)
  {
  	if (StreamS[0].s == ISS_ON && INDIClients[i].streamReady)
        {
             for (nr=0; nr < totalBytes; nr+=n)
   	     {
	  	n = write(INDIClients[i].wp, targetFrame + nr, totalBytes - nr );
		if (n <= 0)
		{
			if (nr <= 0)
    			{
      				IDLog("Stream error: %s\n", strerror(errno));
      				StreamS[0].s = ISS_OFF;
      				StreamS[1].s = ISS_ON;
      				StreamSP.s   = IPS_IDLE;
      				IDSetSwitch(&StreamSP, "Stream error: %s.", strerror(errno));
      				return;
    			}
		        break;
		}		
		 
             }
        }
  }
    
  if (!streamTimerID != -1)
    	streamTimerID = addTimer(1000/ (int) FrameRateN[0].value, updateStream, NULL);
   
}

void initDataChannel ()
{
	int pid;
	int rp[2], wp[2];

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
	    close(wp[1]);
	    close(rp[0]);
	    
	    waitForData(wp[0], rp[1]);
	    
	    return;
	}

	close (wp[0]);
	close (rp[1]);
	INDIClients[nclients].rp = rp[0];
	INDIClients[nclients].wp = wp[1];
	
	nclients++;
	
	fprintf(stderr, "Going to listen on fd %d, and write to fd %d\n", rp[0], wp[1]);
	updateDataChannel(NULL);

}

void updateDataChannel(void *p)
{
  char buffer[1];
  fd_set rs;
  int nr;
  timeval tv;
  tv.tv_sec  = 0;
  tv.tv_usec = 0;
  
  for (int i=0; i < nclients; i++)
  {
        if (INDIClients[i].rp < 0)
	 continue;
	 
  	FD_ZERO(&rs);
  	FD_SET(INDIClients[i].rp, &rs);
  
  	nr = select(INDIClients[i].rp+1, &rs, NULL, NULL, &tv);
  
  	if (nr <= 0)
	 continue;

  	nr = read(INDIClients[i].rp, buffer, 1);
  	if (nr > 0 && atoi(buffer) == 0)
	{
	        IDLog("Client %d is ready to receive stream\n", i);
		INDIClients[i].streamReady = true;
	}
	else
	{
		INDIClients[i].streamReady = false;
		INDIClients[i].rp          = -1;
		IDLog("Lost connection with client %d\n", i);
	}
   }
  
   addTimer(1000, updateDataChannel, NULL);
  
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
  
  for (i=0; i < 100; i++)
  {
  	serv_socket.sin_port = htons ((unsigned short)port);
	if (bind(sfd,(struct sockaddr*)&serv_socket,sizeof(serv_socket)) < 0)
	{
	  	fprintf (stderr, "%s: bind: %s\n", me, strerror(errno));
		port +=5;
	}
	else break;
  }
  
  close(sfd);
  if (i == 100) return -1;
  
  return port;
	
}
	
	
  
void waitForData(int rp, int wp)
{
        struct sockaddr_in serv_socket;
	struct sockaddr_in cli_socket;
	int cli_len;
	int sfd, lsocket;
	int reuse = 1;
        fd_set rs;
	int maxfd;
	int i, s, nr, n;
	unsigned char buffer[PIPEBUFSIZ];
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
	
        /* Tell client we're ready to connect */
	/* N.B. This only modifies the child DataChannelNP, the parent's property remains unchanged. */
	DataChannelN[0].value = DataPort;
        DataChannelNP.s = IPS_OK;
        IDSetNumber(&DataChannelNP, NULL);
	
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
	
	/* Tell the driver that this client established a connection to the data channel and is ready
	   to receive stream data */
	sprintf(dummy, "%d", 0);
	write(wp, dummy, 1);
	
	FD_ZERO(&rs);
	FD_SET(rp, &rs);
	
	int bufcount=0;
	
	while (1)
	{
               // IDLog("Waiting for input from driver\n");	
	
		i = select(rp+1, &rs, NULL, NULL, NULL);
		if (i < 0)
		{
	  		fprintf (stderr, "%s: select: %s\n", me, strerror(errno));
			/* Tell driver the client has disconnected the data channel */
			sprintf(dummy, "%d", -1);
			write(wp, dummy, 1);
	  		exit(1);
		}
		
		nr = read(rp, buffer, PIPEBUFSIZ);
		if (nr < 0) {
		sprintf(dummy, "%d", -1);
		write(wp, dummy, 1);
	    	exit(1);
		}
		if (nr == 0) {
		sprintf(dummy, "%d", -1);
		write(wp, dummy, 1);
		fprintf (stderr, "Client %d: EOF\n", s);
	    	exit(1);
		}
		
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
				
		       sprintf(dummy, "%d", -1);
		       write(wp, dummy, 1);
		       exit(1);
		  }		
		}
		
	}

}
