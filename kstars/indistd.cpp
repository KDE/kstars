/*  INDI STD
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This apppication is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    2004-01-18: This class handles INDI Standard properties.
 */
 
 #include "indistd.h"
 #include "Options.h"
 #include "indielement.h"
 #include "indiproperty.h"
 #include "indigroup.h"
 #include "indidevice.h"
 #include "indidriver.h"
 #include "kstars.h"
 #include "kstarsdata.h"
 #include "skymap.h"
 #include "skyobject.h"
 #include "simclock.h"
 #include "devicemanager.h"
 #include "timedialog.h"
 #include "streamwg.h"
 #include "fitsviewer.h"
 #include "indi/lzo/minilzo.h"
 
 #include <sys/socket.h>
 #include <netinet/in.h>
 #include <arpa/inet.h>
 #include <netdb.h>

 #include <qtimer.h>
 #include <qlabel.h>
 #include <qfont.h>
 #include <qeventloop.h>
 #include <qsocketnotifier.h>
 
 #include <klocale.h>
 #include <kdebug.h>
 #include <kpushbutton.h>
 #include <klineedit.h>
 #include <kstatusbar.h>
 #include <kmessagebox.h>
 #include <kapplication.h>
 #include <kprogress.h>
 #include <kurl.h>
 
 #define STD_BUFFER_SIZ		1024000
 #define FRAME_ILEN		64
 
 INDIStdDevice::INDIStdDevice(INDI_D *associatedDevice, KStars * kswPtr)
 {
 
   dp   = associatedDevice;
   ksw  = kswPtr;
   initDevCounter = 0;
   totalCompressedBytes  = totalBytes = 0;
   streamFD = -1;
   streamBuffer     = (unsigned char *) malloc (sizeof(unsigned char) * 1);
   compressedBuffer = (unsigned char *) malloc (sizeof(unsigned char) * 1);
   
   streamWindow   = new StreamWG(this, ksw);
   currentObject  = NULL; 
   devTimer = new QTimer(this);
   QObject::connect( devTimer, SIGNAL(timeout()), this, SLOT(timerDone()) );
   
   downloadDialog = new KProgressDialog(NULL, 0, i18n("INDI"), i18n("Downloading Data..."));
   downloadDialog->cancel();
 }
 
 INDIStdDevice::~INDIStdDevice()
 {
   free(streamBuffer);
   free(compressedBuffer);
 }
 
