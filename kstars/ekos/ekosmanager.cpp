/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikartech.com>

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
#include "kstarsdata.h"
#include "auxiliary/ksuserdb.h"
#include "fitsviewer/fitsviewer.h"

#include "profileeditor.h"
#include "profileinfo.h"

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

    profileModel = NULL;

    mount   =  NULL;
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
    dustCap =  NULL;
    lightBox= NULL;

    mount_di   = NULL;
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
    dustCapProcess = NULL;

    ekosOption     = NULL;

    //kcfg_localMode->setChecked(Options::localMode());
    //kcfg_remoteMode->setChecked(Options::remoteMode());

    toolsWidget->setIconSize(QSize(64,64));
    connect(toolsWidget, SIGNAL(currentChanged(int)), this, SLOT(processTabChange()));

    // Enable scheduler Tab
    toolsWidget->setTabEnabled(1, false);

    // Start/Stop INDI Server
    connect(processINDIB, SIGNAL(clicked()), this, SLOT(processINDI()));

    // Connect/Disconnect INDI devices
    connect(connectB, SIGNAL(clicked()), this, SLOT(connectDevices()));
    connect(disconnectB, SIGNAL(clicked()), this, SLOT(disconnectDevices()));

    // INDI Control Panel
    connect(controlPanelB, SIGNAL(clicked()), GUIManager::Instance(), SLOT(show()));

    // Show Ekos Options
    QAction *a = KStars::Instance()->actionCollection()->action( "show_fits_viewer" );
    connect(a, SIGNAL(changed()), this, SLOT(checkFITSViewerState()));
    connect(optionsB, SIGNAL(clicked()), KStars::Instance(), SLOT(slotViewOps()));

    // Clear Ekos Log
    connect(clearB, SIGNAL(clicked()), this, SLOT(clearLog()));

    // Profiles
    connect(addProfileB, SIGNAL(clicked()), this, SLOT(addProfile()));
    connect(editProfileB, SIGNAL(clicked()), this, SLOT(editProfile()));
    connect(deleteProfileB, SIGNAL(clicked()), this, SLOT(deleteProfile()));
    connect(profileCombo, SIGNAL(activated(QString)), this, SLOT(saveDefaultProfile(QString)));

    // Set Profile icons
    addProfileB->setIcon(QIcon::fromTheme("list-add"));
    editProfileB->setIcon(QIcon::fromTheme("edit-entry"));
    deleteProfileB->setIcon(QIcon::fromTheme("list-remove"));

    //connect(profileCombo, SIGNAL(activated(int)), this, SLOT(processINDIModeChange()));

    /*localMode = Options::localMode();

    if (localMode)
        initLocalDrivers();
    else
        initRemoteDrivers();*/

    // Load all drivers
    loadDrivers();

    // Load add driver profiles
    loadProfiles();

    // Setup Tab
    toolsWidget->tabBar()->setTabIcon(0, QIcon::fromTheme("kstars_ekos_setup"));
    toolsWidget->tabBar()->setTabToolTip(0, i18n("Setup"));

    // Initialize Ekos Scheduler Module
    schedulerProcess = new Ekos::Scheduler();
    toolsWidget->addTab( schedulerProcess, QIcon::fromTheme("kstars_ekos_scheduler"), "");
    toolsWidget->tabBar()->setTabToolTip(1, i18n("Scheduler"));
    connect(schedulerProcess, SIGNAL(newLog()), this, SLOT(updateLog()));

    // Temporary fix. Not sure how to resize Ekos Dialog to fit contents of the various tabs in the QScrollArea which are added
    // dynamically. I used setMinimumSize() but it doesn't appear to make any difference.
    // Also set Layout policy to SetMinAndMaxSize as well. Any idea how to fix this?
    // FIXME
    resize(900,750);
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
    delete dustCapProcess;
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

#if 0
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
#endif

