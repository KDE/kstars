/*  Image Sequence
    Capture image sequence from an imaging device.
    
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */
 
#include "imagesequence.h"
#include "kstars.h"
#include "indidriver.h"
#include "indielement.h"
#include "indiproperty.h"
#include "indimenu.h"
#include "indidevice.h"
#include "indistd.h"
#include "devicemanager.h"

#include <kdebug.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <klineedit.h>
#include <kprogress.h>
#include <knuminput.h>

#include <qtimer.h>
#include <qcombobox.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qcheckbox.h>

#define RETRY_MAX	12
#define RETRY_PERIOD	5000

imagesequence::imagesequence(QWidget* parent, const char* name, bool modal, WFlags fl)
: imgSequenceDlg(parent,name, modal,fl)
{
  
  ksw = (KStars *) parent;
  
  seqTimer = new QTimer(this);
  
  setModal(false);
  
  // Connect signals and slots
  connect(startB, SIGNAL(clicked()), this, SLOT(startSequence()));
  connect(stopB, SIGNAL(clicked()), this, SLOT(stopSequence()));
  connect(closeB, SIGNAL(clicked()), this, SLOT(close()));
  connect(seqTimer, SIGNAL(timeout()), this, SLOT(captureImage()));
  connect(deviceCombo, SIGNAL(activated(const QString &)), this, SLOT(checkDevice(const QString &)));
  
  active 		= false;
  ISOStamp		= false;
  seqExpose		= 0;
  seqTotalCount 	= 0;
  seqCurrentCount	= 0;
  seqDelay		= 0;
  lastItem              = 0;
  stdDev                = NULL;
  
}

imagesequence::~imagesequence()
{
}

bool imagesequence::updateStatus()
{
  bool imgDeviceFound = false;
  INDI_P *imgProp;
  INDIMenu *devMenu = ksw->getINDIMenu();
  if (devMenu == NULL)
     return false;
  
  deviceCombo->clear();
  
  for (uint i=0; i < devMenu->mgr.count(); i++)
  {
	for (uint j=0; j < devMenu->mgr.at(i)->indi_dev.count(); j++)
	{
  		imgProp = devMenu->mgr.at(i)->indi_dev.at(j)->findProp("EXPOSE_DURATION");
		if (!imgProp)
			 continue;
				 
		imgDeviceFound = true;
		 
		if (devMenu->mgr.at(i)->indi_dev.at(j)->label.isEmpty())
			devMenu->mgr.at(i)->indi_dev.at(j)->label = devMenu->mgr.at(i)->indi_dev.at(j)->name;
				
		deviceCombo->insertItem(devMenu->mgr.at(i)->indi_dev.at(j)->label);
			
	}
	
   }
         
  if (imgDeviceFound)
  {
  	deviceCombo->setCurrentItem(lastItem);
  	currentDevice = deviceCombo->currentText();
  }
  else return false;
  
  if (!verifyDeviceIntegrity())
  {
   stopSequence();
   return false;
  }
  else
  {
    INDI_P *exposeProp;
    INDI_E *exposeElem;
    
    exposeProp = stdDev->dp->findProp("EXPOSE_DURATION");
    if (!exposeProp)
    {
      KMessageBox::error(this, i18n("Device does not support EXPOSE_DURATION property."));
      return false;
    }
  
    exposeElem = exposeProp->findElement("EXPOSE_S");
    if (!exposeElem)
    {
      KMessageBox::error(this, i18n("EXPOSE_DURATION property is missing EXPOSE_S element."));
      return false;
    }
    
    exposureIN->setValue(exposeElem->value);
  }
   
  // Everything okay, let's show the dialog
  return true;
   
 }

void imagesequence::resetButtons()
{
  startB->setEnabled(true);
  stopB->setEnabled(false);

}

void imagesequence::startSequence()
{

   if (active)
    stopSequence();
    
  // Let's find out which device has been selected and that it's connected.
  if (!verifyDeviceIntegrity())
   return;
  
 
  // Get expose paramater
  active 		= true;
  ISOStamp		= ISOCheck->isChecked() ? true : false;
  seqExpose		= exposureIN->value();
  seqTotalCount 	= countIN->value();
  seqCurrentCount	= 0;
  seqDelay		= delayIN->value() * 1000;		/* in ms */
  currentDevice		= deviceCombo->currentText();
  lastItem              = deviceCombo->currentItem();
  
  fullImgCountOUT->setText( QString("%1").arg(seqTotalCount));
  currentImgCountOUT->setText(QString("%1").arg(seqCurrentCount));
  
  
  // Ok, now let's connect signals and slots for this device
  connect(stdDev, SIGNAL(FITSReceived(QString)), this, SLOT(newFITS(QString)));
  
  // set the progress info
  imgProgress->setEnabled(true);
  imgProgress->setTotalSteps(seqTotalCount);
  imgProgress->setProgress(seqCurrentCount);
  
  stdDev->batchMode    = true;
  stdDev->ISOMode      = ISOStamp;
  // Set this LAST
  stdDev->updateSequencePrefix(prefixIN->text());
  
  
  // Update button status
  startB->setEnabled(false);
  stopB->setEnabled(true);
  
  captureImage();
}

void imagesequence::stopSequence()
{
 
  retries              = 0;
  seqTotalCount        = 0;
  seqCurrentCount      = 0;
  active               = false;
  
  imgProgress->setEnabled(false);
  fullImgCountOUT->setText("");
  currentImgCountOUT->setText("");
  
  resetButtons();
  seqTimer->stop();
  
  if (stdDev)
  {
    stdDev->seqCount     = 0;
    stdDev->batchMode    = false;
    stdDev->ISOMode      = false;
    
    stdDev->disconnect( SIGNAL(FITSReceived(QString)));
  }

}

