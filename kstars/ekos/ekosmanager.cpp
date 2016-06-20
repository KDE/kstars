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
#include "indi/indiwebmanager.h"

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
    remoteManagerStart=false;

    indiConnectionStatus = STATUS_IDLE;
    ekosStartingStatus   = STATUS_IDLE;

    profileModel = NULL;

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
    nRemoteDevices=0;

    useGuideHead           = false;    
    useST4                 = false;    

    removeTabs();    

    genericDevices.clear();
    managedDevices.clear();

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

    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);    
    processINDIB->setEnabled(true);

    processINDIB->setText(i18n("Start INDI"));
}

void EkosManager::processINDI()
{
    if (ekosStartingStatus == STATUS_SUCCESS || ekosStartingStatus == STATUS_PENDING)
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
    if (localMode)
        qDeleteAll(managedDrivers);
    managedDrivers.clear();

    reset();

    ProfileInfo *currentProfile = getCurrentProfile();
    localMode = currentProfile->isLocal();

    // Load profile location if one exists
    updateProfileLocation(currentProfile);

    bool haveCCD=false, haveGuider=false;

    if (localMode)
    {
        DriverInfo *drv = NULL;        

        drv = driversList.value(currentProfile->mount());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->ccd());
        if (drv != NULL)
        {
            managedDrivers.append(drv->clone());
            haveCCD = true;
        }

        drv = driversList.value(currentProfile->guider());
        if (drv != NULL)
        {
            haveGuider = true;

                // If the guider and ccd are the same driver, we have two cases:
                // #1 Drivers that only support ONE device per driver (such as sbig)
                // #2 Drivers that supports multiples devices per driver (such as sx)
                // For #1, we modify guider_di to make a unique label for the other device with postfix "Guide"
                // For #2, we set guider_di to NULL and we prompt the user to select which device is primary ccd and which is guider
                // since this is the only way to find out in real time.
                if (haveCCD && currentProfile->guider() == currentProfile->ccd())
                {
                    if (drv->getAuxInfo().value("mdpd", false).toBool() == true)
                        drv = NULL;
                    else
                    {
                        drv->setUniqueLabel(drv->getTreeLabel() + " Guide");
                    }
                }

            if (drv)
                managedDrivers.append(drv->clone());
        }

        drv = driversList.value(currentProfile->ao());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->filter());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->focuser());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->dome());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->weather());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->aux1());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->aux2());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->aux3());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        drv = driversList.value(currentProfile->aux4());
        if (drv != NULL)
            managedDrivers.append(drv->clone());

        if (haveCCD == false && haveGuider == false)
        {
            KMessageBox::error(this, i18n("Ekos requires at least one CCD or Guider to operate."));
            managedDrivers.clear();
            return false;
        }

        nDevices = managedDrivers.count();
    }
    else
    {
        DriverInfo *remote_indi = new DriverInfo(QString("Ekos Remote Host"));

        remote_indi->setHostParameters(currentProfile->host, QString::number(currentProfile->port));

        remote_indi->setDriverSource(GENERATED_SOURCE);

        managedDrivers.append(remote_indi);

        haveCCD    = currentProfile->drivers.contains("CCD");
        haveGuider = currentProfile->drivers.contains("Guider");

        if (haveCCD == false && haveGuider == false)
        {
            KMessageBox::error(this, i18n("Ekos requires at least one CCD or Guider to operate."));
            delete (remote_indi);
            nDevices=0;
            return false;
        }

        nDevices = currentProfile->drivers.count();

        nRemoteDevices=0;
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

        if (DriverManager::Instance()->startDevices(managedDrivers) == false)
        {
            INDIListener::Instance()->disconnect(this);
            qDeleteAll(managedDrivers);
            managedDrivers.clear();
            ekosStartingStatus = STATUS_ERROR;            
            return false;
        }

        connect(DriverManager::Instance(), SIGNAL(serverTerminated(QString,QString)), this, SLOT(cleanDevices()));

        ekosStartingStatus = STATUS_PENDING;

        appendLogText(i18n("INDI services started on port %1. Please connect devices.", managedDrivers.first()->getPort()));


        QTimer::singleShot(MAX_LOCAL_INDI_TIMEOUT, this, SLOT(checkINDITimeout()));

    }
    else
    {
        // If we need to use INDI Web Manager
        if (currentProfile->INDIWebManagerPort != -1)
        {
            remoteManagerStart = false;
            if (INDI::WebManager::isOnline(currentProfile))
            {
                if (INDI::WebManager::areDriversRunning(currentProfile) == false)
                {
                    INDI::WebManager::stopProfile(currentProfile);

                    if (INDI::WebManager::startProfile(currentProfile) == false)
                    {
                        appendLogText(i18n("Failed to start profile on remote INDI Web Manager."));
                        return false;
                    }

                    appendLogText(i18n("Starting profile on remote INDI Web Manager..."));
                    remoteManagerStart = true;
                }
            }
            else
                appendLogText(i18n("Warning: INDI Web Manager is not online."));
        }

        appendLogText(i18n("Connecting to remote INDI server at %1 on port %2 ...", currentProfile->host, currentProfile->port));
        qApp->processEvents();

        QApplication::setOverrideCursor(Qt::WaitCursor);

        if (DriverManager::Instance()->connectRemoteHost(managedDrivers.first()) == false)
        {
            appendLogText(i18n("Failed to connect to remote INDI server!"));
            INDIListener::Instance()->disconnect(this);
            qDeleteAll(managedDrivers);
            managedDrivers.clear();
            ekosStartingStatus = STATUS_ERROR;
            QApplication::restoreOverrideCursor();
            return false;
        }

        QApplication::restoreOverrideCursor();
        ekosStartingStatus = STATUS_PENDING;

        appendLogText(i18n("INDI services started. Connection to remote INDI server is successful. Waiting for devices..."));

        QTimer::singleShot(MAX_REMOTE_INDI_TIMEOUT, this, SLOT(checkINDITimeout()));

    }

    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);

    profileGroup->setEnabled(false);

    processINDIB->setText(i18n("Stop INDI"));

    return true;
}