void INDIStdDevice::establishDataChannel(QString host, int port)
{
        QString errMsg;
	struct sockaddr_in pin;
	struct hostent *serverHostName = gethostbyname(host.ascii());
	errMsg = QString("Connection to INDI host at %1 on port %2 failed.").arg(host).arg(port);
	
	memset(&pin, 0, sizeof(pin));
	pin.sin_family 		= AF_INET;
	pin.sin_addr.s_addr 	= ((struct in_addr *) (serverHostName->h_addr))->s_addr;
	pin.sin_port 		= htons(port);

	if ( (streamFD = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
	 KMessageBox::error(0, i18n("Cannot create socket."));
	 return;
	}

	if ( ::connect(streamFD, (struct sockaddr*) &pin, sizeof(pin)) == -1)
	{
	  KMessageBox::error(0, errMsg);
	  streamFD = -1;
	  return;
	}

	if (lzo_init() != LZO_E_OK)
           kdDebug() << "lzo_init() failed !!!" << endl;
	   
	// callback notified
	sNotifier = new QSocketNotifier( streamFD, QSocketNotifier::Read, this);
        QObject::connect( sNotifier, SIGNAL(activated(int)), this, SLOT(streamReceived()));
}

void INDIStdDevice::allocateCompressedBuffer()
{
  //delete (compressedBuffer);
  
  //kdDebug() << "new compress size " << totalCompressedBytes << endl;
  
  compressedBuffer = (unsigned char *) realloc (compressedBuffer, sizeof(unsigned char) * totalCompressedBytes);
  
  if (compressedBuffer == NULL)
   kdDebug() << "Low memory! Failed to initialize compressed buffer." << endl;
  
}

void INDIStdDevice::allocateStreamBuffer()
{

  //kdDebug() << "new FULL size " << totalBytes << endl;
  
  streamBuffer = (unsigned char *) realloc (streamBuffer, sizeof(unsigned char) * totalBytes);
  
  if (streamBuffer == NULL)
   kdDebug() << "Low memory! Failed to initialize compressed buffer." << endl;

}



void INDIStdDevice::streamReceived()
{

   char msg[1024];
   char frameSize[FRAME_ILEN];
   int nr=0, n=0, r;
   unsigned int newCompressSize, newFrameSize, fullFrameSize;
   
   /* #1 Read first the frame size in bytes */
   for (int i=0; i < FRAME_ILEN; i+=n)
   {
     n = read (streamFD, frameSize + i, 1);
     if (n <= 0)
	   {
	    	if (n < 0)
			sprintf (msg, "INDI: input error.");
	    	else
			sprintf (msg, "INDI: agent closed connection.");

	    sNotifier->disconnect();
	    close(streamFD);
	    streamWindow->close();
	    KMessageBox::error(0, QString(msg));
            return;
	   }
	   
    /* A. Check for data type */
    if (frameSize[i] == ';')
    {
      frameSize[i] = '\0';
      if (!strcmp(frameSize, "VIDEO"))
       		dataType = DATA_STREAM;
      else if (!strcmp(frameSize, "FITS"))
      		dataType = DATA_FITS;
      else   
      { 
      		dataType = DATA_OTHER;
		dataExt  = QString(frameSize);
      }
      
      i = n = 0;
    }
    /* B. Check for full uncompressed frame size */
    if (frameSize[i] == ',')
    {
      frameSize[i] = '\0';
      fullFrameSize = atoi(frameSize);
      if (fullFrameSize != totalBytes)
      {
        totalBytes = fullFrameSize;
	allocateStreamBuffer();
	if (streamBuffer == NULL) return;
      }
      i = n = 0;
    }
    /* C. Check for compressed frame size */
    else if (frameSize[i] == ']')
    {
       frameSize[i] = '\0';
       newCompressSize = atoi(frameSize);
       if (newCompressSize != totalCompressedBytes)
       {
         totalCompressedBytes = newCompressSize;
	 allocateCompressedBuffer();
	 if (compressedBuffer == NULL) return;
       }
       
       if (dataType == DATA_FITS || dataType == DATA_OTHER)
       {
         downloadDialog->progressBar()->setTotalSteps(totalCompressedBytes);
	 downloadDialog->setMinimumWidth(300);
	 downloadDialog->show();
       }
       //kdDebug() << "*** Uncompressed size is " << totalBytes << " and compressed is " << totalCompressedBytes << endl;
       break;
    }
   }
   
    //kdDebug() << "Will read actual frame now ... " << endl;
   
    /* #2 Read actual frame */    
    for (nr = 0; nr < totalCompressedBytes; nr+=n)
    {
           n = read (streamFD, compressedBuffer + nr, totalCompressedBytes - nr);
	   if (n <= 0)
	   {
	    	if (n < 0)
			sprintf (msg, "INDI: input error.");
	    	else
			sprintf (msg, "INDI: agent closed connection.");

	    sNotifier->disconnect();
	    close(streamFD);
	    streamWindow->close();
	    KMessageBox::error(0, QString(msg));
            return;
	   }
	   
	   if (dataType == DATA_FITS || dataType == DATA_OTHER)
	   {
	       downloadDialog->progressBar()->setProgress(nr);
	       kapp->eventLoop()->processEvents(QEventLoop::ExcludeSocketNotifiers);
	   }
    }
    
     //kdDebug() << "We're done reading .... " << endl;
    
    downloadDialog->cancel();
    
    r = lzo1x_decompress(compressedBuffer,totalCompressedBytes, streamBuffer , &newFrameSize,NULL);
    
    //kdDebug() << "Finished decompress with r = " << r << endl;
    
    if (r != LZO_E_OK)
      //kdDebug() << "decompressed " << totalCompressedBytes << " bytes back into " << newFrameSize << " bytes" << endl;
     //else
     {
        /* this should NEVER happen */
	kdDebug() << "internal error - decompression failed: " << r << endl;
	return;
     }
	
     if (dataType == DATA_STREAM)
     {
       if (!streamWindow->processStream)
	  return;
	
	streamWindow->show();
	
	streamWindow->streamFrame->newFrame(streamBuffer, streamWindow->streamWidth, streamWindow->streamHeight, streamWindow->colorFrame);
     }
     else if (dataType == DATA_FITS || dataType == DATA_OTHER)
     {
       char filename[256];
       FILE *fitsTempFile;
       int fd, nr, n=0;
       QString currentDir = Options::fitsSaveDirectory();
       
       //streamWindow->enableStream(false);
       streamWindow->close();
        
	if (dataType == DATA_FITS)
        {
	  strcpy(filename, "/tmp/fitsXXXXXX");
	  if ((fd = mkstemp(filename)) < 0)
          { 
            KMessageBox::error(NULL, "Error making temporary filename.");
            return;
          }
          close(fd);
	}
	else
        {
	  char ts[32];
	  struct tm *tp;
          time_t t;
          time (&t);
          tp = gmtime (&t);
	  
	  if (currentDir[currentDir.length() -1] == '/')
	    currentDir.truncate(currentDir.length() - 1);
	  
	  strncpy(filename, currentDir.ascii(), currentDir.length());
	  filename[currentDir.length()] = '\0';
          strftime (ts, sizeof(ts), "/file-%Y-%m-%dT%H:%M:%S.", tp);
	  strncat(filename, ts, sizeof(ts));
	  strncat(filename, dataExt.ascii(), 3);
        }
	  
       fitsTempFile = fopen(filename, "w");
       
       if (fitsTempFile == NULL) return;
       
       for (nr=0; nr < newFrameSize; nr += n)
           n = fwrite(streamBuffer + nr, 1, newFrameSize - nr, fitsTempFile);
       
       fclose(fitsTempFile);
       
       // We're done if we have DATA_OTHER
       if (dataType == DATA_OTHER)
       {
         ksw->statusBar()->changeItem( i18n("Data file saved to %1").arg(filename), 0);
         return;
       }
       
       KURL fileURL(filename);
       
       FITSViewer * fv = new FITSViewer(&fileURL, ksw);
       fv->fitsChange();
       fv->show();
       
    }
       

}

 void INDIStdDevice::setTextValue(INDI_P *pp)
 {
   INDI_E *el;
   int wd, ht;
   
  switch (pp->stdID)
  {
    case EQUATORIAL_COORD:
      ksw->map()->forceUpdateNow();
      break;
    
    case TIME:
      if ( Options::indiAutoTime() )
       handleDevCounter();
      break;
      
    case SDTIME:
      if ( Options::indiAutoTime())
       handleDevCounter();
      break;
    
    case GEOGRAPHIC_COORD:
      if ( Options::indiAutoGeo() )
       handleDevCounter();
      break;
      
    case EXPOSE_DURATION:
       if (pp->state == PS_IDLE || pp->state == PS_OK)
         pp->set_w->setText(i18n("Start"));
       break;
       
    case IMAGE_SIZE:
         el = pp->findElement("WIDTH");
	 if (!el) return;
	 wd = (int) el->value;
	 el = pp->findElement("HEIGHT");
	 if (!el) return;
	 ht = (int) el->value;
	 
	 streamWindow->setSize(wd, ht);
	 //streamWindow->allocateStreamBuffer();
	 break;
	 
      case DATA_CHANNEL:
         el = pp->findElement("CHANNEL");
	 /*if (el && el->value && streamWindow->streamFD == -1)
	    streamWindow->establishDataChannel(dp->parentMgr->host, (int) el->value);*/
	    if (el && el->value && streamFD == -1)
	      establishDataChannel(dp->parentMgr->host, (int) el->value);
         break;
     
    default:
        break;
	
  }
 
 }

 void INDIStdDevice::setLabelState(INDI_P *pp)
 {
    INDI_E *lp;
    QFont buttonFont;
    
    switch (pp->stdID)
    {
      case CONNECTION:
      lp = pp->findElement("CONNECT");
      if (!lp) return;
      
      if (lp->state == PS_ON)
      {
        initDeviceOptions();
	emit linkAccepted();
      }
      else
      {
        if (streamWindow)
	{
	  streamWindow->enableStream(false);
	  streamWindow->close();
	}
       	ksw->map()->forceUpdateNow();
        emit linkRejected();
      }
      break;
      
      case VIDEO_STREAM:
       if (streamFD == -1)
       {
          pp->state = PS_OFF;
	  pp->drawLt(pp->state);
	  lp = pp->findElement("ON");
	  if (!lp) return;
	  lp->state = PS_OFF;
	  lp->push_w->setDown(lp->state == PS_ON ? true : false);
	  buttonFont = lp->push_w->font();
	  buttonFont.setBold(lp->state == PS_ON ? TRUE : FALSE);
	  lp->push_w->setFont(buttonFont);
	  lp = pp->findElement("OFF");
	  if (!lp) return;
	  lp->state = PS_ON;
	  lp->push_w->setDown(lp->state == PS_ON ? true : false);
	  buttonFont = lp->push_w->font();
	  buttonFont.setBold(lp->state == PS_ON ? TRUE : FALSE);
	  lp->push_w->setFont(buttonFont);
	  break;
       }
	
       lp = pp->findElement("ON");
       if (!lp) return;
       if (lp->state == PS_ON)
          streamWindow->enableStream(true);
       else
          streamWindow->enableStream(false);
       break;
       
    case IMAGE_TYPE:
      lp = pp->findElement("COLOR");
      if (!lp) return;
      if (lp->state == PS_ON)
        streamWindow->setColorFrame(true);
      else
        streamWindow->setColorFrame(false);
	
       // streamWindow->allocateStreamBuffer();
      break;
      
    default:
      break;
    }
 
 }
 
 void INDIStdDevice::streamDisabled()
 {
    INDI_P *pp;
    
    pp = dp->findProp("CONNECTION");
    if (!pp) return;
    if (pp->state == PS_OFF) return;
    
    pp = dp->findProp("VIDEO_STREAM");
    if (!pp) return;
    
    // Turn stream off
    pp->newSwitch(1); 
   
}
 
 void INDIStdDevice::updateTime()
{
  INDI_P *pp;
  INDI_E *lp;
  
  pp = dp->findProp("TIME");
  if (!pp) return;
  
  lp = pp->findElement("UTC");
  
  if (!lp) return;
  
  QTime newTime( ksw->data()->ut().time());
  ExtDate newDate( ksw->data()->ut().date());

  lp->write_w->setText(QString("%1-%2-%3T%4:%5:%6").arg(newDate.year()).arg(newDate.month())
					.arg(newDate.day()).arg(newTime.hour())
					.arg(newTime.minute()).arg(newTime.second()));
  pp->newText();
  
  pp  = dp->findProp("SDTIME");
  if (!pp) return;
  lp = pp->findElement("LST");
  if (!lp) return;
   
  lp->write_w->setText(ksw->LST()->toHMSString());
  pp->newText();
}

void INDIStdDevice::updateLocation()
{
   INDI_P *pp;
   INDI_E * latEle, * longEle;
   GeoLocation *geo = ksw->geo();

   pp = dp->findProp("GEOGRAPHIC_COORD");
   if (!pp) return;
    
  dms tempLong (geo->lng()->degree(), geo->lng()->arcmin(), geo->lng()->arcsec());
  dms fullCir(360,0,0);

  if (tempLong.degree() < 0)
     tempLong.setD ( fullCir.Degrees() + tempLong.Degrees());

    latEle  = pp->findElement("LAT");
    if (!latEle) return;
    longEle = pp->findElement("LONG");
    if (!longEle) return;
    
    longEle->write_w->setText(QString("%1:%2:%3").arg(tempLong.degree()).arg(tempLong.arcmin()).arg(tempLong.arcsec()));
    latEle->write_w->setText(QString("%1:%2:%3").arg(geo->lat()->degree()).arg(geo->lat()->arcmin()).arg(geo->lat()->arcsec()));
    
    pp->newText();
}

 
 void INDIStdDevice::registerProperty(INDI_P *pp)
 {
   INDI_E * portEle;
   INDIDriver *drivers = ksw->getINDIDriver();
   QString str;
 
   switch (pp->stdID)
   {
     case DEVICE_PORT:
     portEle = pp->findElement("PORT");
     if (!portEle) return;
     
     if (drivers)
     {
       for (unsigned int i=0; i < drivers->devices.size(); i++)
       {
         if (drivers->devices[i]->mgrID == dp->parentMgr->mgrID)
	 {
	        if (drivers->devices[i]->deviceType == KSTARS_TELESCOPE)
		{
     		   portEle->read_w->setText( Options::indiTelescopePort() );
     		   portEle->write_w->setText( Options::indiTelescopePort() );
     		   portEle->text = Options::indiTelescopePort();
		   break;
		}
		else if (drivers->devices[i]->deviceType == KSTARS_VIDEO)
		{
		   portEle->read_w->setText( Options::indiVideoPort() );
     		   portEle->write_w->setText( Options::indiVideoPort() );
     		   portEle->text = Options::indiVideoPort();
		   break;
		}
	}
      }     
     }
     break;
     
   }
  
}
 
void INDIStdDevice::initDeviceOptions()
{

  INDI_P *prop;

  initDevCounter = 0;

  if ( Options::indiAutoTime() )
  {
  	prop = dp->findProp("TIME");
  	  if (prop)
	  {
            updateTime();
	    initDevCounter += 5;
	  }
  }

  if ( Options::indiAutoGeo() )
  {
  	prop = dp->findProp("GEOGRAPHIC_COORD");
   	if (prop)
	{
   		updateLocation();
         	initDevCounter += 2;
	}
  }

  if ( Options::indiMessages() )
    ksw->statusBar()->changeItem( i18n("%1 is online.").arg(dp->name), 0);

  ksw->map()->forceUpdateNow();
}

 void INDIStdDevice::handleDevCounter()
{

  if (initDevCounter <= 0)
   return;

  initDevCounter--;

  if ( initDevCounter == 0 && Options::indiMessages() )
    ksw->statusBar()->changeItem( i18n("%1 is online and ready.").arg(dp->name), 0);

}

bool INDIStdDevice::handleNonSidereal(SkyObject *o)
{
   if (!o)
      return false;

    int trackIndex=0;

    kdDebug() << "Object of type " << o->typeName() << endl;
  //TODO Meade claims that the library access is available to
  // all telescopes, which is unture. Only classic meade support
  // that. They claim that library funcion will be available to all
  // in "later" firmware revisions for the autostar and GPS.
  // As a temprory solution, I'm going to explicity check for the
  // device name which ideally I'm not supposed to do since all properties
  // should be defined from the INDI driver, but since the INDI autostar
  // and gps define the library functions (based on Meade's future claims)
  // I will check the device name until Meade's respondes to my query.

  // Update: Solution
  // Only Meade Classic will offer an explicit SOLAR_SYSTEM property. If such a property exists
  // then we take advantage of it. Otherwise, we send RA/DEC to the telescope and start a timer
  // based on the object type. Objects with high proper motions will require faster updates.
  // handle Non Sideral is ONLY called when tracking an object, not slewing.


  INDI_P *prop = dp->findProp(QString("SOLAR_SYSTEM"));
  INDI_P *setMode = dp->findProp(QString("ON_COORD_SET"));

  // If the device support it
  if (prop && setMode)
  {
    for (unsigned int i=0; i < setMode->el.count(); i++)
      if (setMode->el.at(i)->name == "TRACK")
      { trackIndex = i; break; }

    kdDebug() << "Device supports SOLAR_SYSTEM property" << endl;

    for (unsigned int i=0; i < prop->el.count(); i++)
     if (o->name().lower() == prop->el.at(i)->label.lower())
     {
       prop->newSwitch(i);
       setMode->newSwitch(trackIndex);
       return true;
     }
  }

   kdDebug() << "Device doesn't support SOLAR_SYSTEM property, issuing a timer" << endl;
   kdDebug() << "Starting timer for object of type " << o->typeName() << endl;
   currentObject = o;

   switch (currentObject->type())
   {
    // Planet/Moon
    case 2:
       kdDebug() << "Initiating pulse tracking for " << currentObject->name() << endl;
       devTimer->start(INDI_PULSE_TRACKING);
       break;
    // Comet/Asteroid
    case 9:
    case 10:
      kdDebug() << "Initiating pulse tracking for " << currentObject->name() << endl;
      devTimer->start(INDI_PULSE_TRACKING);
      break;
    default:
      break;
    }

   return false;
}


void INDIStdDevice::timerDone()
{
      INDI_P *prop;
      INDI_E *RAEle, *DecEle;
      INDI_E *el;

       if (!dp->isOn())
       {
        devTimer->stop();
	return;
       }

       prop = dp->findProp("ON_COORD_SET");
       if (prop == NULL || !currentObject)
        return;

       el   = prop->findElement("TRACK");
       if (!el) return;
       
       if (el->state != PS_ON)
       {
        devTimer->stop();
        return;
       }
	
       prop = dp->findProp("EQUATORIAL_COORD");
       if (prop == NULL || !currentObject)
        return;

       // wait until slew is done
       if (prop->state == PS_BUSY)
        return;

       kdDebug() << "Timer called, starting processing" << endl;

	SkyPoint sp(currentObject->ra(), currentObject->dec());
	
	kdDebug() << "RA: " << currentObject->ra()->toHMSString() << " - DEC: " << currentObject->dec()->toDMSString() << endl;
	kdDebug() << "Az: " << currentObject->az()->toHMSString() << " - Alt " << currentObject->alt()->toDMSString() << endl;

        sp.apparentCoord( ksw->data()->ut().djd() , (long double) J2000);

       // We need to get from JNow (Skypoint) to J2000
       // The ra0() of a skyPoint is the same as its JNow ra() without this process

       // Use J2000 coordinate as required by INDI
       RAEle  = prop->findElement("RA");
       if (!RAEle) return;
       DecEle = prop->findElement("DEC");
       if (!DecEle) return;
     
       RAEle->write_w->setText(QString("%1:%2:%3").arg(sp.ra()->hour())
        						 .arg(sp.ra()->minute())
							 .arg(sp.ra()->second()));
        DecEle->write_w->setText(QString("%1:%2:%3").arg(sp.dec()->degree())
                                                         .arg(sp.dec()->arcmin())
							 .arg(sp.dec()->arcsec()));
	prop->newText();

}

INDIStdProperty::INDIStdProperty(INDI_P *associatedProperty, KStars * kswPtr, INDIStdDevice *stdDevPtr)
{
   pp     = associatedProperty;
   ksw    = kswPtr;
   stdDev = stdDevPtr;
}
 
 INDIStdProperty::~INDIStdProperty() 
 {
 
 }
 
 void INDIStdProperty::newText()
 {
   INDI_E *lp;
   INDIDriver *drivers = ksw->getINDIDriver();
   
   switch (pp->stdID)
   {
     /* Set expose duration button to 'cancel' when busy */
     case EXPOSE_DURATION:
       pp->set_w->setText(i18n("Cancel"));
       break;
     
      /* Save Port name in KStars options */
     case DEVICE_PORT:
        lp = pp->findElement("PORT");
	
        if (lp && drivers) 
	{
          for (unsigned int i=0; i < drivers->devices.size(); i++)
          {
              if (drivers->devices[i]->mgrID == stdDev->dp->parentMgr->mgrID)
	      {
	        if (drivers->devices[i]->deviceType == KSTARS_TELESCOPE)
		{
		   Options::setIndiTelescopePort( lp->text );
		   kdDebug() << "Setting telescope port " << lp->text << endl;
		}
		else if (drivers->devices[i]->deviceType == KSTARS_VIDEO)
		{
		   Options::setIndiVideoPort( lp->text );
		   kdDebug() << "Setting video port " << lp->text << endl;
		}
		break;
	      }
	  }
        }
     
	break;
   }
 
 }
 
 bool INDIStdProperty::convertSwitch(int switchIndex, INDI_E *lp)
 {
 
  INDI_E *RAEle, *DecEle;
  INDI_P * prop;
  SkyPoint sp;
  
  switch (pp->stdID)
  {
    /* Handle Slew/Track/Sync */
    case ON_COORD_SET:
    	// #1 set current object to NULL
    	stdDev->currentObject = NULL;
   	// #2 Deactivate timer if present
   	if (stdDev->devTimer->isActive())
		 	stdDev->devTimer->stop();

   	prop = pp->pg->dp->findProp("EQUATORIAL_COORD");
       		if (prop == NULL)
        		return false;

   	RAEle  = prop->findElement("RA");
   	if (!RAEle) return false;
   	DecEle = prop->findElement("DEC");
   	if (!DecEle) return false;
   
  	// Track is a special case, if object is sidereal and support it, then let 
	// the telescope track it, otherwise track the telescope manually via a timer.
   	if ((lp->name == "TRACK"))
       		if (stdDev->handleNonSidereal(ksw->map()->clickedObject()))
	        	return true;

        fprintf(stderr, "\n******** The point BEFORE it was precessed ********\n");
	fprintf(stderr, "RA : %s - DEC : %s\n", ksw->map()->clickedPoint()->ra()->toHMSString().ascii(),
	ksw->map()->clickedPoint()->dec()->toDMSString().ascii());

       // We need to get from JNow (Skypoint) to J2000
       // The ra0() of a skyPoint is the same as its JNow ra() without this process
       if (stdDev->currentObject)
         sp.set (stdDev->currentObject->ra(), stdDev->currentObject->dec());
       else
         sp.set (ksw->map()->clickedPoint()->ra(), ksw->map()->clickedPoint()->dec());

         sp.apparentCoord( ksw->data()->ut().djd() , (long double) J2000);

           // Use J2000 coordinate as required by INDI
    	   RAEle->write_w->setText(QString("%1:%2:%3").arg(sp.ra()->hour()).arg(sp.ra()->minute()).arg(sp.ra()->second()));
	   DecEle->write_w->setText(QString("%1:%2:%3").arg(sp.dec()->degree()).arg(sp.dec()->arcmin()).arg(sp.dec()->arcsec()));

           pp->newSwitch(switchIndex);
	   prop->newText();
	
	return true;
	break;
   
   /* Handle Abort */
   case ABORT_MOTION:
         fprintf(stderr, "Stopping timer.\n");
	 stdDev->devTimer->stop();
 	 pp->newSwitch(switchIndex);
	 return true;
	 break;
	 
   case MOVEMENT:
      pp->newSwitch(switchIndex);
      break;
   
   case IMAGE_TYPE:
     pp->newSwitch(switchIndex);
     break;
      
   default:
         return false;
	 break;
   }
  
  return false;
 
 }
 
// Return true if the complete operation is done here, or false if the operation is to be completed in the properties newSwitch()
bool INDIStdProperty::newSwitch(int id, INDI_E* el)
{
  INDI_P *prop;
  id=id; el=el;

  switch (pp->stdID)
  {
    case CONNECTION:      
      prop = pp->pg->dp->findProp("DEVICE_PORT");
      if (prop)
      prop->newText();
      break;
    case ABORT_MOTION:
    case PARK:
    case MOVEMENT:
       //TODO add text in the status bar "Slew aborted."
       stdDev->devTimer->stop();
       break;
    default:
       break;
   }
   
   return false;

}

void INDIStdProperty::newTime()
{
   INDI_E * timeEle;
   INDI_P *SDProp;
   
   timeEle = pp->findElement("UTC");
   if (!timeEle) return;
   
   TimeDialog timedialog ( ksw->data()->ut(), ksw );

	if ( timedialog.exec() == QDialog::Accepted )
	{
		QTime newTime( timedialog.selectedTime() );
		ExtDate newDate( timedialog.selectedDate() );

                timeEle->write_w->setText(QString("%1-%2-%3T%4:%5:%6")
					.arg(newDate.year()).arg(newDate.month())
					.arg(newDate.day()).arg(newTime.hour())
					.arg(newTime.minute()).arg(newTime.second()));
	        pp->newText();
	}
	
   SDProp  = pp->pg->dp->findProp("SDTIME");
   if (!SDProp) return;
   timeEle = SDProp->findElement("LST");
   if (!timeEle) return;
   
   timeEle->write_w->setText(ksw->LST()->toHMSString());
   SDProp->newText();
}

#include "indistd.moc"
