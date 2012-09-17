/*  Telescope wizard
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "telescopewizardprocess.h"

#include <QFile>
#include <QPixmap>
#include <QTimer>

#include <KMessageBox>
#include <KProgressDialog>
#include <KDateTime>
#include <KStatusBar>

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "dialogs/timedialog.h"
#include "ksutils.h"

#include "guimanager.h"
#include "indilistener.h"
#include "driverinfo.h"
#include "drivermanager.h"
#include "indielement.h"
#include "indiproperty.h"
#include "indistd.h"
#include "indidevice.h"

#define TIMEOUT_THRESHHOLD	20

telescopeWizardProcess::telescopeWizardProcess( QWidget* parent, const char* /*name*/ ) : QDialog(parent)
{
    currentPort  = -1;
    timeOutCount = 0;
    progressScan = NULL;
    linkRejected = false;
    ui = new Ui::telescopeWizard();
    ui->setupUi(this);

    QString locStr;
    QFile sideIMG;

    if (KSUtils::openDataFile(sideIMG, "wzscope.png"))
        ui->wizardPix->setPixmap(QPixmap(sideIMG.fileName()));

    ui->backB->hide();
    currentPage = INTRO_P;

    INDIMessageBar = Options::showINDIMessages();
    Options::setShowINDIMessages( false );

    QTime newTime( KStars::Instance()->data()->lt().time() );
    QDate newDate( KStars::Instance()->data()->lt().date() );

    ui->timeOut->setText( QString().sprintf("%02d:%02d:%02d", newTime.hour(), newTime.minute(), newTime.second()));
    ui->dateOut->setText( QString().sprintf("%d-%02d-%02d", newDate.year(), newDate.month(), newDate.day()));

    // FIXME is there a better way to check if a translated message is empty?
    //if (KStars::Instance()->data()->geo()->translatedProvince().isEmpty())
    if (KStars::Instance()->data()->geo()->translatedProvince() == QString("(I18N_EMPTY_MESSAGE)"))
        ui->locationOut->setText( QString("%1, %2").arg(KStars::Instance()->data()->geo()->translatedName()).arg(KStars::Instance()->data()->geo()->translatedCountry()));
    else
        ui->locationOut->setText( QString("%1, %2, %3").arg(KStars::Instance()->data()->geo()->translatedName())
                                  .arg(KStars::Instance()->data()->geo()->translatedProvince())
                                  .arg(KStars::Instance()->data()->geo()->translatedCountry()));


    foreach (DriverInfo *dv, DriverManager::Instance()->getDrivers())
    {
        if (dv->getType() == KSTARS_TELESCOPE)
        {
            ui->telescopeCombo->addItem(dv->getTreeLabel());
            driversList[dv->getTreeLabel()] = dv;
        }
    }

    portList << "/dev/ttyS0" <<  "/dev/ttyS1" << "/dev/ttyS2" << "/dev/ttyS3" << "/dev/ttyS4"
    << "/dev/ttyUSB0" << "/dev/ttyUSB1" << "/dev/ttyUSB2" << "/dev/ttyUSB3";

    connect(ui->helpB, SIGNAL(clicked()), parent, SLOT(appHelpActivated()));
    connect(ui->cancelB, SIGNAL(clicked()), this, SLOT(cancelCheck()));
    connect(ui->nextB, SIGNAL(clicked()), this, SLOT(processNext()));
    connect(ui->backB, SIGNAL(clicked()), this, SLOT(processBack()));
    connect(ui->setTimeB, SIGNAL(clicked()), this, SLOT(newTime()));
    connect(ui->setLocationB, SIGNAL(clicked()), this, SLOT(newLocation()));

    //newDeviceTimer = new QTimer(this);
    //QObject::connect( newDeviceTimer, SIGNAL(timeout()), this, SLOT(processPort()) );

}

telescopeWizardProcess::~telescopeWizardProcess()
{
    Options::setShowINDIMessages( INDIMessageBar );
    delete ui;
    //Reset();
}

//Called when cancel is clicked, gives a warning if past the first couple of steps
void telescopeWizardProcess::cancelCheck(void)
{
    switch (currentPage)
    {
    case TELESCOPE_P:
    case LOCAL_P:
    case PORT_P:
	if ( KMessageBox::warningYesNo(0, i18n("Are you sure you want to cancel?")) == KMessageBox::Yes )
	    emit reject();
	break;
    default:
	emit reject();
	break;
    }
}

void telescopeWizardProcess::processNext(void)
{

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
    TimeDialog timedialog (KStars::Instance()->data()->lt(), KStars::Instance()->data()->geo(), KStars::Instance());

    if ( timedialog.exec() == QDialog::Accepted )
    {
        KStarsDateTime dt( timedialog.selectedDate(), timedialog.selectedTime() );
        KStars::Instance()->data()->changeDateTime( dt );

        ui->timeOut->setText( QString().sprintf("%02d:%02d:%02d", dt.time().hour(), dt.time().minute(), dt.time().second()));
        ui->dateOut->setText( QString().sprintf("%d-%02d-%02d", dt.date().year(), dt.date().month(), dt.date().day()));
    }
}

