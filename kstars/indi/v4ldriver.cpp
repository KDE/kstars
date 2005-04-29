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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <zlib.h>
#include <asm/types.h>

#include "indidevapi.h"
#include "indicom.h"
#include "fitsrw.h"
#include "eventloop.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_LINUX_VIDEODEV2_H
#include "webcam/v4l2_base.h"
V4L2_Base *v4l_base;
#else
#include "webcam/v4l1_base.h"
V4L1_Base *v4l_base;
#endif 

#define ERRMSGSIZ	1024

void ISInit(void);
void ISPoll(void *);
void connectV4L(void);
void updateStream(void * p);
void getBasicData(void);
void uploadFile(const char * filename);
int  writeFITS(const char *filename, char errmsg[]);
int  grabImage(void);
int  checkPowerN(INumberVectorProperty *np);
int  checkPowerS(ISwitchVectorProperty *sp);
int  checkPowerT(ITextVectorProperty *tp);
FITS_HDU_LIST * create_fits_header (FITS_FILE *ofp, uint width, uint height, uint bpp);
void updateV4L1Controls();
void updateV4L2Controls();
void newFrame(void *p);

 
extern char* me;
extern int errno;
static int frameCount = 0;

#define mydev           "Video4Linux Generic Device"

#define COMM_GROUP	"Main Control"
#define IMAGE_GROUP	"Image Settings"
#define DATA_GROUP      "Data Channel"


#define MAX_PIXELS	4096		/* Max number of pixels in one dimension */
#define POLLMS		1000		/* Polling time (ms) */

#define FILENAMESIZ	2048
#define LIBVERSIZ 	ERRMSGSIZ
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

img_t * V4LFrame;			/* V4L frame */
static int streamTimerID;		/* Stream ID */
double divider;

/*INDI controls */

/* Connect/Disconnect */
static ISwitch PowerS[]          	= {{"CONNECT" , "Connect" , ISS_OFF, 0, 0},{"DISCONNECT", "Disconnect", ISS_ON, 0, 0}};
static ISwitchVectorProperty PowerSP	= { mydev, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, PowerS, NARRAY(PowerS), "", 0};

/* Ports */
static IText PortT[]			= {{"PORT", "Port", 0, 0, 0, 0}};
static ITextVectorProperty PortTP	= { mydev, "DEVICE_PORT", "Ports", COMM_GROUP, IP_RW, 0, IPS_IDLE, PortT, NARRAY(PortT), "", 0};

/* Camera Name */
static IText camNameT[]		        = {{"Model", "", 0, 0, 0, 0 }};
static ITextVectorProperty camNameTP    = { mydev, "Camera Model", "", COMM_GROUP, IP_RO, 0, IPS_IDLE, camNameT, NARRAY(camNameT), "", 0};

/* Video Stream */
static ISwitch StreamS[]		= {{"ON", "", ISS_OFF, 0, 0}, {"OFF", "", ISS_ON, 0, 0} };
static ISwitchVectorProperty StreamSP   = { mydev, "VIDEO_STREAM", "Video Stream", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, StreamS, NARRAY(StreamS), "", 0 };

/* Frame Rate */
static INumber FrameRateN[]		= {{"RATE", "Rate", "%0.f", 1., 50., 1., 10., 0, 0, 0}};
static INumberVectorProperty FrameRateNP= { mydev, "FRAME_RATE", "Frame Rate", COMM_GROUP, IP_RW, 60, IPS_IDLE, FrameRateN, NARRAY(FrameRateN), "", 0};

/* Compression */
static ISwitch CompressS[]          	= {{"ON" , "" , ISS_ON, 0, 0},{"OFF", "", ISS_OFF, 0, 0}};
static ISwitchVectorProperty CompressSP	= { mydev, "Compression" , "", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE, CompressS, NARRAY(CompressS), "", 0};

/* Image color */
static ISwitch ImageTypeS[]		= {{ "Grey", "", ISS_ON, 0, 0}, { "Color", "", ISS_OFF, 0, 0 }};
static ISwitchVectorProperty ImageTypeSP= { mydev, "Image Type", "", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, ImageTypeS, NARRAY(ImageTypeS), "", 0};

/* Frame dimension */
static INumber ImageSizeN[]		= {{"WIDTH", "Width", "%0.f", 0., 0., 10., 0., 0, 0, 0},
					   {"HEIGHT", "Height", "%0.f", 0., 0., 10., 0., 0, 0, 0}};
