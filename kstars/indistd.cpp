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
 #include "ccdpreviewwg.h"
 #include "fitsviewer.h"
 
 #include <sys/socket.h>
 #include <netinet/in.h>
 #include <arpa/inet.h>
 #include <netdb.h>
 #include <ctype.h>
 #include <zlib.h>
 #include <stdlib.h>
 
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
 #include <kdirlister.h>
 #include <kaction.h>
 
  
 #define STD_BUFFER_SIZ		1024000
 #define FRAME_ILEN		1024
 
 INDIStdDevice::INDIStdDevice(INDI_D *associatedDevice, KStars * kswPtr)
 {
 
   dp			= associatedDevice;
   ksw  		= kswPtr;
   initDevCounter	= 0;
   seqCount		= 0;
   batchMode 		= false;
   ISOMode   		= false;
   
   currentObject  	= NULL; 
   streamWindow   	= new StreamWG(this, ksw);
   CCDPreviewWindow   	= new CCDPreviewWG(this, ksw);
	 
   devTimer 		= new QTimer(this);
   seqLister		= new KDirLister();
   
   telescopeSkyObject   = new SkyObject(0, 0, 0, 0, i18n("Telescope"));
   ksw->data()->appendTelescopeObject(telescopeSkyObject);
	
   connect( devTimer, SIGNAL(timeout()), this, SLOT(timerDone()) );
   connect( seqLister, SIGNAL(newItems (const KFileItemList & )), this, SLOT(checkSeqBoundary(const KFileItemList &)));
   
   downloadDialog = new KProgressDialog(NULL, 0, i18n("INDI"), i18n("Downloading Data..."));
   downloadDialog->cancel();
   
   parser		= newLilXML();
 }
 
 INDIStdDevice::~INDIStdDevice()
 {
   streamWindow->enableStream(false);
   streamWindow->close();
   CCDPreviewWindow->enableStream(false);
   CCDPreviewWindow->close();
   streamDisabled();
   delete (seqLister);
 }
 
