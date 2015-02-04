/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "ekosmanager.h"

#include <basedevice.h>

#include "Options.h"
#include "kstars.h"

#include <KMessageBox>
#include <QComboBox>

#include <config-kstars.h>

#include "indi/clientmanager.h"
#include "indi/indielement.h"
#include "indi/indiproperty.h"
#include "indi/driverinfo.h"
#include "indi/drivermanager.h"
#include "indi/indilistener.h"
#include "indi/guimanager.h"

#include "ekosadaptor.h"

#define MAX_REMOTE_INDI_TIMEOUT 55000

EkosManager::EkosManager(QWidget *parent)
        : QDialog(parent)
{
    setupUi(this);

    new EkosAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos",  this);

    nDevices=0;
    useGuideHead    =false;
    useST4          =false;
    ccdStarted      =false;
    ccdDriverSelected =false;
    scopeRegistered = false;
    remoteCCDRegistered    = false;
    remoteGuideRegistered  = false;

    scope   =  NULL;
    ccd     =  NULL;
    guider  =  NULL;
    focuser =  NULL;
    filter  =  NULL;
    aux     =  NULL;
    ao      =  NULL;
    dome    =  NULL;

    scope_di   = NULL;
    ccd_di     = NULL;
    guider_di  = NULL;
    focuser_di = NULL;
    filter_di  = NULL;
    aux_di     = NULL;
    ao_di      = NULL;
    dome_di    = NULL;
    remote_indi= NULL;

    captureProcess = NULL;
    focusProcess   = NULL;
    guideProcess   = NULL;
    alignProcess   = NULL;
    mountProcess   = NULL;

    kcfg_localMode->setChecked(Options::localMode());
    kcfg_remoteMode->setChecked(Options::remoteMode());

    connect(toolsWidget, SIGNAL(currentChanged(int)), this, SLOT(processTabChange()));

    toolsWidget->setTabEnabled(1, false);

    connect(processINDIB, SIGNAL(clicked()), this, SLOT(processINDI()));

    connect(connectB, SIGNAL(clicked()), this, SLOT(connectDevices()));

    connect(disconnectB, SIGNAL(clicked()), this, SLOT(disconnectDevices()));

    connect(controlPanelB, SIGNAL(clicked()), GUIManager::Instance(), SLOT(show()));

    connect(optionsB, SIGNAL(clicked()), KStars::Instance(), SLOT(slotViewOps()));

    connect(clearB, SIGNAL(clicked()), this, SLOT(clearLog()));

    connect(kcfg_localMode, SIGNAL(toggled(bool)), this, SLOT(processINDIModeChange()));

    localMode = Options::localMode();

    if (localMode)
        initLocalDrivers();
    else
        initRemoteDrivers();

    playFITSFile  = new QMediaPlayer();
    playFITSFile->setMedia(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::DataLocation, "ekos-fits.ogg" )));

    playOkFile    = new QMediaPlayer();
    playOkFile->setMedia(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::DataLocation, "ekos-ok.ogg" )));

    playErrorFile = new QMediaPlayer();
    playErrorFile->setMedia(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::DataLocation, "ekos-error.ogg" )));
}

EkosManager::~EkosManager()
{
    delete captureProcess;
    delete focusProcess;
    delete guideProcess;
    delete alignProcess;
    delete mountProcess;
    delete playFITSFile;
    delete playOkFile;
    delete playErrorFile;
}

void EkosManager::setConnectionMode(bool isLocal)
{
    if (isLocal)
        kcfg_localMode->setChecked(true);
    else
        kcfg_remoteMode->setChecked(true);
}

void EkosManager::processINDIModeChange()
{
    Options::setLocalMode(kcfg_localMode->isChecked());
    Options::setRemoteMode(kcfg_remoteMode->isChecked());

    bool newLocalMode = kcfg_localMode->isChecked();

    if (newLocalMode == localMode)
        return;

    if (managedDevices.count() > 0 || remote_indi != NULL)
    {
        KMessageBox::error(0, i18n("Cannot switch modes while INDI services are running."), i18n("Ekos Mode"));
        kcfg_localMode->setChecked(!newLocalMode);
        kcfg_remoteMode->setChecked(newLocalMode);
        return;
    }

    localMode = newLocalMode;

    reset();

    if (localMode)
        initLocalDrivers();
    else
        initRemoteDrivers();


}

void EkosManager::initLocalDrivers()
{

    telescopeCombo->clear();
    ccdCombo->clear();
    guiderCombo->clear();
    AOCombo->clear();
    focuserCombo->clear();
    filterCombo->clear();
    domeCombo->clear();
    auxCombo->clear();

    telescopeCombo->addItem("--");
    ccdCombo->addItem("--");
    guiderCombo->addItem("--");
    AOCombo->addItem("--");
    focuserCombo->addItem("--");
    filterCombo->addItem("--");
    domeCombo->addItem("--");
    auxCombo->addItem("--");

    foreach(DriverInfo *dv, DriverManager::Instance()->getDrivers())
    {
        switch (dv->getType())
        {
        case KSTARS_TELESCOPE:
             telescopeCombo->addItem(dv->getTreeLabel());
             break;

        case KSTARS_CCD:
            ccdCombo->addItem(dv->getTreeLabel());
            guiderCombo->addItem(dv->getTreeLabel());
            break;

        case KSTARS_ADAPTIVE_OPTICS:
            AOCombo->addItem(dv->getTreeLabel());
            break;

        case KSTARS_FOCUSER:
            focuserCombo->addItem(dv->getTreeLabel());
            break;

        case KSTARS_FILTER:
            filterCombo->addItem(dv->getTreeLabel());
            break;

        case KSTARS_DOME:
            domeCombo->addItem(dv->getTreeLabel());
            break;

        case KSTARS_AUXILIARY:
            auxCombo->addItem(dv->getTreeLabel());
            break;

        default:
            continue;
            break;

        }

        driversList[dv->getTreeLabel()] = dv;
    }

    loadDefaultDrivers();


}

