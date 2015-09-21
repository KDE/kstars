/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <QComboBox>

#include <KConfigDialog>
#include <KMessageBox>
#include <KActionCollection>
#include <KNotifications/KNotification>

#include <config-kstars.h>
#include <basedevice.h>

#include "ekosmanager.h"

#include "Options.h"
#include "kstars.h"
#include "fitsviewer/fitsviewer.h"

#include "indi/clientmanager.h"
#include "indi/indielement.h"
#include "indi/indiproperty.h"
#include "indi/driverinfo.h"
#include "indi/drivermanager.h"
#include "indi/indilistener.h"
#include "indi/guimanager.h"

#include "ekosadaptor.h"

#define MAX_REMOTE_INDI_TIMEOUT 15000
#define MAX_LOCAL_INDI_TIMEOUT 5000

EkosManager::EkosManager()
        : QDialog(KStars::Instance())
{
    setupUi(this);

    new EkosAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos",  this);

    setWindowIcon(QIcon::fromTheme("kstars_ekos"));

    nDevices=0;
    nConnectedDevices=0;
    useGuideHead    =false;
    useST4          =false;
    ccdStarted      =false;
    ccdDriverSelected =false;
    scopeRegistered = false;
    remoteCCDRegistered    = false;
    remoteGuideRegistered  = false;

    indiConnectionStatus = STATUS_IDLE;
    ekosStartingStatus   = STATUS_IDLE;

    scope   =  NULL;
    ccd     =  NULL;
    guider  =  NULL;
    focuser =  NULL;
    filter  =  NULL;
    aux1    =  NULL;
    aux2    =  NULL;
    aux3    =  NULL;
    aux4    =  NULL;
    ao      =  NULL;
    dome    =  NULL;
    weather =  NULL;

    scope_di   = NULL;
    ccd_di     = NULL;
    guider_di  = NULL;
    focuser_di = NULL;
    filter_di  = NULL;
    aux1_di    = NULL;
    aux2_di    = NULL;
    aux3_di    = NULL;
    aux4_di    = NULL;
    ao_di      = NULL;
    dome_di    = NULL;
    weather_di = NULL;
    remote_indi= NULL;

    captureProcess = NULL;
    focusProcess   = NULL;
    guideProcess   = NULL;
    alignProcess   = NULL;
    mountProcess   = NULL;
    domeProcess    = NULL;
    schedulerProcess = NULL;
    weatherProcess = NULL;

    ekosOption     = NULL;

    kcfg_localMode->setChecked(Options::localMode());
    kcfg_remoteMode->setChecked(Options::remoteMode());

    connect(toolsWidget, SIGNAL(currentChanged(int)), this, SLOT(processTabChange()));

    toolsWidget->setTabEnabled(1, false);

    connect(processINDIB, SIGNAL(clicked()), this, SLOT(processINDI()));

    connect(connectB, SIGNAL(clicked()), this, SLOT(connectDevices()));

    connect(disconnectB, SIGNAL(clicked()), this, SLOT(disconnectDevices()));

    connect(controlPanelB, SIGNAL(clicked()), GUIManager::Instance(), SLOT(show()));

    connect(INDIB, SIGNAL(clicked()), this, SLOT(toggleINDIPanel()));

    connect(fitsViewerB, SIGNAL(clicked(bool)), this, SLOT(toggleFITSViewer()));

    QAction *a = KStars::Instance()->actionCollection()->action( "show_fits_viewer" );
    connect(a, SIGNAL(changed()), this, SLOT(checkFITSViewerState()));

    connect(optionsB, SIGNAL(clicked()), KStars::Instance(), SLOT(slotViewOps()));

    connect(clearB, SIGNAL(clicked()), this, SLOT(clearLog()));

    connect(kcfg_localMode, SIGNAL(toggled(bool)), this, SLOT(processINDIModeChange()));

    localMode = Options::localMode();

    if (localMode)
        initLocalDrivers();
    else
        initRemoteDrivers();

    schedulerProcess = new Ekos::Scheduler();
    toolsWidget->addTab( schedulerProcess, i18n("Scheduler"));
    connect(schedulerProcess, SIGNAL(newLog()), this, SLOT(updateLog()));

}

EkosManager::~EkosManager()
{
    delete captureProcess;
    delete focusProcess;
    delete guideProcess;
    delete alignProcess;
    delete domeProcess;
    delete weatherProcess;
    delete mountProcess;
    delete schedulerProcess;
}

void EkosManager::closeEvent(QCloseEvent * /*event*/)
{
    QAction *a = KStars::Instance()->actionCollection()->action( "show_ekos" );
    a->setChecked(false);
}

void EkosManager::hideEvent(QHideEvent * /*event*/)
{
    QAction *a = KStars::Instance()->actionCollection()->action( "show_ekos" );
    a->setChecked(false);
}

