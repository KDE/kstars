/*  INDI STD
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This apppication is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    2004-01-18: This class handles INDI Standard properties.
 */
 
 #include "indielement.h"
 #include "indiproperty.h"
 #include "indigroup.h"
 #include "indidevice.h"
 #include "indistd.h"
 #include "kstars.h"
 #include "timedialog.h"
 
 #include <qtimer.h>
 #include <qlabel.h>
 
 #include <klineedit.h>
 #include <kstatusbar.h>
 
 INDIStdDevice::INDIStdDevice(INDI_D *associatedDevice, KStars * kswPtr)
 {
 
   dp   = associatedDevice;
   ksw  = kswPtr;
   initDevCounter = 0;
   
   currentObject  = NULL; 
   devTimer = new QTimer(this);
   QObject::connect( devTimer, SIGNAL(timeout()), this, SLOT(timerDone()) );
 
 }
 
 INDIStdDevice::~INDIStdDevice()
 {
 
 }
 
 void INDIStdDevice::setTextValue(INDI_P *pp)
 {
 
  switch (pp->stdID)
  {
    case EQUATORIAL_COORD:
      ksw->map()->forceUpdateNow();
      break;
    
    case TIME:
      if (ksw->options()->indiAutoTime)
       handleDevCounter();
      break;
      
    case GEOGRAPHIC_COORD:
      if (ksw->options()->indiAutoGeo)
       handleDevCounter();
      break;
      
    case EXPOSE_DURATION:
       if (pp->state == PS_IDLE || pp->state == PS_OK)
         pp->set_w->setText(i18n("Start"));
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
      
    default:
      break;
    }
 
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
 
   if (pp->name == "DEVICE_PORT")
   {
     portEle = pp->findElement("PORT");
     if (!portEle) return;
     
     portEle->read_w->setText (ksw->options()->indiPortName);
     portEle->write_w->setText(ksw->options()->indiPortName);
     portEle->text = ksw->options()->indiPortName;
   }
 }
 
void INDIStdDevice::initDeviceOptions()
{

  INDI_P *prop;

  initDevCounter = 0;

  if (ksw->options()->indiAutoTime)
  {
  	prop = dp->findProp("TIME");
  	  if (prop)
	  {
            updateTime();
	    initDevCounter += 2;
	  }
  }

  if (ksw->options()->indiAutoGeo)
  {
  	prop = dp->findProp("GEOGRAPHIC_COORD");
   	if (prop)
	{
   		updateLocation();
         	initDevCounter += 2;
	}
  }

  if (ksw->options()->indiMessages)
    ksw->statusBar()->changeItem( i18n("%1 is online.").arg(dp->name), 0);

  ksw->map()->forceUpdateNow();
}

 void INDIStdDevice::handleDevCounter()
{

  if (initDevCounter <= 0)
   return;

  initDevCounter--;

  if (initDevCounter == 0 && ksw->options()->indiMessages)
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

       if (!dp->isOn())
       {
        devTimer->stop();
	return;
       }

       prop = dp->findProp("ON_COORD_SET");
       if (prop == NULL || !currentObject)
        return;

       // wait until slew is done
       if (prop->state == PS_BUSY)
        return;

       kdDebug() << "Timer called, starting processing" << endl;

       prop = dp->findProp("EQUATORIAL_COORD");
       if (prop == NULL)
        return;

	SkyPoint sp(currentObject->ra(), currentObject->dec());

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
   
   switch (pp->stdID)
   {
     /* Set expose duration button to 'cancel' when busy */
     case EXPOSE_DURATION:
       pp->set_w->setText(i18n("Cancel"));
       break;
     
      /* Save Port name in KStars options */
     case DEVICE_PORT:
        lp = pp->findElement("PORT");
        if (lp)
          ksw->options()->indiPortName = lp->text;
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
	 
   default:
         return false;
	 break;
   }
  
  return false;
 
 }
 
void INDIStdProperty::newSwitch(int id)
{
  switch (pp->stdID)
  {
    case ABORT_MOTION:
       stdDev->devTimer->stop();
       break;
    
    default:
       break;
   }

}

void INDIStdProperty::newTime()
{
   INDI_E * timeEle;
   
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
}