void EkosManager::loadProfiles()
{
    profiles = KStarsData::Instance()->userdb()->GetAllProfiles();

    if (profileModel)
        profileModel->clear();
    else
        profileModel = new QStandardItemModel(0, 4);

    profileModel->setHorizontalHeaderLabels(QStringList() << "id" << "name" << "host" << "port");

    foreach(ProfileInfo *pi, profiles)
    {
        QList<QStandardItem*> info;

        info << new QStandardItem(pi->id) << new QStandardItem(pi->name) << new QStandardItem(pi->host) << new QStandardItem(pi->port);
        profileModel->appendRow(info);
    }

    profileModel->sort(0);
    profileCombo->setModel(profileModel);
    profileCombo->setModelColumn(1);

    // Load last used profile from options
    int index = profileCombo->findText(Options::profile());
    // If not found, set it to first item
    if (index == -1)
        index=0;
    profileCombo->setCurrentIndex(index);
}

void EkosManager::loadDrivers()
{
    foreach(DriverInfo *dv, DriverManager::Instance()->getDrivers())
    {
        if (dv->getDriverSource() != HOST_SOURCE)
            driversList[dv->getTreeLabel()] = dv;
    }
}

#if 0
void EkosManager::initLocalDrivers()
{
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
            if (telescopeCombo->findText(dv->getTreeLabel()) == -1)
                telescopeCombo->addItem(dv->getTreeLabel());
        }
        break;

        case KSTARS_CCD:
        {
            if (ccdCombo->findText(dv->getTreeLabel()) == -1)
                ccdCombo->addItem(dv->getTreeLabel());

            if (guiderCombo->findText(dv->getTreeLabel()) == -1)
                guiderCombo->addItem(dv->getTreeLabel());

            // Also add CCD drivers to AUX list
            if (aux1Combo->findText(dv->getTreeLabel()) == -1)
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
            if (AOCombo->findText(dv->getTreeLabel()) == -1)
                AOCombo->addItem(dv->getTreeLabel());
        }
        break;

        case KSTARS_FOCUSER:
        {
            if (focuserCombo->findText(dv->getTreeLabel()) == -1)
                focuserCombo->addItem(dv->getTreeLabel());
        }
            break;

        case KSTARS_FILTER:
        {
            if (filterCombo->findText(dv->getTreeLabel()) == -1)
                filterCombo->addItem(dv->getTreeLabel());
        }
        break;

        case KSTARS_DOME:
        {
            if (domeCombo->findText(dv->getTreeLabel()) == -1)
                domeCombo->addItem(dv->getTreeLabel());
        }
        break;

        case KSTARS_WEATHER:
        {
            if (weatherCombo->findText(dv->getTreeLabel()) == -1)
                weatherCombo->addItem(dv->getTreeLabel());
        }
        break;

        case KSTARS_AUXILIARY:
        {
            if (aux1Combo->findText(dv->getTreeLabel()) == -1)
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
        filterCombo->addItem(remoteFilterName);

    if (remoteFocuserName.isEmpty() == false)
        focuserCombo->addItem(remoteFocuserName);

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
            if (telescopeCombo->findText(dv->getName()) == -1)
                telescopeCombo->addItem(dv->getName());
        }
        break;

        case KSTARS_CCD:
        {
            if (ccdCombo->findText(dv->getName()) == -1)
                ccdCombo->addItem(dv->getName());

            if (guiderCombo->findText(dv->getName()) == -1)
                guiderCombo->addItem(dv->getName());

            // Also add CCD drivers to AUX list
            if (aux1Combo->findText(dv->getName()) == -1)
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
            if (AOCombo->findText(dv->getName()) == -1)
                AOCombo->addItem(dv->getName());
        }
        break;

        case KSTARS_FOCUSER:
        {
            if (focuserCombo->findText(dv->getName()) == -1)
                focuserCombo->addItem(dv->getName());
        }
            break;

        case KSTARS_FILTER:
        {
            if (filterCombo->findText(dv->getName()) == -1)
                filterCombo->addItem(dv->getName());
        }
        break;

        case KSTARS_DOME:
        {
            if (domeCombo->findText(dv->getName()) == -1)
                domeCombo->addItem(dv->getName());
        }
        break;

        case KSTARS_WEATHER:
        {
            if (weatherCombo->findText(dv->getName()) == -1)
                weatherCombo->addItem(dv->getName());
        }
        break;

        case KSTARS_AUXILIARY:
        {
            if (aux1Combo->findText(dv->getName()) == -1)
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
#endif

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

    mount   =  NULL;
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

    delete(mount_di);
    mount_di   = NULL;
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
    dustCapProcess = NULL;

    ekosStartingStatus  = STATUS_IDLE;
    indiConnectionStatus= STATUS_IDLE;

    guiderCCDName  = "";
    primaryCCDName = "";

    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);    
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

    profileGroup->setEnabled(true);

    return true;
}