void EkosManager::showEvent(QShowEvent * /*event*/)
{
    QAction *a = KStars::Instance()->actionCollection()->action( "show_ekos" );
    a->setChecked(true);
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
    else
    {
        if ( Options::remotePort().isEmpty() || Options::remoteHost().isEmpty())
        {
           appendLogText(i18n("Please fill the remote server information in Ekos options before switching to remote mode."));
           if (KConfigDialog::showDialog("settings") == false)
               optionsB->click();

           KConfigDialog *mConfigDialog = KConfigDialog::exists("settings");
           if (mConfigDialog && ekosOption)
           {
                   mConfigDialog->setCurrentPage(ekosOption);
                   kcfg_localMode->setChecked(!newLocalMode);
                   kcfg_remoteMode->setChecked(newLocalMode);
                   return;

            }

        }
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
    int i=0;

    telescopeCombo->clear();
    ccdCombo->clear();
    guiderCombo->clear();
    AOCombo->clear();
    focuserCombo->clear();
    filterCombo->clear();
    domeCombo->clear();
    weatherCombo->clear();
    aux1Combo->clear();
    aux2Combo->clear();
    aux3Combo->clear();
    aux4Combo->clear();

    telescopeCombo->addItem("--");
    ccdCombo->addItem("--");
    guiderCombo->addItem("--");
    AOCombo->addItem("--");
    focuserCombo->addItem("--");
    filterCombo->addItem("--");
    domeCombo->addItem("--");
    weatherCombo->addItem("--");
    aux1Combo->addItem("--");
    aux2Combo->addItem("--");
    aux3Combo->addItem("--");
    aux4Combo->addItem("--");

    QString TelescopeDriver     = (Options::telescopeDriver() == "--") ? "" : Options::telescopeDriver();
    QString CCDDriver           = (Options::cCDDriver() == "--") ? "" : Options::cCDDriver();
    QString GuiderDriver        = (Options::guiderDriver() == "--") ? "" : Options::guiderDriver();
    QString AODriver            = (Options::aODriver() == "--") ? "" : Options::aODriver();
    QString FocuserDriver       = (Options::focuserDriver() == "--") ? "" : Options::focuserDriver();
    QString FilterDriver        = (Options::filterDriver() == "--") ? "" : Options::filterDriver();
    QString DomeDriver          = (Options::domeDriver() == "--") ? "" : Options::domeDriver();
    QString WeatherDriver       = (Options::weatherDriver() == "--") ? "" : Options::weatherDriver();
    QString Aux1Driver           = (Options::aux1Driver() == "--") ? "" : Options::aux1Driver();
    QString Aux2Driver           = (Options::aux2Driver() == "--") ? "" : Options::aux2Driver();
    QString Aux3Driver           = (Options::aux3Driver() == "--") ? "" : Options::aux3Driver();
    QString Aux4Driver           = (Options::aux4Driver() == "--") ? "" : Options::aux4Driver();

    if (TelescopeDriver.isEmpty() == false)
        telescopeCombo->addItem(TelescopeDriver);

    if (CCDDriver.isEmpty() == false)
        ccdCombo->addItem(CCDDriver);

    if (GuiderDriver.isEmpty() == false)
        guiderCombo->addItem(GuiderDriver);

    if (AODriver.isEmpty() == false)
        AOCombo->addItem(AODriver);

    if (FocuserDriver.isEmpty() == false)
        focuserCombo->addItem(FocuserDriver);

    if (FilterDriver.isEmpty() == false)
        filterCombo->addItem(FilterDriver);

    if (DomeDriver.isEmpty() == false)
        domeCombo->addItem(DomeDriver);

    if (WeatherDriver.isEmpty() == false)
        weatherCombo->addItem(WeatherDriver);

    if (Aux1Driver.isEmpty() == false)
        aux1Combo->addItem(Aux1Driver);

    if (Aux2Driver.isEmpty() == false)
        aux2Combo->addItem(Aux2Driver);

    if (Aux3Driver.isEmpty() == false)
        aux3Combo->addItem(Aux3Driver);

    if (Aux4Driver.isEmpty() == false)
        aux4Combo->addItem(Aux4Driver);

    foreach(DriverInfo *dv, DriverManager::Instance()->getDrivers())
    {
        switch (dv->getType())
        {
        case KSTARS_TELESCOPE:
        {
            for (i=0; i < telescopeCombo->count(); i++)
                if (telescopeCombo->itemText(i) == dv->getTreeLabel())
                    break;
            if (i == telescopeCombo->count())
                telescopeCombo->addItem(dv->getTreeLabel());
        }
        break;

        case KSTARS_CCD:
        {
            for (i=0; i < ccdCombo->count(); i++)
                if (ccdCombo->itemText(i) == dv->getTreeLabel())
                    break;

            if (i == ccdCombo->count())
            {
                ccdCombo->addItem(dv->getTreeLabel());
                guiderCombo->addItem(dv->getTreeLabel());
            }

            // Also add CCD drivers to AUX list
            for (i=0; i < aux1Combo->count(); i++)
                if (aux1Combo->itemText(i) == dv->getTreeLabel())
                    break;
            if (i == aux1Combo->count())
            {
                aux1Combo->addItem(dv->getTreeLabel());
                aux2Combo->addItem(dv->getTreeLabel());
                aux3Combo->addItem(dv->getTreeLabel());
                aux4Combo->addItem(dv->getTreeLabel());
            }

        }
        break;

        case KSTARS_ADAPTIVE_OPTICS:
        {
            for (i=0; i < AOCombo->count(); i++)
                if (AOCombo->itemText(i) == dv->getTreeLabel())
                    break;
            if (i == AOCombo->count())
                AOCombo->addItem(dv->getTreeLabel());
        }
        break;

        case KSTARS_FOCUSER:
        {
            for (i=0; i < focuserCombo->count(); i++)
                if (focuserCombo->itemText(i) == dv->getTreeLabel())
                    break;
            if (i == focuserCombo->count())
                focuserCombo->addItem(dv->getTreeLabel());
        }
            break;

        case KSTARS_FILTER:
        {
            for (i=0; i < filterCombo->count(); i++)
                if (filterCombo->itemText(i) == dv->getTreeLabel())
                    break;
            if (i == filterCombo->count())
                filterCombo->addItem(dv->getTreeLabel());
        }
        break;

        case KSTARS_DOME:
        {
            for (i=0; i < domeCombo->count(); i++)
                if (domeCombo->itemText(i) == dv->getTreeLabel())
                    break;
            if (i == domeCombo->count())
                domeCombo->addItem(dv->getTreeLabel());
        }
        break;

        case KSTARS_WEATHER:
        {
            for (i=0; i < weatherCombo->count(); i++)
                if (weatherCombo->itemText(i) == dv->getTreeLabel())
                    break;
            if (i == weatherCombo->count())
                weatherCombo->addItem(dv->getTreeLabel());
        }
        break;

        case KSTARS_AUXILIARY:
        {
            for (i=0; i < aux1Combo->count(); i++)
                if (aux1Combo->itemText(i) == dv->getTreeLabel())
                    break;
            if (i == aux1Combo->count())
            {
                aux1Combo->addItem(dv->getTreeLabel());
                aux2Combo->addItem(dv->getTreeLabel());
                aux3Combo->addItem(dv->getTreeLabel());
                aux4Combo->addItem(dv->getTreeLabel());
            }
        }
        break;

        default:
            continue;
            break;
        }

        driversList[dv->getTreeLabel()] = dv;
    }

    if (TelescopeDriver.isEmpty() == false)
        telescopeCombo->setCurrentIndex(1);

    if (CCDDriver.isEmpty() == false)
        ccdCombo->setCurrentIndex(1);

    if (GuiderDriver.isEmpty() == false)
        guiderCombo->setCurrentIndex(1);

    if (AODriver.isEmpty() == false)
        AOCombo->setCurrentIndex(1);

    if (FocuserDriver.isEmpty() == false)
        focuserCombo->setCurrentIndex(1);

    if (FilterDriver.isEmpty() == false)
        filterCombo->setCurrentIndex(1);

    if (DomeDriver.isEmpty() == false)
        domeCombo->setCurrentIndex(1);

    if (WeatherDriver.isEmpty() == false)
        weatherCombo->setCurrentIndex(1);

    if (Aux1Driver.isEmpty() == false)
        aux1Combo->setCurrentIndex(1);

    if (Aux2Driver.isEmpty() == false)
        aux2Combo->setCurrentIndex(1);

    if (Aux3Driver.isEmpty() == false)
        aux3Combo->setCurrentIndex(1);

    if (Aux4Driver.isEmpty() == false)
        aux4Combo->setCurrentIndex(1);
}

void EkosManager::saveLocalDrivers()
{

   Options::setTelescopeDriver(telescopeCombo->currentText());

   Options::setCCDDriver(ccdCombo->currentText());

   Options::setGuiderDriver(guiderCombo->currentText());

   Options::setFilterDriver(filterCombo->currentText());

   Options::setFocuserDriver(focuserCombo->currentText());

   Options::setAux1Driver(aux1Combo->currentText());

   Options::setAux2Driver(aux2Combo->currentText());

   Options::setAux3Driver(aux3Combo->currentText());

   Options::setAux4Driver(aux4Combo->currentText());

   Options::setDomeDriver(domeCombo->currentText());

   Options::setWeatherDriver(weatherCombo->currentText());

   Options::setAODriver(AOCombo->currentText());

}

void EkosManager::initRemoteDrivers()
{
    int i=0;

    telescopeCombo->clear();
    ccdCombo->clear();
    guiderCombo->clear();
    AOCombo->clear();
    focuserCombo->clear();
    filterCombo->clear();
    domeCombo->clear();
    weatherCombo->clear();
    aux1Combo->clear();
    aux2Combo->clear();
    aux3Combo->clear();
    aux4Combo->clear();

    telescopeCombo->addItem("--");
    ccdCombo->addItem("--");
    guiderCombo->addItem("--");
    AOCombo->addItem("--");
    focuserCombo->addItem("--");
    filterCombo->addItem("--");
    domeCombo->addItem("--");
    weatherCombo->addItem("--");
    aux1Combo->addItem("--");
    aux2Combo->addItem("--");
    aux3Combo->addItem("--");
    aux4Combo->addItem("--");

    QString remoteScopeName     = (Options::remoteScopeName() == "--") ? "" : Options::remoteScopeName();
    QString remoteCCDName       = (Options::remoteCCDName() == "--") ? "" : Options::remoteCCDName();
    QString remoteGuiderName    = (Options::remoteGuiderName() == "--") ? "" : Options::remoteGuiderName();
    QString remoteAOName        = (Options::remoteAOName() == "--") ? "" : Options::remoteAOName();
    QString remoteFilterName    = (Options::remoteFilterName() == "--") ? "" : Options::remoteFilterName();
    QString remoteFocuserName   = (Options::remoteFocuserName() == "--") ? "" : Options::remoteFocuserName();
    QString remoteDomeName      = (Options::remoteDomeName() == "--") ? "" : Options::remoteDomeName();
    QString remoteWeatherName   = (Options::remoteWeatherName() == "--") ? "" : Options::remoteWeatherName();
    QString remoteAux1Name      = (Options::remoteAux1Name() == "--") ? "" : Options::remoteAux1Name();
    QString remoteAux2Name      = (Options::remoteAux2Name() == "--") ? "" : Options::remoteAux2Name();
    QString remoteAux3Name      = (Options::remoteAux3Name() == "--") ? "" : Options::remoteAux3Name();
    QString remoteAux4Name      = (Options::remoteAux4Name() == "--") ? "" : Options::remoteAux4Name();

    if (remoteScopeName.isEmpty() == false)
        telescopeCombo->addItem(remoteScopeName);

    if (remoteCCDName.isEmpty() == false)
        ccdCombo->addItem(remoteCCDName);

    if (remoteGuiderName.isEmpty() == false)
        guiderCombo->addItem(remoteGuiderName);

    if (remoteAOName.isEmpty() == false)
        AOCombo->addItem(remoteAOName);

    if (remoteFilterName.isEmpty() == false)
        focuserCombo->addItem(remoteFilterName);

    if (remoteFocuserName.isEmpty() == false)
        filterCombo->addItem(remoteFocuserName);

    if (remoteDomeName.isEmpty() == false)
        domeCombo->addItem(remoteDomeName);

    if (remoteWeatherName.isEmpty() == false)
        weatherCombo->addItem(remoteWeatherName);

    if (remoteAux1Name.isEmpty() == false)
        aux1Combo->addItem(remoteAux1Name);

    if (remoteAux2Name.isEmpty() == false)
        aux2Combo->addItem(remoteAux2Name);

    if (remoteAux3Name.isEmpty() == false)
        aux3Combo->addItem(remoteAux3Name);

    if (remoteAux4Name.isEmpty() == false)
        aux4Combo->addItem(remoteAux4Name);

    foreach(DriverInfo *dv, DriverManager::Instance()->getDrivers())
    {
        switch (dv->getType())
        {
        case KSTARS_TELESCOPE:
        {
            for (i=0; i < telescopeCombo->count(); i++)
                if (telescopeCombo->itemText(i) == dv->getName())
                    break;
            if (i == telescopeCombo->count())
                telescopeCombo->addItem(dv->getName());
        }
             break;

        case KSTARS_CCD:
        {
            for (i=0; i < ccdCombo->count(); i++)
                if (ccdCombo->itemText(i) == dv->getName())
                    break;

            if (i == ccdCombo->count())
            {
                ccdCombo->addItem(dv->getName());
                guiderCombo->addItem(dv->getName());
            }

            for (i=0; i < aux1Combo->count(); i++)
                if (aux1Combo->itemText(i) == dv->getName())
                    break;
            if (i == aux1Combo->count())
            {
                aux1Combo->addItem(dv->getName());
                aux2Combo->addItem(dv->getName());
                aux3Combo->addItem(dv->getName());
                aux4Combo->addItem(dv->getName());
            }

        }
        break;

        case KSTARS_ADAPTIVE_OPTICS:
        {
            for (i=0; i < AOCombo->count(); i++)
                if (AOCombo->itemText(i) == dv->getName())
                    break;
            if (i == AOCombo->count())
                AOCombo->addItem(dv->getName());
        }
        break;

        case KSTARS_FOCUSER:
        {
            for (i=0; i < focuserCombo->count(); i++)
                if (focuserCombo->itemText(i) == dv->getName())
                    break;
            if (i == focuserCombo->count())
                focuserCombo->addItem(dv->getName());
        }
            break;

        case KSTARS_FILTER:
        {
            for (i=0; i < filterCombo->count(); i++)
                if (filterCombo->itemText(i) == dv->getName())
                    break;
            if (i == filterCombo->count())
                filterCombo->addItem(dv->getName());
        }
        break;

        case KSTARS_DOME:
        {
            for (i=0; i < domeCombo->count(); i++)
                if (domeCombo->itemText(i) == dv->getName())
                    break;
            if (i == domeCombo->count())
                domeCombo->addItem(dv->getName());
        }
        break;

        case KSTARS_WEATHER:
        {
            for (i=0; i < weatherCombo->count(); i++)
                if (weatherCombo->itemText(i) == dv->getName())
                    break;
            if (i == weatherCombo->count())
                weatherCombo->addItem(dv->getName());
        }
        break;

        case KSTARS_AUXILIARY:
        {
            for (i=0; i < aux1Combo->count(); i++)
                if (aux1Combo->itemText(i) == dv->getName())
                    break;
            if (i == aux1Combo->count())
            {
                aux1Combo->addItem(dv->getName());
                aux2Combo->addItem(dv->getName());
                aux3Combo->addItem(dv->getName());
                aux4Combo->addItem(dv->getName());
            }
        }
        break;

        default:
            continue;
            break;

        }
    }

    if (remoteScopeName.isEmpty() == false)
        telescopeCombo->setCurrentIndex(1);

    if (remoteCCDName.isEmpty() == false)
        ccdCombo->setCurrentIndex(1);

    if (remoteGuiderName.isEmpty() == false)
        guiderCombo->setCurrentIndex(1);

    if (remoteAOName.isEmpty() == false)
        AOCombo->setCurrentIndex(1);

    if (remoteFilterName.isEmpty() == false)
        filterCombo->setCurrentIndex(1);

    if (remoteFocuserName.isEmpty() == false)
        focuserCombo->setCurrentIndex(1);

    if (remoteDomeName.isEmpty() == false)
        domeCombo->setCurrentIndex(1);

    if (remoteWeatherName.isEmpty() == false)
        weatherCombo->setCurrentIndex(1);

    if (remoteAux1Name.isEmpty() == false)
        aux1Combo->setCurrentIndex(1);

    if (remoteAux2Name.isEmpty() == false)
        aux2Combo->setCurrentIndex(1);

    if (remoteAux3Name.isEmpty() == false)
        aux3Combo->setCurrentIndex(1);

    if (remoteAux4Name.isEmpty() == false)
        aux4Combo->setCurrentIndex(1);

}

void EkosManager::saveRemoteDrivers()
{

   Options::setRemoteScopeName(telescopeCombo->currentText());

   Options::setRemoteCCDName(ccdCombo->currentText());

   Options::setRemoteGuiderName(guiderCombo->currentText());

   Options::setRemoteFilterName(filterCombo->currentText());

   Options::setRemoteFocuserName(focuserCombo->currentText());   

   Options::setRemoteDomeName(domeCombo->currentText());

   Options::setRemoteWeatherName(weatherCombo->currentText());

   Options::setRemoteAOName(AOCombo->currentText());

   Options::setRemoteAux1Name(aux1Combo->currentText());

   Options::setRemoteAux2Name(aux2Combo->currentText());

   Options::setRemoteAux3Name(aux3Combo->currentText());

   Options::setRemoteAux4Name(aux4Combo->currentText());

}

void EkosManager::reset()
{
    nDevices=0;
    nConnectedDevices=0;

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
    dome    =  NULL;
    weather =  NULL;
    ao      =  NULL;
    aux1    =  NULL;
    aux2    =  NULL;
    aux3    =  NULL;
    aux4    = NULL;

    delete(scope_di);
    scope_di   = NULL;
    delete(ccd_di);
    ccd_di     = NULL;
    delete(guider_di);
    guider_di  = NULL;
    delete(focuser_di);
    focuser_di = NULL;
    delete(filter_di);
    filter_di  = NULL;
    delete(dome_di);
    dome_di    = NULL;
    delete (weather_di);
    weather_di = NULL;
    delete(ao_di);
    ao_di      = NULL;
    delete(aux1_di);
    aux1_di    = NULL;
    delete(aux2_di);
    aux2_di    = NULL;
    delete(aux3_di);
    aux3_di    = NULL;
    delete(aux4_di);
    aux4_di = NULL;

    captureProcess = NULL;
    focusProcess   = NULL;
    guideProcess   = NULL;
    domeProcess    = NULL;
    alignProcess   = NULL;
    mountProcess   = NULL;
    weatherProcess = NULL;

    ekosStartingStatus  = STATUS_IDLE;
    indiConnectionStatus= STATUS_IDLE;

    guiderCCDName  = "";
    primaryCCDName = "";

    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);
    fitsViewerB->setEnabled(false);
    INDIB->setEnabled(false);
    processINDIB->setEnabled(true);

    processINDIB->setText(i18n("Start INDI"));
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
    ekosStartingStatus = STATUS_IDLE;
    return true;
}