void telescopeWizardProcess::newLocation()
{
    KStars::Instance()->slotGeoLocator();
    GeoLocation* geo = KStars::Instance()->data()->geo();
    ui->locationOut->setText( QString("%1, %2, %3")
                                  .arg( geo->translatedName() )
                                  .arg( geo->translatedProvince() )
                                  .arg( geo->translatedCountry()) );
    ui->timeOut->setText( QString().sprintf("%02d:%02d:%02d",
                                            KStars::Instance()->data()->lt().time().hour(),
                                            KStars::Instance()->data()->lt().time().minute(),
                                            KStars::Instance()->data()->lt().time().second()));
    ui->dateOut->setText( QString().sprintf("%d-%02d-%02d",
                                            KStars::Instance()->data()->lt().date().year(),
                                            KStars::Instance()->data()->lt().date().month(),
                                            KStars::Instance()->data()->lt().date().day()));
}

void telescopeWizardProcess::establishLink()
{
    managedDevice.clear();

    DriverInfo *dv = driversList.value(ui->telescopeCombo->currentText());

    if (dv == NULL)
        return;

    managedDevice.append(dv);
    connect(INDIListener::Instance(), SIGNAL(newDevice(ISD::GDInterface*)), this, SLOT(processTelescope(ISD::GDInterface*)));

    if (ui->portIn->text().isEmpty())
    {
        progressScan = new KProgressDialog(this, i18n("Telescope Wizard"),
                                           i18n("Please wait while KStars scan communication ports for "
                                                "attached telescopes.\nThis process might take few "
                                                "minutes to complete."), Qt::Dialog);
        progressScan->progressBar()->setValue(0);
        //kDebug() << "KProgressDialog for automatic search has been initiated";

    }
    else
    {
        progressScan = new KProgressDialog(this, i18n("Telescope Wizard"), i18n("Please wait while KStars tries to connect to your telescope..."), Qt::Dialog);
        progressScan->progressBar()->setValue(portList.count());
        //kDebug() << "KProgressDialog for manual search has been initiated";
    }

    progressScan->setAutoClose(true);
    progressScan->setAutoReset(true);
    progressScan->progressBar()->setMinimum(0);
    progressScan->progressBar()->setMaximum(portList.count());
    progressScan->show();

    if (dv->getClientState() == false)
    {
        if (DriverManager::Instance()->startDevices(managedDevice) == false)
        {
            Reset();
            close();
            return;
        }
    }
    else
       processTelescope(INDIListener::Instance()->getDevice(dv->getName()));


}

void telescopeWizardProcess::processTelescope(ISD::GDInterface *telescope)
{    
    if (telescope == NULL)
        return;

    scopeDevice = telescope;

    // port empty, start autoscan
    if (ui->portIn->text().isEmpty())
    {
        //newDeviceTimer->stop();
        linkRejected = false;
        connect(scopeDevice, SIGNAL(Connected()), this, SLOT(linkSuccess()), Qt::QueuedConnection);
        connect(scopeDevice, SIGNAL(Disconnected()), this, SLOT(scanPorts()), Qt::QueuedConnection);
        scanPorts();
    }
    else
    {
        QString scopePort = ui->portIn->text();

        scopeDevice->runCommand(INDI_SET_PORT, &scopePort);

        scopeDevice->runCommand(INDI_CONNECT);

        Options::setShowINDIMessages( INDIMessageBar );

        close();

    }



    /*
    pp = indiDev->findProp("DEVICE_PORT");
    if (!pp) return;
    lp = pp->findElement("PORT");
    if (!lp) return;

    lp->write_w->setText(ui->portIn->text());

    pp = indiDev->findProp("CONNECTION");
    if (!pp) return;

    //newDeviceTimer->stop();
    */



    /*lp = pp->findElement("CONNECT");
    pp->newSwitch(lp);

    Reset();

    indimenu->show();*/



}

void telescopeWizardProcess::scanPorts()
{
    if (progressScan == NULL)
    {
        close();
        return;
    }

    currentPort++;

    if (progressScan->wasCancelled())
    {
        if (linkRejected)
            return;

        disconnect(this, SLOT(linkSuccess()));
        disconnect(this, SLOT(scanPorts()));

        DriverManager::Instance()->stopDevices(managedDevice);
        linkRejected = true;
        Reset();
        return;
    }

    progressScan->progressBar()->setValue(currentPort);

    //qDebug() << "Current port is " << currentPort << " and port count is " << portList.count() << endl;

    if ( currentPort >= portList.count())
    {
        if (linkRejected)
            return;

        disconnect(this, SLOT(scanPorts()));
        disconnect(this, SLOT(linkSuccess()));
        linkRejected = true;
        DriverManager::Instance()->stopDevices(managedDevice);
        Reset();

        KMessageBox::sorry(0, i18n("Sorry. KStars failed to detect any attached telescopes, please check your settings and try again."));
        return;
    }

    QString scopePort = portList[currentPort];
    scopeDevice->runCommand(INDI_SET_PORT, &scopePort);

    scopeDevice->runCommand(INDI_CONNECT);

}

void telescopeWizardProcess::linkSuccess()
{
    Reset();

    KStars::Instance()->statusBar()->changeItem( i18n("Telescope Wizard completed successfully."), 0);

    close();

    GUIManager::Instance()->show();

}

void telescopeWizardProcess::Reset()
{
    currentPort = -1;
    timeOutCount = 0;
    linkRejected = false;

    delete (progressScan);
    progressScan = NULL;

}

#include "telescopewizardprocess.moc"