static INumberVectorProperty ImageSizeNP = { mydev, "IMAGE_SIZE", "Image Size", IMAGE_GROUP, IP_RW, 60, IPS_IDLE, ImageSizeN, NARRAY(ImageSizeN), "", 0};

/* Exposure time */
static INumber ExposeTimeN[]    = {{ "EXPOSE_S", "Duration (s)", "%5.2f", 1., 1., .0, 1., 0, 0, 0}};
static INumberVectorProperty ExposeTimeNP = { mydev, "EXPOSE_DURATION", "Expose", COMM_GROUP, IP_RW, 60, IPS_IDLE, ExposeTimeN, NARRAY(ExposeTimeN), "", 0};

#ifndef HAVE_LINUX_VIDEODEV2_H
static INumber ImageAdjustN[] = {{"Contrast", "", "%0.f", 0., 256., 1., 0., 0, 0, 0 }, 
                                   {"Brightness", "", "%0.f", 0., 256., 1., 0., 0 ,0 ,0}, 
				   {"Hue", "", "%0.f", 0., 256., 1., 0., 0, 0, 0}, 
				   {"Color", "", "%0.f", 0., 256., 1., 0., 0 , 0 ,0}, 
				   {"Whiteness", "", "%0.f", 0., 256., 1., 0., 0 , 0 ,0}};
				   
static INumberVectorProperty ImageAdjustNP = {mydev, "Image Adjustments", "", IMAGE_GROUP, IP_RW, 0, IPS_IDLE, ImageAdjustN, NARRAY(ImageAdjustN), "", 0 };
#else
INumberVectorProperty ImageAdjustNP = {mydev, "Image Adjustments", "", IMAGE_GROUP, IP_RW, 0, IPS_IDLE, NULL, 0, "", 0 };
#endif

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
 
  #ifdef HAVE_LINUX_VIDEODEV2_H
    v4l_base = new V4L2_Base();
  #else
    v4l_base = new V4L1_Base();
    divider = 128.;
  #endif 
  
 
 PortT[0].text     = NULL;
 IUSaveText(&PortT[0], "/dev/video0");

 camNameT[0].text  = NULL;
 
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
  #ifndef HAVE_LINUX_VIDEODEV2_H
  IDDefNumber(&FrameRateNP, NULL);
  #endif
  IDDefNumber(&ExposeTimeNP, NULL);
  IDDefBLOB(&imageBP, NULL);
  
  /* Image properties */
  IDDefSwitch(&CompressSP, NULL);
  IDDefSwitch(&ImageTypeSP, NULL);
  IDDefNumber(&ImageSizeNP, NULL);
  
  #ifndef HAVE_LINUX_VIDEODEV2_H
  IDDefNumber(&ImageAdjustNP, NULL);
  #endif


  
}
 
 void ISNewBLOB (const char */*dev*/, const char */*name*/, int */*sizes[]*/, char **/*blobs[]*/, char **/*formats[]*/, char **/*names[]*/, int /*n*/)
{}

void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	char errmsg[ERRMSGSIZ];

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

     /* Compression */
     if (!strcmp(name, CompressSP.name))
     {
       IUResetSwitches(&CompressSP);
       IUUpdateSwitches(&CompressSP, states, names, n);
       CompressSP.s = IPS_OK;
       
       IDSetSwitch(&CompressSP, NULL);
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
       
          
       if (StreamS[0].s == ISS_ON)
       {
         frameCount = 0;
         IDLog("Starting the video stream.\n");
         v4l_base->start_capturing(errmsg);
	 StreamSP.s  = IPS_BUSY; 
       }
       else
       {
         IDLog("The video stream has been disabled. Frame count %d\n", frameCount);
         v4l_base->stop_capturing(errmsg);
       }
       
       IDSetSwitch(&StreamSP, NULL);
       return;
     }
     
}

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int /*n*/)
{
	IText *tp;

	ISInit();
 
       /* ignore if not ours */ 
       if (dev && strcmp (mydev, dev))
         return;

	if (!strcmp(name, PortTP.name) )
	{
	  PortTP.s = IPS_OK;
	  tp = IUFindText( &PortTP, names[0] );
	  if (!tp)
	   return;

          IUSaveText(tp, texts[0]);
	  IDSetText (&PortTP, NULL);
	  return;
	}
}