bool EkosManager::start()
{
    managedDevices.clear();
    reset();

    if (localMode)
    {
        DriverInfo *drv = NULL;

        drv = driversList.value(telescopeCombo->currentText());
        if (drv != NULL)
            scope_di   = drv->clone();

        drv = driversList.value(ccdCombo->currentText());
        if (drv != NULL)
            ccd_di     = drv->clone();

        drv = driversList.value(guiderCombo->currentText());
        if (drv != NULL)
            guider_di  = drv->clone();

        drv = driversList.value(AOCombo->currentText());
        if (drv != NULL)
            ao_di      = drv->clone();

        drv = driversList.value(filterCombo->currentText());
        if (drv != NULL)
            filter_di  = drv->clone();

        drv = driversList.value(focuserCombo->currentText());
        if (drv != NULL)
            focuser_di = drv->clone();

        drv = driversList.value(domeCombo->currentText());
        if (drv != NULL)
            dome_di    = drv->clone();

        drv = driversList.value(weatherCombo->currentText());
        if (drv != NULL)
            weather_di    = drv->clone();

        drv = driversList.value(aux1Combo->currentText());
        if (drv != NULL)
            aux1_di    = drv->clone();

        drv = driversList.value(aux2Combo->currentText());
        if (drv != NULL)
            aux2_di    = drv->clone();

        drv = driversList.value(aux3Combo->currentText());
        if (drv != NULL)
            aux3_di    = drv->clone();

        drv = driversList.value(aux4Combo->currentText());
        if (drv != NULL)
            aux4_di    = drv->clone();

        if (guider_di)
        {
            // If the guider and ccd are the same driver, we have two cases:
            // #1 Drivers that only support ONE device per driver (such as sbig)
            // #2 Drivers that supports multiples devices per driver (such as sx)
            // For #1, we modify guider_di to make a unique label for the other device with postfix "Guide"
            // For #2, we set guider_di to NULL and we prompt the user to select which device is primary ccd and which is guider
            // since this is the only way to find out in real time.
            if (guider_di->getDriver() == ccd_di->getDriver())
            {
                if (guider_di->getAuxInfo().value("mdpd", false).toBool() == true)
                    guider_di = NULL;
                else
                {
                    //QVariantMap vMap = guider_di->getAuxInfo();
                    //vMap.insert("DELETE", 1);
                    //guider_di = new DriverInfo(ccd_di);
                    guider_di->setUniqueLabel(guider_di->getTreeLabel() + " Guide");
                    //guider_di->setAuxInfo(vMap);
                }
            }
        }

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
        if (weather_di != NULL)
            managedDevices.append(weather_di);
        if (aux1_di != NULL)
        {
            QVariantMap vMap = aux1_di->getAuxInfo();
            vMap.insert("AUX#", 1);
            aux1_di->setAuxInfo(vMap);
            managedDevices.append(aux1_di);
        }
        if (aux2_di != NULL)
        {
            managedDevices.append(aux2_di);
            QVariantMap vMap = aux2_di->getAuxInfo();
            vMap.insert("AUX#", 2);
            aux2_di->setAuxInfo(vMap);
        }
        if (aux3_di != NULL)
        {
            QVariantMap vMap = aux3_di->getAuxInfo();
            vMap.insert("AUX#", 3);
            aux3_di->setAuxInfo(vMap);
            managedDevices.append(aux3_di);
        }
        if (aux4_di != NULL)
        {
            QVariantMap vMap = aux4_di->getAuxInfo();
            vMap.insert("AUX#", 4);
            aux4_di->setAuxInfo(vMap);
            managedDevices.append(aux4_di);
        }

        if (ccd_di == NULL && guider_di == NULL)
        {
            KMessageBox::error(this, i18n("Ekos requires at least one CCD or Guider to operate."));
            managedDevices.clear();
            return false;
        }

        nDevices = managedDevices.count();

        saveLocalDrivers();
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
            KMessageBox::error(this, i18n("Ekos requires at least one CCD or Guider to operate."));
            delete (remote_indi);
            nDevices=0;
            return false;
        }

        // If the user puts identical device names for both CCD & Guider then
        // this is a case for Multiple-Devices-Per-Driver.
        // We reduce the number of devices since it will be increased once device is detected.
        //if (guiderCombo->currentText() == ccdCombo->currentText())
             //nDevices--;

        if (AOCombo->currentText() != "--")
            nDevices++;
        if (focuserCombo->currentText() != "--")
            nDevices++;
        if (filterCombo->currentText() != "--")
            nDevices++;
        if (domeCombo->currentText() != "--")
            nDevices++;
        if (weatherCombo->currentText() != "--")
            nDevices++;
        if (aux1Combo->currentText() != "--")
            nDevices++;
        if (aux2Combo->currentText() != "--")
            nDevices++;
        if (aux3Combo->currentText() != "--")
            nDevices++;
        if (aux4Combo->currentText() != "--")
            nDevices++;

        nRemoteDevices=0;

        saveRemoteDrivers();
    }

    connect(INDIListener::Instance(), SIGNAL(newDevice(ISD::GDInterface*)), this, SLOT(processNewDevice(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newTelescope(ISD::GDInterface*)), this, SLOT(setTelescope(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newCCD(ISD::GDInterface*)), this, SLOT(setCCD(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newFilter(ISD::GDInterface*)), this, SLOT(setFilter(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newFocuser(ISD::GDInterface*)), this, SLOT(setFocuser(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newDome(ISD::GDInterface*)), this, SLOT(setDome(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newWeather(ISD::GDInterface*)), this, SLOT(setWeather(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newST4(ISD::ST4*)), this, SLOT(setST4(ISD::ST4*)));
    connect(INDIListener::Instance(), SIGNAL(deviceRemoved(ISD::GDInterface*)), this, SLOT(removeDevice(ISD::GDInterface*)), Qt::DirectConnection);
    connect(DriverManager::Instance(), SIGNAL(serverTerminated(QString,QString)), this, SLOT(cleanDevices()));

    if (localMode)
    {
        if (isRunning("indiserver"))
        {
            if (KMessageBox::Yes == (KMessageBox::questionYesNo(0, i18n("Ekos detected an instance of INDI server running. Do you wish to shut down the existing instance before starting a new one?"),
                                                                i18n("INDI Server"), KStandardGuiItem::yes(), KStandardGuiItem::no(), "ekos_shutdown_existing_indiserver")))
            {
                //TODO is there a better way to do this.
                QProcess p;
                p.start("pkill indiserver");
                p.waitForFinished();
            }
        }

        appendLogText(i18n("Starting INDI services..."));

        if (DriverManager::Instance()->startDevices(managedDevices) == false)
        {
            INDIListener::Instance()->disconnect(this);
            ekosStartingStatus = STATUS_ERROR;
            return false;
        }

        ekosStartingStatus = STATUS_PENDING;

        appendLogText(i18n("INDI services started. Please connect devices."));

        if (Options::verboseLogging())
            qDebug() << "INDI services started.";

        QTimer::singleShot(MAX_LOCAL_INDI_TIMEOUT, this, SLOT(checkINDITimeout()));

    }
    else
    {
        appendLogText(i18n("Connecting to remote INDI server at %1 on port %2 ...", Options::remoteHost(), Options::remotePort()));

        QApplication::setOverrideCursor(Qt::WaitCursor);

        if (DriverManager::Instance()->connectRemoteHost(remote_indi) == false)
        {
            appendLogText(i18n("Failed to connect to remote INDI server!"));
            INDIListener::Instance()->disconnect(this);
            delete (remote_indi);
            remote_indi=0;
            ekosStartingStatus = STATUS_ERROR;
            QApplication::restoreOverrideCursor();
            return false;
        }

        QApplication::restoreOverrideCursor();
        ekosStartingStatus = STATUS_PENDING;

        appendLogText(i18n("INDI services started. Connection to remote INDI server is successful."));

        QTimer::singleShot(MAX_REMOTE_INDI_TIMEOUT, this, SLOT(checkINDITimeout()));

    }

    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);
    fitsViewerB->setEnabled(false);
    INDIB->setEnabled(false);

    processINDIB->setText(i18n("Stop INDI"));

    return true;


}

void EkosManager::checkINDITimeout()
{
    if (nDevices <= 0)
    {
        ekosStartingStatus = STATUS_SUCCESS;
        return;
    }

    if (localMode)
    {
        QStringList remainingDevices;
        if (scope_di && scope == NULL)
            remainingDevices << QString("+ %1").arg(scope_di->getName());
        if (ccd_di && ccd == NULL)
            remainingDevices << QString("+ %1").arg(ccd_di->getName());
        if (guider_di && guider == NULL)
            remainingDevices << QString("+ %1").arg(guider_di->getName());
        if (focuser_di && focuser == NULL)
            remainingDevices << QString("+ %1").arg(focuser_di->getName());
        if (filter_di && filter == NULL)
            remainingDevices << QString("+ %1").arg(filter_di->getName());        
        if (ao_di && ao == NULL)
            remainingDevices << QString("+ %1").arg(ao_di->getName());
        if (dome_di && dome == NULL)
            remainingDevices << QString("+ %1").arg(dome_di->getName());
        if (weather_di && weather == NULL)
            remainingDevices << QString("+ %1").arg(weather_di->getName());
        if (aux1_di && aux1 == NULL)
            remainingDevices << QString("+ %1").arg(aux1_di->getName());
        if (aux2_di && aux2 == NULL)
            remainingDevices << QString("+ %1").arg(aux2_di->getName());
        if (aux3_di && aux3 == NULL)
            remainingDevices << QString("+ %1").arg(aux3_di->getName());
        if (aux4_di && aux4 == NULL)
            remainingDevices << QString("+ %1").arg(aux4_di->getName());

        if (remainingDevices.count() == 1)
        {
            appendLogText(i18n("Unable to establish:\n%1\nPlease ensure the device is connected and powered on.", remainingDevices.at(0)));
            KNotification::beep(i18n("Ekos startup error"));
        }
        else
        {
            appendLogText(i18n("Unable to establish the following devices:\n%1\nPlease ensure each device is connected and powered on.", remainingDevices.join("\n")));
            KNotification::beep(i18n("Ekos startup error"));
        }
    }
    else
    {
        QStringList remainingDevices;
        if (telescopeCombo->currentText() != "--" && scope == NULL)
            remainingDevices << QString("+ %1").arg(telescopeCombo->currentText());
        if (ccdCombo->currentText() != "--" && ccd == NULL)
            remainingDevices << QString("+ %1").arg(ccdCombo->currentText());
        if (guiderCombo->currentText() != "--" && guider == NULL)
            remainingDevices << QString("+ %1").arg(guiderCombo->currentText());
        if (focuserCombo->currentText() != "--" && focuser == NULL)
            remainingDevices << QString("+ %1").arg(focuserCombo->currentText());
        if (filterCombo->currentText() != "--" && filter == NULL)
            remainingDevices << QString("+ %1").arg(filterCombo->currentText());
        if (domeCombo->currentText() != "--" && dome == NULL)
            remainingDevices << QString("+ %1").arg(domeCombo->currentText());        
        if (weatherCombo->currentText() != "--" && weather == NULL)
            remainingDevices << QString("+ %1").arg(weatherCombo->currentText());
        if (AOCombo->currentText() != "--" && ao == NULL)
            remainingDevices << QString("+ %1").arg(AOCombo->currentText());
        if (aux1Combo->currentText() != "--" && aux1 == NULL)
            remainingDevices << QString("+ %1").arg(aux1Combo->currentText());
        if (aux2Combo->currentText() != "--" && aux2 == NULL)
            remainingDevices << QString("+ %1").arg(aux2Combo->currentText());
        if (aux3Combo->currentText() != "--" && aux3 == NULL)
            remainingDevices << QString("+ %1").arg(aux3Combo->currentText());
        if (aux4Combo->currentText() != "--" && aux4 == NULL)
            remainingDevices << QString("+ %1").arg(aux3Combo->currentText());

        if (remainingDevices.count() == 1)
        {
            appendLogText(i18n("Unable to establish remote device:\n%1\nPlease ensure remote device name corresponds to actual device name.", remainingDevices.at(0)));
            KNotification::beep(i18n("Ekos startup error"));
        }
        else
        {
            appendLogText(i18n("Unable to establish remote devices:\n%1\nPlease ensure remote device name corresponds to actual device name.", remainingDevices.join("\n")));
            KNotification::beep(i18n("Ekos startup error"));
        }
    }

    ekosStartingStatus = STATUS_ERROR;
}

void EkosManager::refreshRemoteDrivers()
{
    if (localMode == false)
        initRemoteDrivers();
}

void EkosManager::connectDevices()
{

    indiConnectionStatus = STATUS_PENDING;

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

    if (weather)
        weather->Connect();

    if (aux1)
        aux1->Connect();

    if (aux2)
        aux2->Connect();

    if (aux3)
        aux3->Connect();

    if (aux4)
        aux4->Connect();

    connectB->setEnabled(false);
    disconnectB->setEnabled(true);

    appendLogText(i18n("Connecting INDI devices..."));

    if (Options::verboseLogging())
        qDebug() << "Connecting INDI devices...";
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
            disconnect(focuser, SIGNAL(numberUpdated(INumberVectorProperty*)), focusProcess, SLOT(processFocusNumber(INumberVectorProperty*)));
            focusProcess->setEnabled(false);
        }
     }

    if (dome)
        dome->Disconnect();

    if (weather)
        weather->Disconnect();

    if (aux1)
        aux1->Disconnect();

    if (aux2)
        aux2->Disconnect();

    if (aux3)
        aux3->Disconnect();

    if (aux4)
        aux4->Disconnect();


    appendLogText(i18n("Disconnecting INDI devices..."));

    if (Options::verboseLogging())
        qDebug() << "Disconnecting INDI devices...";

}

void EkosManager::cleanDevices()
{

    INDIListener::Instance()->disconnect(this);

    if (localMode)
    {
        DriverManager::Instance()->stopDevices(managedDevices);
        /*if (guider_di)
        {
            if (guider_di->getAuxInfo().value("DELETE") == 1)
            {
                delete(guider_di);
                guider_di=NULL;
            }
        }*/
    }
    else if (remote_indi)
    {
        DriverManager::Instance()->disconnectRemoteHost(remote_indi);
        remote_indi = 0;
    }


    nRemoteDevices=0;
    nDevices = 0;
    managedDevices.clear();

    reset();

    processINDIB->setText(i18n("Start INDI"));
    processINDIB->setEnabled(true);
    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);
    fitsViewerB->setEnabled(false);
    INDIB->setEnabled(false);

    appendLogText(i18n("INDI services stopped."));

    if (Options::verboseLogging())
        qDebug() << "Stopping INDI services.";
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
    if (Options::verboseLogging())
        qDebug() << "Ekos received a new device: " << devInterface->getDeviceName();

    DriverInfo *di = devInterface->getDriverInfo();

            switch (di->getType())
            {
               case KSTARS_TELESCOPE:
                scope = devInterface;
                break;


               case KSTARS_CCD:
                if (aux1_di == di)
                {
                    aux1 = devInterface;
                    break;
                }
                else if (aux2_di == di)
                {
                    aux2 = devInterface;
                    break;
                }
                else if (aux3_di == di)
                {
                    aux3 = devInterface;
                    break;
                }
                else if (aux4_di == di)
                {
                    aux4 = devInterface;
                    break;
                }

                if (guider_di == di)
                {
                    guider = devInterface;
                    guiderCCDName = devInterface->getDeviceName();
                }
                else if (ccd_di->getAuxInfo().value("mdpd", false).toBool() == true)
                {
                    if (ccdDriverSelected == false)
                    {
                        ccd = devInterface;
                        primaryCCDName = QString(devInterface->getDeviceName());
                        ccdDriverSelected = true;

                    }
                    else
                    {
                        guider = devInterface;
                        guiderCCDName = devInterface->getDeviceName();
                        //guideDriverSelected = true;

                    }
                }
                else
                {
                    primaryCCDName = QString(devInterface->getDeviceName());
                    ccd = devInterface;
                }

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

            case KSTARS_WEATHER:
               if (weather_di == di)
                   weather = devInterface;
               break;

             case KSTARS_AUXILIARY:
                if (aux1_di == di)
                    aux1 = devInterface;
                else if (aux2_di == di)
                    aux2 = devInterface;
                else if (aux3_di == di)
                    aux3 = devInterface;
                else if (aux4_di == di)
                    aux4 = devInterface;
                    break;

              default:
                return;
                break;
            }

     nDevices--;

    connect(devInterface, SIGNAL(Connected()), this, SLOT(deviceConnected()));
    connect(devInterface, SIGNAL(Disconnected()), this, SLOT(deviceDisconnected()));
    connect(devInterface, SIGNAL(propertyDefined(INDI::Property*)), this, SLOT(processNewProperty(INDI::Property*)));

    if (nDevices <= 0)
    {
        ekosStartingStatus = STATUS_SUCCESS;

        connectB->setEnabled(true);
        disconnectB->setEnabled(false);
        controlPanelB->setEnabled(true);
        INDIB->setEnabled(true);
        //fitsViewerB->setEnabled(true);
    }

}

void EkosManager::processRemoteDevice(ISD::GDInterface *devInterface)
{
    QString deviceName(devInterface->getDeviceName());

    if (deviceName == Options::remoteScopeName())
        scope = devInterface;
    else if (remoteCCDRegistered == false && (deviceName == Options::remoteCCDName() ||
                                              deviceName.startsWith(Options::remoteCCDName())))
    {
        ccd = devInterface;
        primaryCCDName = QString(devInterface->getDeviceName());
        remoteCCDRegistered = true;
    }
    else if (remoteGuideRegistered == false &&
            ( (Options::remoteGuiderName().isEmpty() == false && deviceName.startsWith(Options::remoteGuiderName()) ) ||
              (deviceName.startsWith(Options::remoteCCDName()))))
    {
        guider = devInterface;
        guiderCCDName = QString(devInterface->getDeviceName());
        remoteGuideRegistered = true;
     }
    else if (deviceName == Options::remoteAOName())
        ao = devInterface;
    else if (deviceName == Options::remoteFocuserName())
        focuser = devInterface;
    else if (deviceName == Options::remoteFilterName())
        filter = devInterface;
    else if (deviceName == Options::remoteDomeName())
        dome = devInterface;
    else if (deviceName == Options::remoteWeatherName())
        weather = devInterface;
    else if (deviceName == Options::remoteAux1Name().toLatin1())
        aux1 = devInterface;
    else if (deviceName == Options::remoteAux2Name().toLatin1())
        aux2 = devInterface;
    else if (deviceName == Options::remoteAux3Name().toLatin1())
        aux3 = devInterface;
    else if (deviceName == Options::remoteAux4Name().toLatin1())
        aux4 = devInterface;
    else
        return;

    nDevices--;
    nRemoteDevices++;
    connect(devInterface, SIGNAL(Connected()), this, SLOT(deviceConnected()));
    connect(devInterface, SIGNAL(Disconnected()), this, SLOT(deviceDisconnected()));
    connect(devInterface, SIGNAL(propertyDefined(INDI::Property*)), this, SLOT(processNewProperty(INDI::Property*)));

    if (nDevices <= 0)
    {
        ekosStartingStatus = STATUS_SUCCESS;

        if (nDevices == 0)
            appendLogText(i18n("Remote devices established. Please connect devices."));

        connectB->setEnabled(true);
        disconnectB->setEnabled(false);
        controlPanelB->setEnabled(true);
        INDIB->setEnabled(true);
        //fitsViewerB->setEnabled(true);
    }
}

void EkosManager::deviceConnected()
{
    connectB->setEnabled(false);
    disconnectB->setEnabled(true);

    processINDIB->setEnabled(false);

    nConnectedDevices++;

    if (nConnectedDevices == managedDevices.count() || nConnectedDevices == nRemoteDevices)
        indiConnectionStatus = STATUS_SUCCESS;

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

    if (weather && weather->isConnected())
    {
        configProp = weather->getBaseDevice()->getSwitch("CONFIG_PROCESS");
        if (configProp && configProp->s == IPS_IDLE)
           weather->setConfig(tConfig);
    }

    if (aux1 && aux1->isConnected())
    {
        configProp = aux1->getBaseDevice()->getSwitch("CONFIG_PROCESS");
        if (configProp && configProp->s == IPS_IDLE)
           aux1->setConfig(tConfig);
    }

    if (aux2 && aux2->isConnected())
    {
        configProp = aux2->getBaseDevice()->getSwitch("CONFIG_PROCESS");
        if (configProp && configProp->s == IPS_IDLE)
           aux2->setConfig(tConfig);
    }

    if (aux3 && aux3->isConnected())
    {
        configProp = aux3->getBaseDevice()->getSwitch("CONFIG_PROCESS");
        if (configProp && configProp->s == IPS_IDLE)
           aux3->setConfig(tConfig);
    }

    if (aux4 && aux4->isConnected())
    {
        configProp = aux4->getBaseDevice()->getSwitch("CONFIG_PROCESS");
        if (configProp && configProp->s == IPS_IDLE)
           aux4->setConfig(tConfig);
    }
}

void EkosManager::deviceDisconnected()
{
    ISD::GDInterface *dev = static_cast<ISD::GDInterface *> (sender());

    if (dev)
    {
        if (dev->getState("CONNECTION") == IPS_ALERT)
            indiConnectionStatus = STATUS_ERROR;
        else
            indiConnectionStatus = STATUS_IDLE;
    }
    else
        indiConnectionStatus = STATUS_IDLE;

    nConnectedDevices--;

    if (nConnectedDevices < 0)
        nConnectedDevices = 0;

    connectB->setEnabled(true);
    disconnectB->setEnabled(false);

    processINDIB->setEnabled(true);

}

void EkosManager::setTelescope(ISD::GDInterface *scopeDevice)
{
    scope = scopeDevice;

    appendLogText(i18n("%1 is online.", scope->getDeviceName()));

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
    bool isPrimaryCCD = (primaryCCDName == QString(ccdDevice->getDeviceName()));

    if (isPrimaryCCD)
    {
        ccd = ccdDevice;
    }

    initCapture();

    captureProcess->addCCD(ccdDevice, isPrimaryCCD);

    initFocus();

    focusProcess->addCCD(ccdDevice, isPrimaryCCD);

    initAlign();

    alignProcess->addCCD(ccdDevice, isPrimaryCCD);

    if (guiderCCDName.isEmpty() == false || useGuideHead)
    {
        guider = ccdDevice;
        initGuide();
        guideProcess->addCCD(ccdDevice, !isPrimaryCCD);
        if (scopeRegistered)
            guideProcess->setTelescope(scope);
    }

    appendLogText(i18n("%1 is online.", ccdDevice->getDeviceName()));

    connect(ccdDevice, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processNewNumber(INumberVectorProperty*)), Qt::UniqueConnection);

   if (scopeRegistered)
   {
      alignProcess->setTelescope(scope);
      captureProcess->setTelescope(scope);
   }
}

/*void EkosManager::setCCD(ISD::GDInterface *ccdDevice)
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
        appendLogText(i18n("%1 is online.", ccdDevice->getDeviceName()));

        initGuide();

        guideProcess->addCCD(guider, true);
        guideProcess->addCCD(ccd, false);

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
            appendLogText(i18n("%1 is online.", ccdDevice->getDeviceName()));

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

}*/

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

    initCapture();

    initFocus();

    focusProcess->addFocuser(focuser);

    appendLogText(i18n("%1 is online.", focuser->getDeviceName()));
}

