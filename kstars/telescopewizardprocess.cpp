/*  Telescope wizard
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <qpixmap.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qwidgetstack.h>
#include <qdatetime.h>
#include <qstring.h>
#include <qtimer.h>
#include <qtable.h>
#include <qtextedit.h>

#include <klistview.h>
#include <klineedit.h>
#include <kmessagebox.h>
#include <kprogress.h>

#include "kstars.h"
#include "telescopewizardprocess.h"
#include "timedialog.h"
#include "ksutils.h"

#include "indimenu.h"
#include "indidriver.h"
#include "indidevice.h"

#define TIMEOUT_THRESHHOLD	20

telescopeWizardProcess::telescopeWizardProcess( QWidget* parent, const char* name ) : telescopeWizard(parent, name)//, INTRO_P(0), MODEL_P(1), TELESCOPE_P(2), LOCAL_P(3), PORT_P(4)
{
   currentPort  = -1;
   timeOutCount = 0;
   indiDev = NULL;

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

   INDIMessageBar = ksw->options()->indiMessages;
   ksw->options()->indiMessages = false;

  QTime newTime( ksw->data()->LTime.time() );
  QDate newDate( ksw->data()->LTime.date() );

  timeOut->setText( QString().sprintf("%02d:%02d:%02d", newTime.hour(), newTime.minute(), newTime.second()));
  dateOut->setText( QString().sprintf("%d-%02d-%02d", newDate.year(), newDate.month(), newDate.day()));

  locationOut->setText( QString("%1, %2, %3").arg(ksw->geo()->translatedName())
  					     .arg(ksw->geo()->translatedProvince())
					     .arg(ksw->geo()->translatedCountry()));


   for (unsigned int i=0; i < indidriver->devices.size(); i++)
   		telescopeCombo->insertItem(i18n(indidriver->devices[i]->name));

   connect(helpB, SIGNAL(clicked()), parent, SLOT(appHelpActivated()));
   connect(nextB, SIGNAL(clicked()), this, SLOT(processNext()));
   connect(backB, SIGNAL(clicked()), this, SLOT(processBack()));
   connect(setTimeB, SIGNAL(clicked()), this, SLOT(newTime()));
   connect(setLocationB, SIGNAL(clicked()), this, SLOT(newLocation()));

   newDeviceTimer = new QTimer(this);
   QObject::connect( newDeviceTimer, SIGNAL(timeout()), this, SLOT(processPort()) );

}

void telescopeWizardProcess::processNext(void)
{
 // for now, just display the next page, and restart once we reached the end

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
     if (establishLink())
     {

progressScan = new KProgressDialog(this, "autoscan", i18n("Autoscan"), i18n("Please wait while KStars scan communication ports for attached telescopes.\n This process might take few minutes to complete."), true);
   progressScan->setAllowCancel(true);
   progressScan->setAutoClose(true);
   progressScan->setAutoReset(true);
   progressScan->progressBar()->setRange(0, 11);
   progressScan->progressBar()->setValue(0);
   progressScan->show();
    }
     else
      KMessageBox::queuedMessageBox(0, KMessageBox::Information, i18n("Please wait while KStars tries to connect to your telescope..."));
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

  TimeDialog timedialog (ksw->data()->LTime, ksw);

	if ( timedialog.exec() == QDialog::Accepted )
	{
		QTime newTime( timedialog.selectedTime() );
		QDate newDate( timedialog.selectedDate() );

		ksw->data()->changeTime(newDate, newTime);

  		timeOut->setText( QString().sprintf("%02d:%02d:%02d", newTime.hour(), newTime.minute(), newTime.second()));
  		dateOut->setText( QString().sprintf("%d-%02d-%02d", newDate.year(), newDate.month(), newDate.day()));
	}

}

void telescopeWizardProcess::newLocation()
{

   ksw->slotGeoLocator();

   locationOut->setText( QString("%1, %2, %3").arg(ksw->geo()->translatedName())
  					     .arg(ksw->geo()->translatedProvince())
					     .arg(ksw->geo()->translatedCountry()));
   timeOut->setText( QString().sprintf("%02d:%02d:%02d", ksw->data()->LTime.time().hour(), ksw->data()->LTime.time().minute(), ksw->data()->LTime.time().second()));

  dateOut->setText( QString().sprintf("%d-%02d-%02d", ksw->data()->LTime.date().year(),
  ksw->data()->LTime.date().month() ,ksw->data()->LTime.date().day()));



}

int telescopeWizardProcess::establishLink()
{

	if (!indidriver || !indimenu)
	  return (0);

	QListViewItem *driverItem = NULL;

	if (!indidriver->isDeviceRunning(telescopeCombo->currentText().ascii()))
	{
        	driverItem = indidriver->localListView->findItem(telescopeCombo->currentText(), 0);
		currentLabel = new char[ strlen(telescopeCombo->currentText().ascii()) + 1];
		indimenu->getCustomLabel(telescopeCombo->currentText().ascii(), currentLabel);

		if (driverItem)
		{

	 		indidriver->localListView->setSelected(driverItem, true);
	 		indidriver->processDeviceStatus(0);
		}
	}
	else
	{
	  currentLabel = new char [strlen(telescopeCombo->currentText().ascii())+1];
	  strcpy(currentLabel, telescopeCombo->currentText().ascii());
	}

//	fprintf(stderr, "current Label is %s\n", currentLabel);

	newDeviceTimer->start(1500);

	if (portIn->text().isEmpty())
	 return (1);
        else
	 return (0);

}

void telescopeWizardProcess::processPort()
{
     INDI_P * pp;


     if (!indidriver || !indimenu)
       return;


     timeOutCount++;

     if (timeOutCount >= TIMEOUT_THRESHHOLD)
     {
       Reset();
       KMessageBox::error(0, i18n("Error: connection timeout. Unable to communicate with an INDI server"));
       close();
       return;
     }


    indiDev = indimenu->findDeviceByLabel(currentLabel);

    if (!indiDev)
      return;

     // port empty, start autoscan
     if (portIn->text().isEmpty())
     {
       newDeviceTimer->stop();
       connect(indiDev, SIGNAL(linkRejected()), this, SLOT(scanPorts()));
       connect(indiDev, SIGNAL(linkAccepted()), this, SLOT(linkSuccess()));
       scanPorts();
       return;
     }

     pp = indiDev->findProp("Ports");

     if (!pp)
       return;


     if (pp->perm == PP_RW)
     {
            	pp->table_w->setText(0, 1, portIn->text());
		pp->newText();
     }
	    else if (pp->perm == PP_WO)
     {
		pp->table_w->setText(0, 0, portIn->text());
		pp->newText();
     }

     pp = indiDev->findProp("CONNECTION");

     if (!pp)
      return;

     newDeviceTimer->stop();

     ksw->options()->indiMessages = INDIMessageBar;

     pp->newSwitch(0);

     timeOutCount = 0;

     indimenu->show();

     close();

}

void telescopeWizardProcess::scanPorts()
{

     if (!indiDev)
      return;

     if (!indidriver || !indimenu)
     	  return;


     currentPort++;

     //fprintf(stderr, "\n *** in scan ports, port is %d \n", currentPort);

     if (currentPort > 9)
     {
      Reset();
      KMessageBox::sorry(0, i18n("Sorry. KStars failed to detect any attached telescopes, please check your settings and try again."));
      //close();
      return;
     }

     if (progressScan->wasCancelled())
     {
       Reset();
       return;
     }

     progressScan->progressBar()->setValue(currentPort);

     INDI_P * pp;

     indiDev->msgST_w->clear();

     pp = indiDev->findProp("Ports");

     if (!pp)
       return;

    if (pp->perm == PP_RW)
    	{
		pp->table_w->setText(0, 1, QString("/dev/ttyS%1").arg(currentPort));
		pp->newText();
	}
	    else if (pp->perm == PP_WO)
	{
		pp->table_w->setText(0, 0, QString("/dev/ttyS%1").arg(currentPort));
		pp->newText();
     	}

     pp = indiDev->findProp("CONNECTION");

     if (!pp)
      return;

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
  progressScan->close();

  ksw->options()->indiMessages = INDIMessageBar;
  indiDev = NULL;

}




