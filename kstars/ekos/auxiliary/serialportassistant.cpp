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
#include <QNetworkReply>
#include <QButtonGroup>
#include <QRegularExpression>
#include <basedevice.h>

#include "ksnotification.h"
#include "indi/indiwebmanager.h"
#include "serialportassistant.h"
#include "indi/clientmanager.h"
#include "indi/driverinfo.h"
#include "ekos_debug.h"
#include "kspaths.h"
#include "Options.h"

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
        gotoDevicePage(devices[devices.indexOf(m_CurrentDevice)+1]);
    });

    loadRules();

    connect(rulesView->selectionModel(), &QItemSelectionModel::selectionChanged, [&](const QItemSelection &selected) {
        clearRuleB->setEnabled(selected.count() > 0);
    });
    connect(model.get(), &QStandardItemModel::rowsRemoved, [&]() { clearRuleB->setEnabled(model->rowCount() > 0); });
    connect(clearRuleB, &QPushButton::clicked, this, &SerialPortAssistant::removeActiveRule);

    displayOnStartupC->setChecked(Options::autoLoadSerialAssistant());
    connect(displayOnStartupC, &QCheckBox::toggled, [&](bool enabled) {
        Options::setAutoLoadSerialAssistant(enabled);
    });

    connect(closeB, &QPushButton::clicked, [&]() {
        gotoDevicePage(nullptr);
        close();
    });
}

void SerialPortAssistant::addDevice(ISD::GDInterface *device)
{
    qCDebug(KSTARS_EKOS) << "Serial Port Assistant new device" << device->getDeviceName();

    addDevicePage(device);
}

void SerialPortAssistant::addDevicePage(ISD::GDInterface *device)
{
    devices.append(device);

    QWidget *devicePage = new QWidget(this);
    devicePage->setObjectName(device->getDeviceName());

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
    startButton->setObjectName("startButton");

    QPushButton *homeButton = new QPushButton(QIcon::fromTheme("go-home"), i18n("Home"), devicePage);
    connect(homeButton, &QPushButton::clicked, [&]() {
        gotoDevicePage(nullptr);
    });

    QPushButton *skipButton = new QPushButton(i18n("Skip Device"), devicePage);
    connect(skipButton, &QPushButton::clicked, [&]() {
        gotoDevicePage(devices[devices.indexOf(m_CurrentDevice)+1]);
    });
    QCheckBox *hardwareSlotC = new QCheckBox(i18n("Physical Port Mapping"), devicePage);
    hardwareSlotC->setObjectName("hardwareSlot");
    hardwareSlotC->setToolTip(i18n("Assign the permanent name based on which physical port the device is plugged to in StellarMate. "
                                   "This is useful to distinguish between two identical USB adapters. The device must <b>always</b> be "
                                   "plugged into the same port for this to work."));
    actionsLayout->addItem(new QSpacerItem(10,10, QSizePolicy::Preferred));
    actionsLayout->addWidget(startButton);
    actionsLayout->addWidget(skipButton);
    actionsLayout->addWidget(hardwareSlotC);
    actionsLayout->addItem(new QSpacerItem(10,10, QSizePolicy::Preferred));
    actionsLayout->addWidget(homeButton);
    layout->addLayout(actionsLayout);

    QHBoxLayout *animationLayout = new QHBoxLayout(devicePage);
    QLabel *smAnimation = new QLabel(devicePage);
    smAnimation->setFixedSize(QSize(360,203));
    QMovie *smGIF = new QMovie(":/videos/sm_animation.gif");
    smAnimation->setMovie(smGIF);
    smAnimation->setObjectName("animation");

    animationLayout->addItem(new QSpacerItem(10,10, QSizePolicy::Preferred));
    animationLayout->addWidget(smAnimation);
    animationLayout->addItem(new QSpacerItem(10,10, QSizePolicy::Preferred));

    QButtonGroup *actionGroup = new QButtonGroup(devicePage);
    actionGroup->setObjectName("actionGroup");
    actionGroup->setExclusive(false);
    actionGroup->addButton(startButton);
    actionGroup->addButton(skipButton);
    actionGroup->addButton(hardwareSlotC);
    actionGroup->addButton(homeButton);

    layout->addLayout(animationLayout);
    //smGIF->start();
    //smAnimation->hide();

    serialPortWizard->insertWidget(serialPortWizard->count()-1, devicePage);

    connect(startButton, &QPushButton::clicked, [=]() {
        startButton->setText(i18n("Standby, Scanning..."));
        for (auto b : actionGroup->buttons())
            b->setEnabled(false);
        smGIF->start();
        scanDevices();
    });
}