void EkosManager::setDome(ISD::GDInterface *domeDevice)
{
    initDome();

    dome = domeDevice;

    domeProcess->setDome(dome);

    appendLogText(i18n("%1 is online.", dome->getDeviceName()));
}

void EkosManager::setWeather(ISD::GDInterface *weatherDevice)
{
    initWeather();

    weather = weatherDevice;

    weatherProcess->setWeather(weather);

    appendLogText(i18n("%1 is online.", weather->getDeviceName()));
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


    appendLogText(i18n("%1 is offline.", devInterface->getDeviceName()));

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
        if (focusProcess)
            focusProcess->syncCCDInfo();

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

    if (focusProcess && currentWidget != focusProcess)
    {
        if (focusProcess)
            focusProcess->resetFrame();
    }

    if (alignProcess && currentWidget == alignProcess)
    {
        if (alignProcess->isEnabled() == false && captureProcess->isEnabled() && ccd && ccd->isConnected())
        {
            if (alignProcess->isParserOK())
                alignProcess->setEnabled(true);
        }

        alignProcess->checkCCD();
    }
    else if (captureProcess && currentWidget == captureProcess)
    {
        captureProcess->checkCCD();
    }
    else if (focusProcess && currentWidget == focusProcess)
    {
        focusProcess->checkCCD();
    }
    else if (guideProcess && currentWidget == guideProcess)
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
     if (currentWidget == schedulerProcess)
        ekosLogOut->setPlainText(schedulerProcess->getLogText());


}

