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
#include "lzo/minilzo.h"
#include "indidevapi.h"
#include "indicom.h"
#include "fitsrw.h"
#include "eventloop.h"

void ISInit(void);
void ISPoll(void *);
void connectV4L(void);
void initDataChannel(void);
void waitForData(int rp, int wp);
void updateDataChannel(void *p);
void updateStream(void * p);
void getBasicData(void);
void uploadFile(char * filename);
int  writeFITS(char *filename, char errmsg[]);
int  grabImage(void);
int  checkPowerN(INumberVectorProperty *np);
int  checkPowerS(ISwitchVectorProperty *sp);
int  checkPowerT(ITextVectorProperty *tp);
int  findPort(void);
FITS_HDU_LIST * create_fits_header (FITS_FILE *ofp, uint width, uint height, uint bpp);


extern char* me;
extern int errno;

#define mydev           "Video4Linux Generic Device"

#define COMM_GROUP	"Main Control"
#define IMAGE_GROUP	"Image Settings"
#define DATA_GROUP      "Data Channel"


#define MAX_PIXELS	4096		/* Max number of pixels in one dimension */
#define POLLMS		1000		/* Polling time (ms) */

#define FILENAMESIZ	2048
#define LIBVERSIZ 	1024
#define PREFIXSIZ	64
#define PIPEBUFSIZ	8192
#define FRAME_ILEN	64