void SerialPortAssistant::gotoDevicePage(ISD::GDInterface *device)
{
    int index = devices.indexOf(device);

    // reset to home page
    if (index < 0)
    {
        m_CurrentDevice = nullptr;
        serialPortWizard->setCurrentIndex(0);
        return;
    }

    m_CurrentDevice = device;
    serialPortWizard->setCurrentIndex(index+1);
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

        model->setHeaderData(0, Qt::Horizontal, i18nc("Vendor ID", "VID"));
        model->setHeaderData(1, Qt::Horizontal, i18nc("Product ID", "PID"));
        model->setHeaderData(2, Qt::Horizontal, i18n("Link"));
        model->setHeaderData(3, Qt::Horizontal, i18n("Serial #"));
        model->setHeaderData(4, Qt::Horizontal, i18n("Hardware Port?"));


        // Get all the drivers running remotely
        for (auto value : array)
        {
            QJsonObject rule = value.toObject();
            QList<QStandardItem*> items;
            QStandardItem *vid = new QStandardItem(rule["vid"].toString());
            QStandardItem *pid = new QStandardItem(rule["pid"].toString());
            QStandardItem *link = new QStandardItem(rule["symlink"].toString());
            QStandardItem *serial = new QStandardItem(rule["serial"].toString());
            QStandardItem *hardware = new QStandardItem(rule["port"].toString());
            items << vid << pid << link << serial << hardware;
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

void SerialPortAssistant::resetCurrentPage()
{
    // Reset all buttons
    QButtonGroup *actionGroup = serialPortWizard->currentWidget()->findChild<QButtonGroup*>("actionGroup");
    for (auto b : actionGroup->buttons())
        b->setEnabled(true);

    // Set start button to start scanning
    QPushButton *startButton = serialPortWizard->currentWidget()->findChild<QPushButton*>("startButton");
    startButton->setText(i18n("Start Scanning"));

    // Clear animation
    QLabel *animation = serialPortWizard->currentWidget()->findChild<QLabel*>("animation");
    animation->movie()->stop();
    animation->clear();
}

void SerialPortAssistant::scanDevices()
{
    QUrl url(QString("http://%1:%2/api/udev/watch").arg(m_Profile->host).arg(m_Profile->INDIWebManagerPort));

    QNetworkReply *response = manager.get(QNetworkRequest(url));

    // We need to disconnect the device first
    m_CurrentDevice->Disconnect();

    connect(response, &QNetworkReply::finished, this, &SerialPortAssistant::parseDevices);
}

void SerialPortAssistant::parseDevices()
{
    QNetworkReply *response = qobject_cast<QNetworkReply*>(sender());
    response->deleteLater();
    if (response->error() != QNetworkReply::NoError)
    {
        qCCritical(KSTARS_EKOS) << response->errorString();
        KSNotification::error(i18n("Failed to scan devices."));
        resetCurrentPage();
        return;
    }

    QJsonDocument jsonDoc = QJsonDocument::fromJson(response->readAll());
    if (jsonDoc.isObject() == false)
    {
        KSNotification::error(i18n("Failed to detect any devices. Please make sure device is powered and connected to StellarMate via USB."));
        resetCurrentPage();
        return;
    }

    QJsonObject rule = jsonDoc.object();

    // Make sure we have valid vendor ID
    if (rule.contains("ID_VENDOR_ID") == false || rule["ID_VENDOR_ID"].toString().count() != 4)
    {
        KSNotification::error(i18n("Failed to detect any devices. Please make sure device is powered and connected to StellarMate via USB."));
        resetCurrentPage();
        return;
    }

    QString serial = "--";

    QRegularExpression re("^[0-9a-zA-Z-]+$");
    QRegularExpressionMatch match = re.match(rule["ID_SERIAL"].toString());
    if (match.hasMatch())
        serial = rule["ID_SERIAL"].toString();

    // Remove any spaces from the device name
    QString symlink = serialPortWizard->currentWidget()->objectName().toLower().remove(" ");

    QJsonObject newRule = {
        {"vid", rule["ID_VENDOR_ID"].toString() },
        {"pid", rule["ID_MODEL_ID"].toString() },
        {"serial", serial },
        {"symlink", symlink },
    };

    QCheckBox *hardwareSlot = serialPortWizard->currentWidget()->findChild<QCheckBox*>("hardwareSlot");
    if (hardwareSlot->isChecked())
    {
        QString devPath = rule["DEVPATH"].toString();
        int index = devPath.lastIndexOf("/");
        if (index > 0)
        {
            newRule.insert("port", devPath.mid(index+1));
        }
    }
    else if (model)
    {
        bool vidMatch = !(model->findItems(newRule["vid"].toString(), Qt::MatchExactly, 0).empty());
        bool pidMatch = !(model->findItems(newRule["pid"].toString(), Qt::MatchExactly, 1).empty());
        if (vidMatch && pidMatch)
        {
            KSNotification::error(i18n("Duplicate devices detected. You must remove one mapping or enable hardware slot mapping."));
            resetCurrentPage();
            return;
        }
    }


    addRule(newRule);
    // Remove current device page since it is no longer required.
    serialPortWizard->removeWidget(serialPortWizard->currentWidget());
    gotoDevicePage(nullptr);
}

bool SerialPortAssistant::addRule(const QJsonObject &rule)
{
    QUrl url(QString("http://%1:%2/api/udev/add_rule").arg(m_Profile->host).arg(m_Profile->INDIWebManagerPort));
    QByteArray data = QJsonDocument(rule).toJson(QJsonDocument::Compact);
    if (INDI::WebManager::getWebManagerResponse(QNetworkAccessManager::PostOperation, url, nullptr, &data))
    {
        KSNotification::info(i18n("Mapping is successful. Please unplug and replug your device again now."));
        ITextVectorProperty *devicePort = m_CurrentDevice->getBaseDevice()->getText("DEVICE_PORT");
        if (devicePort)
        {
            // Set port in device and then save config
            IUSaveText(&devicePort->tp[0], QString("/dev/%1").arg(rule["symlink"].toString()).toLatin1().constData());
            m_CurrentDevice->getDriverInfo()->getClientManager()->sendNewText(devicePort);
            m_CurrentDevice->setConfig(SAVE_CONFIG);
            m_CurrentDevice->Connect();
            //serialPortWizard->setCurrentIndex(serialPortWizard->currentIndex()+1);
        }

        loadRules();
        return true;
    }

    KSNotification::sorry(i18n("Failed to add a new rule."));
    resetCurrentPage();
    return false;
}