void EkosManager::appendLogText(const QString &text)
{

    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

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
    else if (currentWidget == schedulerProcess)
        schedulerProcess->clearLog();

}

void EkosManager::initCapture()
{
    if (captureProcess)
        return;

     captureProcess = new Ekos::Capture();
     toolsWidget->addTab( captureProcess, i18n("CCD"));
     connect(captureProcess, SIGNAL(newLog()), this, SLOT(updateLog()));

     if (focusProcess)
     {
         // Autofocus
         connect(captureProcess, SIGNAL(checkFocus(double)), focusProcess, SLOT(checkFocus(double)), Qt::UniqueConnection);
         connect(focusProcess, SIGNAL(autoFocusFinished(bool, double)), captureProcess, SLOT(updateAutofocusStatus(bool, double)),  Qt::UniqueConnection);

         // Meridian Flip
         connect(captureProcess, SIGNAL(meridianFlipStarted()), focusProcess, SLOT(resetFocusFrame()), Qt::UniqueConnection);
     }

     if (alignProcess)
     {
         // Alignment flag
         connect(alignProcess, SIGNAL(solverComplete(bool)), captureProcess, SLOT(enableAlignmentFlag()), Qt::UniqueConnection);
         connect(alignProcess, SIGNAL(solverSlewComplete()), captureProcess, SLOT(checkAlignmentSlewComplete()), Qt::UniqueConnection);

         // Meridian Flip
         connect(captureProcess, SIGNAL(meridialFlipTracked()), alignProcess, SLOT(captureAndSolve()), Qt::UniqueConnection);
     }

     if (mountProcess)
     {
         // Meridian Flip
         connect(captureProcess, SIGNAL(meridianFlipStarted()), mountProcess, SLOT(disableAltLimits()), Qt::UniqueConnection);
         connect(captureProcess, SIGNAL(meridianFlipCompleted()), mountProcess, SLOT(enableAltLimits()), Qt::UniqueConnection);
     }

}

