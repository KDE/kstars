/*  Telescope wizard
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <qfile.h>
#include <qpixmap.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qwidgetstack.h>
#include <qstring.h>
#include <qtimer.h>
#include <qtable.h>
#include <qtextedit.h>
#include <qradiobutton.h>

#include <klistview.h>
#include <klineedit.h>
#include <kmessagebox.h>
#include <kprogress.h>

#include "telescopewizardprocess.h"
#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "timedialog.h"
#include "ksutils.h"
#include "libkdeedu/extdate/extdatetime.h"

#include "indimenu.h"
#include "indidriver.h"
#include "indielement.h"
#include "indiproperty.h"
#include "indistd.h"
#include "indidevice.h"

#define TIMEOUT_THRESHHOLD	20

telescopeWizardProcess::telescopeWizardProcess( QWidget* parent, const char* name ) : telescopeWizard(parent, name)
{
   currentPort  = -1;
   timeOutCount = 0;
   indiDev = NULL;
   progressScan = NULL;
   linkRejected = false;

   QString locStr;
   QFile sideIMG;

   if (KSUtils::openDataFile(sideIMG, "wizardside.png"))
       wizardPix->setPixmap(QPixmap(sideIMG.name()));

   backB->hide();
   currentPage = INTRO_P;

   ksw = (KStars *) parent;

   ksw->establishINDI();

   indimenu   = ksw->getINDIMenu();
   indidriver = ksw->getINDIDriver();

   INDIMessageBar = Options::indiMessages();
   Options::setIndiMessages( false );

  QTime newTime( ksw->data()->lt().time() );
  ExtDate newDate( ksw->data()->lt().date() );

  timeOut->setText( QString().sprintf("%02d:%02d:%02d", newTime.hour(), newTime.minute(), newTime.second()));
  dateOut->setText( QString().sprintf("%d-%02d-%02d", newDate.year(), newDate.month(), newDate.day()));

  if (ksw->geo()->translatedProvince().isEmpty())
  	locationOut->setText( QString("%1, %2").arg(ksw->geo()->translatedName()).arg(ksw->geo()->translatedCountry()));
  else
  	locationOut->setText( QString("%1, %2, %3").arg(ksw->geo()->translatedName())
  						     .arg(ksw->geo()->translatedProvince())
						     .arg(ksw->geo()->translatedCountry()));


   for (unsigned int i=0; i < indidriver->devices.size(); i++)
              if (indidriver->devices[i]->deviceType == KSTARS_TELESCOPE)
   		    telescopeCombo->insertItem(indidriver->devices[i]->label);

   if ( !Options::indiTelescopePort().isEmpty())
    portList << Options::indiTelescopePort();
    
    portList << "/dev/ttyS0" <<  "/dev/ttyS1" << "/dev/ttyS2" << "/dev/ttyS3" << "/dev/ttyS4"
             << "/dev/ttyUSB0" << "/dev/ttyUSB1" << "/dev/ttyUSB2" << "/dev/ttyUSB3";// << "/dev/ttyUSB4";

   connect(helpB, SIGNAL(clicked()), parent, SLOT(appHelpActivated()));
   connect(nextB, SIGNAL(clicked()), this, SLOT(processNext()));
   connect(backB, SIGNAL(clicked()), this, SLOT(processBack()));
   connect(setTimeB, SIGNAL(clicked()), this, SLOT(newTime()));
   connect(setLocationB, SIGNAL(clicked()), this, SLOT(newLocation()));

   newDeviceTimer = new QTimer(this);
   QObject::connect( newDeviceTimer, SIGNAL(timeout()), this, SLOT(processPort()) );

}

telescopeWizardProcess::~telescopeWizardProcess()
{
  if (progressScan)
    if (progressScan->wasCancelled())
      indidriver->processDeviceStatus(1);

    Options::setIndiMessages( INDIMessageBar );

    Reset();
}

void telescopeWizardProcess::processNext(void)
{
  int linkResult=0;

 switch (currentPage)
 {
   case INTRO_P:
     currentPage++;
     backB->show();
     wizardContainer->raiseWidget(currentPage);
     break;
  case MODEL_P:
     currentPage++;
     wizardContainer->raiseWidget(currentPage);
     break;
  case TELESCOPE_P:
     currentPage++;
     wizardContainer->raiseWidget(currentPage);
     break;
  case LOCAL_P:
     currentPage++;
     wizardContainer->raiseWidget(currentPage);
     break;
  case PORT_P:
     linkResult = establishLink();
     if ( linkResult == 1)
     {
progressScan = new KProgressDialog(this, "autoscan", i18n("Autoscan"), i18n("Please wait while KStars scan communication ports for attached telescopes.\nThis process might take few minutes to complete."), true);
   progressScan->setAllowCancel(true);
   progressScan->setAutoClose(true);
   progressScan->setAutoReset(true);
   progressScan->progressBar()->setTotalSteps(portList.count());
   progressScan->progressBar()->setValue(0);
   progressScan->show();
    }
    else if (linkResult == 2)
      KMessageBox::queuedMessageBox(0, KMessageBox::Information, i18n("Please wait while KStars tries to connect to your telescope..."));
    else if (linkResult == -1)
      KMessageBox::error(0, i18n("Error. Unable to locate telescope drivers."));
     break;
  default:
     break;
  }

}

void telescopeWizardProcess::processBack(void)
{
 // for now, just display the next page, and restart once we reached the end

 switch (currentPage)
 {
   case INTRO_P:
   // we shouldn't be here!
   break;
     break;
  case MODEL_P:
     currentPage--;
     backB->hide();
     wizardContainer->raiseWidget(currentPage);
     break;
  case TELESCOPE_P:
     currentPage--;
     wizardContainer->raiseWidget(currentPage);
     break;
  case LOCAL_P:
     currentPage--;
     wizardContainer->raiseWidget(currentPage);
     break;
  case PORT_P:
     currentPage--;
     wizardContainer->raiseWidget(currentPage);
     break;
  default:
     break;
  }

}

void telescopeWizardProcess::newTime()
{
	TimeDialog timedialog (ksw->data()->lt(), ksw);

	if ( timedialog.exec() == QDialog::Accepted )
	{
		KStarsDateTime dt( timedialog.selectedDate(), timedialog.selectedTime() );
		ksw->data()->changeDateTime( dt );

		timeOut->setText( QString().sprintf("%02d:%02d:%02d", dt.time().hour(), dt.time().minute(), dt.time().second()));
		dateOut->setText( QString().sprintf("%d-%02d-%02d", dt.date().year(), dt.date().month(), dt.date().day()));
	}
}

void telescopeWizardProcess::newLocation()
{

   ksw->slotGeoLocator();

   locationOut->setText( QString("%1, %2, %3").arg(ksw->geo()->translatedName())
  					     .arg(ksw->geo()->translatedProvince())
					     .arg(ksw->geo()->translatedCountry()));
   timeOut->setText( QString().sprintf("%02d:%02d:%02d", ksw->data()->lt().time().hour(), ksw->data()->lt().time().minute(), ksw->data()->lt().time().second()));

  dateOut->setText( QString().sprintf("%d-%02d-%02d", ksw->data()->lt().date().year(),
  ksw->data()->lt().date().month() ,ksw->data()->lt().date().day()));



}

int telescopeWizardProcess::establishLink()
{

	if (!indidriver || !indimenu)
	  return (0);
	  
	QListViewItem *driverItem = NULL;
	driverItem = indidriver->localListView->findItem(telescopeCombo->currentText(), 0);
	if (driverItem == NULL) return -1;

	// If device is already running, we need to shut it down first
	if (indidriver->isDeviceRunning(telescopeCombo->currentText()))
	{
		indidriver->localListView->setSelected(driverItem, true);
		indidriver->processDeviceStatus(1);
	}
	   
	// Set custome label for device
	indimenu->setCustomLabel(telescopeCombo->currentText());
	currentDevice = indimenu->currentLabel;
	// Select it
	indidriver->localListView->setSelected(driverItem, true);
	// Make sure we start is locally
	indidriver->localR->setChecked(true);
	// Run it
	indidriver->processDeviceStatus(0);

	if (!indidriver->isDeviceRunning(telescopeCombo->currentText()))
	 return (3);

	newDeviceTimer->start(1500);
	
	if (portIn->text().isEmpty())
	 return (1);
        else
	 return (2);

}

void telescopeWizardProcess::processPort()
{
     INDI_P * pp;
     INDI_E * lp;

     if (!indidriver || !indimenu)
       return;
     
     timeOutCount++;

     if (timeOutCount >= TIMEOUT_THRESHHOLD)
     {
       indidriver->processDeviceStatus(1);
       Reset();
       KMessageBox::error(0, i18n("Error: connection timeout. Unable to communicate with an INDI server"));
       close();
       return;
     }

    indiDev = indimenu->findDeviceByLabel(currentDevice);
    if (!indiDev) return;

     // port empty, start autoscan
     if (portIn->text().isEmpty())
     {
       newDeviceTimer->stop();
       linkRejected = false;
       connect(indiDev->stdDev, SIGNAL(linkRejected()), this, SLOT(scanPorts()));
       connect(indiDev->stdDev, SIGNAL(linkAccepted()), this, SLOT(linkSuccess()));
       scanPorts();
       return;
     }

     pp = indiDev->findProp("DEVICE_PORT");
     if (!pp) return;
     lp = pp->findElement("PORT");
     if (!lp) return;

     lp->write_w->setText(portIn->text());
     
     pp = indiDev->findProp("CONNECTION");
     if (!pp) return;

     newDeviceTimer->stop();

     Options::setIndiMessages( INDIMessageBar );

     pp->newSwitch(0);

     timeOutCount = 0;

     indimenu->show();

     close();

}

void telescopeWizardProcess::scanPorts()
{
     INDI_P * pp;
     INDI_E *lp;
     
     if (!indiDev || !indidriver || !indimenu || linkRejected)
      return;

     currentPort++;

     progressScan->progressBar()->setValue(currentPort);

     if ( (unsigned) currentPort >= portList.count())
     {
      KMessageBox::sorry(0, i18n("Sorry. KStars failed to detect any attached telescopes, please check your settings and try again."));
      linkRejected = true;
      indidriver->processDeviceStatus(1);
      Reset();
      return;
     }

     if (indiDev->msgST_w)
     	indiDev->msgST_w->clear();

     pp = indiDev->findProp("DEVICE_PORT");
     if (!pp) return;
     lp = pp->findElement("PORT");
     
     lp->write_w->setText(portList[currentPort]);
     pp->newText();
	
     pp = indiDev->findProp("CONNECTION");
     if (!pp) return;

     pp->newSwitch(0);

}

void telescopeWizardProcess::linkSuccess()
{
  Reset();

  indimenu->show();

  close();

}

void telescopeWizardProcess::Reset()
{

  currentPort = -1;
  timeOutCount = 0;

  if (progressScan)
  	progressScan->close();

  indiDev = NULL;

}

#include "telescopewizardprocess.moc"