void EkosManager::initRemoteDrivers()
{

    telescopeCombo->clear();
    ccdCombo->clear();
    guiderCombo->clear();
    AOCombo->clear();
    focuserCombo->clear();
    filterCombo->clear();
    domeCombo->clear();
    auxCombo->clear();

    if (Options::remoteScopeName().isEmpty() == false)
        telescopeCombo->addItem(Options::remoteScopeName());
    else
        telescopeCombo->addItem("--");


    if (Options::remoteCCDName().isEmpty() == false)
        ccdCombo->addItem(Options::remoteCCDName());
    else
        ccdCombo->addItem("--");

    if (Options::remoteGuiderName().isEmpty() == false)
        guiderCombo->addItem(Options::remoteGuiderName());
    else
        guiderCombo->addItem("--");

    if (Options::remoteAOName().isEmpty() == false)
        AOCombo->addItem(Options::remoteAOName());
    else
        AOCombo->addItem("--");

    if (Options::remoteFocuserName().isEmpty() == false)
        focuserCombo->addItem(Options::remoteFocuserName());
    else
        focuserCombo->addItem("--");

    if (Options::remoteFilterName().isEmpty() == false)
        filterCombo->addItem(Options::remoteFilterName());
    else
        filterCombo->addItem("--");

    if (Options::remoteDomeName().isEmpty() == false)
        domeCombo->addItem(Options::remoteDomeName());
    else
        domeCombo->addItem("--");

    if (Options::remoteAuxName().isEmpty() == false)
        auxCombo->addItem(Options::remoteAuxName());
    else
        auxCombo->addItem("--");

}

void EkosManager::reset()
{
    nDevices=0;

    useGuideHead           = false;
    guideStarted           = false;
    useST4                 = false;
    ccdStarted             = false;
    ccdDriverSelected      = false;
    scopeRegistered        = false;
    remoteCCDRegistered    = false;
    remoteGuideRegistered  = false;

    removeTabs();

    scope   =  NULL;
    ccd     =  NULL;
    guider  =  NULL;
    focuser =  NULL;
    filter  =  NULL;
    aux     =  NULL;
    dome    =  NULL;
    ao      =  NULL;

    scope_di   = NULL;
    ccd_di     = NULL;
    guider_di  = NULL;
    focuser_di = NULL;
    filter_di  = NULL;
    aux_di     = NULL;
    dome_di    = NULL;
    ao_di      = NULL;

    captureProcess = NULL;
    focusProcess   = NULL;
    guideProcess   = NULL;
    alignProcess   = NULL;
    mountProcess   = NULL;

    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);
    processINDIB->setEnabled(true);

    processINDIB->setText(xi18n("Start INDI"));
}

void EkosManager::loadDefaultDrivers()
{
    QString TelescopeDriver = Options::telescopeDriver();
    QString CCDDriver = Options::cCDDriver();
    QString GuiderDriver = Options::guiderDriver();
    QString AODriver = Options::aODriver();
    QString FocuserDriver = Options::focuserDriver();
    QString FilterDriver = Options::filterDriver();
    QString DomeDriver   = Options::domeDriver();
    QString AuxDriver = Options::auxDriver();

    if (TelescopeDriver.isEmpty() == false && TelescopeDriver != "--")
    {
        for (int i=0; i < telescopeCombo->count(); i++)
            if (telescopeCombo->itemText(i) == TelescopeDriver)
            {
                telescopeCombo->setCurrentIndex(i);
                break;
            }
    }

    if (CCDDriver.isEmpty() == false && CCDDriver != "--")
    {
        for (int i=0; i < ccdCombo->count(); i++)
            if (ccdCombo->itemText(i) == CCDDriver)
            {
                ccdCombo->setCurrentIndex(i);
                break;
            }
    }

    if (GuiderDriver.isEmpty() == false && GuiderDriver != "--")
    {
        for (int i=0; i < guiderCombo->count(); i++)
            if (guiderCombo->itemText(i) == GuiderDriver)
            {
                guiderCombo->setCurrentIndex(i);
                break;
            }
    }

    if (AODriver.isEmpty() == false && AODriver != "--")
    {
        for (int i=0; i < AOCombo->count(); i++)
            if (AOCombo->itemText(i) == AODriver)
            {
                AOCombo->setCurrentIndex(i);
                break;
            }
    }

    if (FilterDriver.isEmpty() == false && FilterDriver != "--")
    {
        for (int i=0; i < filterCombo->count(); i++)
            if (filterCombo->itemText(i) == FilterDriver)
            {
                filterCombo->setCurrentIndex(i);
                break;
            }
    }

    if (FocuserDriver.isEmpty() == false && FocuserDriver != "--")
    {
        for (int i=0; i < focuserCombo->count(); i++)
            if (focuserCombo->itemText(i) == FocuserDriver)
            {
                focuserCombo->setCurrentIndex(i);
                break;
            }
    }

    if (DomeDriver.isEmpty() == false && DomeDriver != "--")
    {
        for (int i=0; i < domeCombo->count(); i++)
            if (domeCombo->itemText(i) == DomeDriver)
            {
                domeCombo->setCurrentIndex(i);
                break;
            }
    }

    if (AuxDriver.isEmpty() == false && AuxDriver != "--")
    {
        for (int i=0; i < auxCombo->count(); i++)
            if (auxCombo->itemText(i) == AuxDriver)
            {
                auxCombo->setCurrentIndex(i);
                break;
            }
    }

}

void EkosManager::saveDefaultDrivers()
{

   Options::setTelescopeDriver(telescopeCombo->currentText());

   Options::setCCDDriver(ccdCombo->currentText());

   Options::setGuiderDriver(guiderCombo->currentText());

   Options::setFilterDriver(filterCombo->currentText());

   Options::setFocuserDriver(focuserCombo->currentText());

   Options::setAuxDriver(auxCombo->currentText());

   Options::setDomeDriver(domeCombo->currentText());

   Options::setAODriver(AOCombo->currentText());

}