void EkosManager::initAlign()
{
    if (alignProcess)
        return;

     alignProcess = new Ekos::Align();
     toolsWidget->addTab( alignProcess, i18n("Alignment"));
     connect(alignProcess, SIGNAL(newLog()), this, SLOT(updateLog()));

     if (captureProcess)
     {
         // Alignment flag
         connect(alignProcess, SIGNAL(solverComplete(bool)), captureProcess, SLOT(enableAlignmentFlag()), Qt::UniqueConnection);
         connect(alignProcess, SIGNAL(solverSlewComplete()), captureProcess, SLOT(checkAlignmentSlewComplete()), Qt::UniqueConnection);

         // Meridian Flip
         connect(captureProcess, SIGNAL(meridialFlipTracked()), alignProcess, SLOT(captureAndSolve()), Qt::UniqueConnection);
     }

     if (focusProcess)
     {
         // Filter lock
         connect(focusProcess, SIGNAL(filterLockUpdated(ISD::GDInterface*,int)), alignProcess, SLOT(setLockedFilter(ISD::GDInterface*,int)), Qt::UniqueConnection);
     }
}


void EkosManager::initFocus()
{
    if (focusProcess)
        return;

    focusProcess = new Ekos::Focus();
    toolsWidget->addTab( focusProcess, i18n("Focus"));
    connect(focusProcess, SIGNAL(newLog()), this, SLOT(updateLog()));

    if (captureProcess)
    {
        // Autofocus
        connect(captureProcess, SIGNAL(checkFocus(double)), focusProcess, SLOT(checkFocus(double)), Qt::UniqueConnection);        
        connect(focusProcess, SIGNAL(autoFocusFinished(bool, double)), captureProcess, SLOT(updateAutofocusStatus(bool, double)), Qt::UniqueConnection);

        // Meridian Flip
        connect(captureProcess, SIGNAL(meridianFlipStarted()), focusProcess, SLOT(resetFocusFrame()), Qt::UniqueConnection);
    }

    if (guideProcess)
    {
        // Suspend
        connect(focusProcess, SIGNAL(suspendGuiding(bool)), guideProcess, SLOT(setSuspended(bool)), Qt::UniqueConnection);
    }

    if (alignProcess)
    {
        // Filter lock
        connect(focusProcess, SIGNAL(filterLockUpdated(ISD::GDInterface*,int)), alignProcess, SLOT(setLockedFilter(ISD::GDInterface*,int)), Qt::UniqueConnection);
    }

}

