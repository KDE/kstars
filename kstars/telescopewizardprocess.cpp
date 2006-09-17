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
#include <qstring.h>
#include <qtimer.h>

#include <QTextEdit>
#include <QRadioButton>

#include <k3listview.h>
#include <klineedit.h>
#include <kmessagebox.h>
#include <kprogressdialog.h>

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

telescopeWizardProcess::telescopeWizardProcess( QWidget* parent, const char* /*name*/ ) : QDialog(parent)
{
   currentPort  = -1;
   timeOutCount = 0;
   indiDev = NULL;
   progressScan = NULL;
   linkRejected = false;
   ui = new Ui::telescopeWizard();
   ui->setupUi(this);

   QString locStr;
   QFile sideIMG;

   if (KSUtils::openDataFile(sideIMG, "wizardside.png"))
       ui->wizardPix->setPixmap(QPixmap(sideIMG.fileName()));

   ui->backB->hide();
   currentPage = INTRO_P;

   ksw = (KStars *) parent;

   ksw->establishINDI();

   indimenu   = ksw->getINDIMenu();
   indidriver = ksw->getINDIDriver();

   INDIMessageBar = Options::indiMessages();
   Options::setIndiMessages( false );

  QTime newTime( ksw->data()->lt().time() );
  ExtDate newDate( ksw->data()->lt().date() );

  ui->timeOut->setText( QString().sprintf("%02d:%02d:%02d", newTime.hour(), newTime.minute(), newTime.second()));
  ui->dateOut->setText( QString().sprintf("%d-%02d-%02d", newDate.year(), newDate.month(), newDate.day()));

  if (ksw->geo()->translatedProvince().isEmpty())
  	ui->locationOut->setText( QString("%1, %2").arg(ksw->geo()->translatedName()).arg(ksw->geo()->translatedCountry()));
  else
  	ui->locationOut->setText( QString("%1, %2, %3").arg(ksw->geo()->translatedName())
  						     .arg(ksw->geo()->translatedProvince())
						     .arg(ksw->geo()->translatedCountry()));


   //for (unsigned int i=0; i < indidriver->devices.size(); i++)
    foreach (IDevice *device, indidriver->devices)
              if (device->deviceType == KSTARS_TELESCOPE)
   		    ui->telescopeCombo->addItem(device->label);

   if ( !Options::indiTelescopePort().isEmpty())
    portList << Options::indiTelescopePort();
    
    portList << "/dev/ttyS0" <<  "/dev/ttyS1" << "/dev/ttyS2" << "/dev/ttyS3" << "/dev/ttyS4"
             << "/dev/ttyUSB0" << "/dev/ttyUSB1" << "/dev/ttyUSB2" << "/dev/ttyUSB3";// << "/dev/ttyUSB4";

   connect(ui->helpB, SIGNAL(clicked()), parent, SLOT(appHelpActivated()));
   connect(ui->nextB, SIGNAL(clicked()), this, SLOT(processNext()));
   connect(ui->backB, SIGNAL(clicked()), this, SLOT(processBack()));
   connect(ui->setTimeB, SIGNAL(clicked()), this, SLOT(newTime()));
   connect(ui->setLocationB, SIGNAL(clicked()), this, SLOT(newLocation()));

   newDeviceTimer = new QTimer(this);
   QObject::connect( newDeviceTimer, SIGNAL(timeout()), this, SLOT(processPort()) );

}

telescopeWizardProcess::~telescopeWizardProcess()
{
    Options::setIndiMessages( INDIMessageBar );

    //Reset();
}

