#if 0
    INDI driver for SBIG CCD
    Copyright (C) 2005 Chris Curran (ccurran AT planetcurran DOT com)

    Based on Apogee PPI driver by Jasem Mutlaq (mutlaqja AT ikarustech DOT com)

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
 
#include <ctype.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <zlib.h>

#include "sbigccd.h"
#include "lilxml.h"
#include "base64.h"

extern char* me;			/* argv[0] */
SBIGCam *MainCam = NULL;		/* Main and only camera */

/* send client definitions of all properties */
void ISInit()
{
  if (MainCam == NULL)
    MainCam = new SBIGCam();
}
    
void ISGetProperties (const char *dev)
{ 
  if (dev && strcmp (mydev, dev))
    return;
  
   ISInit();
  
  MainCam->ISGetProperties(dev);
}


void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	
  /* ignore if not ours */
  if (dev && strcmp (dev, mydev))
    return;
	    
  ISInit();
  
  MainCam->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
  /* ignore if not ours */ 
  if (dev && strcmp (mydev, dev))
    return;

   ISInit();
   
   MainCam->ISNewText(dev, name, texts, names, n);
}


void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
      
  /* ignore if not ours */
  if (dev && strcmp (dev, mydev))
    return;
	    
  ISInit();
  
  MainCam->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB (const char */*dev*/, const char */*name*/, int */*sizes[]*/, char **/*blobs[]*/, char **/*formats[]*/, char **/*names[]*/, int /*n*/)
{

  // We use this if we're receving binary data from the client. Most likely we won't for this driver.

}

SBIGCam::SBIGCam()
{
  initProperties();
  IEAddTimer (POLLMS, SBIGCam::ISStaticPoll, this);
}

SBIGCam::~SBIGCam()
{
  
}