void EkosManager::initMount()
{
    if (mountProcess)
        return;

    mountProcess = new Ekos::Mount();
    toolsWidget->addTab(mountProcess, i18n("Mount"));
    connect(mountProcess, SIGNAL(newLog()), this, SLOT(updateLog()));

    if (captureProcess)
    {
        // Meridian Flip
        connect(captureProcess, SIGNAL(meridianFlipStarted()), mountProcess, SLOT(disableAltLimits()), Qt::UniqueConnection);
        connect(captureProcess, SIGNAL(meridianFlipCompleted()), mountProcess, SLOT(enableAltLimits()), Qt::UniqueConnection);
    }

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

        toolsWidget->addTab( guideProcess, i18n("Guide"));
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
        connect(guideProcess, SIGNAL(ditherFailed()), captureProcess, SLOT(abort()));
        connect(guideProcess, SIGNAL(ditherToggled(bool)), captureProcess, SLOT(setGuideDither(bool)));        
        connect(captureProcess, SIGNAL(exposureComplete()), guideProcess, SLOT(dither()));

        // Parking
        connect(captureProcess, SIGNAL(telescopeParking()), guideProcess, SLOT(stopGuiding()));

        // Guide Head
        connect(captureProcess, SIGNAL(suspendGuiding(bool)), guideProcess, SLOT(setSuspended(bool)));
        connect(guideProcess, SIGNAL(guideChipUpdated(ISD::CCDChip*)), captureProcess, SLOT(setGuideChip(ISD::CCDChip*)));

        // Meridian Flip
        connect(captureProcess, SIGNAL(meridianFlipStarted()), guideProcess, SLOT(stopGuiding()), Qt::UniqueConnection);
        connect(captureProcess, SIGNAL(meridianFlipCompleted()), guideProcess, SLOT(startAutoCalibrateGuiding()), Qt::UniqueConnection);
    }

    if (focusProcess)
    {
        // Suspend
        connect(focusProcess, SIGNAL(suspendGuiding(bool)), guideProcess, SLOT(setSuspended(bool)), Qt::UniqueConnection);
    }

}

