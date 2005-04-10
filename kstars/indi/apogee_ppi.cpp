#if 0
    FLI CCD
    INDI Interface for Finger Lakes Instruments CCDs
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
 
#include <ctype.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <zlib.h>

#include "apogee_ppi.h"
#include "lilxml.h"

extern char* me;			/* argv[0] */
ApogeeCam *MainCam = NULL;		/* Main and only camera */

/* Initiate static class members */
int ApogeeCam::streamTimerID			= -1;

/* send client definitions of all properties */
void ISInit()
{
  if (MainCam == NULL)
    MainCam = new ApogeeCam();
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

ApogeeCam::ApogeeCam()
{
  streamTimerID = -1;
  ApogeeModelS = NULL;
  
  initProperties();
  
}

ApogeeCam::~ApogeeCam()
{
  
}

void ApogeeCam::initProperties()
{
  fillSwitch(&PowerS[0], "CONNECT", "Connect", ISS_OFF);
  fillSwitch(&PowerS[1], "DISCONNECT", "Disconnect", ISS_ON);
  fillSwitchVector(&PowerSP, PowerS, NARRAY(PowerS), "CONNECTION", "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
  
  fillSwitch(&FrameTypeS[0], "FRAME_LIGHT", "Light", ISS_ON);
  fillSwitch(&FrameTypeS[1], "FRAME_BIAS", "Bias", ISS_OFF);
  fillSwitch(&FrameTypeS[2], "FRAME_DARK", "Dark", ISS_OFF);
  fillSwitch(&FrameTypeS[3], "FRAME_FLAT", "Flat Field", ISS_OFF);
  fillSwitchVector(&FrameTypeSP, FrameTypeS, NARRAY(FrameTypeS), "FRAME_TYPE", "Frame Type", EXPOSE_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

  fillNumber(&FrameN[0], "X", "", "%.0f", 0., MAX_PIXELS, 1., 0.);
  fillNumber(&FrameN[1], "Y", "", "%.0f", 0., MAX_PIXELS, 1., 0.);
  fillNumber(&FrameN[2], "Width", "", "%.0f", 0., MAX_PIXELS, 1., 0.);
  fillNumber(&FrameN[3], "Height", "", "%.0f", 0., MAX_PIXELS, 1., 0.);
  fillNumberVector(&FrameNP, FrameN, NARRAY(FrameN), "FRAME", "Frame", IMAGE_GROUP, IP_RW, 60, IPS_IDLE);
  
  fillNumber(&BinningN[0], "HOR_BIN", "X", "%0.f", 1., MAXHBIN, 1., 1.);
  fillNumber(&BinningN[1], "VER_BIN", "Y", "%0.f", 1., MAXVBIN, 1., 1.);
  fillNumberVector(&BinningNP, BinningN, NARRAY(BinningN), "BINNING", "Binning", IMAGE_GROUP, IP_RW, 60, IPS_IDLE);
    
  fillNumber(&ExposeTimeN[0], "EXPOSE_S", "Duration (s)", "%5.2f", 0., 36000., 0.5, 1.);
  fillNumberVector(&ExposeTimeNP, ExposeTimeN, NARRAY(ExposeTimeN), "EXPOSE_DURATION", "Expose", EXPOSE_GROUP, IP_RW, 60, IPS_IDLE);
  
  fillNumber(&TemperatureN[0], "TEMPERATURE", "Temperature", "%+06.2f", MIN_CCD_TEMP, MAX_CCD_TEMP, 0.2, 0.);
  fillNumberVector(&TemperatureNP, TemperatureN, NARRAY(TemperatureN), "CCD_TEMPERATURE", "Expose", EXPOSE_GROUP, IP_RW, 60, IPS_IDLE);
  
  IBLOB imageB = {"CCD1", "Feed", "", 0, 0, 0, 0, 0, 0, 0};
  static IBLOBVectorProperty imageBP = {mydev, "Video", "Video", COMM_GROUP,
    IP_RO, 0, IPS_IDLE, &imageB, 1, "", 0};
    
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
  
  loadXMLModel();
}

bool ApogeeCam::loadXMLModel()
{
  LilXML *XMLParser = newLilXML();
  XMLEle *root = NULL, *camera = NULL;
  XMLAtt *modelName;
  FILE *modelSpecFile = NULL;
  char errmsg[1024];
  int ncams = 0;
  
  //IDLog("Top dir is "TOP_DATADIR, NULL);
  modelSpecFile = fopen(TOP_DATADIR"/apogee_caminfo.xml", "r");
  //modelSpecFile = fopen("/opt/kde3/share/apps/kstars/apogee_caminfo.xml", "r");
  if (modelSpecFile == NULL) 
  {
    IDLog("Error: Unable to open file apogee_caminfo.xml\n");
    IDMessage(mydev, "Error: Unable to open file apogee_caminfo.xml");
    return false;
  }
  
  root = readXMLFile(modelSpecFile, XMLParser, errmsg);
  if (root == NULL)
  {
    IDLog("Error: Unable to process apogee_caminfo.xml. %s\n", errmsg);
    IDMessage(mydev, "Error: Unable to process apogee_caminfo.xml. %s\n", errmsg);
    fclose(modelSpecFile);
    delLilXML(XMLParser);
    return false;
  }
    
  for (camera = nextXMLEle (root, 1); camera != NULL; camera = nextXMLEle (root, 0))
  {
    modelName = findXMLAtt(camera, "model");
    if (modelName == NULL) 
      continue;
    
    ApogeeModelS = (ApogeeModelS == NULL) ? (ISwitch *) malloc (sizeof(ISwitch))
                                          : (ISwitch *) realloc(ApogeeModelS, sizeof(ISwitch) * (ncams + 1));
    
    snprintf(ApogeeModelS[ncams].name, MAXINDINAME, "Model%d", ncams);
    strcpy(ApogeeModelS[ncams].label, valuXMLAtt(modelName));
    ApogeeModelS[ncams].s = (ncams == 0) ? ISS_ON : ISS_OFF;
    ApogeeModelS[ncams].svp = NULL;
    ApogeeModelS[ncams].aux = NULL;
    
    ncams++;
  }
  
  fclose(modelSpecFile);
  delLilXML(XMLParser);
  
  if (ncams > 0)
  {
  	fillSwitchVector(&ApogeeModelSP, ApogeeModelS, ncams, "Model", "", COMM_GROUP, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
	return true;
  }
  
  return false;
  
}

void ApogeeCam::fillSwitch(ISwitch *sp, const char *name, const char * label, ISState s)
{
  strcpy(sp->name, name);
  strcpy(sp->label, label);
  sp->s = s;
  sp->svp = NULL;
  sp->aux = NULL;
}

void ApogeeCam::fillSwitchVector(ISwitchVectorProperty *svp, ISwitch *sp, int nsp, const char *name, const char *label, const char *group, IPerm p, ISRule r, double timeout, IPState s)
{
  strcpy(svp->device, mydev);
  strcpy(svp->name, name);
  strcpy(svp->label, label);
  strcpy(svp->group, group);
  strcpy(svp->timestamp, "");
  
  svp->p	= p;
  svp->r	= r;
  svp->timeout	= timeout;
  svp->s	= s;
  svp->sp	= sp;
  svp->nsp	= nsp;
}
 
void ApogeeCam::fillNumber(INumber *np, const char *name, const char * label, const char *format, double min, double max, double step, double value)
{

  strcpy(np->name, name);
  strcpy(np->label, label);
  strcpy(np->format, format);
  
  np->min	= min;
  np->max	= max;
  np->step	= step;
  np->value	= value;
  np->nvp	= NULL;
  np->aux0	= NULL;
  np->aux1	= NULL;
}

void ApogeeCam::fillNumberVector(INumberVectorProperty *nvp, INumber *np, int nnp, const char *name, const char *label, const char* group, IPerm p, double timeout, IPState s)
{
  
  strcpy(nvp->device, mydev);
  strcpy(nvp->name, name);
  strcpy(nvp->label, label);
  strcpy(nvp->group, group);
  strcpy(nvp->timestamp, "");
  
  nvp->p	= p;
  nvp->timeout	= timeout;
  nvp->s	= s;
  nvp->np	= np;
  nvp->nnp	= nnp;
  
}

void ApogeeCam::ISGetProperties(const char *dev)
{
  
  /* COMM_GROUP */
  IDDefSwitch(&PowerSP, NULL);
  if (loadXMLModel())
    IDDefSwitch(&ApogeeModelSP, NULL);
  else
    IDMessage(mydev, "Error: Unable to read camera specifications. Driver is disabled.");
  IDDefBLOB(&imageBP, NULL);

  /* Expose */
  IDDefSwitch(&FrameTypeSP, NULL);  
  IDDefNumber(&ExposeTimeNP, NULL);
  IDDefNumber(&TemperatureNP, NULL);
  
  /* Image Group */
  IDDefNumber(&FrameNP, NULL);
  IDDefNumber(&BinningNP, NULL);
  
  IEAddTimer (POLLMS, ApogeeCam::ISStaticPoll, this);

  
}

void ApogeeCam::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
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
     
     	/* Apogee Model */
     	if (!strcmp(ApogeeModelSP.name, name))
	{
  		IUResetSwitches(&ApogeeModelSP);
  		IUUpdateSwitches(&ApogeeModelSP, states, names, n);
		ApogeeModelSP.s = IPS_OK;
		IDSetSwitch(&ApogeeModelSP, NULL);
  		return;
	}
	
}

void ApogeeCam::ISNewText (const char */*dev*/, const char */*name*/, char **/*texts[]*/, char **/*names[]*/, int /*n*/)
{
  
}

void ApogeeCam::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    /* Exposure time */
    if (!strcmp (ExposeTimeNP.name, name))
    {
       if (checkPowerN(&ExposeTimeNP))
         return;

       if (ExposeTimeNP.s == IPS_BUSY)
       {
	  cam->Reset();
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
    cam->write_CoolerMode(0);
    cam->write_CoolerMode(1);
    cam->write_CoolerSetPoint(targetTemp);
    
    TemperatureNP.s = IPS_BUSY;
    
    IDSetNumber(&TemperatureNP, "Setting CCD temperature to %+06.2f C", values[0]);
    IDLog("Setting CCD temperature to %+06.2f C\n", values[0]);
    return;
   }
   
   if (!strcmp(FrameNP.name, name))
   {
     if (checkPowerN(&FrameNP))
      return;
      
     FrameNP.s = IPS_OK;
     IUUpdateNumbers(&FrameNP, values, names, n);
     
     cam->m_StartX = (int) FrameN[0].value;
     cam->m_StartY = (int) FrameN[1].value;
     cam->m_NumX   = (int) FrameN[2].value;
     cam->m_NumY   = (int) FrameN[3].value;
     IDSetNumber(&FrameNP, NULL);
      
   } /* end FrameNP */
      
    
   if (!strcmp(BinningNP.name, name))
   {
     if (checkPowerN(&BinningNP))
       return;
       
     
     BinningNP.s = IPS_OK;
     IUUpdateNumbers(&BinningNP, values, names, n);
     
     cam->m_BinX = (int) BinningN[0].value;
     cam->m_BinX = (int) BinningN[0].value;
     
     IDLog("Binning is: %.0f x %.0f\n", BinningN[0].value, BinningN[1].value);
     return;
   }
}


void ApogeeCam::ISStaticPoll(void *p)
{
	if (!((ApogeeCam *)p)->isCCDConnected())
	{
	  IEAddTimer (POLLMS, ApogeeCam::ISStaticPoll, p);
	  return;
	}
	
	((ApogeeCam *) p)->ISPoll();
	
	IEAddTimer (POLLMS, ApogeeCam::ISStaticPoll, p);
}

void ApogeeCam::ISPoll()
{
  static int mtc=5;
  int readStatus=0;
  double ccdTemp;
	
  switch (ExposeTimeNP.s)
  {
    case IPS_IDLE:
    case IPS_OK:
      break;
	    
    case IPS_BUSY:
      
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
	/* grab and save image */
	grabImage();
	return;
      }
      
      ExposeTimeN[0].value --;
      IDSetNumber(&ExposeTimeNP, NULL);
      break;
	    
	case IPS_ALERT:
	    break;
	}
	 
	 switch (TemperatureNP.s)
	 {
	   case IPS_IDLE:
	   case IPS_OK:
	     mtc--;
	     
	     if (mtc == 0)
	     {
	       TemperatureN[0].value = cam->read_Temperature();
	       IDSetNumber(&TemperatureNP, NULL);
	       mtc = 5;
	     }
	     break;
	     
	   case IPS_BUSY:
	   
	     ccdTemp = cam->read_Temperature();
	     
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
void ApogeeCam::grabImage()
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
     
   img_size = APGFrame.width * APGFrame.height * sizeof(unsigned short);
  
   APGFrame.img = (unsigned short *) malloc (img_size);
   
  if (APGFrame.img == NULL)
  {
    IDMessage(mydev, "Not enough memory to store image.");
    IDLog("Not enough memory to store image.\n");
    return;
  }
  
  if (!cam->GetImage( APGFrame.img , APGFrame.width, APGFrame.height ))
  {
       free(APGFrame.img);
       IDMessage(mydev, "GetImage() failed.");
       IDLog("GetImage() failed.");
       return;
  }
  
   err = writeFITS(filename, errmsg);
   
   if (err)
   {
       free(APGFrame.img);
       IDMessage(mydev, errmsg, NULL);
       return;
   }
   
  free(APGFrame.img);
   
}

int ApogeeCam::writeFITS(char *filename, char errmsg[])
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
  
  width  = APGFrame.width;
  height = APGFrame.height;
  bpp    = sizeof(unsigned short); /* Bytes per Pixel */
  bpsl   = bpp * APGFrame.width;    /* Bytes per Line */
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
  
  /* Convert buffer to BIG endian FIXME do I need this???*/
  for (int i=0; i < height; i++)
    for (int j=0 ; j < width; j++)
      APGFrame.img[width * i + j] = getBigEndian( (APGFrame.img[width * i + j]) );
  
  for (int i= 0; i < height  ; i++)
  {
    fwrite(APGFrame.img + (i * width), 2, width, ofp->fp);
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

void ApogeeCam::uploadFile(char * filename)
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
void ApogeeCam::handleExposure(void *p)
{
  
  int curFrame = getOnSwitch(&FrameTypeSP);
  
  /* no warning */
  p=p;
  
  switch (curFrame)
  {
    /* Light frame */ 
   case FRAME_LIGHT:
   if (!cam->Expose( (int) ExposeTimeN[0].value, true ))
   {  
     ExposeTimeNP.s = IPS_IDLE;
     IDSetNumber(&ExposeTimeNP, "Light Camera exposure failed.");
     IDLog("Light Camera exposure failed.\n");
     return;
   }
   break;
   
   /* BIAS frame is the same as DARK but with minimum period. i.e. readout from camera electronics.
   */
    case FRAME_BIAS:
      if (!cam->Expose( 0.05 , false ))
      {  
	ExposeTimeNP.s = IPS_IDLE;
	IDSetNumber(&ExposeTimeNP, "Bias Camera exposure failed.");
	IDLog("Bias Camera exposure failed.\n");
	return;
      }
      break;
      
      /* Dark frame */
    case FRAME_DARK:
      if (!cam->Expose( (int) ExposeTimeN[0].value , false ))
      {  
	ExposeTimeNP.s = IPS_IDLE;
	IDSetNumber(&ExposeTimeNP, "Dark Camera exposure failed.");
	IDLog("Dark Camera exposure failed.\n");
	return;
      }
      break;
      
    case FRAME_FLAT:
      if (!cam->Expose( (int) ExposeTimeN[0].value , true ))
      {  
	ExposeTimeNP.s = IPS_IDLE;
	IDSetNumber(&ExposeTimeNP, "Flat Camera exposure failed.");
	IDLog("Flat Camera exposure failed.\n");
	return;
      }
      break;
  }
      
  APGFrame.frameType	= curFrame;
  APGFrame.width	= (int) FrameN[2].value;
  APGFrame.height	= (int) FrameN[3].value;
  APGFrame.expose	= (int) ExposeTimeN[0].value;
  
  ExposeTimeNP.s = IPS_BUSY;
		  
  IDSetNumber(&ExposeTimeNP, "Taking a %g seconds frame...", ExposeTimeN[0].value);
  IDLog("Taking a frame...\n");
   
}

/* Retrieves basic data from the CCD upon connection like temperature, array size, firmware..etc */
void ApogeeCam::getBasicData()
{


  // Maximum resolution
  FrameN[2].max = cam->m_NumX;
  FrameN[3].max = cam->m_NumY;
  IUUpdateMinMax(&FrameNP);
  
  // Maximum Bin
  BinningN[0].max = cam->m_MaxBinX;
  BinningN[1].max = cam->m_MaxBinX;
  IUUpdateMinMax(&BinningNP);
  
  // Current Temperature
  TemperatureN[0].value = cam->read_Temperature();
  IDSetNumber(&TemperatureNP, NULL);
  
}

int ApogeeCam::manageDefaults(char errmsg[])
{
  
#if 0

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

#endif
  
  /* Success */
  return 0;
    
}

int ApogeeCam::getOnSwitch(ISwitchVectorProperty *sp)
{
  int i=0;
 for (i=0; i < sp->nsp ; i++)
 {
   /*IDLog("Switch %s is %s\n", sp->sp[i].name, sp->sp[i].s == ISS_ON ? "On" : "Off");*/
     if (sp->sp[i].s == ISS_ON)
      return i;
 }

 return -1;
}

int ApogeeCam::checkPowerS(ISwitchVectorProperty *sp)
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

int ApogeeCam::checkPowerN(INumberVectorProperty *np)
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

int ApogeeCam::checkPowerT(ITextVectorProperty *tp)
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

void ApogeeCam::connectCCD()
{
  
  /* USB by default {USB, SERIAL, PARALLEL, INET} */
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
      break;
     }

}

bool ApogeeCam::initCamera()
{
  LilXML *XMLParser = newLilXML();
  XMLEle *root = NULL, *camera = NULL, *ele = NULL;
  XMLEle *system = NULL, *geometry = NULL, *temp = NULL, *ccd = NULL;
  XMLAtt *ap;
  FILE *spFile = NULL;
  char errmsg[1024];
  
  spFile = fopen(TOP_DATADIR"/apogee_caminfo.xml", "r");
  //spFile = fopen("/opt/kde3/share/apps/kstars/apogee_caminfo.xml", "r");
  if (spFile == NULL) 
  {
    IDLog("Error: Unable to open file apogee_caminfo.xml\n");
    IDMessage(mydev, "Error: Unable to open file apogee_caminfo.xml");
    return false;
  }
  
  root = readXMLFile(spFile, XMLParser, errmsg);
  if (root == NULL)
  {
    IDLog("Error: Unable to process apogee_caminfo.xml. %s\n", errmsg);
    IDMessage(mydev, "Error: Unable to process apogee_caminfo.xml. %s\n", errmsg);
    fclose(spFile);
    delLilXML(XMLParser);
    return false;
  }
  
  fclose(spFile);
  
  // Let's locate which camera to load the configuration for  
  camera = findXMLEle(root, "Apogee_Camera");
  
  if (camera == NULL)
  {
    IDLog("Error: Unable to find Apogee_Camera element.\n");
    IDMessage(mydev, "Error: Unable to find Apogee_Camera element.");
    delLilXML(XMLParser);
    return false;
  }
   
  IDLog("Looking for %s - len %d\n", ApogeeModelS[getOnSwitch(&ApogeeModelSP)].label, strlen(ApogeeModelS[getOnSwitch(&ApogeeModelSP)].label));
  
  ap = findXMLAtt(camera, "model");
  if (!ap)
  {
    IDLog("Error: Unable to find attribute model.\n");
    IDMessage(mydev, "Error: Unable to find attribute model.");
    return false;
  }
  
  if (strcmp(valuXMLAtt(ap), ApogeeModelS[getOnSwitch(&ApogeeModelSP)].label))
  {
    IDLog("Camera %s not found in XML file\n", ApogeeModelS[getOnSwitch(&ApogeeModelSP)].label);
    IDMessage(mydev, "Camera %s not found in XML file\n", ApogeeModelS[getOnSwitch(&ApogeeModelSP)].label);
    delLilXML(XMLParser);
    return false;
  }
  
  // Let's get the subsections now
  system   = findXMLEle(camera, "System");
  geometry = findXMLEle(camera, "Geometry");
  temp     = findXMLEle(camera, "Temp");
  ccd      = findXMLEle(camera, "CCD");
  
  if (system == NULL)
  {
    IDLog("Error: Unable to find System element in camera.\n");
    IDMessage(mydev, "Error: Unable to find System element in camera.");
    delLilXML(XMLParser);
    return false;
  }
  
  if (geometry == NULL)
  {
    IDLog("Error: Unable to find Geometry element in camera.\n");
    IDMessage(mydev, "Error: Unable to find Geometry element in camera.");
    delLilXML(XMLParser);
    return false;
  }
  
  if (temp == NULL)
  {
    IDLog("Error: Unable to find Temp element in camera.\n");
    IDMessage(mydev, "Error: Unable to find Temp element in camera.");
    delLilXML(XMLParser);
    return false;
  }
  
  if (ccd == NULL)
  {
    IDLog("Error: Unable to find CCD element in camera.\n");
    IDMessage(mydev, "Error: Unable to find CCD element in camera.");
    delLilXML(XMLParser);
    return false;
  }
  
  cam = new CCameraIO();
  
  if (cam == NULL)
  {
    IDLog("Error: Failed to create CCameraIO object.\n");
    IDMessage(mydev, "Error: Failed to create CCameraIO object.");
    delLilXML(XMLParser);
    return false; 
  }
  
  int bAddr = 0x378;
  int val   = 0;
  
  bAddr = hextoi(valuXMLAtt(findXMLAtt(system, "Base"))) & 0xFFF;
  
  // Rows
  ap = findXMLAtt(geometry, "Rows");
  if (!ap)
  {
    IDLog("Error: Unable to find attribute Rows.\n");
    IDMessage(mydev, "Error: Unable to find attribute Rows.");
    delLilXML(XMLParser);
    return false;
  }
  
  cam->m_Rows = hextoi(valuXMLAtt(ap));
  
  // Columns
  ap = findXMLAtt(geometry, "Columns");
  if (!ap)
  {
    IDLog("Error: Unable to find attribute Columns.\n");
    IDMessage(mydev, "Error: Unable to find attribute Columns.");
    delLilXML(XMLParser);
    return false;
  }
  
  cam->m_Columns = hextoi(valuXMLAtt(ap));
  
  // pp_repeat
  ele = findXMLEle(system, "PP_Repeat");
  if (!ele)
  {
    IDLog("Error: Unable to find element PP_Repeat.\n");
    IDMessage(mydev, "Error: Unable to find element PP_Repeat.");
    delLilXML(XMLParser);
    return false;
  }
  
  val = hextoi(pcdataXMLEle(ele));
  if (val > 0 && val <= 1000)
      cam->m_PPRepeat = val;

  // Initiate driver
  if (!cam->InitDriver(0))
  {
    IDLog("Error: Failed to Init Driver.\n");
    IDMessage(mydev, "Error: Failed to Init Driver.");
    delLilXML(XMLParser);
    return false;
  }

  cam->Reset();
  
  // Cable length
  ele = findXMLEle(system, "Cable");
  if (!ele)
  {
    IDLog("Error: Unable to find element Cable.\n");
    IDMessage(mydev, "Error: Unable to find element Cable.");
    delLilXML(XMLParser);
    return false;
 }
 
 if (!strcmp("Long", pcdataXMLEle(ele)))
 {
   cam->write_LongCable( true );
   IDLog("Cable is long\n");
 }
 else
 {
   cam->write_LongCable( false );
   IDLog("Cable is short\n");
 }
 
 
 if (!cam->read_Present())
 {
   IDLog("Error: read_Present() failed.\n");
   IDMessage(mydev, "Error: read_Present() failed.");
   delLilXML(XMLParser);
   return false;
}
 
 // Default settings
 cam->write_UseTrigger( false );
 cam->write_ForceShutterOpen( false );
 
 // High priority
 ele = findXMLEle(system, "High_Priority");
 if (ele)
 {
   if (!strcmp(pcdataXMLEle(ele), "True"))
     cam->m_HighPriority = true;
   else
     cam->m_HighPriority = false;
 }
 
 // Data bits
 ele = findXMLEle(system, "Data_Bits");
 if (ele)
 {
   val = hextoi(pcdataXMLEle(ele));
   if (val >= 8 && val <= 18) cam->m_DataBits = val;
 }
 
 // Sensor
 ele = findXMLEle(system, "Sensor");
 if (ele)
 {
   if (!strcmp(pcdataXMLEle(ele), "CCD"))
     cam->m_SensorType = Camera_SensorType_CCD;
   else
     cam->m_SensorType = Camera_SensorType_CMOS;
 }
 
 // Mode
 ele = findXMLEle(system, "Mode");
 if (ele)
 {
   val = hextoi(pcdataXMLEle(ele)) & 0xF;
   cam->write_Mode( val );
   IDLog("Mode %d\n", val);
 }
 else
   cam->write_Mode( 0 );
 
 // Test
 ele = findXMLEle(system, "Test");
 if (ele)
 {
   val = hextoi(pcdataXMLEle(ele)) & 0xF;
   cam->write_TestBits( val );
   IDLog("Test bits %d\n", val);
 }
 else
   cam->write_TestBits( 0 );
 
 // Test2
 ele = findXMLEle(system, "Test2");
 if (ele)
 {
   val = hextoi(pcdataXMLEle(ele)) & 0xF;
   cam->write_Test2Bits( val );
   IDLog("Test 2 bits %d\n", val);
 }
 else
   cam->write_Test2Bits( 0 );
 
 // Shutter Speed
 ele = findXMLEle(system, "Shutter_Speed");
 if (ele)
 {
   cam->m_MaxExposure = 10485.75;
   
   if (!strcmp(pcdataXMLEle(ele), "Normal"))
   {
     cam->m_FastShutter = false;
     cam->m_MinExposure = 0.01;
     IDLog("Shutter speed normal\n");
   }
   else if ( (!strcmp(pcdataXMLEle(ele), "Fast")) || (!strcmp(pcdataXMLEle(ele), "Dual")) )
   {
     cam->m_FastShutter = true;
     cam->m_MinExposure = 0.001;
     IDLog("Shutter speed fast\n");
   }
 }
 
 // Shutter Bits
 ele = findXMLEle(system, "Shutter_Bits");
 if (ele)
 {
   val = hextoi(pcdataXMLEle(ele));
   cam->m_FastShutterBits_Mode = val & 0x0F;
   cam->m_FastShutterBits_Test = ( val & 0xF0 ) >> 4;
   IDLog("Shutter bits %d\n", val);
 }
 
 // Max X Bin
 ele = findXMLEle(system, "MaxBinX");
 if (ele)
 {
   val = hextoi(pcdataXMLEle(ele));
   if (val >= 1 && val <= MAXHBIN) 
     cam->m_MaxBinX = val;
 }
 
 // Max Y Bin
 ele = findXMLEle(system, "MaxBinY");
 if (ele)
 {
   val = hextoi(pcdataXMLEle(ele));
   if (val >= 1 && val <= MAXVBIN) 
     cam->m_MaxBinY = val;
 }
 
 // Guider Relays
 ele = findXMLEle(system, "Guider_Relays");
 if (ele)
 {
   if (!strcmp(pcdataXMLEle(ele), "True"))
     cam->m_GuiderRelays = true;
   else
     cam->m_GuiderRelays = false;
 }
 
 // Timeout
 ele = findXMLEle(system, "Timeout");
 if (ele)
 {
   double dval = atof(pcdataXMLEle(ele));
   if (dval >= 0.0 && dval <= 10000.0) cam->m_Timeout = dval;
 }
 
 // BIC
  ele = findXMLEle(geometry, "BIC");
  if (ele)
  {
    val = hextoi(pcdataXMLEle(ele));
    if (val >= 1 && val <= MAXCOLUMNS)
      cam->m_BIC = val;
  }
  
   // BIR
  ele = findXMLEle(geometry, "BIR");
  if (ele)
  {
    val = hextoi(pcdataXMLEle(ele));
    if (val >= 1 && val <= MAXROWS)
      cam->m_BIR = val;
  }
  
  // SKIPC
  ele = findXMLEle(geometry, "SKIPC");
  if (ele)
  {
    val = hextoi(pcdataXMLEle(ele));
    if (val >= 1 && val <= MAXCOLUMNS)
      cam->m_SkipC = val;
  }
  
  // SKIPR
  ele = findXMLEle(geometry, "SKIPR");
  if (ele)
  {
    val = hextoi(pcdataXMLEle(ele));
    if (val >= 1 && val <= MAXROWS)
      cam->m_SkipR = val;
  }
  
  // IMG COlS
  ele = findXMLEle(geometry, "ImgCols");
  if (ele)
  {
    val = hextoi(pcdataXMLEle(ele));
    if (val >= 1 && val <= MAXTOTALCOLUMNS)
      cam->m_ImgColumns = val;
  }
  else
    cam->m_ImgColumns = cam->m_Columns - cam->m_BIC - cam->m_SkipC;
  
  // IMG ROWS
  ele = findXMLEle(geometry, "ImgRows");
  if (ele)
  {
    val = hextoi(pcdataXMLEle(ele));
    if (val >= 1 && val <= MAXTOTALROWS)
      cam->m_ImgRows = val;
  }
  else
    cam->m_ImgRows = cam->m_Rows - cam->m_BIR - cam->m_SkipR;
  
  // Hor Flush
  ele = findXMLEle(geometry, "HFlush");
  if (ele)
  {
    val = hextoi(pcdataXMLEle(ele));
    if (val >= 1 && val <= MAXHBIN) 
      cam->m_HFlush = val;
  }
  
  // Ver Flush
  ele = findXMLEle(geometry, "VFlush");
  if (ele)
  {
    val = hextoi(pcdataXMLEle(ele));
    if (val >= 1 && val <= MAXVBIN) 
      cam->m_VFlush = val;
  }

  // Default to full frame
  cam->m_NumX = cam->m_ImgColumns;
  cam->m_NumY = cam->m_ImgRows;
  
  // Temp Control 
  ap = findXMLAtt(temp, "Control");
  if (ap)
  {
    if (!strcmp(valuXMLAtt(ap), "True"))
      cam->m_TempControl = true;
    else
      cam->m_TempControl = false;
  }
  
  // Calibration
  ap = findXMLAtt(temp, "Cal");
  if (ap)
  {
    val = hextoi(valuXMLAtt(ap));
    if (val >= 1 && val <= 255) 
      cam->m_TempCalibration = val;
  }
  
  // Scale
  ap = findXMLAtt(temp, "Scale");
  if (ap)
  {
    double dval = atof(valuXMLAtt(ap));
    if (dval >= 1.0 && dval <= 10.0) 
      cam->m_TempScale = dval;
  }
  
  // Target
  ap = findXMLAtt(temp, "Target");
  if (ap)
  {
    double dval = atof(valuXMLAtt(ap));
    if (dval >= -60.0 && dval <= 40.0) 
      cam->write_CoolerSetPoint( dval );
    else
      cam->write_CoolerSetPoint( -10.0 );		// Default   
    
    IDLog("Target: %g\n", dval);
  }
  
  // Sensor
  ap = findXMLAtt(ccd, "Sensor");
  if (ap)
  {
    strncpy (cam->m_Sensor, valuXMLAtt(ap), 255);
    IDLog("Sensor: %s\n", cam->m_Sensor);
  }
  
  // Color
  ele = findXMLEle(ccd, "Color");
  if (ele)
  {
    if (!strcmp(pcdataXMLEle(ele), "True"))
    {
      cam->m_Color = true;
      IDLog("Color: true\n");
    }
    else
    { 
      cam->m_Color = false;
      IDLog("Color: false\n");
    }
  }
  
  // Noise
  ele = findXMLEle(ccd, "Noise");
  if (ele)
    cam->m_Noise = atof( pcdataXMLEle(ele) );
  
  // Noise
  ele = findXMLEle(ccd, "Gain");
  if (ele)
    cam->m_Gain = atof( pcdataXMLEle(ele) );
  
  // Pixel X Size
  ele = findXMLEle(ccd, "PixelXSize");
  if (ele)
  {
    cam->m_PixelXSize = atof( pcdataXMLEle(ele) );
    IDLog("Pixel X Size: %g\n", cam->m_PixelXSize);
  }
  
  // Pixel Y Size
  ele = findXMLEle(ccd, "PixelYSize");
  if (ele)
  {
    cam->m_PixelYSize = atof( pcdataXMLEle(ele) );
    IDLog("Pixel Y Size: %g\n", cam->m_PixelYSize);
  }
  
  // Log all values
  IDLog("Cam Row: %d - Cam Cols: %d - PP_Repeat %d\n",cam->m_Rows,  cam->m_Columns, cam->m_PPRepeat);
  IDLog("High_Priority %s - Data_Bits %d - Sensor %s\n", cam->m_HighPriority ? "true" : "false", cam->m_DataBits, (cam->m_SensorType == Camera_SensorType_CCD) ? "CCD" : "CMOS");
  IDLog("Max X Bin: %d - Max Y Bin: %d - Guider Relays: %s\n", cam->m_MaxBinX, cam->m_MaxBinY, cam->m_GuiderRelays ? "true" : "false");
  IDLog("BIC: %d - BIR: %d - SKIPC: %d - SKIPR: %d - ImgRows: %d - ImgCols %d\n", cam->m_BIC, cam->m_BIR, cam->m_SkipC, cam->m_SkipR, cam->m_ImgRows, cam->m_ImgColumns);
  IDLog("HFlush: %d - VFlush: %d - Control: %s - Cal: %d - Scale: %g\n", cam->m_HFlush, cam->m_VFlush, cam->m_TempControl ? "true" : "false", cam->m_TempCalibration, cam->m_TempScale); 
  
  delLilXML(XMLParser);
  
  return true;
}

/* isCCDConnected: return 1 if we have a connection, 0 otherwise */
int ApogeeCam::isCCDConnected(void)
{
  return ((PowerS[0].s == ISS_ON) ? 1 : 0);
}

FITS_HDU_LIST * ApogeeCam::create_fits_header (FITS_FILE *ofp, uint width, uint height, uint bpp)
{
 
 FITS_HDU_LIST *hdulist;
 
#if 0

 char temp_s[80], expose_s[80], binning_s[80], pixel_s[80], frame_s[80];
 char obsDate[80];
 char ts[32];
 struct tm *tp;
 time_t t;
 
 time (&t);
 tp = gmtime (&t);
 strftime (ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tp);
 
 snprintf(obsDate, 80, "DATE-OBS= '%s' /Observation Date UTC", ts);
 
 hdulist = fits_add_hdu (ofp);
 if (hdulist == NULL) return (NULL);

 hdulist->used.simple = 1;
 hdulist->bitpix = 16;/*sizeof(unsigned short) * 8;*/
 hdulist->naxis = 2;
 hdulist->naxisn[0] = width;
 hdulist->naxisn[1] = height;
 hdulist->naxisn[2] = bpp;
 hdulist->used.datamin = min();
 hdulist->datamin = min();
 hdulist->used.datamax = max();
 hdulist->datamax = max();
 hdulist->used.bzero = 1;
 hdulist->bzero = 0.0;
 hdulist->used.bscale = 1;
 hdulist->bscale = 1.0;
 
 sprintf(temp_s, "CCD-TEMP= %g / degrees celcius", TemperatureN[0].value);
 sprintf(expose_s, "EXPOSURE= %d / milliseconds", FLIImg->expose);
 sprintf(binning_s, "BINNING = '(%g x %g)'", BinningN[0].value, BinningN[1].value);
 sprintf(pixel_s, "PIX-SIZ = '%.0f microns square'", PixelSizeN[0].value);
 switch (FLIImg->frameType)
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
 fits_add_card (hdulist, pixel_s);
 fits_add_card (hdulist, "INSTRUME= 'Finger Lakes Instruments'");
 fits_add_card (hdulist, obsDate);
  
#endif

 return (hdulist);
}

double min()
{
#if 0 
  double lmin = FLIImg->img[0];
  int ind=0, i, j;
 

  for (i= 0; i < FLIImg->height ; i++)
    for (j= 0; j < FLIImg->width; j++)
    {
       ind = (i * FLIImg->width) + j;
       if (FLIImg->img[ind] < lmin) lmin = FLIImg->img[ind];
    }
    

    return lmin;
#endif

return 0;
}

double max()
{
#if 0

  double lmax = FLIImg->img[0];
  int ind=0, i, j;
  


   for (i= 0; i < FLIImg->height ; i++)
    for (j= 0; j < FLIImg->width; j++)
    {
      ind = (i * FLIImg->width) + j;
       if (FLIImg->img[ind] > lmax) lmax = FLIImg->img[ind];
    }
 


 return lmax;
#endif

return 0;
 
}

// Convert a string to a decimal or hexadecimal integer
// Code taken from Dave Mills Apogee driver
unsigned short ApogeeCam::hextoi(char *instr)
{
  unsigned short val, tot = 0;
  bool IsHEX = false;

  long n = strlen( instr );
  if ( n > 1 )
  {	// Look for hex format e.g. 8Fh, A3H or 0x5D
    if ( instr[ n - 1 ] == 'h' || instr[ n - 1 ] == 'H' )
      IsHEX = true;
    else if ( *instr == '0' && *(instr+1) == 'x' )
    {
      IsHEX = true;
      instr += 2;
    }
  }

  if ( IsHEX )
  {
    while (instr && *instr && isxdigit(*instr))
    {
      val = *instr++ - '0';
      if (9 < val)
	val -= 7;
      tot <<= 4;
      tot |= (val & 0x0f);
    }
  }
  else
    tot = atoi( instr );

  return tot;
}