void SBIGCam::initProperties()
{
  fillSwitch(&PowerS[0], "CONNECT", "Connect", ISS_OFF);
  fillSwitch(&PowerS[1], "DISCONNECT", "Disconnect", ISS_ON);
  fillSwitchVector(&PowerSP, PowerS, NARRAY(PowerS), mydev, "CONNECTION", "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
  
  fillSwitch(&FrameTypeS[0], "FRAME_LIGHT", "Light", ISS_ON);
  fillSwitch(&FrameTypeS[1], "FRAME_BIAS", "Bias", ISS_OFF);
  fillSwitch(&FrameTypeS[2], "FRAME_DARK", "Dark", ISS_OFF);
  fillSwitch(&FrameTypeS[3], "FRAME_FLAT", "Flat Field", ISS_OFF);
  fillSwitchVector(&FrameTypeSP, FrameTypeS, NARRAY(FrameTypeS), mydev, "CCD_FRAME_TYPE", "Frame Type", EXPOSE_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

  fillNumber(&FrameN[0], "X", "X", "%.0f", 0., MAX_PIXELS, 1., 0.);
  fillNumber(&FrameN[1], "Y", "Y", "%.0f", 0., MAX_PIXELS, 1., 0.);
  fillNumber(&FrameN[2], "WIDTH", "Width", "%.0f", 0., MAX_PIXELS, 1., 0.);
  fillNumber(&FrameN[3], "HEIGHT", "Height", "%.0f", 0., MAX_PIXELS, 1., 0.);
  fillNumberVector(&FrameNP, FrameN, NARRAY(FrameN), mydev, "CCD_FRAME", "Frame", IMAGE_GROUP, IP_RW, 60, IPS_IDLE);
  
  fillNumber(&BinningN[0], "HOR_BIN", "X", "%0.f", 1., MAXHBIN, 1., 1.);
  fillNumber(&BinningN[1], "VER_BIN", "Y", "%0.f", 1., MAXVBIN, 1., 1.);
  fillNumberVector(&BinningNP, BinningN, NARRAY(BinningN), mydev, "CCD_BINNING", "Binning", IMAGE_GROUP, IP_RW, 60, IPS_IDLE);
    
  fillNumber(&ExposeTimeN[0], "EXPOSE_DURATION", "Duration (s)", "%5.2f", 0., 36000., 0.5, 1.);
  fillNumberVector(&ExposeTimeNP, ExposeTimeN, NARRAY(ExposeTimeN), mydev, "CCD_EXPOSE_DURATION", "Expose", EXPOSE_GROUP, IP_RW, 60, IPS_IDLE);
  
  fillNumber(&TemperatureN[0], "TEMPERATURE", "Temperature", "%+06.2f", MIN_CCD_TEMP, MAX_CCD_TEMP, 0.2, 0.);
  fillNumberVector(&TemperatureNP, TemperatureN, NARRAY(TemperatureN), mydev, "CCD_TEMPERATURE", "Expose", EXPOSE_GROUP, IP_RW, 60, IPS_IDLE);

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
  
  strcpy(imageBP.device, mydev);
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

void SBIGCam::ISGetProperties(const char */*dev*/)
{
  
  /* COMM_GROUP */
  IDDefSwitch(&PowerSP, NULL);
  IDDefBLOB(&imageBP, NULL);

  /* Expose */
  IDDefSwitch(&FrameTypeSP, NULL);  
  IDDefNumber(&ExposeTimeNP, NULL);
  IDDefNumber(&TemperatureNP, NULL);
  
  /* Image Group */
  IDDefNumber(&FrameNP, NULL);
  IDDefNumber(&BinningNP, NULL);
  
}

void SBIGCam::ISNewSwitch (const char */*dev*/, const char *name, ISState *states, char *names[], int n)
{
  
	/* Connection */
	if (!strcmp (name, PowerSP.name))
	{
  		IUResetSwitches(&PowerSP);
  		IUUpdateSwitches(&PowerSP, states, names, n);
  		connectCCD();
  		return;
	}
	
     	/* Frame Type */
     	if (!strcmp(FrameTypeSP.name, name))
	{
  		if (checkPowerS(&FrameTypeSP))
    		return;
		
		IUResetSwitches(&FrameTypeSP);
		IUUpdateSwitches(&FrameTypeSP, states, names, n);
		FrameTypeSP.s = IPS_OK;
		IDSetSwitch(&FrameTypeSP, NULL);
		
		return;
	}
     
}

void SBIGCam::ISNewText (const char */*dev*/, const char */*name*/, char **/*texts[]*/, char **/*names[]*/, int /*n*/)
{
  
}

void SBIGCam::ISNewNumber (const char */*dev*/, const char *name, double values[], char *names[], int n)
{
    /* Exposure time */
    if (!strcmp (ExposeTimeNP.name, name))
    {
       if (checkPowerN(&ExposeTimeNP))
         return;

       if (ExposeTimeNP.s == IPS_BUSY)
       {
	  ExposeTimeNP.s = IPS_IDLE;
	  ExposeTimeN[0].value = 0;

	  IDSetNumber(&ExposeTimeNP, "Exposure cancelled.");
	  IDLog("Exposure Cancelled.\n");
	  return;
        }
    
       ExposeTimeNP.s = IPS_IDLE;
       
       IUUpdateNumbers(&ExposeTimeNP, values, names, n);
       
      IDLog("Exposure Time (ms) is: %g\n", ExposeTimeN[0].value);
      
      handleExposure(NULL);
      return;
    } 
    
  if (!strcmp(TemperatureNP.name, name))
  {
    if (checkPowerN(&TemperatureNP))
      return;
      
    TemperatureNP.s = IPS_IDLE;
    
    if (values[0] < MIN_CCD_TEMP || values[0] > MAX_CCD_TEMP)
    {
      IDSetNumber(&TemperatureNP, "Error: valid range of temperature is from %d to %d", MIN_CCD_TEMP, MAX_CCD_TEMP);
      return;
    }
    
    targetTemp = values[0];

    // JM: Below, tell SBIG to update to temperature to targetTemp
    // e.g. cam->setTemp(targetTemp);
    

    // Now we set property to busy and poll in ISPoll for CCD temp
    TemperatureNP.s = IPS_BUSY;
    
    IDSetNumber(&TemperatureNP, "Setting CCD temperature to %+06.2f C", values[0]);
    IDLog("Setting CCD temperature to %+06.2f C\n", values[0]);
    return;
   }
   
   // Subframing
   if (!strcmp(FrameNP.name, name))
   {
     if (checkPowerN(&FrameNP))
      return;
      
     FrameNP.s = IPS_OK;
     IUUpdateNumbers(&FrameNP, values, names, n);
     
     // JM: Below, we setup the frame size we want to take exposure of
     // The way it is done depends on the camera API. Example below
     //cam->m_StartX = (int) FrameN[0].value;
     //cam->m_StartY = (int) FrameN[1].value;
     //cam->m_Width   = (int) FrameN[2].value;
     //cam->m_Height  = (int) FrameN[3].value;

     IDSetNumber(&FrameNP, NULL);
      
   } /* end FrameNP */
      

   // Binning     
   if (!strcmp(BinningNP.name, name))
   {
     if (checkPowerN(&BinningNP))
       return;
       
     
     BinningNP.s = IPS_OK;
     IUUpdateNumbers(&BinningNP, values, names, n);
     
     // JM: Below we set the camera binning.
     // eg:
     //cam->m_BinX = (int) BinningN[0].value;
     //cam->m_BinY = (int) BinningN[1].value;
     
     IDLog("Binning is: %.0f x %.0f\n", BinningN[0].value, BinningN[1].value);
     return;
   }
}


void SBIGCam::ISStaticPoll(void *p)
{
	if (!((SBIGCam *)p)->isCCDConnected())
	{
	  IEAddTimer (POLLMS, SBIGCam::ISStaticPoll, p);
	  return;
	}
	
	((SBIGCam *) p)->ISPoll();
	
	IEAddTimer (POLLMS, SBIGCam::ISStaticPoll, p);
}

void SBIGCam::ISPoll()
{
  static int mtc=5;
  int readStatus=0;
  double ccdTemp (0);
	
  switch (ExposeTimeNP.s)
  {
    case IPS_IDLE:
    case IPS_OK:
      break;
	    
    case IPS_BUSY:
      
      // JM: Here we check the status of the camera (whether it's still capturing an image or has finished that)
      // ISPoll is called once per second. e.g. below for how we do this for Apogee Cameras
      /*
      readStatus = cam->read_Status();
      if (readStatus < 0)
      {
	IDLog("Error in exposure!\n");
	ExposeTimeNP.s = IPS_IDLE;
	ExposeTimeN[0].value = 0;
	IDSetNumber(&ExposeTimeNP, "Error in exposure procedure.");
	return;
      }
      else if (readStatus == Camera_Status_ImageReady)
      {
	ExposeTimeN[0].value = 0;
	ExposeTimeNP.s = IPS_OK;
	IDSetNumber(&ExposeTimeNP, "Exposure done, downloading image...");
	IDLog("Exposure done, downloading image...\n");
	// grab and save image. Don't forget to call this!
	grabImage();
	return;
      } */
      
      ExposeTimeN[0].value --;
      IDSetNumber(&ExposeTimeNP, NULL);
      break;
	    
	case IPS_ALERT:
	    break;
	}
	 
     
     switch (TemperatureNP.s)
     {
           // JM: If we're not setting a new temperature (i.e. state is either ok or idle)
           // We simply check for the temp every 5 seconds (this is why mtc = 5 and then decremented).
	   case IPS_IDLE:
	   case IPS_OK:
	     mtc--;
	     
	     if (mtc == 0)
	     {
	       //TemperatureN[0].value = cam->read_Temperature();
	       IDSetNumber(&TemperatureNP, NULL);
	       mtc = 5;
	     }
	     break;
	     
           // JM: If we're setting a new temperature, we continously check for it here untill we get
           // a "close enough" value. This close enough value as seen below is the TEMP_THRESHOLD (0.25 C)
	   case IPS_BUSY:
	   
             // Read in current CCD temp
	     //ccdTemp = cam->read_Temperature();
	     
	     if (fabs(targetTemp - ccdTemp) <= TEMP_THRESHOLD)
	       TemperatureNP.s = IPS_OK;
	     	       
	      mtc = 1;
              TemperatureN[0].value = ccdTemp;
	      IDSetNumber(&TemperatureNP, NULL);
	      break;
	      
	    case IPS_ALERT:
	     break;
	  }
  	  
}

/* Downloads the image from the CCD row by row and store them
   in a raw file.
 N.B. No processing is done on the image */
void SBIGCam::grabImage()
{
  
  long err;
  int img_size, fd;
  char errmsg[1024];
  char filename[] = "/tmp/fitsXXXXXX";
  
   if ((fd = mkstemp(filename)) < 0)
   { 
    IDMessage(mydev, "Error making temporary filename.");
    IDLog("Error making temporary filename.\n");
    return;
   }
   close(fd);
     
   // JM: allocate memory for buffer. In most cameras, the bit depth is 16 bit and so we use unsigned short
   // But change if the bit depth is different
   img_size = SBIGFrame.width * SBIGFrame.height * sizeof(unsigned short);
  
   SBIGFrame.img = (unsigned short *) malloc (img_size);
   
  if (SBIGFrame.img == NULL)
  {
    IDMessage(mydev, "Not enough memory to store image.");
    IDLog("Not enough memory to store image.\n");
    return;
  }
  
  // JM: Here we fetch the image from the camera
  // 
  /*if (!cam->GetImage( SBIGFrame.img , SBIGFrame.width, SBIGFrame.height ))
  {
       free(SBIGFrame.img);
       IDMessage(mydev, "GetImage() failed.");
       IDLog("GetImage() failed.");
       return;
  }*/
  
   err = writeFITS(filename, errmsg);
   
   if (err)
   {
       free(SBIGFrame.img);
       IDMessage(mydev, errmsg, NULL);
       return;
   }
   
  free(SBIGFrame.img);
   
}

int SBIGCam::writeFITS(char *filename, char errmsg[])
{
  
  FITS_FILE* ofp;
  int bpp, bpsl, width, height;
  long nbytes;
  FITS_HDU_LIST *hdu;
  
  ofp = fits_open (filename, "w");
  if (!ofp)
  {
    sprintf(errmsg, "Error: cannot open file for writing.");
    return (-1);
  }
  
  // We're assuming bit depth is 16 (unsigned short). Change as desired.
  width  = SBIGFrame.width;
  height = SBIGFrame.height;
  bpp    = sizeof(unsigned short);   /* Bytes per Pixel */
  bpsl   = bpp * SBIGFrame.width;    /* Bytes per Line */
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
  
  // JM: Here we're performing little endian to big endian (network order) conversion
  // Since FITS is stored in big endian. If the camera provides the image buffer in big endian
  // then there is no need for this conversion
  for (int i=0; i < height; i++)
    for (int j=0 ; j < width; j++)
      SBIGFrame.img[width * i + j] = getBigEndian( (SBIGFrame.img[width * i + j]) );
  
  // The "2" below is for two bytes (16 bit or unsigned short). Again, change as desired.
  for (int i= 0; i < height  ; i++)
  {
    fwrite(SBIGFrame.img + (i * width), 2, width, ofp->fp);
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
 ExposeTimeNP.s = IPS_OK;
 IDSetNumber(&ExposeTimeNP, NULL);
 IDLog("Loading FITS image...\n");
 
 uploadFile(filename);

 return 0;

}

void SBIGCam::uploadFile(char * filename)
{

   FILE * fitsFile;
   unsigned char *fitsData, *compressedData;
   int r=0;
   unsigned int i =0, nr = 0;
   uLongf compressedBytes=0;
   uLong  totalBytes;
   struct stat stat_p; 
 
   if ( -1 ==  stat (filename, &stat_p))
   { 
     IDLog(" Error occoured attempting to stat %s\n", filename); 
     return; 
   }
   
   totalBytes     = stat_p.st_size;
   fitsData       = (unsigned char *) malloc (sizeof(unsigned char) * totalBytes);
   compressedData = (unsigned char *) malloc (sizeof(unsigned char) * totalBytes + totalBytes / 64 + 16 + 3);
   
   if (fitsData == NULL || compressedData == NULL)
   {
     IDLog("Error! low memory. Unable to initialize fits buffers.\n");
     return;
   }
   
   fitsFile = fopen(filename, "r");
   
   if (fitsFile == NULL)
    return;
   
   /* #1 Read file from disk */ 
   for (i=0; i < totalBytes; i+= nr)
   {
      nr = fread(fitsData + i, 1, totalBytes - i, fitsFile);
     
     if (nr <= 0)
     {
        IDLog("Error reading temporary FITS file.\n");
        return;
     }
   }
   
   compressedBytes = sizeof(char) * totalBytes + totalBytes / 64 + 16 + 3;
     
   /* #2 Compress it */
   r = compress2(compressedData, &compressedBytes, fitsData, totalBytes, 9);
   if (r != Z_OK)
   {
 	/* this should NEVER happen */
 	IDLog("internal error - compression failed: %d\n", r);
	return;
   }
   
   /* #3 Send it */
   imageB.blob = compressedData;
   imageB.bloblen = compressedBytes;
   imageB.size = totalBytes;
   strcpy(imageB.format, ".fits.z");
   imageBP.s = IPS_OK;
   IDSetBLOB (&imageBP, NULL);
   
   free (fitsData);   
   free (compressedData);
   
}

/* Initiates the exposure procedure */
void SBIGCam::handleExposure(void */*p*/)
{
  
  int curFrame = getOnSwitch(&FrameTypeSP);
  
  switch (curFrame)
  {
    /* Light frame */ 
   case LIGHT_FRAME:
   
   /*if (!cam->Expose( (int) ExposeTimeN[0].value, true ))
   {  
     ExposeTimeNP.s = IPS_IDLE;
     IDSetNumber(&ExposeTimeNP, "Light Camera exposure failed.");
     IDLog("Light Camera exposure failed.\n");
     return;
   }*/
   break;
   
   /* BIAS frame is the same as DARK but with minimum period. i.e. readout from camera electronics.
   */
    case BIAS_FRAME:
      
      /*if (!cam->Expose( 0.05 , false ))
      {  
	ExposeTimeNP.s = IPS_IDLE;
	IDSetNumber(&ExposeTimeNP, "Bias Camera exposure failed.");
	IDLog("Bias Camera exposure failed.\n");
	return;
      }*/
      break;
      
      /* Dark frame */
    case DARK_FRAME:
      
      /*if (!cam->Expose( (int) ExposeTimeN[0].value , false ))
      {  
	ExposeTimeNP.s = IPS_IDLE;
	IDSetNumber(&ExposeTimeNP, "Dark Camera exposure failed.");
	IDLog("Dark Camera exposure failed.\n");
	return;
      }*/
      break;
      
    case FLAT_FRAME:
      
      /*if (!cam->Expose( (int) ExposeTimeN[0].value , true ))
      {  
	ExposeTimeNP.s = IPS_IDLE;
	IDSetNumber(&ExposeTimeNP, "Flat Camera exposure failed.");
	IDLog("Flat Camera exposure failed.\n");
	return;
      }*/
      break;
  }
      
  SBIGFrame.frameType	= curFrame;
  SBIGFrame.width	= (int) FrameN[2].value;
  SBIGFrame.height	= (int) FrameN[3].value;
  SBIGFrame.expose	= (int) ExposeTimeN[0].value;
  SBIGFrame.temperature =       TemperatureN[0].value;
  SBIGFrame.binX        = (int) BinningN[0].value;
  SBIGFrame.binY        = (int) BinningN[1].value;
  
  ExposeTimeNP.s = IPS_BUSY;
		  
  IDSetNumber(&ExposeTimeNP, "Taking a %g seconds frame...", ExposeTimeN[0].value);
  IDLog("Taking a frame...\n");
   
}

/* Retrieves basic data from the CCD upon connection like temperature, array size, firmware..etc */
void SBIGCam::getBasicData()
{


  // Maximum resolution

  // JM: Update here basic variables upon connection. 

  /*
  FrameN[2].max = cam->m_NumX;
  FrameN[3].max = cam->m_NumY;
  IUUpdateMinMax(&FrameNP);
  
  // Maximum Bin
  BinningN[0].max = cam->m_MaxBinX;
  BinningN[1].max = cam->m_MaxBinX;
  IUUpdateMinMax(&BinningNP);
  
  // Current Temperature
  TemperatureN[0].value = cam->read_Temperature();
  IDSetNumber(&TemperatureNP, NULL);*/
  
}

int SBIGCam::getOnSwitch(ISwitchVectorProperty *sp)
{
 for (int i=0; i < sp->nsp ; i++)
 {
     if (sp->sp[i].s == ISS_ON)
      return i;
 }

 return -1;
}

int SBIGCam::checkPowerS(ISwitchVectorProperty *sp)
{
  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(sp->label, ""))
    	IDMessage (mydev, "Cannot change property %s while the CCD is offline.", sp->name);
    else
    	IDMessage (mydev, "Cannot change property %s while the CCD is offline.", sp->label);
	
    sp->s = IPS_IDLE;
    IDSetSwitch(sp, NULL);
    return -1;
  }

  return 0;
}

int SBIGCam::checkPowerN(INumberVectorProperty *np)
{
  if (PowerSP.s != IPS_OK)
  {
     if (!strcmp(np->label, ""))
    	IDMessage (mydev, "Cannot change property %s while the CCD is offline.", np->name);
    else
    	IDMessage (mydev, "Cannot change property %s while the CCD is offline.", np->label);
    
    np->s = IPS_IDLE;
    IDSetNumber(np, NULL);
    return -1;
  }

  return 0;
}

int SBIGCam::checkPowerT(ITextVectorProperty *tp)
{

  if (PowerSP.s != IPS_OK)
  {
    if (!strcmp(tp->label, ""))
    	IDMessage (mydev, "Cannot change property %s while the CCD is offline.", tp->name);
    else
    	IDMessage (mydev, "Cannot change property %s while the CCD is offline.", tp->label);
	
    tp->s = IPS_IDLE;
    IDSetText(tp, NULL);
    return -1;
  }

  return 0;

}

void SBIGCam::connectCCD()
{
  
  switch (PowerS[0].s)
  {
    case ISS_ON:
      if (initCamera())
      {
	/* Sucess! */
	PowerS[0].s = ISS_ON;
	PowerS[1].s = ISS_OFF;
	PowerSP.s = IPS_OK;
	IDSetSwitch(&PowerSP, "CCD is online. Retrieving basic data.");
	IDLog("CCD is online. Retrieving basic data.\n");
	getBasicData();
	
      }
      else
      {
      	PowerSP.s = IPS_IDLE;
      	PowerS[0].s = ISS_OFF;
      	PowerS[1].s = ISS_ON;
      	IDSetSwitch(&PowerSP, "Error: no cameras were detected.");
      	IDLog("Error: no cameras were detected.\n");
      	return;
      }
      
      break;
      
    case ISS_OFF:
      PowerS[0].s = ISS_OFF;
      PowerS[1].s = ISS_ON;
      PowerSP.s = IPS_IDLE;
      IDSetSwitch(&PowerSP, "CCD is offline.");

      // JM: Disconnect cam and clean up
      // e.g. disConnectSBIG();
      break;
     }

}

bool SBIGCam::initCamera()
{
  // JM: Here, we do all camera initilization stuff and we establish a connection to the camera
  // If everything goes well, we return true, otherwise false
  // Please use IDLog(...) to report errors.

  return false;
}

/* isCCDConnected: return 1 if we have a connection, 0 otherwise */
int SBIGCam::isCCDConnected(void)
{
  return ((PowerS[0].s == ISS_ON) ? 1 : 0);
}

FITS_HDU_LIST * SBIGCam::create_fits_header (FITS_FILE *ofp, uint width, uint height, uint bpp)
{
 
 FITS_HDU_LIST *hdulist;
 
 char temp_s[FITS_CARD_SIZE], expose_s[FITS_CARD_SIZE], binning_s[FITS_CARD_SIZE], pixel_s[FITS_CARD_SIZE], frame_s[FITS_CARD_SIZE];
 char obsDate[FITS_CARD_SIZE];
 
 snprintf(obsDate, FITS_CARD_SIZE, "DATE-OBS= '%s' /Observation Date UTC", timestamp());
 
 hdulist = fits_add_hdu (ofp);
 if (hdulist == NULL) return (NULL);

 hdulist->used.simple = 1;
 hdulist->bitpix = 16;
 hdulist->naxis = 2;
 hdulist->naxisn[0] = width;
 hdulist->naxisn[1] = height;
 hdulist->naxisn[2] = bpp;
 // JM: Record here the minimum and maximum pixel values
 /*hdulist->used.datamin = min();
 hdulist->datamin = min();
 hdulist->used.datamax = max();
 hdulist->datamax = max();*/
 hdulist->used.bzero = 1;
 hdulist->bzero = 0.0;
 hdulist->used.bscale = 1;
 hdulist->bscale = 1.0;
 
 snprintf(temp_s, FITS_CARD_SIZE, "CCD-TEMP= %g / degrees celcius", SBIGFrame.temperature);
 snprintf(expose_s, FITS_CARD_SIZE, "EXPOSURE= %d / milliseconds", SBIGFrame.expose);
 snprintf(binning_s, FITS_CARD_SIZE, "BINNING = '(%d x %d)'", SBIGFrame.binX, SBIGFrame.binY);
 //sprintf(pixel_s, "PIX-SIZ = '%.0f microns square'", PixelSizeN[0].value);
 switch (SBIGFrame.frameType)
  {
    case LIGHT_FRAME:
      	strcpy(frame_s, "FRAME   = 'Light'");
	break;
    case BIAS_FRAME:
        strcpy(frame_s, "FRAME   = 'Bias'");
	break;
    case FLAT_FRAME:
        strcpy(frame_s, "FRAME   = 'Flat Field'");
	break;
    case DARK_FRAME:
        strcpy(frame_s, "FRAME   = 'Dark'");
	break;
  }
 
 fits_add_card (hdulist, frame_s);   
 fits_add_card (hdulist, temp_s);
 fits_add_card (hdulist, expose_s);
 //fits_add_card (hdulist, pixel_s);

 // JM: If there is a way to get the specific model of the camera, use that.
 fits_add_card (hdulist, "INSTRUME= 'SBIG CCD'");
 fits_add_card (hdulist, obsDate);
  
 return (hdulist);
}