void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
      char errmsg[ERRMSGSIZ];

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
      
      if (v4l_base->setSize( (int) ImageSizeN[0].value, (int) ImageSizeN[1].value) != -1)
      {
         ImageSizeN[0].value = v4l_base->getWidth();
	 ImageSizeN[1].value = v4l_base->getHeight();
         IDSetNumber(&ImageSizeNP, NULL);
	 return;
      }
      else
      {
        ImageSizeN[0].value = oldW;
	ImageSizeN[1].value = oldH;
        ImageSizeNP.s = IPS_ALERT;
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
       
     v4l_base->setFPS( (int) FrameRateN[0].value );
     
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
     
     #ifndef HAVE_LINUX_VIDEODEV2_H
     v4l_base->setContrast( (int) (ImageAdjustN[0].value * divider));
     v4l_base->setBrightness( (int) (ImageAdjustN[1].value * divider));
     v4l_base->setHue( (int) (ImageAdjustN[2].value * divider));
     v4l_base->setColor( (int) (ImageAdjustN[3].value * divider));
     v4l_base->setWhiteness( (int) (ImageAdjustN[4].value * divider));
     
     ImageAdjustN[0].value = v4l_base->getContrast() / divider;
     ImageAdjustN[1].value = v4l_base->getBrightness() / divider;
     ImageAdjustN[2].value = v4l_base->getHue() / divider;
     ImageAdjustN[3].value = v4l_base->getColor() / divider;
     ImageAdjustN[4].value = v4l_base->getWhiteness() / divider;

     #else
     int ctrl_id;
     for (int i=0; i < ImageAdjustNP.nnp; i++)
     {
         ctrl_id = *((int *) ImageAdjustNP.np[i].aux0);
         if (v4l_base->setINTControl( ctrl_id , ImageAdjustNP.np[0].value, errmsg) < 0)
         {
            ImageAdjustNP.s = IPS_ALERT;
            IDSetNumber(&ImageAdjustNP, "Unable to adjust setting. %s", errmsg);
            return;
         }
     }
     #endif
     
     ImageAdjustNP.s = IPS_OK;
     IDSetNumber(&ImageAdjustNP, NULL);
     return;
   }
   
   
    /* Exposure */
    if (!strcmp (ExposeTimeNP.name, name))
    {
       
       if (checkPowerN(&ExposeTimeNP))
         return;
    
        if (StreamS[0].s == ISS_ON) 
          v4l_base->stop_capturing(errmsg);

	streamTimerID = -1;
	StreamS[0].s  = ISS_OFF;
	StreamS[1].s  = ISS_ON;
	StreamSP.s    = IPS_IDLE;
	IDSetSwitch(&StreamSP, NULL);
        
	
        
	V4LFrame->expose = 1000;
	/*V4LFrame->Y      = v4l_base->getY();
	V4LFrame->U      = v4l_base->getU();
	V4LFrame->V      = v4l_base->getV();*/

        v4l_base->start_capturing(errmsg);
        ExposeTimeNP.s   = IPS_BUSY;
	IDSetNumber(&ExposeTimeNP, NULL);
	
        /*if (grabImage() < 0)
	{
	 ExposeTimeNP.s   = IPS_IDLE;
	 IDSetNumber(&ExposeTimeNP, NULL);
	}*/
	  
	
     return;
    } 
      
  
  	
}

void newFrame(void */*p*/)
{
  char errmsg[ERRMSGSIZ];
  static int dropLarge = 3;

  V4LFrame->Y      = v4l_base->getY();
  V4LFrame->U      = v4l_base->getU();
  V4LFrame->V      = v4l_base->getV();

  if (StreamSP.s == IPS_BUSY)
  {
      frameCount++;

     // Drop some frames
     if (ImageSizeN[0].value > 160)
     {
        dropLarge--;
        if (dropLarge == 0)
        {
          dropLarge = 3;
          return;
        }
        else if (dropLarge < 2) return;
        
      }
     updateStream(NULL);
  }
  else if (ExposeTimeNP.s == IPS_BUSY)
  {
     v4l_base->stop_capturing(errmsg);
     grabImage();
  }

}

/* Downloads the image from the CCD row by row and store them
   in a raw file.
 N.B. No processing is done on the image */
