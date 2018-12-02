/*  Ekos Serial Port Assistant tool
    Copyright (C) 2019 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <QMovie>
#include <QCheckBox>

#include "serialportassistant.h"
#include "ekos_debug.h"
#include "kspaths.h"

SerialPortAssistant::SerialPortAssistant(ProfileInfo *profile, QWidget *parent) : QDialog(parent),
    m_Profile(profile)
{
    setupUi(this);

    QPixmap im;
    if (im.load(KSPaths::locate(QStandardPaths::GenericDataLocation, "wzserialportassistant.png")))
        wizardPix->setPixmap(im);
    else if (im.load(QDir(QCoreApplication::applicationDirPath() + "/../Resources/data").absolutePath() +
                     "/wzserialportassistant.png"))
        wizardPix->setPixmap(im);

    connect(nextB, &QPushButton::clicked, [&]() {
        serialPortWizard->setCurrentIndex(serialPortWizard->currentIndex()+1);
    });
}

void SerialPortAssistant::addDevice(ISD::GDInterface *device)
{
    qCDebug(KSTARS_EKOS) << "Serial Port Assistant new device" << device->getDeviceName();

    addPage(device);
}

void SerialPortAssistant::addPage(ISD::GDInterface *device)
{
    QWidget *devicePage = new QWidget(this);

    devices.append(device);

    QVBoxLayout *layout = new QVBoxLayout(devicePage);

    QLabel *deviceLabel = new QLabel(devicePage);
    deviceLabel->setText(QString("<h1>%1</h1>").arg(device->getDeviceName()));
    layout->addWidget(deviceLabel);

    QLabel *instructionsLabel = new QLabel(devicePage);
    instructionsLabel->setText(i18n("To assign a permanent designation to the device, you need to unplug the device from stellarmate "
                                    "then replug it after 1 second. Click on the <b>Start Scan</b> to begin this procedure."));
    instructionsLabel->setWordWrap(true);
    layout->addWidget(instructionsLabel);

    QHBoxLayout *actionsLayout = new QHBoxLayout(devicePage);
    QPushButton *startButton = new QPushButton(i18n("Start Scan"), devicePage);
    QPushButton *skipButton = new QPushButton(i18n("Skip Device"), devicePage);
    QCheckBox *hardwareSlotC = new QCheckBox(i18n("Physical Port Mapping"), devicePage);
    hardwareSlotC->setToolTip(i18n("Assign the permanent name based on which physical port the device is plugged to in StellarMate. "
                                   "This is useful to distinguish between two identical USB adapters. The device must <b>always</b> be "
                                   "plugged into the same port for this to work."));
    actionsLayout->addItem(new QSpacerItem(10,10, QSizePolicy::Preferred));
    actionsLayout->addWidget(startButton);
    actionsLayout->addWidget(skipButton);
    actionsLayout->addWidget(hardwareSlotC);
    actionsLayout->addItem(new QSpacerItem(10,10, QSizePolicy::Preferred));
    layout->addLayout(actionsLayout);

    QHBoxLayout *animationLayout = new QHBoxLayout(devicePage);
    QLabel *smAnimation = new QLabel(devicePage);
    //smAnimation->setFixedSize(QSize(360,203));
    QMovie *smGIF = new QMovie(":/videos/sm_animation.gif");
    smAnimation->setMovie(smGIF);

    animationLayout->addItem(new QSpacerItem(10,10, QSizePolicy::Preferred));
    animationLayout->addWidget(smAnimation);
    animationLayout->addItem(new QSpacerItem(10,10, QSizePolicy::Preferred));

    layout->addLayout(animationLayout);
    smGIF->start();
    //smAnimation->hide();

    serialPortWizard->insertWidget(serialPortWizard->count()-1, devicePage);
}