void EkosManager::processINDI()
{
    if (managedDevices.count() > 0 || remote_indi != NULL)
    {
        stop();
    }
    else
        start();
}

bool EkosManager::stop()
{
    cleanDevices();
    return true;
}

bool EkosManager::start()
{
    managedDevices.clear();
    reset();

    if (localMode)
    {
        scope_di   = driversList.value(telescopeCombo->currentText());
        ccd_di     = driversList.value(ccdCombo->currentText());
        guider_di  = driversList.value(guiderCombo->currentText());
        ao_di      = driversList.value(AOCombo->currentText());
        filter_di  = driversList.value(filterCombo->currentText());
        focuser_di = driversList.value(focuserCombo->currentText());
        aux_di     = driversList.value(auxCombo->currentText());
        dome_di    = driversList.value(domeCombo->currentText());

        if (guider_di)
        {
            // If the guider and ccd are the same driver, we have two cases:
            // #1 Drivers that only support ONE device per driver (such as sbig)
            // #2 Drivers that supports multiples devices per driver (such as sx)
            // For #1, we modify guider_di to make a unique label for the other device with postfix "Guide"
            // For #2, we set guider_di to NULL and we prompt the user to select which device is primary ccd and which is guider
            // since this is the only way to find out in real time.
            if (guider_di == ccd_di)
            {
                if (guider_di->getAuxInfo().value("mdpd", false).toBool() == true)
                    guider_di = NULL;
                else
                {
                    QVariantMap vMap = guider_di->getAuxInfo();
                    vMap.insert("DELETE", 1);
                    guider_di = new DriverInfo(ccd_di);
                    guider_di->setUniqueLabel(guider_di->getTreeLabel() + " Guide");
                    guider_di->setAuxInfo(vMap);
                   // useGuiderFromCCD = true;
                   // guider_di = NULL;

                }
            }
        }

        /*if (filter_di)
        {
            if (filter_di == ccd_di)
            {
                useFilterFromCCD = true;
                filter_di = NULL;
            }
        }*/

        if (scope_di != NULL)
            managedDevices.append(scope_di);
        if (ccd_di != NULL)
            managedDevices.append(ccd_di);
        if (guider_di != NULL)
            managedDevices.append(guider_di);
        if (ao_di != NULL)
            managedDevices.append(ao_di);
        if (filter_di != NULL)
            managedDevices.append(filter_di);
        if (focuser_di != NULL)
            managedDevices.append(focuser_di);
        if (dome_di != NULL)
            managedDevices.append(dome_di);
        if (aux_di != NULL)
            managedDevices.append(aux_di);        

        if (managedDevices.empty())
            return false;

        if (ccd_di == NULL && guider_di == NULL)
        {
            KMessageBox::error(this, xi18n("Ekos requires at least one CCD or Guider to operate."));
            managedDevices.clear();
            return false;
        }

        nDevices = managedDevices.count();

        saveDefaultDrivers();
    }
    else
    {
        delete (remote_indi);
        bool haveCCD=false, haveGuider=false;

        remote_indi = new DriverInfo(QString("Ekos Remote Host"));

        remote_indi->setHostParameters(Options::remoteHost(), Options::remotePort());

        remote_indi->setDriverSource(GENERATED_SOURCE);

        if (telescopeCombo->currentText() != "--")
            nDevices++;
        if (ccdCombo->currentText() != "--")
        {
            haveCCD = true;
            nDevices++;
        }
        if (guiderCombo->currentText() != "--")
        {
            haveGuider = true;
            nDevices++;
        }

        if (haveCCD == false && haveGuider == false)
        {
            KMessageBox::error(this, xi18n("Ekos requires at least one CCD or Guider to operate."));
            delete (remote_indi);
            nDevices=0;
            return false;
        }

        /*{
            if (guiderCombo->currentText() != ccdCombo->currentText())
                nDevices++;
            else
                useGuiderFromCCD = true;
        }*/

        if (AOCombo->currentText() != "--")
            nDevices++;
        if (focuserCombo->currentText() != "--")
            nDevices++;
        if (filterCombo->currentText() != "--")
            nDevices++;
        /*{
            if (filterCombo->currentText() != ccdCombo->currentText())
                nDevices++;
            else
                useFilterFromCCD = true;
        }*/
        if (domeCombo->currentText() != "--")
            nDevices++;
        if (auxCombo->currentText() != "--")
            nDevices++;

    }

    connect(INDIListener::Instance(), SIGNAL(newDevice(ISD::GDInterface*)), this, SLOT(processNewDevice(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newTelescope(ISD::GDInterface*)), this, SLOT(setTelescope(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newCCD(ISD::GDInterface*)), this, SLOT(setCCD(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newFilter(ISD::GDInterface*)), this, SLOT(setFilter(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newFocuser(ISD::GDInterface*)), this, SLOT(setFocuser(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newST4(ISD::ST4*)), this, SLOT(setST4(ISD::ST4*)));
    connect(INDIListener::Instance(), SIGNAL(deviceRemoved(ISD::GDInterface*)), this, SLOT(removeDevice(ISD::GDInterface*)));
    connect(DriverManager::Instance(), SIGNAL(serverTerminated(QString,QString)), this, SLOT(cleanDevices()));

    if (localMode)
    {
        if (isRunning("indiserver"))
        {
            if (KMessageBox::Yes == (KMessageBox::questionYesNo(0, xi18n("Ekos detected an instance of INDI server running. Do you wish to shut down the existing instance before starting a new one?"),
                                                                xi18n("INDI Server"), KStandardGuiItem::yes(), KStandardGuiItem::no(), "ekos_shutdown_existing_indiserver")))
            {
                //TODO is there a better way to do this.
                QProcess p;
                p.start("pkill indiserver");
                p.waitForFinished();
            }
        }

        if (DriverManager::Instance()->startDevices(managedDevices) == false)
        {
            INDIListener::Instance()->disconnect(this);
            return false;
        }

        appendLogText(xi18n("INDI services started. Please connect devices."));

    }
    else
    {
        if (DriverManager::Instance()->connectRemoteHost(remote_indi) == false)
        {
            INDIListener::Instance()->disconnect(this);
            delete (remote_indi);
            remote_indi=0;
            return false;
        }

        appendLogText(xi18n("INDI services started. Connection to %1 at %2 is successful.", Options::remoteHost(), Options::remotePort()));

        QTimer::singleShot(MAX_REMOTE_INDI_TIMEOUT, this, SLOT(checkINDITimeout()));

    }

    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);

    processINDIB->setText(xi18n("Stop INDI"));

    return true;


}

void EkosManager::checkINDITimeout()
{
    if (localMode)
        return;

    if (nDevices != 0)
        KMessageBox::error(this, xi18np("Unable to completely establish remote devices. %1 device remaining. Please ensure remote device name corresponds to actual device name.", "Unable to completely establish remote devices. %1 devices remaining. Please ensure remote device name corresponds to actual device name.", nDevices));
}

void EkosManager::refreshRemoteDrivers()
{
    if (localMode == false)
        initRemoteDrivers();
}

void EkosManager::connectDevices()
{
    if (scope)
    {
         scope->Connect();

         if (mountProcess)
             mountProcess->setEnabled(true);
    }

    if (ccd)
    {
        ccd->Connect();

        if (captureProcess)
            captureProcess->setEnabled(true);
        if (focusProcess)
            focusProcess->setEnabled(true);
        if (alignProcess)
            alignProcess->setEnabled(true);
    }


    if (guider && guider != ccd)
        guider->Connect();

    if (guideProcess)
        guideProcess->setEnabled(true);

    if (filter && filter != ccd)
        filter->Connect();

    if (ao)
        ao->Connect();

    if (focuser)
    {
        focuser->Connect();

        if (focusProcess)
            focusProcess->setEnabled(true);
    }

    if (dome)
        dome->Connect();

    if (aux)
        aux->Connect();

    connectB->setEnabled(false);
    disconnectB->setEnabled(true);

    appendLogText(xi18n("Connecting INDI devices..."));
}

void EkosManager::disconnectDevices()
{

    if (scope)
    {
         scope->Disconnect();

         if (mountProcess)
             mountProcess->setEnabled(false);
    }

    if (ccd)
    {
        ccdStarted      =false;
        ccd->Disconnect();

        if (captureProcess)
            captureProcess->setEnabled(false);
        if (alignProcess)
            alignProcess->setEnabled(false);
        if (focusProcess)
            focusProcess->setEnabled(false);
    }

    if (guider && guider != ccd)
        guider->Disconnect();

    if (guideProcess)
        guideProcess->setEnabled(false);

    if (ao)
        ao->Disconnect();

    if (filter && filter != ccd)
        filter->Disconnect();

    if (focuser)
    {
        focuser->Disconnect();

        if (focusProcess)
        {
            disconnect(focuser, SIGNAL(numberUpdated(INumberVectorProperty*)), focusProcess, SLOT(processFocusProperties(INumberVectorProperty*)));
            focusProcess->setEnabled(false);
        }
     }

    if (dome)
        dome->Disconnect();

    if (aux)
        aux->Disconnect();


    appendLogText(xi18n("Disconnecting INDI devices..."));

}

void EkosManager::cleanDevices()
{

    INDIListener::Instance()->disconnect(this);

    if (localMode)
    {
        DriverManager::Instance()->stopDevices(managedDevices);
        if (guider_di)
        {
            if (guider_di->getAuxInfo().value("DELETE") == 1)
            {
                delete(guider_di);
                guider_di=NULL;
            }
        }
    }
    else if (remote_indi)
    {
        DriverManager::Instance()->disconnectRemoteHost(remote_indi);
        remote_indi = 0;
    }


    nDevices = 0;
    managedDevices.clear();

    reset();

    processINDIB->setText(xi18n("Start INDI"));
    processINDIB->setEnabled(true);
    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);

    appendLogText(xi18n("INDI services stopped."));
}

void EkosManager::processNewDevice(ISD::GDInterface *devInterface)
{

    if (localMode)
        processLocalDevice(devInterface);
    else
        processRemoteDevice(devInterface);

}

void EkosManager::processLocalDevice(ISD::GDInterface *devInterface)
{
    DriverInfo *di = devInterface->getDriverInfo();

            switch (di->getType())
            {
               case KSTARS_TELESCOPE:
                scope = devInterface;
                break;


               case KSTARS_CCD:
                if (guider_di == di)
                {
                    guider = devInterface;
                    guiderName = devInterface->getDeviceName();
                }
                else if (ccd_di->getAuxInfo().value("mdpd", false).toBool() == true)
                {
                    if (ccdDriverSelected == false)
                    {
                        ccd = devInterface;
                        ccdDriverSelected = true;

                    }
                    else
                    {
                        guider = devInterface;
                        guiderName = devInterface->getDeviceName();
                        //guideDriverSelected = true;

                    }
                }
                else
                    ccd = devInterface;

                break;

            case KSTARS_ADAPTIVE_OPTICS:
                ao = devInterface;
                break;

            case KSTARS_FOCUSER:
                focuser = devInterface;
                break;

             case KSTARS_FILTER:
                if (filter_di == di)
                    filter = devInterface;
                //if (useFilterFromCCD)
                   // ccd = devInterface;
                break;

             case KSTARS_DOME:
                if (dome_di == di)
                    dome = devInterface;
                break;

             case KSTARS_AUXILIARY:
                if (aux_di == di)
                    aux = devInterface;
                    break;

              default:
                return;
                break;
            }

     nDevices--;

    connect(devInterface, SIGNAL(Connected()), this, SLOT(deviceConnected()));
    connect(devInterface, SIGNAL(Disconnected()), this, SLOT(deviceDisconnected()));
    connect(devInterface, SIGNAL(propertyDefined(INDI::Property*)), this, SLOT(processNewProperty(INDI::Property*)));

    if (nDevices == 0)
    {
        connectB->setEnabled(true);
        disconnectB->setEnabled(false);
        controlPanelB->setEnabled(true);
    }

}

void EkosManager::processRemoteDevice(ISD::GDInterface *devInterface)
{
    QString deviceName(devInterface->getDeviceName());

    if (deviceName == Options::remoteScopeName())
        scope = devInterface;
    else if (deviceName == Options::remoteCCDName()
             || (remoteCCDRegistered == false &&  deviceName.startsWith(Options::remoteCCDName())))
    {
        ccd = devInterface;

        remoteCCDRegistered = true;

        // In case we have a partial match only, then we are connecting to a driver that provides multiple devices per driver
        // in such instance we increase the number of devices we expect to receive by one.
        if (remoteGuideRegistered == false && deviceName != Options::remoteCCDName())
        {
            remote_indi->addAuxInfo("mdpd", true);
            nDevices++;
        }

    }
    else if (deviceName == Options::remoteGuiderName()
             || (remoteGuideRegistered == false &&  deviceName.startsWith(Options::remoteCCDName())))

    {
        guider = devInterface;
        guiderName = devInterface->getDeviceName();
        remoteGuideRegistered = true;

        // We only increase number of devices if CCD is not registered yet above.
        if (remoteCCDRegistered == false &&  Options::remoteGuiderName().isEmpty() && deviceName.startsWith(Options::remoteCCDName()))
        {
            remote_indi->addAuxInfo("mdpd", true);
            nDevices++;
        }
    }
    else if (deviceName == Options::remoteAOName())
        ao = devInterface;
    else if (deviceName == Options::remoteFocuserName())
        focuser = devInterface;
    else if (deviceName == Options::remoteFilterName())
        filter = devInterface;
    else if (deviceName == Options::remoteDomeName())
        dome = devInterface;
    else if (deviceName == Options::remoteAuxName().toLatin1())
        aux = devInterface;
    else
        return;

    nDevices--;
    connect(devInterface, SIGNAL(Connected()), this, SLOT(deviceConnected()));
    connect(devInterface, SIGNAL(Disconnected()), this, SLOT(deviceDisconnected()));
    connect(devInterface, SIGNAL(propertyDefined(INDI::Property*)), this, SLOT(processNewProperty(INDI::Property*)));


    if (nDevices == 0)
    {
        connectB->setEnabled(true);
        disconnectB->setEnabled(false);
        controlPanelB->setEnabled(true);

        appendLogText(xi18n("Remote devices established. Please connect devices."));
    }

}

void EkosManager::deviceConnected()
{
    connectB->setEnabled(false);
    disconnectB->setEnabled(true);

    processINDIB->setEnabled(false);

    if (Options::neverLoadConfig())
        return;

    INDIConfig tConfig = Options::loadConfigOnConnection() ? LOAD_LAST_CONFIG : LOAD_DEFAULT_CONFIG;

    ISwitchVectorProperty *configProp = NULL;

    if (scope && scope->isConnected())
    {
        configProp = scope->getBaseDevice()->getSwitch("CONFIG_PROCESS");
        if (configProp && configProp->s == IPS_IDLE)
           scope->setConfig(tConfig);
    }

    if (ccd && ccd->isConnected())
    {
        configProp = ccd->getBaseDevice()->getSwitch("CONFIG_PROCESS");
        if (configProp && configProp->s == IPS_IDLE)
           ccd->setConfig(tConfig);
    }


    if (guider && guider != ccd && guider->isConnected())
    {
        configProp = guider->getBaseDevice()->getSwitch("CONFIG_PROCESS");
        if (configProp && configProp->s == IPS_IDLE)
           guider->setConfig(tConfig);
    }

    if (ao && ao->isConnected())
    {
        configProp = ao->getBaseDevice()->getSwitch("CONFIG_PROCESS");
        if (configProp && configProp->s == IPS_IDLE)
           ao->setConfig(tConfig);
    }

    if (focuser && focuser->isConnected())
    {
        configProp = focuser->getBaseDevice()->getSwitch("CONFIG_PROCESS");
        if (configProp && configProp->s == IPS_IDLE)
           focuser->setConfig(tConfig);
    }

    if (filter && filter->isConnected())
    {
        configProp = filter->getBaseDevice()->getSwitch("CONFIG_PROCESS");
        if (configProp && configProp->s == IPS_IDLE)
           filter->setConfig(tConfig);
    }

    if (dome && dome->isConnected())
    {
        configProp = dome->getBaseDevice()->getSwitch("CONFIG_PROCESS");
        if (configProp && configProp->s == IPS_IDLE)
           dome->setConfig(tConfig);
    }

    if (aux && aux->isConnected())
    {
        configProp = aux->getBaseDevice()->getSwitch("CONFIG_PROCESS");
        if (configProp && configProp->s == IPS_IDLE)
           aux->setConfig(tConfig);
    }
}

void EkosManager::deviceDisconnected()
{
    connectB->setEnabled(true);
    disconnectB->setEnabled(false);

    processINDIB->setEnabled(true);

}

void EkosManager::setTelescope(ISD::GDInterface *scopeDevice)
{
    scope = scopeDevice;

    appendLogText(xi18n("%1 is online.", scope->getDeviceName()));

    scopeRegistered = true;

    connect(scopeDevice, SIGNAL(numberUpdated(INumberVectorProperty *)), this, SLOT(processNewNumber(INumberVectorProperty*)));

    initMount();

    mountProcess->setTelescope(scope);

    if (guideProcess)
        guideProcess->setTelescope(scope);

    if (alignProcess)
        alignProcess->setTelescope(scope);
}

void EkosManager::setCCD(ISD::GDInterface *ccdDevice)
{
    bool isPrimaryCCD = false;

    // For multiple devices per driver, we always treat the first device as the primary CCD
    // but this shouldn't matter since the user can select the final ccd from the drop down in both CCD and Guide modules
    if ( (ccd_di && ccd_di->getAuxInfo().value("mdpd", false).toBool()) == true || (remote_indi && remote_indi->getAuxInfo().value("mdpd", false).toBool()))
    {
        if (ccdStarted == false)
            isPrimaryCCD = true;
    }
    else if ( ccd_di == ccdDevice->getDriverInfo() || (ccd_di == NULL && Options::remoteCCDName() == ccdDevice->getDeviceName()))
        isPrimaryCCD = true;

    initCapture();

    captureProcess->addCCD(ccdDevice, isPrimaryCCD);

    initFocus();

    focusProcess->addCCD(ccdDevice, isPrimaryCCD);

    connect(ccdDevice, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processNewNumber(INumberVectorProperty*)), Qt::UniqueConnection);

    // If we have a guider and it's the same as the CCD driver, then let's establish it separately.
    if (isPrimaryCCD == false)
    {
        guider = ccdDevice;
        appendLogText(xi18n("%1 is online.", ccdDevice->getDeviceName()));

        initGuide();
        guideProcess->addCCD(guider, true);

        initAlign();
        alignProcess->addCCD(guider, false);

        if (scopeRegistered)
        {
            guideProcess->setTelescope(scope);
            captureProcess->setTelescope(scope);
        }
    }
    else
    {
        ccd = ccdDevice;

        if (ccdStarted == false)
            appendLogText(xi18n("%1 is online.", ccdDevice->getDeviceName()));

        ccdStarted = true;

        initAlign();
        alignProcess->addCCD(ccd, isPrimaryCCD);
        if (scope && scope->isConnected())
        {
            alignProcess->setTelescope(scope);
            captureProcess->setTelescope(scope);
        }

        // For multiple devices per driver, we also add the CCD to the guider selection
        // since the order of initialization is not guranteed in real time
        if (ccd_di && ccd_di->getAuxInfo().value("mdpd", false).toBool() == true)
        {
            initGuide();
            guideProcess->addCCD(ccdDevice, true);
        }
    }

}

void EkosManager::setFilter(ISD::GDInterface *filterDevice)
{

   filter = filterDevice;
   appendLogText(i18n("%1 filter is online.", filter->getDeviceName()));

    initCapture();

    connect(filter, SIGNAL(numberUpdated(INumberVectorProperty *)), this, SLOT(processNewNumber(INumberVectorProperty*)));
    connect(filter, SIGNAL(textUpdated(ITextVectorProperty*)), this, SLOT(processNewText(ITextVectorProperty*)));

    captureProcess->addFilter(filter);

    initFocus();

    focusProcess->addFilter(filter);
}

void EkosManager::setFocuser(ISD::GDInterface *focuserDevice)
{
    focuser = focuserDevice;

    initFocus();

    focusProcess->addFocuser(focuser);

    appendLogText(i18n("%1 focuser is online.", focuser->getDeviceName()));
}

void EkosManager::removeDevice(ISD::GDInterface* devInterface)
{
    switch (devInterface->getType())
    {
        case KSTARS_CCD:
        removeTabs();

        break;

    case KSTARS_FOCUSER:
    break;

      default:
         break;
    }


    appendLogText(xi18n("%1 is offline.", devInterface->getDeviceName()));

    foreach(DriverInfo *drvInfo, managedDevices)
    {
        if (drvInfo == devInterface->getDriverInfo())
        {
            managedDevices.removeOne(drvInfo);

            if (managedDevices.count() == 0)
             cleanDevices();

            break;
        }
    }



}

void EkosManager::processNewText(ITextVectorProperty *tvp)
{
    if (!strcmp(tvp->name, "FILTER_NAME"))
    {
        if (captureProcess)
            captureProcess->checkFilter();

        if (focusProcess)
            focusProcess->checkFilter();
    }
}

void EkosManager::processNewNumber(INumberVectorProperty *nvp)
{
    if (!strcmp(nvp->name, "TELESCOPE_INFO"))
    {
        if (guideProcess)
        {
           guideProcess->setTelescope(scope);
           guideProcess->syncTelescopeInfo();
        }

        if (alignProcess)
        {
            alignProcess->setTelescope(scope);
            alignProcess->syncTelescopeInfo();
        }

        return;

    }

    if (!strcmp(nvp->name, "CCD_INFO") || !strcmp(nvp->name, "GUIDER_INFO")
            || !strcmp(nvp->name, "CCD_FRAME") || !strcmp(nvp->name, "GUIDER_FRAME"))
    {
        if (guideProcess)
            guideProcess->syncCCDInfo();

        if (alignProcess)
            alignProcess->syncCCDInfo();

        return;
    }

    if (!strcmp(nvp->name, "FILTER_SLOT"))
    {
        if (captureProcess)
            captureProcess->checkFilter();

        if (focusProcess)
            focusProcess->checkFilter();
    }

}

void EkosManager::processNewProperty(INDI::Property* prop)
{
    if (!strcmp(prop->getName(), "CCD_INFO") || !strcmp(prop->getName(), "GUIDER_INFO"))
    {
        if (guideProcess)
            guideProcess->syncCCDInfo();

        if (alignProcess)
            alignProcess->syncCCDInfo();

        return;
    }

    if (!strcmp(prop->getName(), "TELESCOPE_INFO"))
    {
        if (guideProcess)
        {
           guideProcess->setTelescope(scope);
           guideProcess->syncTelescopeInfo();
        }

        if (mountProcess)
        {
           mountProcess->setTelescope(scope);
           mountProcess->syncTelescopeInfo();
        }



        return;

    }

    if (!strcmp(prop->getName(), "GUIDER_EXPOSURE"))
    {
        initCapture();        

        if (ccd && !strcmp(ccd->getDeviceName(), prop->getDeviceName()))
        {
            useGuideHead=true;
            initGuide();
            captureProcess->addGuideHead(ccd);
            guideProcess->addGuideHead(ccd);
        }
        else if (guider)
        {
            captureProcess->addGuideHead(guider);
            guideProcess->addGuideHead(guider);
        }

        return;
    }

    if (!strcmp(prop->getName(), "CCD_FRAME_TYPE"))
    {
        if (captureProcess)
        {
            if (ccd && !strcmp(ccd->getDeviceName(), prop->getDeviceName()))
                captureProcess->syncFrameType(ccd);
            else if (guider)
                captureProcess->syncFrameType(guider);
        }

        return;
    }

    if (!strcmp(prop->getName(), "TELESCOPE_PARK"))
    {
        if (captureProcess)
            captureProcess->setTelescope(scope);

        return;
    }

    if (!strcmp(prop->getName(), "FILTER_NAME"))
    {
        if (captureProcess)
            captureProcess->checkFilter();

        if (focusProcess)
            focusProcess->checkFilter();

        return;
    }

}

void EkosManager::processTabChange()
{
    QWidget *currentWidget = toolsWidget->currentWidget();

    if (currentWidget != focusProcess)
    {
        if (focusProcess)
            focusProcess->resetFrame();
    }

    if (currentWidget == alignProcess)
    {
        if (alignProcess->isEnabled() == false && captureProcess->isEnabled() && ccd && ccd->isConnected())
        {
            if (alignProcess->isParserOK())
                alignProcess->setEnabled(true);
        }

        alignProcess->checkCCD();
    }
    else if (currentWidget == captureProcess)
    {
        captureProcess->checkCCD();
    }
    else if (currentWidget == focusProcess)
    {
        focusProcess->checkCCD();
    }
    else if (currentWidget == guideProcess)
    {
        guideProcess->checkCCD();
    }

    updateLog();

}

void EkosManager::updateLog()
{
    if (enableLoggingCheck->isChecked() == false)
        return;

    QWidget *currentWidget = toolsWidget->currentWidget();

    if (currentWidget == setupTab)
        ekosLogOut->setPlainText(logText.join("\n"));
    else if (currentWidget == alignProcess)
        ekosLogOut->setPlainText(alignProcess->getLogText());
    else if (currentWidget == captureProcess)
        ekosLogOut->setPlainText(captureProcess->getLogText());
    else if (currentWidget == focusProcess)
        ekosLogOut->setPlainText(focusProcess->getLogText());
    else if (currentWidget == guideProcess)
        ekosLogOut->setPlainText(guideProcess->getLogText());
    else if (currentWidget == mountProcess)
        ekosLogOut->setPlainText(mountProcess->getLogText());

}

void EkosManager::appendLogText(const QString &text)
{

    logText.insert(0, xi18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    updateLog();
}

void EkosManager::clearLog()
{
    QWidget *currentWidget = toolsWidget->currentWidget();

    if (currentWidget == setupTab)
    {
        logText.clear();
        updateLog();
    }
    else if (currentWidget == alignProcess)
        alignProcess->clearLog();
    else if (currentWidget == captureProcess)
        captureProcess->clearLog();
    else if (currentWidget == focusProcess)
        focusProcess->clearLog();
    else if (currentWidget == guideProcess)
        guideProcess->clearLog();
    else if (currentWidget == mountProcess)
        mountProcess->clearLog();

}

void EkosManager::initCapture()
{
    if (captureProcess)
        return;

     captureProcess = new Ekos::Capture();
     toolsWidget->addTab( captureProcess, xi18n("CCD"));
     connect(captureProcess, SIGNAL(newLog()), this, SLOT(updateLog()));

     if (focusProcess)
     {
         // Autofocus
         captureProcess->disconnect(focusProcess);
         focusProcess->disconnect(captureProcess);
         connect(captureProcess, SIGNAL(checkFocus(double)), focusProcess, SLOT(checkFocus(double)));
         connect(focusProcess, SIGNAL(autoFocusFinished(bool, double)), captureProcess, SLOT(updateAutofocusStatus(bool, double)),  Qt::UniqueConnection);
     }

}

void EkosManager::initAlign()
{
    if (alignProcess)
        return;

     alignProcess = new Ekos::Align();
     toolsWidget->addTab( alignProcess, xi18n("Alignment"));
     connect(alignProcess, SIGNAL(newLog()), this, SLOT(updateLog()));
}


void EkosManager::initFocus()
{
    if (focusProcess)
        return;

    focusProcess = new Ekos::Focus();
    toolsWidget->addTab( focusProcess, xi18n("Focus"));
    connect(focusProcess, SIGNAL(newLog()), this, SLOT(updateLog()));

    if (captureProcess)
    {
        // Autofocus
        connect(captureProcess, SIGNAL(checkFocus(double)), focusProcess, SLOT(checkFocus(double)), Qt::UniqueConnection);
        connect(focusProcess, SIGNAL(autoFocusFinished(bool, double)), captureProcess, SLOT(updateAutofocusStatus(bool, double)), Qt::UniqueConnection);
    }

    if (guideProcess)
    {
        // Suspend
        connect(focusProcess, SIGNAL(suspendGuiding(bool)), guideProcess, SLOT(setSuspended(bool)), Qt::UniqueConnection);
    }

}

void EkosManager::initMount()
{
    if (mountProcess)
        return;

    mountProcess = new Ekos::Mount();
    toolsWidget->addTab(mountProcess, xi18n("Mount"));
    connect(mountProcess, SIGNAL(newLog()), this, SLOT(updateLog()));

}

void EkosManager::initGuide()
{    
    if (guideProcess == NULL)
        guideProcess = new Ekos::Guide();

    if ( ( (guider && guider->isConnected()) || useGuideHead) && useST4 && guideStarted == false)
    {
        guideStarted = true;
        if (scope && scope->isConnected())
            guideProcess->setTelescope(scope);

        toolsWidget->addTab( guideProcess, xi18n("Guide"));
        connect(guideProcess, SIGNAL(newLog()), this, SLOT(updateLog()));

    }

    if (captureProcess)
    {
        guideProcess->disconnect(captureProcess);
        captureProcess->disconnect(guideProcess);

        // Guide Limits
        connect(guideProcess, SIGNAL(guideReady()), captureProcess, SLOT(enableGuideLimits()));
        connect(guideProcess, SIGNAL(newAxisDelta(double,double)), captureProcess, SLOT(setGuideDeviation(double,double)));

        // Dithering
        connect(guideProcess, SIGNAL(autoGuidingToggled(bool,bool)), captureProcess, SLOT(setAutoguiding(bool,bool)));
        connect(guideProcess, SIGNAL(ditherComplete()), captureProcess, SLOT(resumeCapture()));
        connect(guideProcess, SIGNAL(ditherFailed()), captureProcess, SLOT(stopSequence()));
        connect(guideProcess, SIGNAL(ditherToggled(bool)), captureProcess, SLOT(setGuideDither(bool)));        
        connect(captureProcess, SIGNAL(exposureComplete()), guideProcess, SLOT(dither()));

        // Parking
        connect(captureProcess, SIGNAL(telescopeParking()), guideProcess, SLOT(stopGuiding()));

        // Guide Head
        connect(captureProcess, SIGNAL(suspendGuiding(bool)), guideProcess, SLOT(setSuspended(bool)));
        connect(guideProcess, SIGNAL(guideChipUpdated(ISD::CCDChip*)), captureProcess, SLOT(setGuideChip(ISD::CCDChip*)));
    }

    if (focusProcess)
    {
        // Suspend
        connect(focusProcess, SIGNAL(suspendGuiding(bool)), guideProcess, SLOT(setSuspended(bool)), Qt::UniqueConnection);
    }

}

void EkosManager::setST4(ISD::ST4 * st4Driver)
{
     appendLogText(xi18n("Guider port from %1 is ready.", st4Driver->getDeviceName()));
     useST4=true;

     initGuide();

     guideProcess->addST4(st4Driver);

     if (ao && ao->getDeviceName() == st4Driver->getDeviceName())
         guideProcess->setAO(st4Driver);

}

void EkosManager::removeTabs()
{

        for (int i=2; i < toolsWidget->count(); i++)
                toolsWidget->removeTab(i);

        delete (alignProcess);
        alignProcess = NULL;

        ccd = NULL;
        delete (captureProcess);
        captureProcess = NULL;


        guider = NULL;
        delete (guideProcess);
        guideProcess = NULL;

        delete (mountProcess);
        mountProcess = NULL;

        ao = NULL;

        focuser = NULL;
        delete (focusProcess);
        focusProcess = NULL;

        dome = NULL;
        aux = NULL;

}

bool EkosManager::isRunning(const QString &process)
{
  QProcess ps;
  ps.start("ps", QStringList() << "-o" << "comm" << "--no-headers" << "-C" << process);
  ps.waitForFinished();
  QString output = ps.readAllStandardOutput();
  return output.startsWith(process);
}

void EkosManager::playFITS()
{
    playFITSFile->play();
}

void EkosManager::playOk()
{
   playOkFile->play();

}

void EkosManager::playError()
{
   playErrorFile->play();
}

void EkosManager::setTelescope(const QString & telescopeName)
{
    if (localMode)
    {
        for (int i=0; i < telescopeCombo->count(); i++)
            if (telescopeCombo->itemText(i) == telescopeName)
            {
                telescopeCombo->setCurrentIndex(i);
                break;
            }
    }
    else
        Options::setRemoteScopeName(telescopeName);

}

void EkosManager::setCCD(const QString & ccdName)
{
    if (localMode)
    {
        for (int i=0; i < ccdCombo->count(); i++)
            if (ccdCombo->itemText(i) == ccdName)
            {
                ccdCombo->setCurrentIndex(i);
                break;
            }
    }
    else
        Options::setRemoteCCDName(ccdName);


}

void EkosManager::setGuider(const QString & guiderName)
{
    if (localMode)
    {
        for (int i=0; i < guiderCombo->count(); i++)
            if (guiderCombo->itemText(i) == guiderName)
            {
                guiderCombo->setCurrentIndex(i);
                break;
            }
    }
    else
        Options::setRemoteGuiderName(guiderName);

}

void EkosManager::setFocuser(const QString & focuserName)
{
    if (localMode)
    {
        for (int i=0; i < focuserCombo->count(); i++)
            if (focuserCombo->itemText(i) == focuserName)
            {
                focuserCombo->setCurrentIndex(i);
                break;
            }
    }
    else
        Options::setRemoteFocuserName(focuserName);

}

void EkosManager::setAO(const QString & AOName)
{
    if (localMode)
    {
        for (int i=0; i < AOCombo->count(); i++)
            if (AOCombo->itemText(i) == AOName)
            {
                AOCombo->setCurrentIndex(i);
                break;
            }
    }
    else
        Options::setRemoteAOName(AOName);
}

void EkosManager::setFilter(const QString & filterName)
{
    if (localMode)
    {
            for (int i=0; i < filterCombo->count(); i++)
                if (filterCombo->itemText(i) == filterName)
                {
                    filterCombo->setCurrentIndex(i);
                    break;
                }

    }
    else
        Options::setRemoteFilterName(filterName);


}

void EkosManager::setDome(const QString & domeName)
{
  if (localMode)
  {
      for (int i=0; i < domeCombo->count(); i++)
          if (domeCombo->itemText(i) == domeName)
          {
              domeCombo->setCurrentIndex(i);
              break;
          }
  }
  else
      Options::setRemoteDomeName(domeName);

}

void EkosManager::setAuxiliary(const QString & auxiliaryName)
{
    if (localMode)
    {
        for (int i=0; i < auxCombo->count(); i++)
            if (auxCombo->itemText(i) == auxiliaryName)
            {
                auxCombo->setCurrentIndex(i);
                break;
            }
    }
    else
        Options::setRemoteAuxName(auxiliaryName);
}
