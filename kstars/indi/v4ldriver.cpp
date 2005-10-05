#if 0
    V4L INDI Driver
    INDI Interface for V4L devices
    Copyright (C) 2003-2005 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "v4ldriver.h"

V4L_Driver::V4L_Driver()
{
  V4LFrame = (img_t *) malloc (sizeof(img_t));
 
  if (V4LFrame == NULL)
  {
     IDMessage(NULL, "Error: unable to initialize driver. Low memory.");
     IDLog("Error: unable to initialize driver. Low memory.");
     return;
  }
 
  camNameT[0].text  = NULL; 
  PortT[0].text     = NULL;
  IUSaveText(&PortT[0], "/dev/video0");

  divider = 128.;
 

}

V4L_Driver::~V4L_Driver()
{
  free (V4LFrame);
}


void V4L_Driver::initProperties(const char *dev)
{
  
  strncpy(device_name, dev, MAXINDIDEVICE);
 
  /* Connection */
  fillSwitch(&PowerS[0], "CONNECT", "Connect", ISS_OFF);
  fillSwitch(&PowerS[1], "DISCONNECT", "Disconnect", ISS_ON);
  fillSwitchVector(&PowerSP, PowerS, NARRAY(PowerS), dev, "CONNECTION", "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

 /* Port */
  fillText(&PortT[0], "PORT", "Port", "/dev/ttyS0");
  fillTextVector(&PortTP, PortT, NARRAY(PortT), dev, "DEVICE_PORT", "Ports", COMM_GROUP, IP_RW, 0, IPS_IDLE);

 /* Video Stream */
  fillSwitch(&StreamS[0], "ON", "", ISS_OFF);
  fillSwitch(&StreamS[1], "OFF", "", ISS_ON);
  fillSwitchVector(&StreamSP, StreamS, NARRAY(StreamS), dev, "VIDEO_STREAM", "Video Stream", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  /* Compression */
  fillSwitch(&CompressS[0], "ON", "", ISS_ON);
  fillSwitch(&CompressS[1], "OFF", "", ISS_OFF);
  fillSwitchVector(&CompressSP, CompressS, NARRAY(StreamS), dev, "Compression", "", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  /* Image type */
  fillSwitch(&ImageTypeS[0], "Grey", "", ISS_ON);
  fillSwitch(&ImageTypeS[1], "Color", "", ISS_OFF);
  fillSwitchVector(&ImageTypeSP, ImageTypeS, NARRAY(ImageTypeS), dev, "Image Type", "", IMAGE_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

  /* Camera Name */
  fillText(&camNameT[0], "Model", "", "");
  fillTextVector(&camNameTP, camNameT, NARRAY(camNameT), dev, "Camera Model", "", COMM_GROUP, IP_RO, 0, IPS_IDLE);
  
  /* Expose */
  fillNumber(&ExposeTimeN[0], "EXPOSE_DURATION", "Duration (s)", "%5.2f", 0., 36000., 0.5, 1.);
  fillNumberVector(&ExposeTimeNP, ExposeTimeN, NARRAY(ExposeTimeN), dev, "CCD_EXPOSE_DURATION", "Expose", COMM_GROUP, IP_RW, 60, IPS_IDLE);

/* Frame Rate */
  fillNumber(&FrameRateN[0], "RATE", "Rate", "%0.f", 1., 50., 1., 10.);
  fillNumberVector(&FrameRateNP, FrameRateN, NARRAY(FrameRateN), dev, "FRAME_RATE", "Frame Rate", COMM_GROUP, IP_RW, 60, IPS_IDLE);

  /* Frame dimension */
  fillNumber(&FrameN[0], "X", "X", "%.0f", 0., 0., 0., 0.);
  fillNumber(&FrameN[1], "Y", "Y", "%.0f", 0., 0., 0., 0.);
  fillNumber(&FrameN[2], "WIDTH", "Width", "%.0f", 0., 0., 10., 0.);
  fillNumber(&FrameN[3], "HEIGHT", "Height", "%.0f", 0., 0., 10., 0.);
  fillNumberVector(&FrameNP, FrameN, NARRAY(FrameN), dev, "CCD_FRAME", "Frame", IMAGE_GROUP, IP_RW, 60, IPS_IDLE);

  /*fillNumber(&ImageSizeN[0], "WIDTH", "Width", "%0.f", 0., 0., 10., 0.);
  fillNumber(&ImageSizeN[1], "HEIGHT", "Height", "%0.f", 0., 0., 10., 0.);
  fillNumberVector(&ImageSizeNP, ImageSizeN, NARRAY(ImageSizeN), dev, "IMAGE_SIZE", "Image Size", IMAGE_GROUP, IP_RW, 60, IPS_IDLE);*/
  
  #ifndef HAVE_LINUX_VIDEODEV2_H
  fillNumber(&ImageAdjustN[0], "Contrast", "", "%0.f", 0., 256., 1., 0.);
  fillNumber(&ImageAdjustN[1], "Brightness", "", "%0.f", 0., 256., 1., 0.);
  fillNumber(&ImageAdjustN[2], "Hue", "", "%0.f", 0., 256., 1., 0.);
  fillNumber(&ImageAdjustN[3], "Color", "", "%0.f", 0., 256., 1., 0.);
  fillNumber(&ImageAdjustN[4], "Whiteness", "", "%0.f", 0., 256., 1., 0.);
  fillNumberVector(&ImageAdjustNP, ImageAdjustN, NARRAY(ImageAdjustN), dev, "Image Adjustments", "", IMAGE_GROUP, IP_RW, 60, IPS_IDLE);
  #else
  fillNumberVector(&ImageAdjustNP, NULL, 0, dev, "Image Adjustments", "", IMAGE_GROUP, IP_RW, 60, IPS_IDLE);
  #endif 

  // We need to setup the BLOB (Binary Large Object) below. Using this property, we can send FITS to our client
  strcpy(imageB.name, "CCD1");
  strcpy(imageB.label, "Feed");
  strcpy(imageB.format, "");
  imageB.blob    = 0;
  imageB.bloblen = 0;
  imageB.size    = 0;
  imageB.bvp     = 0;
  imageB.aux0    = 0;
  imageB.aux1    = 0;
  imageB.aux2    = 0;
  
  strcpy(imageBP.device, dev);
  strcpy(imageBP.name, "Video");
  strcpy(imageBP.label, "Video");
  strcpy(imageBP.group, COMM_GROUP);
  strcpy(imageBP.timestamp, "");
  imageBP.p       = IP_RO;
  imageBP.timeout = 0;
  imageBP.s       = IPS_IDLE;
  imageBP.bp	  = &imageB;
  imageBP.nbp     = 1;
  imageBP.aux     = 0;

}

void V4L_Driver::initCamBase()
{
   #ifndef HAVE_LINUX_VIDEODEV2_H
    v4l_base = new V4L1_Base();
   #else
    v4l_base = new V4L2_Base();
   #endif
}

void V4L_Driver::ISGetProperties (const char *dev)
{ 

  if (dev && strcmp (device_name, dev))
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
  IDDefNumber(&FrameNP, NULL);
  
  #ifndef HAVE_LINUX_VIDEODEV2_H
  IDDefNumber(&ImageAdjustNP, NULL);
  #endif


  
}
 
void V4L_Driver::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	char errmsg[ERRMSGSIZ];

	/* ignore if not ours */
	if (dev && strcmp (device_name, dev))
	    return;
	    
     /* Connection */
     if (!strcmp (name, PowerSP.name))
     {
          IUResetSwitches(&PowerSP);
	  IUUpdateSwitches(&PowerSP, states, names, n);
   	  connectCamera();
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

void V4L_Driver::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int /*n*/)
{
	IText *tp;


       /* ignore if not ours */ 
       if (dev && strcmp (device_name, dev))
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

void V4L_Driver::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
      char errmsg[ERRMSGSIZ];

	/* ignore if not ours */
	if (dev && strcmp (device_name, dev))
	    return;
	    
    
    /* Frame Size */
    if (!strcmp (FrameNP.name, name))
    {
      if (checkPowerN(&FrameNP))
        return;
	
       int oldW = (int) FrameN[2].value;
       int oldH = (int) FrameN[3].value;

      FrameNP.s = IPS_OK;
      
      if (IUUpdateNumbers(&FrameNP, values, names, n) < 0)
       return;
      
      if (v4l_base->setSize( (int) FrameN[2].value, (int) FrameN[3].value) != -1)
      {
         FrameN[2].value = v4l_base->getWidth();
	 FrameN[3].value = v4l_base->getHeight();
         IDSetNumber(&FrameNP, NULL);
	 return;
      }
      else
      {
        FrameN[2].value = oldW;
	FrameN[3].value = oldH;
        FrameNP.s = IPS_ALERT;
	IDSetNumber(&FrameNP, "Failed to set a new image size.");
      }
      
      return;
   }

   #ifndef HAVE_LINUX_VIDEODEV2_H
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
   #endif
   
   
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
     unsigned int ctrl_id;
     for (int i=0; i < ImageAdjustNP.nnp; i++)
     {
         ctrl_id = *((unsigned int *) ImageAdjustNP.np[i].aux0);
         if (v4l_base->setINTControl( ctrl_id , ImageAdjustNP.np[i].value, errmsg) < 0)
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

	StreamS[0].s  = ISS_OFF;
	StreamS[1].s  = ISS_ON;
	StreamSP.s    = IPS_IDLE;
	IDSetSwitch(&StreamSP, NULL);
        
	V4LFrame->expose = 1000;

        v4l_base->start_capturing(errmsg);
        ExposeTimeNP.s   = IPS_BUSY;
	IDSetNumber(&ExposeTimeNP, NULL);
	
        return;
    } 
      
  
  	
}

void V4L_Driver::newFrame(void *p)
{
  ((V4L_Driver *) (p))->updateFrame();
}

void V4L_Driver::updateFrame()
{
  char errmsg[ERRMSGSIZ];
  static int dropLarge = 3;

  if (StreamSP.s == IPS_BUSY)
  {
      frameCount++;

     // Drop some frames
     if (FrameN[2].value > 160)
     {
        dropLarge--;
        if (dropLarge == 0)
        {
          dropLarge = 3;
          return;
        }
        else if (dropLarge < 2) return;
        
      }

     updateStream();
  }
  else if (ExposeTimeNP.s == IPS_BUSY)
  {
     V4LFrame->Y      = v4l_base->getY();
     v4l_base->stop_capturing(errmsg);
     grabImage();
  }

}

void V4L_Driver::updateStream()
{
 
   int width  = v4l_base->getWidth();
   int height = v4l_base->getHeight();
   uLongf compressedBytes = 0;
   uLong totalBytes;
   unsigned char *targetFrame;
   int r;
   
   if (PowerS[0].s == ISS_OFF || StreamS[0].s == ISS_OFF) return;
   
   if (ImageTypeS[0].s == ISS_ON)
      V4LFrame->Y      		= v4l_base->getY();
   else
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
      char errmsg[ERRMSGSIZ];
      v4l_base->start_capturing(errmsg);
   #endif
}

/* Downloads the image from the CCD row by row and store them
   in a raw file.
 N.B. No processing is done on the image */
int V4L_Driver::grabImage()
{
   int err, fd;
   char errmsg[ERRMSG_SIZE];
   char filename[] = "/tmp/fitsXXXXXX";
  
   
   if ((fd = mkstemp(filename)) < 0)
   { 
    IDMessage(device_name, "Error making temporary filename.");
    IDLog("Error making temporary filename.\n");
    return -1;
   }
   close(fd);
  
   err = writeFITS(filename, errmsg);
   if (err)
   {
       IDMessage(device_name, errmsg, NULL);
       return -1;
   }
   
  return 0;
}

int V4L_Driver::writeFITS(const char * filename, char errmsg[])
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

void V4L_Driver::uploadFile(const char * filename)
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

void V4L_Driver::connectCamera()
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
	  IDSetSwitch(&PowerSP, "Error: unable to open device");
	  IDLog("Error: %s\n", errmsg);
	  return;
      }
      
      /* Sucess! */
      PowerS[0].s = ISS_ON;
      PowerS[1].s = ISS_OFF;
      PowerSP.s = IPS_OK;
      IDSetSwitch(&PowerSP, "Video4Linux Generic Device is online. Retrieving basic data.");

      v4l_base->registerCallback(newFrame, this);
      
      V4LFrame->compressedFrame = (unsigned char *) malloc (sizeof(unsigned char) * 1);
      
      IDLog("V4L Device is online. Retrieving basic data.\n");
      getBasicData();
      
      break;
      
    case ISS_OFF:
      PowerS[0].s = ISS_OFF;
      PowerS[1].s = ISS_ON;
      PowerSP.s = IPS_IDLE;
      
      free(V4LFrame->compressedFrame);
      V4LFrame->compressedFrame = NULL;
      v4l_base->disconnectCam();
      
      IDSetSwitch(&PowerSP, "Video4Linux Generic Device is offline.");
      
      break;
     }
}

/* Retrieves basic data from the device upon connection.*/
void V4L_Driver::getBasicData()
{

  int xmax, ymax, xmin, ymin;
  
  v4l_base->getMaxMinSize(xmax, ymax, xmin, ymin);
  
  /* Width */
  FrameN[2].value = v4l_base->getWidth();
  FrameN[2].min = xmin;
  FrameN[2].max = xmax;
  
  /* Height */
  FrameN[3].value = v4l_base->getHeight();
  FrameN[3].min = ymin;
  FrameN[3].max = ymax;
  
  IUUpdateMinMax(&FrameNP);
  IDSetNumber(&FrameNP, NULL);
  
  IUSaveText(&camNameT[0], v4l_base->getDeviceName());
  IDSetText(&camNameTP, NULL);

   #ifndef HAVE_LINUX_VIDEODEV2_H
     updateV4L1Controls();
   #else
    updateV4L2Controls();
   #endif
   
}

#ifdef HAVE_LINUX_VIDEODEV2_H
void V4L_Driver::updateV4L2Controls()
{
    // #1 Query for INTEGER controls, and fill up the structure
      free(ImageAdjustNP.np);
      ImageAdjustNP.nnp = 0;
      
   if (v4l_base->queryINTControls(&ImageAdjustNP) > 0)
      IDDefNumber(&ImageAdjustNP, NULL);
}
#else
void V4L_Driver::updateV4L1Controls()
{

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
  
}
#endif

int V4L_Driver::checkPowerS(ISwitchVectorProperty *sp)
{
  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(sp->label, ""))
        IDMessage (device_name, "Cannot change property %s while the camera is offline.", sp->name);
    else
        IDMessage (device_name, "Cannot change property %s while the camera is offline.", sp->label);
    
    sp->s = IPS_IDLE;
    IDSetSwitch(sp, NULL);
    return -1;
  }

  return 0;
}

int V4L_Driver::checkPowerN(INumberVectorProperty *np)
{
   
  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(np->label, ""))
        IDMessage (device_name, "Cannot change property %s while the camera is offline.", np->name);
    else
        IDMessage (device_name, "Cannot change property %s while the camera is offline.", np->label);
	
    np->s = IPS_IDLE;
    IDSetNumber(np, NULL);
    return -1;
  }

  return 0;
}

int V4L_Driver::checkPowerT(ITextVectorProperty *tp)
{

  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(tp->label, ""))
        IDMessage (device_name, "Cannot change property %s while the camera is offline.", tp->name);
    else
        IDMessage (device_name, "Cannot change property %s while the camera is offline.", tp->label);
	
    tp->s = IPS_IDLE;
    IDSetText(tp, NULL);
    return -1;
  }

  return 0;

}

FITS_HDU_LIST * V4L_Driver::create_fits_header (FITS_FILE *ofp, uint width, uint height, uint bpp)
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



