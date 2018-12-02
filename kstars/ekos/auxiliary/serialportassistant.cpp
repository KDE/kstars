/*  Ekos Serial Port Assistant tool
    Copyright (C) 2019 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <QMovie>
#include <QCheckBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardItem>

#include "indi/indiwebmanager.h"
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

    loadRules();

    connect(rulesView->selectionModel(), &QItemSelectionModel::selectionChanged, [&](const QItemSelection &selected) {
        clearRuleB->setEnabled(selected.count() > 0);
    });
    connect(model.get(), &QStandardItemModel::rowsRemoved, [&]() { clearRuleB->setEnabled(model->rowCount() > 0); });
    connect(clearRuleB, &QPushButton::clicked, this, &SerialPortAssistant::removeActiveRule);
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

void SerialPortAssistant::gotoPage(ISD::GDInterface *device)
{
    int index = devices.indexOf(device);

    if (index < 0)
        return;

    currentDevice = device;

    serialPortWizard->setCurrentIndex( (1 + index) * 2);
}

bool SerialPortAssistant::loadRules()
{
    QUrl url(QString("http://%1:%2/api/udev/rules").arg(m_Profile->host).arg(m_Profile->INDIWebManagerPort));
    QJsonDocument json;

    if (INDI::WebManager::getWebManagerResponse(QNetworkAccessManager::GetOperation, url, &json))
    {
        QJsonArray array = json.array();

        if (array.isEmpty())
            return false;

        model.reset(new QStandardItemModel(0, 5, this));

        model->setHeaderData(0, Qt::Horizontal, i18nc("Product ID", "PID"));
        model->setHeaderData(1, Qt::Horizontal, i18nc("Vendor ID", "VID"));
        model->setHeaderData(2, Qt::Horizontal, i18n("Link"));
        model->setHeaderData(3, Qt::Horizontal, i18n("Serial #"));
        model->setHeaderData(4, Qt::Horizontal, i18n("Hardware Port?"));


        // Get all the drivers running remotely
        for (auto value : array)
        {
            QJsonObject rule = value.toObject();
            QList<QStandardItem*> items;
            QStandardItem *pid = new QStandardItem(rule["pid"].toString());
            QStandardItem *vid = new QStandardItem(rule["vid"].toString());
            QStandardItem *link = new QStandardItem(rule["symlink"].toString());
            QStandardItem *serial = new QStandardItem(rule["serial"].toString());
            QStandardItem *hardware = new QStandardItem(rule["port"].toString());
            items << pid << vid << link << serial << hardware;
            model->appendRow(items);
        }

        rulesView->setModel(model.get());
        return true;
    }

    return false;
}

bool SerialPortAssistant::removeActiveRule()
{
    QUrl url(QString("http://%1:%2/api/udev/remove_rule").arg(m_Profile->host).arg(m_Profile->INDIWebManagerPort));

    QModelIndex index = rulesView->currentIndex();
    if (index.isValid() == false)
        return false;

    QStandardItem *symlink = model->item(index.row(), 2);
    if (symlink == nullptr)
        return false;

    QJsonObject rule = { {"symlink", symlink->text()} };
    QByteArray data = QJsonDocument(rule).toJson(QJsonDocument::Compact);

    if (INDI::WebManager::getWebManagerResponse(QNetworkAccessManager::PostOperation, url, nullptr, &data))
    {
        model->removeRow(index.row());
        return true;
    }

    return false;
}