void INDIStdDevice::handleBLOB(unsigned char *buffer, int bufferSize, QString dataFormat)
{

  if (dataFormat == ".fits") dataType = DATA_FITS;
  else if (dataFormat == ".stream") dataType = DATA_STREAM;
  else if (dataFormat == ".ccdpreview") dataType = DATA_CCDPREVIEW;	  
  else dataType = DATA_OTHER;

  if (dataType == DATA_STREAM)
  {
    if (!streamWindow->processStream)
      return;
	
    streamWindow->show();	
    streamWindow->streamFrame->newFrame( buffer, bufferSize, streamWindow->streamWidth, streamWindow->streamHeight);    
  }
  else if (dataType == DATA_CCDPREVIEW)
  {
    if (!CCDPreviewWindow->processStream)
      return;
    CCDPreviewWindow->show();	
    CCDPreviewWindow->streamFrame->newFrame( buffer, bufferSize, CCDPreviewWindow->streamWidth, CCDPreviewWindow->streamHeight);    
  }
  else if (dataType == DATA_FITS || dataType == DATA_OTHER)
  {
    char filename[256];
    FILE *fitsTempFile;
    int fd, nr, n=0;
    QString currentDir = Options::fitsSaveDirectory();
       
    streamWindow->close();
        
    if (dataType == DATA_FITS && !batchMode && Options::indiFITSDisplay())
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
	  
      if (dataType == DATA_FITS)
      {
	char tempFileStr[256];
	strncpy(tempFileStr, filename, 256);
	    
	if ( batchMode && !ISOMode)
	  snprintf(filename, sizeof(filename), "%s/%s_%02d.fits", tempFileStr, seqPrefix.ascii(), seqCount);
	else if (!batchMode && !Options::indiFITSDisplay())
	{
	  strftime (ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tp);
	  snprintf(filename, sizeof(filename), "%s/file_%s.fits", tempFileStr, ts);
	}
	else
	{
	  strftime (ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tp);
	  snprintf(filename, sizeof(filename), "%s/%s_%02d_%s.fits", tempFileStr, seqPrefix.ascii(), seqCount, ts);
	}
	     
	seqCount++;
      }
      else
      {
	strftime (ts, sizeof(ts), "/file-%Y-%m-%dT%H:%M:%S.", tp);
	strncat(filename, ts, sizeof(ts));
	strncat(filename, dataFormat.ascii(), 10);
      }
    }
	  
    fitsTempFile = fopen(filename, "w");
       
    if (fitsTempFile == NULL) return;
       
    for (nr=0; nr < (int) bufferSize; nr += n)
      n = fwrite( ((unsigned char *) buffer) + nr, 1, bufferSize - nr, fitsTempFile);
       
    fclose(fitsTempFile);
       
       // We're done if we have DATA_OTHER
    if (dataType == DATA_OTHER)
    {
      ksw->statusBar()->changeItem( i18n("Data file saved to %1").arg(filename), 0);
      return;
    }
    else if (dataType == DATA_FITS && (batchMode || !Options::indiFITSDisplay()))
    {
      ksw->statusBar()->changeItem( i18n("FITS file saved to %1").arg(filename), 0);
      emit FITSReceived(dp->label);
      return;
    } 
       
    KURL fileURL(filename);
       
    FITSViewer * fv = new FITSViewer(&fileURL, ksw);
    fv->fitsChange();
    fv->show();
  }
       
}

 /* Process standard Text and Number properties arrives from the driver */
 void INDIStdDevice::setTextValue(INDI_P *pp)
 {
   INDI_E *el;
   int wd, ht, bpp, bo, mu;
   long mgd;
   double fwhm;
   int d, m, y, min, sec, hour;
   ExtDate indiDate;
   QTime indiTime;
   KStarsDateTime indiDateTime;
   
  switch (pp->stdID)
  {

    case TIME:
      if ( Options::indiAutoTime() )
       handleDevCounter();
       
       // Update KStars time once we receive update from INDI
       el = pp->findElement("UTC");
       if (!el) return;
       
       sscanf(el->text.ascii(), "%d%*[^0-9]%d%*[^0-9]%dT%d%*[^0-9]%d%*[^0-9]%d", &y, &m, &d, &hour, &min, &sec);
       indiDate.setYMD(y, m, d);
       indiTime.setHMS(hour, min, sec);
       indiDateTime.setDate(indiDate);
       indiDateTime.setTime(indiTime);
       
       ksw->data()->changeDateTime(indiDateTime);
       ksw->data()->syncLST();
       
      break;
      
    case SDTIME:
      if ( Options::indiAutoTime())
       handleDevCounter();
      break;
    
    case GEOGRAPHIC_COORD:
      if ( Options::indiAutoGeo() )
       handleDevCounter();
      break;
      
    case CCD_EXPOSE_DURATION:
       if (pp->state == PS_IDLE || pp->state == PS_OK)
         pp->set_w->setText(i18n("Start"));
       break;
       
    case CCD_FRAME:
         el = pp->findElement("WIDTH");
	 if (!el) return;
	 wd = (int) el->value;
	 el = pp->findElement("HEIGHT");
	 if (!el) return;
	 ht = (int) el->value;
	 
	 streamWindow->setSize(wd, ht);
	 //streamWindow->allocateStreamBuffer();
	 break;
    case CCDPREVIEW_CTRL:	 
         el = pp->findElement("WIDTH");
	 if (!el) return;
	 wd = (int) el->value;
	 el = pp->findElement("HEIGHT");
	 if (!el) return;
	 ht = (int) el->value;
	 el = pp->findElement("BYTEORDER");
	 if (!el) return;
	 bo = (int) el->value;
	 el = pp->findElement("BYTESPERPIXEL");
	 if (!el) return;
	 bpp = (int) el->value;	 
	 el = pp->findElement("MAXGOODDATA");
	 if (!el) return;
	 mgd = (long) el->value;
	 CCDPreviewWindow->setCtrl(wd, ht, bo ,bpp,mgd);
         
	 break;

   case CCD_INFO:
	 el = pp->findElement("CCD_FWHM_PIXEL");
	 if (!el) return;
	 fwhm = el->value;
	 el = pp->findElement("CCD_PIXEL_SIZE");
	 if (!el) return;
	 mu = (int) el->value;
	 CCDPreviewWindow->setCCDInfo(fwhm, mu);
	 break;

       case EQUATORIAL_COORD:
       case EQUATORIAL_EOD_COORD:
	if (!dp->isOn()) break;
	el = pp->findElement("RA");
	if (!el) return;
	telescopeSkyObject->setRA(el->value);
	el = pp->findElement("DEC");
	if (!el) return;
	telescopeSkyObject->setDec(el->value);
	telescopeSkyObject->EquatorialToHorizontal(ksw->LST(), ksw->geo()->lat());
	// Force immediate update of skymap if the focus object is our telescope. 
	if (ksw->map()->focusObject() == telescopeSkyObject)
		ksw->map()->updateFocus();
	else  
		ksw->map()->update();
	break;

	case HORIZONTAL_COORD:
        if (!dp->isOn()) break;
	el = pp->findElement("ALT");
	if (!el) return;
	telescopeSkyObject->setAlt(el->value);
	el = pp->findElement("AZ");
	if (!el) return;
	telescopeSkyObject->setAz(el->value);
	telescopeSkyObject->HorizontalToEquatorial(ksw->LST(), ksw->geo()->lat());
	// Force immediate update of skymap if the focus object is our telescope. 
	if (ksw->map()->focusObject() == telescopeSkyObject)
	      ksw->map()->updateFocus();
	else
	      ksw->map()->update();
	break;

    default:
        break;
	
  }
 
 }

 void INDIStdDevice::setLabelState(INDI_P *pp)
 {
    INDI_E *lp;
    INDI_P *imgProp;
    KAction *tmpAction;
    INDIDriver *drivers = ksw->getINDIDriver();
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

        imgProp = dp->findProp("CCD_EXPOSE_DURATION");
	if (imgProp)
	{
        	tmpAction = ksw->actionCollection()->action("capture_sequence");
  		if (!tmpAction)
	  		kdDebug() << "Warning: capture_sequence action not found" << endl;
  		else
	  		tmpAction->setEnabled(true);
        }
      }
      else
      {
        if (streamWindow)
	{
	  //sNotifier->disconnect();
	  //dp->parentMgr->sNotifier->disconnect();
	  streamWindow->enableStream(false);
	  streamWindow->close();
	  
	    //close(streamFD);
	}
	
	if (ksw->map()->focusObject() == telescopeSkyObject)
	{
		ksw->map()->stopTracking();
		ksw->map()->setFocusObject(NULL);
	}

	drivers->updateMenuActions();
       	ksw->map()->forceUpdateNow();
        emit linkRejected();
      }
      break;
      
      case VIDEO_STREAM:
       lp = pp->findElement("ON");
       if (!lp) return;
       if (lp->state == PS_ON)
          streamWindow->enableStream(true);
       else
          streamWindow->enableStream(false);
       break;
       
       case CCDPREVIEW_STREAM:
       lp = pp->findElement("ON");
       if (!lp) return;
       if (lp->state == PS_ON)
          CCDPreviewWindow->enableStream(true);
       else
          CCDPreviewWindow->enableStream(false);
       break;
       
    default:
      break;
    }
 
 }

 void INDIStdDevice::streamDisabled()
 {
    INDI_P *pp;
    INDI_E *el;
    
    //pp = dp->findProp("CONNECTION");
    //if (!pp) return;
    //if (pp->state == PS_OFF) return;
    
    pp = dp->findProp("VIDEO_STREAM");
    if (!pp) return;
    
    el = pp->findElement("OFF");
    if (!el) return;
    
    if (el->state == PS_ON)
     return;
    
    // Turn stream off
    pp->newSwitch(1); 
   
}
 
 void INDIStdDevice::updateSequencePrefix(QString newPrefix)
 {
    seqPrefix = newPrefix;
    
    seqLister->setNameFilter(QString("%1_*.fits").arg(seqPrefix));
    
    seqCount = 0;
    
    if (ISOMode) return;
    
    seqLister->openURL(Options::fitsSaveDirectory());
    
    checkSeqBoundary(seqLister->items());
 
 }
 
 void INDIStdDevice::checkSeqBoundary(const KFileItemList & items)
 {
    int newFileIndex;
    QString tempName;
    char *tempPrefix = new char[64];
    
    // No need to check when in ISO mode
    if (ISOMode)
     return;
     
    for ( KFileItemListIterator it( items ) ; it.current() ; ++it )
    {
       tempName = it.current()->name();
       
       // find the prefix first
       if (tempName.find(seqPrefix) == -1)
         continue;
	 
       strncpy(tempPrefix, tempName.ascii(), 64);
       tempPrefix[63] = '\0';
       
       char * t = tempPrefix;
       
       // skip chars
       while (*t) { if (isdigit(*t)) break; t++; }
       //tempPrefix = t;
       
       newFileIndex = strtol(t, NULL, 10);
       
        if (newFileIndex >= seqCount)
	  seqCount = newFileIndex + 1;
   }
   
   delete [] (tempPrefix);
          
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

bool INDIStdDevice::handleNonSidereal()
{
   if (!currentObject)
      return false;

    int trackIndex=0;
    INDI_E *nameEle;

    kdDebug() << "Object of type " << currentObject->typeName() << endl;
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
     if (currentObject->name().lower() == prop->el.at(i)->label.lower())
     {
       prop->newSwitch(i);
       setMode->newSwitch(trackIndex);
       
       /* Send object name if available */
       nameEle = dp->findElem("OBJECT_NAME");
       if (nameEle && nameEle->pp->perm != PP_RO)
       {
           nameEle->write_w->setText(currentObject->name());
           nameEle->pp->newText();
       }

       return true;
     }
  }

   kdDebug() << "Device doesn't support SOLAR_SYSTEM property, issuing a timer" << endl;
   kdDebug() << "Starting timer for object of type " << currentObject->typeName() << endl;
   

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
      bool useJ2000 = false;

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
	
       prop = dp->findProp("EQUATORIAL_EOD_COORD");
       
       if (prop == NULL)
       {
         prop = dp->findProp("EQUATORIAL_COORD");
	 if (prop) useJ2000 = true;
       }
       
       if (prop == NULL || !currentObject)
        return;

       // wait until slew is done
       if (prop->state == PS_BUSY)
        return;

       kdDebug() << "Timer called, starting processing" << endl;

	SkyPoint sp(currentObject->ra(), currentObject->dec());
	
	kdDebug() << "RA: " << currentObject->ra()->toHMSString() << " - DEC: " << currentObject->dec()->toDMSString() << endl;
	kdDebug() << "Az: " << currentObject->az()->toHMSString() << " - Alt " << currentObject->alt()->toDMSString() << endl;

	if (useJ2000)
	{
		sp.set(currentObject->ra(), currentObject->dec());
        	sp.apparentCoord( ksw->data()->ut().djd() , (long double) J2000);
        }

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
     case CCD_EXPOSE_DURATION:
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
 
  INDI_E *RAEle(NULL), *DecEle(NULL), *AzEle(NULL), *AltEle(NULL), *nameEle(NULL);
  INDI_P * prop;
  SkyPoint sp;
  int selectedCoord=0;				/* 0 for Equatorial, 1 for Horizontal */
  bool useJ2000 (false);
  
  switch (pp->stdID)
  {
    /* Handle Slew/Track/Sync */
    case ON_COORD_SET:
    	// #1 set current object to NULL
    	stdDev->currentObject = NULL;
   	// #2 Deactivate timer if present
   	if (stdDev->devTimer->isActive())
		 	stdDev->devTimer->stop();

   	prop = pp->pg->dp->findProp("EQUATORIAL_EOD_COORD");
       	if (prop == NULL)
	{
		  prop = pp->pg->dp->findProp("EQUATORIAL_COORD");
		  if (prop == NULL)
                  {
                    prop = pp->pg->dp->findProp("HORIZONTAL_COORD");
                    if (prop == NULL)
        		return false;
                    else
                        selectedCoord = 1;		/* Select horizontal */
                  }
		  else
		        useJ2000 = true;
	}

        switch (selectedCoord)
        {
          // Equatorial
          case 0:
           if (prop->perm == PP_RO) return false;
           RAEle  = prop->findElement("RA");
       	   if (!RAEle) return false;
   	   DecEle = prop->findElement("DEC");
   	   if (!DecEle) return false;
           break;

         // Horizontal
         case 1:
          if (prop->perm == PP_RO) return false;
          AzEle = prop->findElement("AZ");
          if (!AzEle) return false;
          AltEle = prop->findElement("ALT");
          if (!AltEle) return false;
          break;
        }
   
        stdDev->currentObject = ksw->map()->clickedObject();
  	// Track is similar to slew, except that for non-sidereal objects
	// it tracks the objects automatically via a timer.
   	if ((lp->name == "TRACK"))
       		if (stdDev->handleNonSidereal())
	        	return true;

      /* Send object name if available */
      if (stdDev->currentObject)
          {
             nameEle = pp->pg->dp->findElem("OBJECT_NAME");
             if (nameEle && nameEle->pp->perm != PP_RO)
             {
                 nameEle->write_w->setText(stdDev->currentObject->name());
                 nameEle->pp->newText();
             }
          }

       switch (selectedCoord)
       {
         case 0:
            if (stdDev->currentObject)
		sp.set (stdDev->currentObject->ra(), stdDev->currentObject->dec());
            else
                sp.set (ksw->map()->clickedPoint()->ra(), ksw->map()->clickedPoint()->dec());

      	 if (useJ2000)
            sp.apparentCoord(ksw->data()->ut().djd(), (long double) J2000);

    	   RAEle->write_w->setText(QString("%1:%2:%3").arg(sp.ra()->hour()).arg(sp.ra()->minute()).arg(sp.ra()->second()));
	   DecEle->write_w->setText(QString("%1:%2:%3").arg(sp.dec()->degree()).arg(sp.dec()->arcmin()).arg(sp.dec()->arcsec()));

          break;

       case 1:
         if (stdDev->currentObject)
         {
           sp.setAz(*stdDev->currentObject->az());
           sp.setAlt(*stdDev->currentObject->alt());
         }
         else
         {
           sp.setAz(*ksw->map()->clickedPoint()->az());
           sp.setAlt(*ksw->map()->clickedPoint()->alt());
         }

          AzEle->write_w->setText(QString("%1:%2:%3").arg(sp.az()->degree()).arg(sp.az()->arcmin()).arg(sp.az()->arcsec()));
          AltEle->write_w->setText(QString("%1:%2:%3").arg(sp.alt()->degree()).arg(sp.alt()->arcmin()).arg(sp.alt()->arcsec()));

         break;
       }

       pp->newSwitch(switchIndex);
       prop->newText();
	
	return true;
	break;
   
   /* Handle Abort */
   case ABORT_MOTION:
         kdDebug() << "Stopping timer." << endl;
	 stdDev->devTimer->stop();
 	 pp->newSwitch(switchIndex);
	 return true;
	 break;
	 
   case MOVEMENT:
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
  el=el; 

  switch (pp->stdID)
  {
    case CONNECTION: 
      if (id == 1)
        stdDev->streamDisabled();
      else
      { 
       prop = pp->pg->dp->findProp("DEVICE_PORT");
       if (prop)
       prop->newText();
      }
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
	else return;
	
   SDProp  = pp->pg->dp->findProp("SDTIME");
   if (!SDProp) return;
   timeEle = SDProp->findElement("LST");
   if (!timeEle) return;
   
   timeEle->write_w->setText(ksw->LST()->toHMSString());
   SDProp->newText();
}

#include "indistd.moc"