void telescopeWizardProcess::processNext(void)
{
  int linkResult=0;

 switch (currentPage)
 {
   case INTRO_P:
     currentPage++;
     ui->backB->show();
     ui->wizardContainer->setCurrentIndex(currentPage);
     break;
  case MODEL_P:
     currentPage++;
     ui->wizardContainer->setCurrentIndex(currentPage);
     break;
  case TELESCOPE_P:
     currentPage++;
     ui->wizardContainer->setCurrentIndex(currentPage);
     break;
  case LOCAL_P:
     currentPage++;
     ui->wizardContainer->setCurrentIndex(currentPage);
     break;
  case PORT_P:
     linkResult = establishLink();
     if ( linkResult == 1)
     {
	progressScan = new KProgressDialog(this, i18n("Autoscan"), i18n("Please wait while KStars scan communication ports for attached telescopes.\nThis process might take few minutes to complete."), true);
//   progressScan->setAllowCancel(true);
   	progressScan->setAutoClose(true);
   	progressScan->setAutoReset(true);
   	progressScan->progressBar()->setMinimum(0);
   	progressScan->progressBar()->setMaximum(portList.count());
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
     ui->backB->hide();
     ui->wizardContainer->setCurrentIndex(currentPage);
     break;
  case TELESCOPE_P:
     currentPage--;
     ui->wizardContainer->setCurrentIndex(currentPage);
     break;
  case LOCAL_P:
     currentPage--;
     ui->wizardContainer->setCurrentIndex(currentPage);
     break;
  case PORT_P:
     currentPage--;
     ui->wizardContainer->setCurrentIndex(currentPage);
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

		ui->timeOut->setText( QString().sprintf("%02d:%02d:%02d", dt.time().hour(), dt.time().minute(), dt.time().second()));
		ui->dateOut->setText( QString().sprintf("%d-%02d-%02d", dt.date().year(), dt.date().month(), dt.date().day()));
	}
}

void telescopeWizardProcess::newLocation()
{

   ksw->slotGeoLocator();

   ui->locationOut->setText( QString("%1, %2, %3").arg(ksw->geo()->translatedName())
  					     .arg(ksw->geo()->translatedProvince())
					     .arg(ksw->geo()->translatedCountry()));
   ui->timeOut->setText( QString().sprintf("%02d:%02d:%02d", ksw->data()->lt().time().hour(), ksw->data()->lt().time().minute(), ksw->data()->lt().time().second()));

  ui->dateOut->setText( QString().sprintf("%d-%02d-%02d", ksw->data()->lt().date().year(),
  ksw->data()->lt().date().month() ,ksw->data()->lt().date().day()));



}

int telescopeWizardProcess::establishLink()
{
	QList <QTreeWidgetItem *> found;
	if (!indidriver || !indimenu)
	  return (0);
	  
	
	QTreeWidgetItem *driverItem = NULL;
	
         
	found = indidriver->ui->localTreeWidget->findItems(ui->telescopeCombo->currentText(), Qt::MatchExactly | Qt::MatchRecursive);
	
	if (found.empty()) return -1;
	driverItem = found.first();

	// If device is already running, we need to shut it down first
	if (indidriver->isDeviceRunning(ui->telescopeCombo->currentText()))
	{
		indidriver->ui->localTreeWidget->setCurrentItem(driverItem);
		indidriver->processDeviceStatus(1);
	}
	   
	// Set custome label for device
	indimenu->setCustomLabel(ui->telescopeCombo->currentText());
	currentDevice = indimenu->currentLabel;
	// Select it
	indidriver->ui->localTreeWidget->setCurrentItem(driverItem);
	// Make sure we start is locally
	indidriver->ui->localR->setChecked(true);
	// Run it
	indidriver->processDeviceStatus(0);

	if (!indidriver->isDeviceRunning(ui->telescopeCombo->currentText()))
	 return (3);

	newDeviceTimer->start(1500);

	if (ui->portIn->text().isEmpty())
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
     if (ui->portIn->text().isEmpty())
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

     lp->write_w->setText(ui->portIn->text());
     
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

     if (progressScan->wasCancelled())
     {
      linkRejected = true;
      indidriver->processDeviceStatus(1);
      Reset();
      return;
     }

     progressScan->progressBar()->setValue(currentPort);

     if ( currentPort >= portList.count())
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
     if (!lp) return;
     
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
  {
	delete (progressScan);
	progressScan = NULL;
  }
  indiDev = NULL;

}

#include "telescopewizardprocess.moc"
