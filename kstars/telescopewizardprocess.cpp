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

#include <klistview.h>
#include <klineedit.h>
#include <kmessagebox.h>

#include "kstars.h"
#include "telescopewizardprocess.h"
#include "timedialog.h"
#include "ksutils.h"

#include "indimenu.h"
#include "indidriver.h"
#include "indidevice.h"

telescopeWizardProcess::telescopeWizardProcess( QWidget* parent, const char* name ) : telescopeWizard(parent, name)//, INTRO_P(0), MODEL_P(1), TELESCOPE_P(2), LOCAL_P(3), PORT_P(4)
{
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
     KMessageBox::information(0, i18n("Please wait while KStars tries to connect to your telescope..."));
     establishLink();
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

void telescopeWizardProcess::establishLink()
{
        fprintf(stderr, "in establish Link\n");

	QListViewItem *driverItem = NULL;

        driverItem = indidriver->localListView->findItem(QString(telescopeCombo->currentText()), 0);
	currentLabel = new char[ strlen(telescopeCombo->currentText().ascii()) + 1];
	indimenu->getCustomLabel(telescopeCombo->currentText().ascii(), currentLabel);

	fprintf(stderr, "current Label is %s\n", currentLabel);

	if (driverItem)
	{
	 indidriver->localListView->setSelected(driverItem, true);
	 indidriver->processDeviceStatus(0);
	}

	newDeviceTimer->start(1000);
}

void telescopeWizardProcess::processPort()
{

     fprintf(stderr, "in process Port, searching for label %s\n", currentLabel);

     INDI_P * pp;

     INDI_D * indiDev = indimenu->findDeviceByLabel(currentLabel);

     if (!indiDev)
      return;

     fprintf(stderr, "Device Found!, looking for Ports\n");

     pp = indiDev->findProp("Ports");

     if (!pp)
       return;

    fprintf(stderr, "Ports found, looking for connection\n");

     if (!portIn->text().isEmpty())
     {
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
     }

     pp = indiDev->findProp("CONNECTION");

     if (!pp)
      return;

     fprintf(stderr, "connection Property found\n");

     newDeviceTimer->stop();

     pp->newSwitch(0);

     close();

}



