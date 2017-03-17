/*  Ekos Profile Wizard

    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <QDesktopServices>
#include <QUrl>
#include <KHelpClient>
#include <QTcpSocket>

#include "profilewizard.h"
#include "kstars.h"
#include "auxiliary/kspaths.h"
#include "ksnotification.h"

ProfileWizard::ProfileWizard() : QDialog(KStars::Instance())
{
    setupUi(this);

#ifdef Q_OS_OSX
        setWindowFlags(Qt::Tool| Qt::WindowStaysOnTopHint);
#endif

    QPixmap im;
    if( im.load(KSPaths::locate(QStandardPaths::GenericDataLocation, "wzekos.png")) )
        wizardPix->setPixmap( im );

    if( im.load(KSPaths::locate(QStandardPaths::GenericDataLocation, "windi.png")) )
        windiPix->setPixmap( im );

    connect(buttonBox, SIGNAL(rejected()), this, SLOT(close()));
    connect(buttonBox, &QDialogButtonBox::helpRequested, this, [this]() { KHelpClient::invokeHelp(); });
    connect(buttonBox, &QDialogButtonBox::clicked, this, [this](QAbstractButton *button) {if (button == buttonBox->button(QDialogButtonBox::Reset)) reset();});

    connect(discoverEkosB, &QPushButton::clicked, this, [this]() { QDesktopServices::openUrl(QUrl("http://www.indilib.org/about/ekos.html")); });
    connect(videoTutorialsB, &QPushButton::clicked, this, [this]() { QDesktopServices::openUrl(QUrl("https://www.youtube.com/user/QAstronomy")); });
    connect(INDIInfoB, &QPushButton::clicked, this, [this]() { QDesktopServices::openUrl(QUrl("http://indilib.org/about/discover-indi.html")); });

    // Intro actions
    connect(introNextB, &QPushButton::clicked, this, [this]() { wizardContainer->setCurrentIndex(EQUIPMENT_LOCATION); });

    // Equipment Location actions
    connect(localEquipmentB, SIGNAL(clicked()), this, SLOT(processLocalEquipment()));
    connect(remoteEquipmentB, &QPushButton::clicked, this, [this]() { wizardContainer->setCurrentIndex(REMOTE_EQUIPMENT); });

    // Remote Equipment Action
    connect(remoteEquipmentNextB, SIGNAL(clicked()), this, SLOT(processRemoteEquipment()));

    // Local Windows
    connect(windowsReadyB, SIGNAL(clicked()), this, SLOT(processLocalWindows()));

    // Local Mac
    connect(useExternalINDIB, SIGNAL(clicked()), this, SLOT(processLocalMac()));
    connect(useInternalINDIB, SIGNAL(clicked()), this, SLOT(processLocalMac()));

    // Create Profile
    connect(createProfileB, SIGNAL(clicked()), this, SLOT(createProfile()));
}

void ProfileWizard::reset()
{
    useInternalServer=true;
    useWebManager=false;
    useJoystick=false;
    useRemoteAstrometry=false;
    useWatchDog=false;
    useGuiderType = INTERNAL_GUIDER;

    host.clear();
    port = "7624";

    wizardContainer->setCurrentIndex(INTRO);
}

void ProfileWizard::processLocalEquipment()
{
    #if defined(Q_OS_OSX)
        wizardContainer->setCurrentIndex(MAC_LOCAL);
    #elif defined(Q_OS_WIN)
        wizardContainer->setCurrentIndex(WINDOWS_LOCAL);
    #else
        useInternalServer=true;
        useJoystickCheck->setEnabled(true);
        useRemoteAstrometryCheck->setEnabled(false);
        useWatchDogCheck->setEnabled(false);
        wizardContainer->setCurrentIndex(CREATE_PROFILE);
    #endif
}

void ProfileWizard::processRemoteEquipment()
{
    bool portOK=false;
    remotePortEdit->text().toInt(&portOK);

    if (portOK == false)
    {
        KSNotification::error(i18n("Invalid port!"));
        return;
    }

    if (remoteHostEdit->text().isEmpty())
    {
        KSNotification::error(i18n("Host name cannot be empty!"));
        return;
    }

    useInternalServer = false;

    host = remoteHostEdit->text();
    port = remotePortEdit->text();

    if (webManagerNotSureB->isChecked())
    {
        QTcpSocket socket;
        // Should probably make 8624 configurable too?
        socket.connectToHost(host, 8624);
        useWebManager = socket.waitForConnected();
    }
    else
        useWebManager = webManagerYesR->isChecked();

    useJoystickCheck->setEnabled(true);
    useRemoteAstrometryCheck->setEnabled(true);
    useWatchDogCheck->setEnabled(true);

    wizardContainer->setCurrentIndex(CREATE_PROFILE);
}

void ProfileWizard::processLocalWindows()
{
    useInternalServer = false;
    host = "localhost";
    port = "7624";

    useJoystickCheck->setEnabled(false);
    useRemoteAstrometryCheck->setEnabled(false);
    useWatchDogCheck->setEnabled(false);

    wizardContainer->setCurrentIndex(CREATE_PROFILE);
}

void ProfileWizard::processLocalMac()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (button == NULL)
        return;

    host = "localhost";
    port = "7624";

    useInternalServer = (button == useInternalINDIB);

    useJoystickCheck->setEnabled(false);
    useRemoteAstrometryCheck->setEnabled(false);
    useWatchDogCheck->setEnabled(false);

    wizardContainer->setCurrentIndex(CREATE_PROFILE);
}

void ProfileWizard::createProfile()
{
    if (profileNameEdit->text().isEmpty())
    {
        KSNotification::error(i18n("Profile name cannot be empty!"));
        return;
    }

    useJoystick = useJoystickCheck->isEnabled() && useJoystickCheck->isChecked();
    useWatchDog = useWatchDogCheck->isEnabled() && useWatchDogCheck->isChecked();
    useRemoteAstrometry = useRemoteAstrometryCheck->isEnabled() && useRemoteAstrometryCheck->isChecked();
    if (useInternalGuiderR->isChecked())
        useGuiderType = INTERNAL_GUIDER;
    else if (usePHD2R->isChecked())
        useGuiderType = PHD2_GUIDER;
    else
        useGuiderType = LIN_GUIDER;

    profileName = profileNameEdit->text();

    if (useInternalServer)
    {
        host.clear();
        port.clear();
    }

    accept();
}

QStringList ProfileWizard::selectedAuxDrivers()
{
    QStringList auxList;

    if (useJoystick)
        auxList << "Joystick";
    if (useWatchDog)
        auxList << "WatchDog";
    if (useRemoteAstrometry)
        auxList << "Astrometry";

    return auxList;
}

QString ProfileWizard::selectedExternalGuider()
{
    if (useGuiderType == PHD2_GUIDER)
        return "PHD2";
    else if (useGuiderType == LIN_GUIDER)
        return "LinGuider";
    else
        return QString();
}