bool EkosManager::start()
{
    managedDevices.clear();
    reset();

    ProfileInfo *currentProfile = getCurrentProfile();

    localMode = currentProfile->isLocal();

    // Load profile location if one exists
    updateProfileLocation(currentProfile);

    if (localMode)
    {
        DriverInfo *drv = NULL;

        drv = driversList.value(currentProfile->mount());
        if (drv != NULL)
            mount_di   = drv->clone();

        drv = driversList.value(currentProfile->ccd());
        if (drv != NULL)
            ccd_di     = drv->clone();

        drv = driversList.value(currentProfile->guider());
        if (drv != NULL)
            guider_di  = drv->clone();

        drv = driversList.value(currentProfile->ao());
        if (drv != NULL)
            ao_di      = drv->clone();

        drv = driversList.value(currentProfile->filter());
        if (drv != NULL)
            filter_di  = drv->clone();

        drv = driversList.value(currentProfile->focuser());
        if (drv != NULL)
            focuser_di = drv->clone();

        drv = driversList.value(currentProfile->dome());
        if (drv != NULL)
            dome_di    = drv->clone();

        drv = driversList.value(currentProfile->weather());
        if (drv != NULL)
            weather_di    = drv->clone();

        drv = driversList.value(currentProfile->aux1());
        if (drv != NULL)
            aux1_di    = drv->clone();

        drv = driversList.value(currentProfile->aux2());
        if (drv != NULL)
            aux2_di    = drv->clone();

        drv = driversList.value(currentProfile->aux3());
        if (drv != NULL)
            aux3_di    = drv->clone();

        drv = driversList.value(currentProfile->aux4());
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
            if (ccd_di && guider_di->getDriver() == ccd_di->getDriver())
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

        if (mount_di != NULL)
            managedDevices.append(mount_di);
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

        //saveLocalDrivers();
    }
    else
    {
        delete (remote_indi);
        bool haveCCD=false, haveGuider=false;

        remote_indi = new DriverInfo(QString("Ekos Remote Host"));

        remote_indi->setHostParameters(currentProfile->host, QString::number(currentProfile->port));

        remote_indi->setDriverSource(GENERATED_SOURCE);

        if (currentProfile->drivers.contains("Mount"))
            nDevices++;
        if (currentProfile->drivers.contains("CCD"))
        {
            haveCCD = true;
            nDevices++;
        }
        if (currentProfile->drivers.contains("Guider"))
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

        if (currentProfile->drivers.contains("AO"))
            nDevices++;
        if (currentProfile->drivers.contains("Focuser"))
            nDevices++;
        if (currentProfile->drivers.contains("Filter"))
            nDevices++;
        if (currentProfile->drivers.contains("Dome"))
            nDevices++;
        if (currentProfile->drivers.contains("Weather"))
            nDevices++;
        if (currentProfile->drivers.contains("Aux1"))
            nDevices++;
        if (currentProfile->drivers.contains("Aux2"))
            nDevices++;
        if (currentProfile->drivers.contains("Aux3"))
            nDevices++;
        if (currentProfile->drivers.contains("Aux4"))
            nDevices++;

        nRemoteDevices=0;

        //saveRemoteDrivers();
    }

    connect(INDIListener::Instance(), SIGNAL(newDevice(ISD::GDInterface*)), this, SLOT(processNewDevice(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newTelescope(ISD::GDInterface*)), this, SLOT(setTelescope(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newCCD(ISD::GDInterface*)), this, SLOT(setCCD(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newFilter(ISD::GDInterface*)), this, SLOT(setFilter(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newFocuser(ISD::GDInterface*)), this, SLOT(setFocuser(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newDome(ISD::GDInterface*)), this, SLOT(setDome(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newWeather(ISD::GDInterface*)), this, SLOT(setWeather(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newDustCap(ISD::GDInterface*)), this, SLOT(setDustCap(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newLightBox(ISD::GDInterface*)), this, SLOT(setLightBox(ISD::GDInterface*)));
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

        appendLogText(i18n("INDI services started on port %1. Please connect devices.", ( (ccd_di != NULL) ? ccd_di->getPort() : guider_di->getPort())));

        if (Options::verboseLogging())
            qDebug() << "INDI services started on port " << ( (ccd_di != NULL) ? ccd_di->getPort() : guider_di->getPort());

        QTimer::singleShot(MAX_LOCAL_INDI_TIMEOUT, this, SLOT(checkINDITimeout()));

    }
    else
    {
        appendLogText(i18n("Connecting to remote INDI server at %1 on port %2 ...", currentProfile->host, currentProfile->port));
        qApp->processEvents();

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

    profileGroup->setEnabled(false);

    processINDIB->setText(i18n("Stop INDI"));

    return true;


}

/*
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
            if (ccd_di && guider_di->getDriver() == ccd_di->getDriver())
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
    connect(INDIListener::Instance(), SIGNAL(newDustCap(ISD::GDInterface*)), this, SLOT(setDustCap(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newLightBox(ISD::GDInterface*)), this, SLOT(setLightBox(ISD::GDInterface*)));
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

        appendLogText(i18n("INDI services started on port %1. Please connect devices.", ( (ccd_di != NULL) ? ccd_di->getPort() : guider_di->getPort())));

        if (Options::verboseLogging())
            qDebug() << "INDI services started on port " << ( (ccd_di != NULL) ? ccd_di->getPort() : guider_di->getPort());

        QTimer::singleShot(MAX_LOCAL_INDI_TIMEOUT, this, SLOT(checkINDITimeout()));

    }
    else
    {
        appendLogText(i18n("Connecting to remote INDI server at %1 on port %2 ...", Options::remoteHost(), Options::remotePort()));
        qApp->processEvents();

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
*/

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
        if (mount_di && mount == NULL)
            remainingDevices << QString("+ %1").arg(mount_di->getName());
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
        ProfileInfo *currentProfile = getCurrentProfile();

        QStringList remainingDevices;
        if (currentProfile->drivers.contains("Mount") && mount == NULL)
            remainingDevices << QString("+ %1").arg(currentProfile->mount());
        if (currentProfile->drivers.contains("CCD") && ccd == NULL)
            remainingDevices << QString("+ %1").arg(currentProfile->ccd());
        if (currentProfile->drivers.contains("Guider") && guider == NULL)
            remainingDevices << QString("+ %1").arg(currentProfile->guider());
        if (currentProfile->drivers.contains("Focuser") && focuser == NULL)
            remainingDevices << QString("+ %1").arg(currentProfile->focuser());
        if (currentProfile->drivers.contains("Filter") && filter == NULL)
            remainingDevices << QString("+ %1").arg(currentProfile->filter());
        if (currentProfile->drivers.contains("Dome") && dome == NULL)
            remainingDevices << QString("+ %1").arg(currentProfile->dome());
        if (currentProfile->drivers.contains("Weather") && weather == NULL)
            remainingDevices << QString("+ %1").arg(currentProfile->weather());
        if (currentProfile->drivers.contains("AO") && ao == NULL)
            remainingDevices << QString("+ %1").arg(currentProfile->ao());
        if (currentProfile->drivers.contains("Aux1") && aux1 == NULL)
            remainingDevices << QString("+ %1").arg(currentProfile->aux1());
        if (currentProfile->drivers.contains("Aux2") && aux2 == NULL)
            remainingDevices << QString("+ %1").arg(currentProfile->aux2());
        if (currentProfile->drivers.contains("Aux3") && aux3 == NULL)
            remainingDevices << QString("+ %1").arg(currentProfile->aux3());
        if (currentProfile->drivers.contains("Aux4") && aux4 == NULL)
            remainingDevices << QString("+ %1").arg(currentProfile->aux4());

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

/*void EkosManager::refreshRemoteDrivers()
{
    if (localMode == false)
        initRemoteDrivers();
}*/

void EkosManager::connectDevices()
{

    indiConnectionStatus = STATUS_PENDING;

    if (mount)
    {
         mount->Connect();

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

    if (mount)
    {
         mount->Disconnect();

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
    profileGroup->setEnabled(true);

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
                mount = devInterface;
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
    }

}

void EkosManager::processRemoteDevice(ISD::GDInterface *devInterface)
{
    ProfileInfo *pi = getCurrentProfile();

    QString deviceName(devInterface->getDeviceName());

    if (deviceName == pi->mount())
        mount = devInterface;
    else if (remoteCCDRegistered == false && (deviceName == pi->ccd() || deviceName.startsWith(pi->ccd())))
    {
        ccd = devInterface;
        primaryCCDName = QString(devInterface->getDeviceName());
        remoteCCDRegistered = true;

        // In case CCD = Guider, we decrement the device number.
        if (QString(devInterface->getDeviceName()) == pi->guider())
            nDevices--;
    }
    else if (remoteGuideRegistered == false &&
            ( (pi->guider().isEmpty() == false && deviceName.startsWith(pi->guider()) ) ||
              (deviceName.startsWith(pi->ccd()))))
    {
        guider = devInterface;
        guiderCCDName = QString(devInterface->getDeviceName());
        remoteGuideRegistered = true;
     }
    else if (deviceName == pi->ao())
        ao = devInterface;
    else if (deviceName == pi->focuser())
        focuser = devInterface;
    else if (deviceName == pi->filter())
        filter = devInterface;
    else if (deviceName == pi->dome())
        dome = devInterface;
    else if (deviceName == pi->weather())
        weather = devInterface;
    else if (deviceName == pi->aux1())
        aux1 = devInterface;
    else if (deviceName == pi->aux2())
        aux2 = devInterface;
    else if (deviceName == pi->aux3())
        aux3 = devInterface;
    else if (deviceName == pi->aux4())
        aux4 = devInterface;
    else
        return;

    nDevices--;
    nRemoteDevices++;

    if (Options::verboseLogging())
        qDebug() << "Received new remote device " << deviceName << " nDevices: " << nDevices << " nRemoteDevices: " << nRemoteDevices;

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
    }
}

void EkosManager::deviceConnected()
{
    connectB->setEnabled(false);
    disconnectB->setEnabled(true);

    processINDIB->setEnabled(false);

    nConnectedDevices++;

    if (Options::verboseLogging())
    {
        ISD::GDInterface *device = (ISD::GDInterface *) sender();
        qDebug() << device->getDeviceName() << " is connected.";
        qDebug() << "Managed Devices: " << managedDevices.count() << " Remote Devices: " << nRemoteDevices;
        qDebug() << "Connected Devices: " << nConnectedDevices << " nDevices: " << nDevices;
    }

    if (nConnectedDevices == managedDevices.count() || (nDevices <=0 && nConnectedDevices == nRemoteDevices))
        indiConnectionStatus = STATUS_SUCCESS;

    if (Options::neverLoadConfig())
        return;

    INDIConfig tConfig = Options::loadConfigOnConnection() ? LOAD_LAST_CONFIG : LOAD_DEFAULT_CONFIG;

    ISwitchVectorProperty *configProp = NULL;

    if (mount && mount->isConnected())
    {
        configProp = mount->getBaseDevice()->getSwitch("CONFIG_PROCESS");
        if (configProp && configProp->s == IPS_IDLE)
           mount->setConfig(tConfig);
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
        else if (dev->getState("CONNECTION") == IPS_BUSY)
            indiConnectionStatus = STATUS_PENDING;
        else
            indiConnectionStatus = STATUS_IDLE;
    }
    else
        indiConnectionStatus = STATUS_IDLE;

    if (Options::verboseLogging())
    {
        qDebug() << dev->getDeviceName() << " is disconnected.";
        qDebug() << "Connected Devices: " << nConnectedDevices << " nDevices: " << nDevices;
    }

    if (indiConnectionStatus == STATUS_IDLE)
        nConnectedDevices--;

    if (nConnectedDevices < 0)
        nConnectedDevices = 0;

    connectB->setEnabled(true);
    disconnectB->setEnabled(false);

    processINDIB->setEnabled(true);

}

void EkosManager::setTelescope(ISD::GDInterface *scopeDevice)
{
    mount = scopeDevice;

    appendLogText(i18n("%1 is online.", mount->getDeviceName()));

    scopeRegistered = true;

    connect(scopeDevice, SIGNAL(numberUpdated(INumberVectorProperty *)), this, SLOT(processNewNumber(INumberVectorProperty*)));

    initMount();

    mountProcess->setTelescope(mount);

    if (guideProcess)
        guideProcess->setTelescope(mount);

    if (alignProcess)
        alignProcess->setTelescope(mount);
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
            guideProcess->setTelescope(mount);
    }

    appendLogText(i18n("%1 is online.", ccdDevice->getDeviceName()));

    connect(ccdDevice, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processNewNumber(INumberVectorProperty*)), Qt::UniqueConnection);

   if (scopeRegistered)
   {
      alignProcess->setTelescope(mount);
      captureProcess->setTelescope(mount);
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

void EkosManager::setDustCap(ISD::GDInterface *dustCapDevice)
{
    initDustCap();

    dustCap = dustCapDevice;

    dustCapProcess->setDustCap(dustCap);

    appendLogText(i18n("%1 is online.", dustCap->getDeviceName()));

    if (captureProcess)
        captureProcess->setDustCap(dustCap);
}

void EkosManager::setLightBox(ISD::GDInterface *lightBoxDevice)
{
    lightBox = lightBoxDevice;

    if (captureProcess)
        captureProcess->setLightBox(lightBoxDevice);
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
           guideProcess->setTelescope(mount);
           guideProcess->syncTelescopeInfo();
        }

        if (alignProcess)
        {
            alignProcess->setTelescope(mount);
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
           guideProcess->setTelescope(mount);
           guideProcess->syncTelescopeInfo();
        }

        if (mountProcess)
        {
           mountProcess->setTelescope(mount);
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
            captureProcess->setTelescope(mount);

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
     //toolsWidget->addTab( captureProcess, i18n("CCD"));
     int index = toolsWidget->addTab( captureProcess, QIcon::fromTheme("kstars_ekos_ccd"), "");
     toolsWidget->tabBar()->setTabToolTip(index, i18nc("Charge-Coupled Device", "CCD"));
     connect(captureProcess, SIGNAL(newLog()), this, SLOT(updateLog()));

     if (dustCap)
         captureProcess->setDustCap(dustCap);
     if (lightBox)
         captureProcess->setLightBox(lightBox);

     if (focusProcess)
     {
         // Autofocus
         connect(captureProcess, SIGNAL(checkFocus(double)), focusProcess, SLOT(checkFocus(double)), Qt::UniqueConnection);
         connect(focusProcess, SIGNAL(autoFocusFinished(bool, double)), captureProcess, SLOT(updateAutofocusStatus(bool, double)),  Qt::UniqueConnection);
         connect(focusProcess, SIGNAL(statusUpdated(bool)), captureProcess, SLOT(updateFocusStatus(bool)), Qt::UniqueConnection);

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
     //toolsWidget->addTab( alignProcess, i18n("Alignment"));
     int index = toolsWidget->addTab( alignProcess, QIcon::fromTheme("kstars_ekos_align"), "");
     toolsWidget->tabBar()->setTabToolTip(index, i18n("Align"));
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
         connect(focusProcess, SIGNAL(statusUpdated(bool)), alignProcess, SLOT(updateFocusStatus(bool)), Qt::UniqueConnection);
     }
}


void EkosManager::initFocus()
{
    if (focusProcess)
        return;

    focusProcess = new Ekos::Focus();
    //toolsWidget->addTab( focusProcess, i18n("Focus"));
    int index = toolsWidget->addTab( focusProcess, QIcon::fromTheme("kstars_ekos_focus"), "");
    toolsWidget->tabBar()->setTabToolTip(index, i18n("Focus"));
    connect(focusProcess, SIGNAL(newLog()), this, SLOT(updateLog()));

    if (captureProcess)
    {
        // Autofocus
        connect(captureProcess, SIGNAL(checkFocus(double)), focusProcess, SLOT(checkFocus(double)), Qt::UniqueConnection);        
        connect(focusProcess, SIGNAL(autoFocusFinished(bool, double)), captureProcess, SLOT(updateAutofocusStatus(bool, double)), Qt::UniqueConnection);
        connect(focusProcess, SIGNAL(statusUpdated(bool)), captureProcess, SLOT(updateFocusStatus(bool)), Qt::UniqueConnection);

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
        connect(focusProcess, SIGNAL(statusUpdated(bool)), alignProcess, SLOT(updateFocusStatus(bool)), Qt::UniqueConnection);
    }

}

void EkosManager::initMount()
{
    if (mountProcess)
        return;

    mountProcess = new Ekos::Mount();
    //toolsWidget->addTab(mountProcess, i18n("Mount"));
    int index = toolsWidget->addTab(mountProcess, QIcon::fromTheme("kstars_ekos_mount"), "");
    toolsWidget->tabBar()->setTabToolTip(index, i18n("Mount"));
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
        if (mount && mount->isConnected())
            guideProcess->setTelescope(mount);

        //toolsWidget->addTab( guideProcess, i18n("Guide"));
        int index = toolsWidget->addTab( guideProcess, QIcon::fromTheme("kstars_ekos_guide"), "");
        toolsWidget->tabBar()->setTabToolTip(index, i18n("Guide"));
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
        connect(captureProcess, SIGNAL(mountParking()), guideProcess, SLOT(stopGuiding()));

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

void EkosManager::initDustCap()
{
    if (dustCapProcess)
        return;

    dustCapProcess = new Ekos::DustCap();
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

        dustCap = NULL;
        delete (dustCapProcess);
        dustCapProcess = NULL;

        lightBox = NULL;

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

#if 0
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
#endif

void EkosManager::addObjectToScheduler(SkyObject *object)
{
    if (schedulerProcess)
        schedulerProcess->addObject(object);
}

bool EkosManager::setProfile(const QString &profileName)
{
    int index = profileCombo->findText(profileName);

    if (index < 0)
        return false;

    profileCombo->setCurrentIndex(index);

    return true;
}


void EkosManager::addProfile()
{
    ProfileEditor editor(this);

    if (editor.exec() == QDialog::Accepted)
    {
        qDeleteAll(profiles);
        profiles.clear();
        loadProfiles();
        profileCombo->setCurrentIndex(profileCombo->count()-1);
    }
}

void EkosManager::editProfile()
{
    ProfileEditor editor(this);

    ProfileInfo *currentProfile = getCurrentProfile();

    Q_ASSERT(currentProfile);

    QString name = currentProfile->name;

    editor.setPi(currentProfile);

    if (editor.exec() == QDialog::Accepted)
    {
        qDeleteAll(profiles);
        profiles.clear();
        loadProfiles();
        profileCombo->setCurrentIndex(profileCombo->findText(name));
    }

}

void EkosManager::deleteProfile()
{
    ProfileInfo *currentProfile = getCurrentProfile();

    if (currentProfile->name == "Simulators")
        return;

    if (KMessageBox::questionYesNo(this, i18n("Are you sure you want to delete the profile?"), i18n("Confirm Delete")) == KMessageBox::No)
        return;

    KStarsData::Instance()->userdb()->DeleteProfile(currentProfile);

    qDeleteAll(profiles);
    profiles.clear();
    loadProfiles();
}

ProfileInfo * EkosManager::getCurrentProfile()
{
    ProfileInfo *currentProfile = NULL;

    // Get current profile
    foreach(ProfileInfo *pi, profiles)
    {
        if (profileCombo->currentText() == pi->name)
        {
            currentProfile = pi;
            break;
        }

    }

    return currentProfile;
}

void EkosManager::saveDefaultProfile(const QString &name)
{
    Options::setProfile(name);
}

void EkosManager::updateProfileLocation(ProfileInfo *pi)
{
    if (pi->city.isEmpty() == false)
    {
        bool cityFound = KStars::Instance()->setGeoLocation(pi->city, pi->province, pi->country);
        if (cityFound)
            appendLogText(i18n("Site location updated to %1.", KStarsData::Instance()->geo()->fullName()));
        else
            appendLogText(i18n("Failed to update site location to %1. City not found.", KStarsData::Instance()->geo()->fullName()));
    }
}