#define HEAP_ALLOC(var,size) \
	lzo_align_t __LZO_MMODEL var [ ((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t) ]

static HEAP_ALLOC(wrkmem,LZO1X_1_MEM_COMPRESS);

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

client_t *INDIClients;			/* INDI clients */
img_t * V4LFrame;			/* V4L frame */
static int DataPort;			/* Data Port */
static int streamTimerID;		/* Stream ID */
static int nclients;			/* # of clients using the binary stream */
static int dataChannelTimerID;
bool	   disableUpdate;

/*INDI controls */

/* Connect/Disconnect */
static ISwitch PowerS[]          	= {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
static ISwitchVectorProperty PowerSP	= { mydev, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, PowerS, NARRAY(PowerS), 0, 0};

/* Ports */
static IText PortT[]			= {{"PORT", "Port", 0, 0, 0, 0}};
static ITextVectorProperty PortTP	= { mydev, "DEVICE_PORT", "Ports", COMM_GROUP, IP_RW, 0, IPS_IDLE, PortT, NARRAY(PortT), 0, 0};

/* Camera Name */
static IText camNameT[]		        = {{"Model", "", 0, 0, 0, 0 }};
static ITextVectorProperty camNameTP    = { mydev, "Camera Model", "", COMM_GROUP, IP_RO, 0, IPS_IDLE, camNameT, NARRAY(camNameT), 0 , 0};

/* Video Stream */
static ISwitch StreamS[]		= {{"ON", "", ISS_OFF, 0, 0}, {"OFF", "", ISS_ON, 0, 0} };
static ISwitchVectorProperty StreamSP   = { mydev, "VIDEO_STREAM", "Video Stream", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, StreamS, NARRAY(StreamS), 0, 0 };

/* Frame Rate */
static INumber FrameRateN[]		= {{"RATE", "Rate", "%0.f", 1., 50., 1., 10., 0, 0, 0}};
static INumberVectorProperty FrameRateNP= { mydev, "FRAME_RATE", "Frame Rate", COMM_GROUP, IP_RW, 60, IPS_IDLE, FrameRateN, NARRAY(FrameRateN), 0, 0};

/* Image color */
static ISwitch ImageTypeS[]		= {{ "GREY", "Grey", ISS_ON, 0, 0}, { "COLOR", "Color", ISS_OFF, 0, 0 }};
static ISwitchVectorProperty ImageTypeSP= { mydev, "IMAGE_TYPE", "Image Type", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, ImageTypeS, NARRAY(ImageTypeS), 0, 0};

/* Frame dimension */
static INumber ImageSizeN[]		= {{"WIDTH", "Width", "%0.f", 0., 0., 10., 0., 0, 0, 0},
					   {"HEIGHT", "Height", "%0.f", 0., 0., 10., 0., 0, 0, 0}};
static INumberVectorProperty ImageSizeNP = { mydev, "IMAGE_SIZE", "Image Size", IMAGE_GROUP, IP_RW, 60, IPS_IDLE, ImageSizeN, NARRAY(ImageSizeN), 0, 0};

/* Exposure */
  static ISwitch ExposeS[]    = {{ "Capture Image", "", ISS_OFF, 0, 0}};
  static ISwitchVectorProperty ExposeSP = { mydev, "Capture", "", COMM_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, ExposeS, NARRAY(ExposeS), 0, 0};
  
static INumber ImageAdjustN[] = {{"Contrast", "", "%0.f", 0., 256., 1., 0., 0, 0, 0 }, 
                                   {"Brightness", "", "%0.f", 0., 256., 1., 0., 0 ,0 ,0}, 
				   {"Hue", "", "%0.f", 0., 256., 1., 0., 0, 0, 0}, 
				   {"Color", "", "%0.f", 0., 256., 1., 0., 0 , 0 ,0}, 
				   {"Whiteness", "", "%0.f", 0., 256., 1., 0., 0 , 0 ,0}};
				   
static INumberVectorProperty ImageAdjustNP = {mydev, "Image Adjustments", "", IMAGE_GROUP, IP_RW, 0, IPS_IDLE, ImageAdjustN, NARRAY(ImageAdjustN), 0, 0 };

/* Data channel */
static INumber DataChannelN[]		= {{"CHANNEL", "Channel", "%0.f", 1024., 20000., 1., 0., 0, 0, 0}};
static INumberVectorProperty DataChannelNP={ mydev, "DATA_CHANNEL", "Data Channel", DATA_GROUP, IP_RO, 0, IPS_IDLE, DataChannelN, NARRAY(DataChannelN), 0, 0};

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
 disableUpdate = false;
 streamTimerID = -1;
 //FileNameT[0].text = strcpy(new char[FILENAMESIZ], "image1.fits");
 
 PortT[0].text     = strcpy(new char[32], "/dev/video0");
 camNameT[0].text  = new char[MAXINDILABEL];
 
 INDIClients = NULL;
 nclients    = 0;
 
 if (lzo_init() != LZO_E_OK)
   IDLog("lzo_init() failed !!!\n");
      
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
  IDDefSwitch(&ExposeSP, NULL);
  
  /* Image properties */
  IDDefSwitch(&ImageTypeSP, NULL);
  IDDefNumber(&ImageSizeNP, NULL);
  IDDefNumber(&ImageAdjustNP, NULL);
  
  /* Data Channel */
  IDDefNumber(&DataChannelNP, NULL);

  
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
    
	IUResetSwitches(&StreamSP);
	//rmTimer(streamTimerID);
	streamTimerID = -1;
	StreamS[1].s  = ISS_ON;
	StreamSP.s    = IPS_IDLE;
	IDSetSwitch(&StreamSP, NULL);
	
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
	
	/*if (!strcmp(name, FileNameTP.name))
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
	  
	}*/	  
	      	
}


void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{

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
     
     setContrast( (int) (ImageAdjustN[0].value * 128));
     setBrightness( (int) (ImageAdjustN[1].value * 128));
     setHue( (int) (ImageAdjustN[2].value * 128));
     setColor( (int) (ImageAdjustN[3].value * 128));
     setWhiteness( (int) (ImageAdjustN[4].value * 128));
     
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
   int err, fd;
   char errmsg[1024];
   char filename[] = "/tmp/fitsXXXXXX";
  
   
   if ((fd = mkstemp(filename)) < 0)
   { 
    IDMessage(mydev, "Error making temporary filename.", NULL);
    IDLog("Error making temporary filename.\n", NULL);
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

int writeFITS(char * filename, char errmsg[])
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
 /*IDSetSwitch(&ExposeSP, "FITS image written to %s", filename);
 IDLog("FITS image written to '%s'\n", filename);*/
 IDSetSwitch(&ExposeSP, "Loading FITS image...");
 IDLog("Loading FITS image...\n");
 
 uploadFile(filename);
  
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
      
      V4LFrame->compressedFrame = (unsigned char *) malloc (sizeof(unsigned char) * 1);
      
      if (dataChannelTimerID == -1)
      {
        disableUpdate = false;
        updateDataChannel(NULL);
      }
      IDLog("V4L Device is online. Retrieving basic data.\n");
      getBasicData();
      
      break;
      
    case ISS_OFF:
      PowerS[0].s = ISS_OFF;
      PowerS[1].s = ISS_ON;
      PowerSP.s = IPS_IDLE;
      
      disableUpdate = true;
      
      disconnectCam();
      
      free(V4LFrame->compressedFrame);
      
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

void updateStream(void *p)
{
   int width  = getWidth();
   int height = getHeight();
   unsigned int totalBytes, compressedBytes = 0;
   unsigned char *targetFrame;
   char frameSize[FRAME_ILEN];
   int nr=0, n=0,r, frameLen;
   p=p;
   
   V4LFrame->Y      		= getY();
   V4LFrame->U      		= getU();
   V4LFrame->V      		= getV();
   V4LFrame->colorBuffer 	= getColorBuffer();
  
   totalBytes  = ImageTypeS[0].s == ISS_ON ? width * height : width * height * 4;
   targetFrame = ImageTypeS[0].s == ISS_ON ? V4LFrame->Y : V4LFrame->colorBuffer;
   
   /* Compress frame */
   V4LFrame->compressedFrame = (unsigned char *) realloc (V4LFrame->compressedFrame, sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3);
   r = lzo1x_1_compress(targetFrame,totalBytes,V4LFrame->compressedFrame,&compressedBytes,wrkmem);
   if (r != LZO_E_OK)
      //IDLog("compressed %lu bytes into %lu bytes\n", (long) totalBytes, (long) compressedBytes);
   //else
   {
 	/* this should NEVER happen */
 	IDLog("internal error - compression failed: %d\n", r);
	return;
   }
   
   snprintf(frameSize, FRAME_ILEN, "VIDEO;%d,%d]", totalBytes, compressedBytes);
   frameLen = strlen(frameSize);
   r = 0;
   
  for (int i=0; i < nclients; i++)
  {
  	if (StreamS[0].s == ISS_ON && INDIClients[i].streamReady)
        {
	     /* #1 send frame size to client */
	     for (int j=0; j < frameLen; j+=r)
	      r = write(INDIClients[i].wp, frameSize + j , frameLen - j);
	     
	     /* #2 Send the actual compressed frame */
             for (nr=0; nr < (int) compressedBytes; nr+=n)
   	     {
	  	n = write(INDIClients[i].wp, V4LFrame->compressedFrame + nr, compressedBytes - nr );
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
     
  //IDLog("updating stream...\n");
  
  
  
  if (streamTimerID != -1)
    	streamTimerID = addTimer(1000/ (int) FrameRateN[0].value, updateStream, NULL);
   
}

void uploadFile(char * filename)
{
   FILE * fitsFile;
   char frameSize[FRAME_ILEN];
   unsigned char *fitsData;
   int n=0, i=0, r=0;
   unsigned int totalBytes, compressedBytes = 0, frameLen =0 , nr = 0;
   struct stat stat_p; 
 
   if ( -1 ==  stat (filename, &stat_p))
   { 
     IDLog(" Error occoured attempting to stat %s\n", filename); 
     return; 
   }
   
   totalBytes = stat_p.st_size;
   fitsData = new unsigned char[totalBytes];
   
   /*
   
   IDLog("File size is   %d bytes\n", stat_p.st_size); 
   DataSizeN[0].value = stat_p.st_size;
   IDSetNumber(&DataSizeNP, NULL);
 
    TODO This is temporary to avoid race condition.
            I need to introduce some hand-shake signals later on 
	    
   usleep(500000); */

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
   r = lzo1x_1_compress(fitsData, totalBytes, V4LFrame->compressedFrame, &compressedBytes, wrkmem);
   if (r != LZO_E_OK)
      //IDLog("compressed %lu bytes into %lu bytes\n", (long) totalBytes, (long) compressedBytes);
   //else
   {
 	/* this should NEVER happen */
 	IDLog("internal error - compression failed: %d\n", r);
	return;
   }
   
   snprintf(frameSize, FRAME_ILEN, "FITS;%d,%d]", totalBytes, compressedBytes);
   frameLen = strlen(frameSize);
   r = 0;
   
     for (i=0; i < nclients; i++)
     {
  	if (INDIClients[i].streamReady)
        {
	
	      /* #1 send frame size to client */
	     for (unsigned int j=0; j < frameLen; j+=r)
	      r = write(INDIClients[i].wp, frameSize + j , frameLen - j);
	      
	     /* Send the actual file */
             for (nr=0; nr < compressedBytes; nr+=n)
   	     {
	  	n = write(INDIClients[i].wp, V4LFrame->compressedFrame + nr, compressedBytes - nr );
		if (n <= 0)
		{
			if (nr <= 0)
    			{
      				IDLog("Stream error: %s\n", strerror(errno));
      				return;
    			}
		        break;
		}
		
             }
        }
      } 
    
   
   delete (fitsData);
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
  p=p;
  if (disableUpdate)
  {
   dataChannelTimerID = -1;
   return;
  }
    
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
  
   dataChannelTimerID = addTimer(1000, updateDataChannel, NULL);
  
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
	
	while (1)
	{
                /*IDLog("Waiting for input from driver\n");	*/
	
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