int grabImage()
{
   int err, fd;
   char errmsg[ERRMSG_SIZE];
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

int writeFITS(const char * filename, char errmsg[])
{
  FITS_FILE* ofp;
  int i, bpp, bpsl, width, height;
  long nbytes;
  FITS_HDU_LIST *hdu;
  
  ofp = fits_open (filename, "w");
  if (!ofp)
  {
    snprintf(errmsg, ERRMSG_SIZE, "Error: cannot open file for writing.");
    return (-1);
  }
  
  width  = v4l_base->getWidth();
  height = v4l_base->getHeight();
  bpp    = 1;                      /* Bytes per Pixel */
  bpsl   = bpp * width;    	   /* Bytes per Line */
  nbytes = 0;
  
  hdu = create_fits_header (ofp, width, height, bpp);
  if (hdu == NULL)
  {
     snprintf(errmsg, ERRMSG_SIZE, "Error: creating FITS header failed.");
     return (-1);
  }
  if (fits_write_header (ofp, hdu) < 0)
  {
    snprintf(errmsg, ERRMSG_SIZE, "Error: writing to FITS header failed.");
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
    snprintf(errmsg, ERRMSG_SIZE, "Error: write error occured");
    return (-1);
  }
 
 fits_close (ofp);      
 
  /* Success */
 ExposeTimeNP.s = IPS_OK;
 IDSetNumber(&ExposeTimeNP, NULL);
 
 uploadFile(filename);
  
 return 0;

}


/* Retrieves basic data from the device upon connection.*/
void getBasicData()
{

  int xmax, ymax, xmin, ymin;
  
  v4l_base->getMaxMinSize(xmax, ymax, xmin, ymin);
  
  /* Width */
  ImageSizeN[0].value = v4l_base->getWidth();
  ImageSizeN[0].min = xmin;
  ImageSizeN[0].max = xmax;
  
  /* Height */
  ImageSizeN[1].value = v4l_base->getHeight();
  ImageSizeN[1].min = ymin;
  ImageSizeN[1].max = ymax;
  
  IUUpdateMinMax(&ImageSizeNP);
  IDSetNumber(&ImageSizeNP, NULL);
  
  IUSaveText(&camNameT[0], v4l_base->getDeviceName());
  IDSetText(&camNameTP, NULL);

   #ifndef HAVE_LINUX_VIDEODEV2_H
     updateV4L1Controls();
   #else
    updateV4L2Controls();
   #endif
   
}

void updateV4L2Controls()
{
    #ifdef HAVE_LINUX_VIDEODEV2_H
    char errmsg[ERRMSGSIZ];

    //IDLog("in updateV4L2Controls\n");

    // #1 Query for INTEGER controls, and fill up the structure
      free(ImageAdjustNP.np);
      ImageAdjustNP.nnp = 0;
      
   if (v4l_base->queryINTControls(&ImageAdjustNP) > 0)
      IDDefNumber(&ImageAdjustNP, NULL);
    #endif

/*    if (v4l_base->query_ctrl(V4L2_CID_CONTRAST, ImageAdjustN[0].min, ImageAdjustN[0].max, ImageAdjustN[0].step, ImageAdjustN[0].value, errmsg))
        IDLog("Contract: %s\n", errmsg);
       
    if (v4l_base->query_ctrl(V4L2_CID_BRIGHTNESS, ImageAdjustN[1].min, ImageAdjustN[1].max, ImageAdjustN[1].step, ImageAdjustN[1].value, errmsg))
       IDLog("Brightness: %s\n", errmsg);
    
    if (v4l_base->query_ctrl(V4L2_CID_HUE, ImageAdjustN[2].min, ImageAdjustN[2].max, ImageAdjustN[2].step, ImageAdjustN[2].value, errmsg))
        IDLog("Hue: %s\n", errmsg);
    
    if (v4l_base->query_ctrl(V4L2_CID_SATURATION, ImageAdjustN[3].min, ImageAdjustN[3].max, ImageAdjustN[3].step, ImageAdjustN[3].value, errmsg))
       IDLog("%s\n", errmsg);
    
    if (v4l_base->query_ctrl(V4L2_CID_WHITENESS, ImageAdjustN[4].min, ImageAdjustN[4].max, ImageAdjustN[4].step, ImageAdjustN[4].value, errmsg))
       IDLog("%s\n", errmsg);
    
   IUUpdateMinMax(&ImageAdjustNP);*/

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
  char errmsg[ERRMSGSIZ];
  
    
  switch (PowerS[0].s)
  {
     case ISS_ON:
      if (v4l_base->connectCam(PortT[0].text, errmsg) < 0)
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

      v4l_base->registerCallback(newFrame);
      
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
      v4l_base->disconnectCam();
      
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
 
 snprintf(instrumentName, 80, "INSTRUME= '%s'", v4l_base->getDeviceName());
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
 
 snprintf(expose_s, sizeof(expose_s), "EXPOSURE= %d / milliseconds", V4LFrame->expose);
 
 fits_add_card (hdulist, expose_s);
 fits_add_card (hdulist, instrumentName);
 fits_add_card (hdulist, obsDate);
 
 return (hdulist);
}


void updateStream(void */*p*/)
{
 
   int width  = v4l_base->getWidth();
   int height = v4l_base->getHeight();
   uLongf compressedBytes = 0;
   uLong totalBytes;
   unsigned char *targetFrame;
   int r;
   char errmsg[ERRMSGSIZ];
   
   if (PowerS[0].s == ISS_OFF || StreamS[0].s == ISS_OFF) return;
   
   V4LFrame->Y      		= v4l_base->getY();
   V4LFrame->U      		= v4l_base->getU();
   V4LFrame->V      		= v4l_base->getV();
   V4LFrame->colorBuffer 	= v4l_base->getColorBuffer();
  
   totalBytes  = ImageTypeS[0].s == ISS_ON ? width * height : width * height * 4;
   targetFrame = ImageTypeS[0].s == ISS_ON ? V4LFrame->Y : V4LFrame->colorBuffer;

   /* Do we want to compress ? */
    if (CompressS[0].s == ISS_ON)
    {   
   	/* Compress frame */
   	V4LFrame->compressedFrame = (unsigned char *) realloc (V4LFrame->compressedFrame, sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3);
   
   	compressedBytes = sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3;
   
   	r = compress2(V4LFrame->compressedFrame, &compressedBytes, targetFrame, totalBytes, 4);
   	if (r != Z_OK)
   	{
	 	/* this should NEVER happen */
	 	IDLog("internal error - compression failed: %d\n", r);
		return;
   	}
   
   	/* #3.A Send it compressed */
   	imageB.blob = V4LFrame->compressedFrame;
   	imageB.bloblen = compressedBytes;
   	imageB.size = totalBytes;
   	strcpy(imageB.format, ".stream.z");
     }
     else
     {
       /* #3.B Send it uncompressed */
        imageB.blob = targetFrame;
   	imageB.bloblen = totalBytes;
   	imageB.size = totalBytes;
   	strcpy(imageB.format, ".stream");
     }
        
   imageBP.s = IPS_OK;
   IDSetBLOB (&imageBP, NULL);
   
   #ifndef HAVE_LINUX_VIDEODEV2_H
      v4l_base->start_capturing(errmsg);
   #endif
}

void uploadFile(const char * filename)
{
   
   FILE * fitsFile;
   unsigned char *fitsData;
   int r=0;
   unsigned int nr = 0;
   uLong  totalBytes;
   uLongf compressedBytes = 0;
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
   
   if (CompressS[0].s == ISS_ON)
   {   
   /* #2 Compress it */
   V4LFrame->compressedFrame = (unsigned char *) realloc (V4LFrame->compressedFrame, sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3);
   
    compressedBytes = sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3;
     
     
   r = compress2(V4LFrame->compressedFrame, &compressedBytes, fitsData, totalBytes, 9);
   if (r != Z_OK)
   {
 	/* this should NEVER happen */
 	IDLog("internal error - compression failed: %d\n", r);
	return;
   }

   /* #3.A Send it compressed */
   imageB.blob = V4LFrame->compressedFrame;
   imageB.bloblen = compressedBytes;
   imageB.size = totalBytes;
   strcpy(imageB.format, ".fits.z");
   }
   else
   {
     imageB.blob = fitsData;
     imageB.bloblen = totalBytes;
     imageB.size = totalBytes;
     strcpy(imageB.format, ".fits");
   }
   
   imageBP.s = IPS_OK;
   IDSetBLOB (&imageBP, NULL);
   
   delete (fitsData);
} 

void updateV4L1Controls()
{

  #ifndef HAVE_LINUX_VIDEODEV2_H
  if ( (v4l_base->getContrast() / divider) > ImageAdjustN[0].max)
      divider *=2;

  if ( (v4l_base->getHue() / divider) > ImageAdjustN[2].max)
      divider *=2;

  ImageAdjustN[0].value = v4l_base->getContrast() / divider;
  ImageAdjustN[1].value = v4l_base->getBrightness() / divider;
  ImageAdjustN[2].value = v4l_base->getHue() / divider;
  ImageAdjustN[3].value = v4l_base->getColor() / divider;
  ImageAdjustN[4].value = v4l_base->getWhiteness() / divider;

  ImageAdjustNP.s = IPS_OK;
  IDSetNumber(&ImageAdjustNP, NULL);
  #endif

}