void EkosManager::checkINDITimeout()
{
    // Don't check anything unless we're still pending
    if (ekosStartingStatus != STATUS_PENDING)
        return;

    if (nDevices <= 0)
    {
        ekosStartingStatus = STATUS_SUCCESS;
        return;
    }

    if (localMode)
    {
        QStringList remainingDevices;
        foreach(DriverInfo *drv, managedDrivers)
        {
            if (drv->getDevices().count() == 0)
                remainingDevices << QString("+ %1").arg(drv->getUniqueLabel().isEmpty() == false ? drv->getUniqueLabel() : drv->getName());
        }

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

        foreach(QString driver, currentProfile->drivers.values())
        {
            bool driverFound=false;

            foreach(ISD::GDInterface *device, genericDevices)
            {
                if (device->getBaseDevice()->getDriverName() == driver)
                {
                    driverFound = true;
                    break;
                }
            }

            if (driverFound == false)
                remainingDevices << QString("+ %1").arg(driver);
        }

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

void EkosManager::connectDevices()
{

    indiConnectionStatus = STATUS_PENDING;

    foreach(ISD::GDInterface *device, genericDevices)
        device->Connect();

    connectB->setEnabled(false);
    disconnectB->setEnabled(true);

    appendLogText(i18n("Connecting INDI devices..."));    
}

void EkosManager::disconnectDevices()
{
    foreach(ISD::GDInterface *device, genericDevices)
        device->Disconnect();

    appendLogText(i18n("Disconnecting INDI devices..."));    

}

void EkosManager::cleanDevices()
{

    INDIListener::Instance()->disconnect(this);
    DriverManager::Instance()->disconnect(this);

    if (managedDrivers.isEmpty() == false)
    {
        if (localMode)
        {
            DriverManager::Instance()->stopDevices(managedDrivers);
        }
        else
        {
            DriverManager::Instance()->disconnectRemoteHost(managedDrivers.first());

            ProfileInfo *pi = getCurrentProfile();
            if (remoteManagerStart && pi->INDIWebManagerPort != -1)
            {
                INDI::WebManager::stopProfile(pi);
                remoteManagerStart = false;
            }
        }
    }


    reset();

    processINDIB->setText(i18n("Start INDI"));
    processINDIB->setEnabled(true);
    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);
    profileGroup->setEnabled(true);

    appendLogText(i18n("INDI services stopped."));    
}

void EkosManager::processNewDevice(ISD::GDInterface *devInterface)
{
    if (Options::verboseLogging())
        qDebug() << "Ekos received a new device: " << devInterface->getDeviceName();

    genericDevices.append(devInterface);

     nDevices--;

    connect(devInterface, SIGNAL(Connected()), this, SLOT(deviceConnected()));
    connect(devInterface, SIGNAL(Disconnected()), this, SLOT(deviceDisconnected()));
    connect(devInterface, SIGNAL(propertyDefined(INDI::Property*)), this, SLOT(processNewProperty(INDI::Property*)));

    if (nDevices <= 0)
    {
        ekosStartingStatus = STATUS_SUCCESS;

        if (localMode == false && nDevices == 0)
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
        qDebug() << "Managed Devices: " << managedDrivers.count() << " Remote Devices: " << nRemoteDevices;
        qDebug() << "Connected Devices: " << nConnectedDevices << " nDevices: " << nDevices;
    }

    ProfileInfo *pi = getCurrentProfile();
    //if (nConnectedDevices == managedDrivers.count() || (nDevices <=0 && nConnectedDevices == nRemoteDevices))
    if (nConnectedDevices >= pi->drivers.count())
        indiConnectionStatus = STATUS_SUCCESS;    

    ISD::GDInterface *dev = static_cast<ISD::GDInterface *> (sender());

    if (dev->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE)
    {
        if (mountProcess)
            mountProcess->setEnabled(true);
    }
    else if (dev->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::CCD_INTERFACE)
    {
        if (captureProcess)
            captureProcess->setEnabled(true);
        if (focusProcess)
            focusProcess->setEnabled(true);
        if (alignProcess)
            alignProcess->setEnabled(true);
        if (guideProcess)
            guideProcess->setEnabled(true);
    }
    else if (dev->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::FOCUSER_INTERFACE)
    {
        if (focusProcess)
            focusProcess->setEnabled(true);
    }

    if (Options::neverLoadConfig())
        return;

    INDIConfig tConfig = Options::loadConfigOnConnection() ? LOAD_LAST_CONFIG : LOAD_DEFAULT_CONFIG;

    foreach(ISD::GDInterface *device, genericDevices)
    {
        if (device == dev)
        {
            ISwitchVectorProperty *configProp = device->getBaseDevice()->getSwitch("CONFIG_PROCESS");
            if (configProp && configProp->s == IPS_IDLE)
               device->setConfig(tConfig);
            break;
        }
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

    if (dev->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE)
    {
        if (mountProcess)
            mountProcess->setEnabled(false);
    }
    else if (dev->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::CCD_INTERFACE)
    {
        if (captureProcess)
            captureProcess->setEnabled(false);
        if (focusProcess)
            focusProcess->setEnabled(false);
        if (alignProcess)
            alignProcess->setEnabled(false);
        if (guideProcess)
            guideProcess->setEnabled(false);
    }
    else if (dev->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::FOCUSER_INTERFACE)
    {
        if (focusProcess)
            focusProcess->setEnabled(false);
    }
}

void EkosManager::setTelescope(ISD::GDInterface *scopeDevice)
{
    //mount = scopeDevice;

    managedDevices[KSTARS_TELESCOPE] = scopeDevice;

    appendLogText(i18n("%1 is online.", scopeDevice->getDeviceName()));

    connect(scopeDevice, SIGNAL(numberUpdated(INumberVectorProperty *)), this, SLOT(processNewNumber(INumberVectorProperty*)));

    initMount();

    mountProcess->setTelescope(scopeDevice);

    if (guideProcess)
        guideProcess->setTelescope(scopeDevice);

    if (alignProcess)
        alignProcess->setTelescope(scopeDevice);
}

void EkosManager::setCCD(ISD::GDInterface *ccdDevice)
{    
    managedDevices.insertMulti(KSTARS_CCD, ccdDevice);

    initCapture();

    captureProcess->addCCD(ccdDevice);

    ProfileInfo *pi = getCurrentProfile();
    QString primaryCCD, guiderCCD;

    // Only look for primary & guider CCDs if we can tell a difference between them
    // otherwise rely on saved options
    if (pi->ccd() != pi->guider())
    {
        foreach(ISD::GDInterface *device, findDevices(KSTARS_CCD))
        {
            if (QString(device->getDeviceName()).startsWith(pi->ccd(), Qt::CaseInsensitive))
                primaryCCD = QString(device->getDeviceName());
            else if (QString(device->getDeviceName()).startsWith(pi->guider(), Qt::CaseInsensitive))
                guiderCCD = QString(device->getDeviceName());
        }
    }

    bool rc=false;
    if (Options::defaultCaptureCCD().isEmpty() == false)
        rc = captureProcess->setCCD(Options::defaultCaptureCCD());
    if (rc == false && primaryCCD.isEmpty() == false)
        captureProcess->setCCD(primaryCCD);   

    initFocus();

    focusProcess->addCCD(ccdDevice);

    rc=false;
    if (Options::defaultFocusCCD().isEmpty() == false)
        rc = focusProcess->setCCD(Options::defaultFocusCCD());
    if (rc == false && primaryCCD.isEmpty() == false)
        focusProcess->setCCD(primaryCCD);


    initAlign();

    alignProcess->addCCD(ccdDevice);

    rc=false;
    if (Options::defaultAlignCCD().isEmpty() == false)
        rc = alignProcess->setCCD(Options::defaultAlignCCD());
    if (rc == false && primaryCCD.isEmpty() == false)
        alignProcess->setCCD(primaryCCD);    

    initGuide();

    guideProcess->addCCD(ccdDevice);

    rc=false;
    if (Options::defaultGuideCCD().isEmpty() == false)
        rc = guideProcess->setCCD(Options::defaultGuideCCD());
    if (rc == false && guiderCCD.isEmpty() == false)
            guideProcess->setCCD(guiderCCD);

    appendLogText(i18n("%1 is online.", ccdDevice->getDeviceName()));

    connect(ccdDevice, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(processNewNumber(INumberVectorProperty*)), Qt::UniqueConnection);

   if (managedDevices.contains(KSTARS_TELESCOPE))
   {
      alignProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
      captureProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
      guideProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
   }

}

void EkosManager::setFilter(ISD::GDInterface *filterDevice)
{

   managedDevices.insertMulti(KSTARS_FILTER, filterDevice);

   appendLogText(i18n("%1 filter is online.", filterDevice->getDeviceName()));

    initCapture();

    connect(filterDevice, SIGNAL(numberUpdated(INumberVectorProperty *)), this, SLOT(processNewNumber(INumberVectorProperty*)));
    connect(filterDevice, SIGNAL(textUpdated(ITextVectorProperty*)), this, SLOT(processNewText(ITextVectorProperty*)));

    captureProcess->addFilter(filterDevice);

    initFocus();

    focusProcess->addFilter(filterDevice);
}

void EkosManager::setFocuser(ISD::GDInterface *focuserDevice)
{
    managedDevices.insertMulti(KSTARS_FILTER, focuserDevice);

    initCapture();

    initFocus();

    focusProcess->addFocuser(focuserDevice);

    appendLogText(i18n("%1 is online.", focuserDevice->getDeviceName()));
}

void EkosManager::setDome(ISD::GDInterface *domeDevice)
{
    managedDevices[KSTARS_DOME] = domeDevice;

    initDome();    

    domeProcess->setDome(domeDevice);

    appendLogText(i18n("%1 is online.", domeDevice->getDeviceName()));
}

void EkosManager::setWeather(ISD::GDInterface *weatherDevice)
{
    managedDevices[KSTARS_WEATHER] = weatherDevice;

    initWeather();

    weatherProcess->setWeather(weatherDevice);

    appendLogText(i18n("%1 is online.", weatherDevice->getDeviceName()));
}

void EkosManager::setDustCap(ISD::GDInterface *dustCapDevice)
{
    managedDevices.insertMulti(KSTARS_AUXILIARY, dustCapDevice);

    initDustCap();

    dustCapProcess->setDustCap(dustCapDevice);

    appendLogText(i18n("%1 is online.", dustCapDevice->getDeviceName()));

    if (captureProcess)
        captureProcess->setDustCap(dustCapDevice);
}

void EkosManager::setLightBox(ISD::GDInterface *lightBoxDevice)
{
    managedDevices.insertMulti(KSTARS_AUXILIARY, lightBoxDevice);

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

    foreach(ISD::GDInterface *device, managedDevices.values())
    {
        if (device == devInterface)
        {
            managedDevices.remove(managedDevices.key(device));

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
    if (!strcmp(nvp->name, "TELESCOPE_INFO") && managedDevices.contains(KSTARS_TELESCOPE))
    {
        if (guideProcess)
        {
           guideProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
           guideProcess->syncTelescopeInfo();
        }

        if (alignProcess)
        {
            alignProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
            alignProcess->syncTelescopeInfo();
        }

        return;

    }

    if (!strcmp(nvp->name, "CCD_INFO") || !strcmp(nvp->name, "GUIDER_INFO")  || !strcmp(nvp->name, "CCD_FRAME") || !strcmp(nvp->name, "GUIDER_FRAME"))
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

    if (!strcmp(prop->getName(), "TELESCOPE_INFO") && managedDevices.contains(KSTARS_TELESCOPE))
    {
        if (guideProcess)
        {
           guideProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
           guideProcess->syncTelescopeInfo();
        }

        if (mountProcess)
        {
           mountProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);
           mountProcess->syncTelescopeInfo();
        }

        return;
    }

    if (!strcmp(prop->getName(), "GUIDER_EXPOSURE"))
    {       
        foreach(ISD::GDInterface *device, findDevices(KSTARS_CCD))
        {
            if (!strcmp(device->getDeviceName(), prop->getDeviceName()))
            {
                    initCapture();
                    initGuide();
                    useGuideHead=true;
                    captureProcess->addGuideHead(device);
                    guideProcess->addGuideHead(device);

                    bool rc = false;
                    if (Options::defaultGuideCCD().isEmpty() == false)
                        rc = guideProcess->setCCD(Options::defaultGuideCCD());
                    if (rc == false)
                            guideProcess->setCCD(QString(device->getDeviceName()) + QString(" Guider"));
                    return;
            }
        }

        return;
    }

    if (!strcmp(prop->getName(), "CCD_FRAME_TYPE"))
    {
        if (captureProcess)
        {
            foreach(ISD::GDInterface *device, findDevices(KSTARS_CCD))
            {
                if (!strcmp(device->getDeviceName(), prop->getDeviceName()))
                {
                    captureProcess->syncFrameType(device);
                    return;
                }
            }
        }

        return;
    }

    if (!strcmp(prop->getName(), "TELESCOPE_PARK") && managedDevices.contains(KSTARS_TELESCOPE))
    {
        if (captureProcess)
            captureProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);

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

QList<ISD::GDInterface*> EkosManager::findDevices(DeviceFamily type)
{
    QList<ISD::GDInterface*> deviceList;

    QMapIterator<DeviceFamily, ISD::GDInterface*> i(managedDevices);
    while (i.hasNext())
    {
        i.next();

        if (i.key() == type)
            deviceList.append(i.value());
    }

    return deviceList;
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
        if (alignProcess->isEnabled() == false && captureProcess->isEnabled())
        {
            if (managedDevices[KSTARS_CCD]->isConnected() && alignProcess->isParserOK())
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

    if (Options::verboseLogging())
        qDebug() << "Ekos: " << text;

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


     foreach(ISD::GDInterface *device, findDevices(KSTARS_AUXILIARY))
     {
         if (device->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::DUSTCAP_INTERFACE)
             captureProcess->setDustCap(device);
         if (device->getBaseDevice()->getDriverInterface() & INDI::BaseDevice::LIGHTBOX_INTERFACE)
             captureProcess->setLightBox(device);
     }

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

    //if ( (haveGuider || ccdCount > 1 || useGuideHead) && useST4 && toolsWidget->indexOf(guideProcess) == -1)
    if ( (findDevices(KSTARS_CCD).isEmpty() == false || useGuideHead) && useST4 && toolsWidget->indexOf(guideProcess) == -1)
    {

        //if (mount && mount->isConnected())
        if (managedDevices.contains(KSTARS_TELESCOPE) && managedDevices[KSTARS_TELESCOPE]->isConnected())
            guideProcess->setTelescope(managedDevices[KSTARS_TELESCOPE]);

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
        connect(guideProcess, SIGNAL(autoGuidingToggled(bool)), captureProcess, SLOT(setAutoguiding(bool)));
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

     if (Options::defaultST4Driver().isEmpty() == false)
         guideProcess->setST4(Options::defaultST4Driver());

     //if (ao && ao->getDeviceName() == st4Driver->getDeviceName())
     if (managedDevices.contains(KSTARS_ADAPTIVE_OPTICS) && (st4Driver->getDeviceName() == managedDevices[KSTARS_ADAPTIVE_OPTICS]->getDeviceName()))
         guideProcess->setAO(st4Driver);

}

void EkosManager::removeTabs()
{
    disconnect(toolsWidget, SIGNAL(currentChanged(int)), this, SLOT(processTabChange()));

        for (int i=2; i < toolsWidget->count(); i++)
                toolsWidget->removeTab(i);

        delete (alignProcess);
        alignProcess = NULL;

        //ccd = NULL;
        delete (captureProcess);
        captureProcess = NULL;


        //guider = NULL;
        delete (guideProcess);
        guideProcess = NULL;

        delete (mountProcess);
        mountProcess = NULL;

        //ao = NULL;

        //focuser = NULL;
        delete (focusProcess);
        focusProcess = NULL;

        //dome = NULL;
        delete (domeProcess);
        domeProcess = NULL;

        //weather = NULL;
        delete (weatherProcess);
        weatherProcess = NULL;

        //dustCap = NULL;
        delete (dustCapProcess);
        dustCapProcess = NULL;

        //lightBox = NULL;

        //aux1 = NULL;
        //aux2 = NULL;
        //aux3 = NULL;
        //aux4 = NULL;

        managedDevices.clear();

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

QStringList EkosManager::getProfiles()
{
    QStringList profiles;

    for (int i=0; i < profileCombo->count(); i++)
        profiles << profileCombo->itemText(i);

    return profiles;
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

    editor.setPi(currentProfile);

    if (editor.exec() == QDialog::Accepted)
    {
        int currentIndex = profileCombo->currentIndex();
        qDeleteAll(profiles);
        profiles.clear();
        loadProfiles();
        profileCombo->setCurrentIndex(currentIndex);
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
