/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "profilewizard.h"

#include <QDesktopServices>
#include <QUrl>
#include <QTcpSocket>
#include <QTimer>

#include "kstars.h"
#include "auxiliary/kspaths.h"
#include "ksnotification.h"
#include "qMDNS.h"

ProfileWizard::ProfileWizard() : QDialog(KStars::Instance())
{
    setupUi(this);

#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    QPixmap im;
    if (im.load(KSPaths::locate(QStandardPaths::AppDataLocation, "wzekos.png")))
        wizardPix->setPixmap(im);

    remoteEquipmentSVG->load(QString(":/icons/pi.svg"));

    connect(buttonBox, SIGNAL(rejected()), this, SLOT(close()));
    connect(buttonBox, &QDialogButtonBox::helpRequested, this, []()
    {
        KStars::Instance()->appHelpActivated();
    });
    connect(buttonBox, &QDialogButtonBox::clicked, this, [this](QAbstractButton * button)
    {
        if (button == buttonBox->button(QDialogButtonBox::Reset))
            reset();
    });

    connect(discoverEkosB, &QPushButton::clicked, this,
            []()
    {
        QDesktopServices::openUrl(QUrl("https://www.indilib.org/about/ekos.html"));
    });
    connect(videoTutorialsB, &QPushButton::clicked, this,
            []()
    {
        QDesktopServices::openUrl(QUrl("https://www.youtube.com/user/QAstronomy"));
    });
    connect(INDIInfoB, &QPushButton::clicked, this,
            []()
    {
        QDesktopServices::openUrl(QUrl("https://indilib.org/about/discover-indi.html"));
    });

    // Intro actions
    connect(introNextB, &QPushButton::clicked, this,
            [this]()
    {
        wizardContainer->setCurrentIndex(EQUIPMENT_LOCATION);
    });

    // Equipment Location actions
    connect(localEquipmentB, SIGNAL(clicked()), this, SLOT(processLocalEquipment()));
    connect(remoteEquipmentB, &QPushButton::clicked, this,
            [this]()
    {
        wizardContainer->setCurrentIndex(REMOTE_EQUIPMENT_SELECTION);
    });
    equipmentStellarmateB->setIcon(QIcon(":/icons/stellarmate.svg"));
    connect(equipmentStellarmateB, &QPushButton::clicked, this, &ProfileWizard::processRemoteEquipmentSelection);

    equipmentAtikbaseB->setIcon(QIcon(":/icons/atikbase.svg"));
    connect(equipmentAtikbaseB, &QPushButton::clicked, this, &ProfileWizard::processRemoteEquipmentSelection);

    connect(equipmentOtherB, &QPushButton::clicked, this, &ProfileWizard::processRemoteEquipmentSelection);

    // Remote Equipment Action
    connect(remoteEquipmentNextB, SIGNAL(clicked()), this, SLOT(processRemoteEquipment()));

    // StellarMate Equipment Action
    connect(stellarMateEquipmentNextB, SIGNAL(clicked()), this, SLOT(processPiEquipment()));
#ifdef Q_OS_WIN
    // Auto Detect does not work on Windows yet for some reason.
    // Packet is sent correctly, but no replies are received from anything on the LAN outside of PC.
    PiAutoDetectB->setEnabled(false);
#else
    connect(PiAutoDetectB, SIGNAL(clicked()), this, SLOT(detectStellarMate()));
#endif

    // Local Mac
    connect(useExternalINDIB, SIGNAL(clicked()), this, SLOT(processLocalMac()));
    connect(useInternalINDIB, SIGNAL(clicked()), this, SLOT(processLocalMac()));

    // Create Profile
    connect(createProfileB, SIGNAL(clicked()), this, SLOT(createProfile()));
}

void ProfileWizard::reset()
{
    useInternalServer   = true;
    useWebManager       = false;
    useJoystick         = false;
    useRemoteAstrometry = false;
    useSkySafari        = false;
    useWatchDog         = false;
    useGuiderType       = INTERNAL_GUIDER;

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
    useInternalServer = true;
    useJoystickCheck->setEnabled(true);
    useRemoteAstrometryCheck->setEnabled(false);
    useWatchDogCheck->setEnabled(false);
    useSkySafariCheck->setEnabled(true);
    wizardContainer->setCurrentIndex(CREATE_PROFILE);
#endif
}