void imagesequence::checkDevice(const QString & deviceLabel)
{
  INDI_D *idevice = NULL;
  
  INDIMenu *imenu = ksw->getINDIMenu();
  if (!imenu)
  {
    KMessageBox::error(this, i18n("INDI Menu has not been initialized properly. Restart KStars."));
    return;
  }
  
  idevice = imenu->findDeviceByLabel(deviceLabel);
  
  if (!idevice)
  {
	KMessageBox::error(this, i18n("INDI device %1 no longer exists.").arg(deviceLabel));
    	deviceCombo->setCurrentItem(lastItem);
    	return;
  }
  
  if (!idevice->isOn())
  {
        KMessageBox::error(this, i18n("%1 is disconnected. Establish a connection to the device using the INDI Control Panel.").arg(deviceLabel));
		
	deviceCombo->setCurrentItem(lastItem);
    	return;
  }
  
  currentDevice = deviceLabel;

}

void imagesequence::newFITS(QString deviceLabel)
{
  // If the FITS is not for our device, simply ignore
  if (deviceLabel != currentDevice)
   return;

  seqCurrentCount++;
  imgProgress->setProgress(seqCurrentCount);
  
  currentImgCountOUT->setText( QString("%1").arg(seqCurrentCount));
  
  // if we're done
  if (seqCurrentCount == seqTotalCount)
  {
    stdDev->batchMode    = false;
    stdDev->ISOMode      = false;
    retries              = 0;
    seqTotalCount        = 0;
    seqCurrentCount      = 0;
    active               = false;
    seqTimer->stop();
    
    if (stdDev)
    	stdDev->disconnect( SIGNAL(FITSReceived(QString)));
    
    resetButtons();
  }
  else
   seqTimer->start(seqDelay);
   
}


bool imagesequence::verifyDeviceIntegrity()
{
  INDI_D *idevice = NULL;
  INDI_P *exposeProp;
  INDI_E *exposeElem;
  stdDev = NULL;
  
  INDIMenu *imenu = ksw->getINDIMenu();
  if (!imenu)
  {
    KMessageBox::error(this, i18n("INDI Menu has not been initialized properly. Restart KStars."));
    return false;
  }
  
  // #2 Check if the device exists
  idevice = imenu->findDeviceByLabel(currentDevice);
	
	
  if (!idevice)
  {
    	KMessageBox::error(this, i18n("INDI device %1 no longer exists.").arg(currentDevice));
    	return false;
  }
  
  if (!idevice->isOn())
  {
	    	KMessageBox::error(this, i18n("%1 is disconnected. Establish a connection to the device using the INDI Control Panel.").arg(currentDevice));
		
    		return false;
  }
  
  stdDev = idevice->stdDev;
  
  exposeProp = stdDev->dp->findProp("EXPOSE_DURATION");
  if (!exposeProp)
  {
    KMessageBox::error(this, i18n("Device does not support EXPOSE_DURATION property."));
    return false;
  }
  
  exposeElem = exposeProp->findElement("EXPOSE_S");
  if (!exposeElem)
  {
    KMessageBox::error(this, i18n("EXPOSE_DURATION property is missing EXPOSE_S element."));
    return false;
  }
  
  return true;
}

void imagesequence::captureImage()
{

  INDI_P * exposeProp = NULL;
  INDI_E * exposeElem = NULL;
  
  // Let's capture a new frame in acoord with the settings
  // We need to take into consideration the following conditions:
  // A. The device has been disconnected.
  // B. The device has been lost.
  // C. The property is still busy.
  // D. The property has been lost.
  
  if (!verifyDeviceIntegrity())
  {
    stopSequence();
    return;
  }
  
  exposeProp = stdDev->dp->findProp("EXPOSE_DURATION");
  exposeElem = exposeProp->findElement("EXPOSE_S");
  
  // disable timer until it's called again by newFITS, or called for retries
  seqTimer->stop();
  
  // Make sure it's not busy, if it is then schedual.
  if (exposeProp->state == PS_BUSY)
  {
    retries++;
    
    if (retries > RETRY_MAX)
    {
      seqTimer->stop();
      KMessageBox::error(this, i18n("Device is busy and not responding."));
      stopSequence();
      retries = 0;
      return;
    }
    
    seqTimer->start(RETRY_PERIOD);
  }
  
  // Set duration if applicable. We check the property permission, min, and max values
  if (exposeProp->perm == PP_RW || exposeProp->perm == PP_WO)
  {
    if (seqExpose < exposeElem->min || seqExpose > exposeElem->max)
    {
      stopSequence();
      KMessageBox::error(this, i18n("Expose duration is invalid. %1 supports expose durations from %2 to %3 seconds only.").arg(currentDevice).arg(exposeElem->min).arg(exposeElem->max));
      return;
    }
    
    // we're okay, set it
    exposeElem->targetValue = seqExpose;
    if (exposeElem->spin_w)
    {
      exposeElem->spin_w->setValue(seqExpose);
      exposeElem->spinChanged(seqExpose);
    }
    else
     exposeElem->write_w->setText( QString("%1").arg(seqExpose));
     
  }
      
  // We're done! Send it to the driver
  exposeProp->newText();

}

 
#include "imagesequence.moc"

