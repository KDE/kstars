/*  INDI STD
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This apppication is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    2004-01-18: This class handles INDI Standard properties.
 */
 
 #include "Options.h"
 #include "indielement.h"
 #include "indiproperty.h"
 #include "indigroup.h"
 #include "indidevice.h"
 #include "indidriver.h"
 #include "indistd.h"
 #include "kstars.h"
 #include "devicemanager.h"
 #include "timedialog.h"
 #include "streamwg.h"
 
 #include <qtimer.h>
 #include <qlabel.h>
 
 #include <klineedit.h>
 #include <kstatusbar.h>
 
 INDIStdDevice::INDIStdDevice(INDI_D *associatedDevice, KStars * kswPtr)
 {
 
   dp   = associatedDevice;
   ksw  = kswPtr;
   initDevCounter = 0;
   
   streamWindow   = new StreamWG(this, ksw);
   currentObject  = NULL; 
   devTimer = new QTimer(this);
   QObject::connect( devTimer, SIGNAL(timeout()), this, SLOT(timerDone()) );
 
 }
 
 INDIStdDevice::~INDIStdDevice()
 {
 
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
	 streamWindow->allocateStreamBuffer();
	 break;
	 
      case DATA_CHANNEL:
         el = pp->findElement("CHANNEL");
	 if (el && el->value)
	    streamWindow->establishDataChannel(dp->parentMgr->host, (int) el->value);
     break;
    default:
        break;
	
  }
 
 }
 
 void INDIStdDevice::setLabelState(INDI_P *pp)
 {
    INDI_E *lp;
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
      
    default:
      break;
    }
 
 }
 
 void INDIStdDevice::streamDisabled()
 {
    INDI_P *pp;
    
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
  
  QTime newTime( ksw->data()->clock()->UTC().time());
  QDate newDate( ksw->data()->clock()->UTC().date());

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
       for (int i=0; i < drivers->devices.size(); i++)
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
     
     case FILE_NAME:
     portEle = pp->findElement("FILE");
     if (!portEle) return;
     str = Options::fitsSaveDirectory() + "/" + portEle->text;
     portEle->read_w->setText( str );
     portEle->write_w->setText( str);
     portEle->text = str;
     pp->newText();
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

        sp.apparentCoord( ksw->data()->clock()->JD() , (long double) J2000);

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
          for (int i=0; i < drivers->devices.size(); i++)
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

         sp.apparentCoord( ksw->data()->clock()->JD() , (long double) J2000);

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

  switch (pp->stdID)
  {
    case CONNECTION:      
      if (el->name == "DISCONNECT" && el->state == PS_ON)
      {
        stdDev->streamWindow->enableStream(false);
	stdDev->streamWindow->close();
	break;
      }
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
    case IMAGE_TYPE:
      if (el->name == "COLOR" && el->state == PS_ON)
        stdDev->streamWindow->setColorFrame(true);
      else
        stdDev->streamWindow->setColorFrame(false);
	
       stdDev->streamWindow->allocateStreamBuffer();
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
   
   TimeDialog timedialog ( ksw->data()->UTime, ksw, true );

	if ( timedialog.exec() == QDialog::Accepted )
	{
		QTime newTime( timedialog.selectedTime() );
		QDate newDate( timedialog.selectedDate() );

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