void ProfileWizard::processRemoteEquipment()
{
    bool portOK = false;
    remotePortEdit->text().toInt(&portOK);

    if (portOK == false)
    {
        KSNotification::error(i18n("Invalid port."));
        return;
    }

    if (remoteHostEdit->text().isEmpty())
    {
        KSNotification::error(i18n("Host name cannot be empty."));
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
    useSkySafariCheck->setEnabled(true);

    wizardContainer->setCurrentIndex(CREATE_PROFILE);
}

void ProfileWizard::processPiEquipment()
{
    if (PiHost->text().isEmpty())
    {
        KSNotification::error(i18n("Host name cannot be empty."));
        return;
    }

    host = PiHost->text();
    port = PiPort->text();

    useInternalServer = false;
    useWebManager = true;

    useJoystickCheck->setEnabled(true);
    useRemoteAstrometryCheck->setEnabled(true);
    useWatchDogCheck->setEnabled(true);
    useSkySafariCheck->setEnabled(true);

    wizardContainer->setCurrentIndex(CREATE_PROFILE);
}

void ProfileWizard::processLocalMac()
{
    QPushButton *button = qobject_cast<QPushButton *>(sender());

    if (button == nullptr)
        return;

    host = "localhost";
    port = "7624";

    useInternalServer = (button == useInternalINDIB);

    useJoystickCheck->setEnabled(false);
    useRemoteAstrometryCheck->setEnabled(false);
    useWatchDogCheck->setEnabled(false);
    useSkySafariCheck->setEnabled(true);

    wizardContainer->setCurrentIndex(CREATE_PROFILE);
}

void ProfileWizard::createProfile()
{
    if (profileNameEdit->text().isEmpty())
    {
        KSNotification::error(i18n("Profile name cannot be empty."));
        return;
    }

    useJoystick         = useJoystickCheck->isEnabled() && useJoystickCheck->isChecked();
    useWatchDog         = useWatchDogCheck->isEnabled() && useWatchDogCheck->isChecked();
    useSkySafari        = useSkySafariCheck->isEnabled() && useSkySafariCheck->isChecked();
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
    if (useSkySafari)
        auxList << "SkySafari";
    if (useRemoteAstrometry)
        auxList << "Astrometry";

    return auxList;
}

int ProfileWizard::selectedExternalGuider()
{
    return useGuiderType;
}

void ProfileWizard::detectStellarMate()
{
    stellarMateDetectDialog = new QProgressDialog(this);
    stellarMateDetectDialog->setMinimum(0);
    stellarMateDetectDialog->setMaximum(0);
    stellarMateDetectDialog->setWindowTitle(i18nc("@title:window", "Detecting StellarMate..."));
    stellarMateDetectDialog->setLabelText(i18n("Please wait while searching for StellarMate..."));

    stellarMateDetectDialog->show();

    connect(stellarMateDetectDialog.data(), &QProgressDialog::canceled, [&]()
    {
        qMDNS::getInstance()->disconnect();
    });
    connect(qMDNS::getInstance(), SIGNAL(hostFound(QHostInfo)), this, SLOT(processHostInfo(QHostInfo)));
    QTimer::singleShot(120 * 1000, this, SLOT(detectStellarMateTimeout()));

    qMDNS::getInstance()->lookup("stellarmate");
}

void ProfileWizard::processHostInfo(QHostInfo info)
{
    PiHost->setText(info.hostName());
    qMDNS::getInstance()->disconnect();
    stellarMateDetectDialog->close();
}

void ProfileWizard::detectStellarMateTimeout()
{
    if (stellarMateDetectDialog->isHidden() == false)
    {
        KSNotification::error(i18n("Failed to detect any StellarMate gadget. Make sure it is powered and on the same network."));
        stellarMateDetectDialog->close();
    }
}

void ProfileWizard::processRemoteEquipmentSelection()
{
    QToolButton *button = qobject_cast<QToolButton*>(sender());
    if (!button)
        wizardContainer->setCurrentIndex(GENERIC_EQUIPMENT);
    else if (button == equipmentStellarmateB)
    {
        PiHost->setText("stellarmate.local");
        wizardContainer->setCurrentIndex(PI_EQUIPMENT);
    }
    else if (button == equipmentAtikbaseB)
    {
        PiHost->setText("atikbase.local");
        wizardContainer->setCurrentIndex(PI_EQUIPMENT);
    }
}