void EkosManager::initDome()
{
    if (domeProcess)
        return;

    domeProcess = new Ekos::Dome();
}

void EkosManager::initWeather()
{
    if (weatherProcess)
        return;

    weatherProcess = new Ekos::Weather();
}

void EkosManager::setST4(ISD::ST4 * st4Driver)
{
     appendLogText(i18n("Guider port from %1 is ready.", st4Driver->getDeviceName()));
     useST4=true;

     initGuide();

     guideProcess->addST4(st4Driver);

     if (ao && ao->getDeviceName() == st4Driver->getDeviceName())
         guideProcess->setAO(st4Driver);

}

void EkosManager::removeTabs()
{

    disconnect(toolsWidget, SIGNAL(currentChanged(int)), this, SLOT(processTabChange()));

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
        delete (domeProcess);
        domeProcess = NULL;

        weather = NULL;
        delete (weatherProcess);
        weatherProcess = NULL;

        aux1 = NULL;
        aux2 = NULL;
        aux3 = NULL;
        aux4 = NULL;

        connect(toolsWidget, SIGNAL(currentChanged(int)), this, SLOT(processTabChange()));

}

bool EkosManager::isRunning(const QString &process)
{
  QProcess ps;
  ps.start("ps", QStringList() << "-o" << "comm" << "--no-headers" << "-C" << process);
  ps.waitForFinished();
  QString output = ps.readAllStandardOutput();
  return output.startsWith(process);
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

void EkosManager::setWeather(const QString & weatherName)
{
  if (localMode)
  {
      for (int i=0; i < weatherCombo->count(); i++)
          if (weatherCombo->itemText(i) == weatherName)
          {
              weatherCombo->setCurrentIndex(i);
              break;
          }
  }
  else
      Options::setRemoteWeatherName(weatherName);

}

void EkosManager::setAuxiliary(int index, const QString & auxiliaryName)
{
    if (localMode)
    {
        switch (index)
        {
            case 1:
            for (int i=0; i < aux1Combo->count(); i++)
                if (aux1Combo->itemText(i) == auxiliaryName)
                {
                    aux1Combo->setCurrentIndex(i);
                    break;
                }
            break;

            case 2:
            for (int i=0; i < aux2Combo->count(); i++)
                if (aux2Combo->itemText(i) == auxiliaryName)
                {
                    aux2Combo->setCurrentIndex(i);
                    break;
                }
            break;

            case 3:
            for (int i=0; i < aux3Combo->count(); i++)
                if (aux3Combo->itemText(i) == auxiliaryName)
                {
                    aux3Combo->setCurrentIndex(i);
                    break;
                }
            break;

            case 4:
            for (int i=0; i < aux4Combo->count(); i++)
                if (aux4Combo->itemText(i) == auxiliaryName)
                {
                    aux4Combo->setCurrentIndex(i);
                    break;
                }
        break;

           default:
            break;

        }


    }
    else
    {
        switch (index)
        {
            case 1:
            Options::setRemoteAux1Name(auxiliaryName);
            break;

            case 2:
            Options::setRemoteAux2Name(auxiliaryName);
            break;

            case 3:
            Options::setRemoteAux3Name(auxiliaryName);
            break;

            case 4:
            Options::setRemoteAux4Name(auxiliaryName);
            break;

            default:
                break;
        }
    }
}

void EkosManager::toggleINDIPanel()
{
    if (GUIManager::Instance()->isVisible())
        GUIManager::Instance()->hide();
    else
        GUIManager::Instance()->show();
}

void EkosManager::toggleFITSViewer()
{
    FITSViewer *ccdViewer = NULL, *guiderViewer= NULL;

    if (ccd)
    {
        ISD::CCD *myCCD = static_cast<ISD::CCD *> (ccd);
        if ( (ccdViewer = myCCD->getViewer()) )
        {
            if (myCCD->getViewer()->isVisible())
                myCCD->getViewer()->hide();
            else
            {
                myCCD->getViewer()->raise();
                myCCD->getViewer()->activateWindow();
                myCCD->getViewer()->showNormal();
            }
        }
    }

    if (guider && guider != ccd)
    {
        ISD::CCD * myGuider = static_cast<ISD::CCD *> (guider);
        if ( (guiderViewer = myGuider->getViewer()) )
        {
            if (myGuider->getViewer()->isVisible())
                myGuider->getViewer()->hide();
            else
            {
                myGuider->getViewer()->raise();
                myGuider->getViewer()->activateWindow();
                myGuider->getViewer()->showNormal();
            }
        }
    }

    if (ccdViewer == NULL && guiderViewer == NULL)
        appendLogText(i18n("No active FITS Viewer windows to show/hide."));
}

void EkosManager::checkFITSViewerState()
{
    QAction *a = (QAction *) sender();
    if (a)
        fitsViewerB->setEnabled(a->isEnabled());
}

void EkosManager::addObjectToScheduler(SkyObject *object)
{
    if (schedulerProcess)
        schedulerProcess->addObject(object);
}


