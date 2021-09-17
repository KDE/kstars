/*
    SPDX-FileCopyrightText: 2021 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "portselector.h"

#include "indipropertyswitch.h"
#include "ksnotification.h"
#include "ekos_debug.h"
#include "kspaths.h"
#include "indi/driverinfo.h"
#include "indi/clientmanager.h"
#include "Options.h"

#include <QDialogButtonBox>
#include <QPushButton>

namespace  Selector
{

const QStringList Device::BAUD_RATES = { "9600", "19200", "38400", "57600", "115200", "230400" };
const QString Device::ACTIVE_STYLESHEET = "background-color: #004400;";
//const QString Device::ACTIVE_STYLESHEET = "border-color: #007700; font-weight:bold;";
//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
Device::Device(ISD::GDInterface *device, QGridLayout *grid, uint8_t row) :  m_Device(device), m_Grid(grid), m_Row(row)
{
    ColorCode[IPS_IDLE] = Qt::gray;
    ColorCode[IPS_OK] = Qt::green;
    ColorCode[IPS_BUSY] = Qt::yellow;
    ColorCode[IPS_ALERT] = Qt::red;

    connect(m_Device, &ISD::GDInterface::switchUpdated, this, &Device::processSwitch);
    connect(m_Device, &ISD::GDInterface::textUpdated, this, &Device::processText);
    connect(m_Device, &ISD::GDInterface::Connected, this, &Device::setConnected);
    connect(m_Device, &ISD::GDInterface::Disconnected, this, &Device::setDisconnected);

    if (initGUI())
        syncGUI();
}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
bool Device::initGUI()
{
    INDI::Property modeProperty = m_Device->getBaseDevice()->getSwitch("CONNECTION_MODE");
    if (!modeProperty.isValid())
        return false;

    m_LED = new KLed;
    m_LED->setLook(KLed::Sunken);
    m_LED->setState(KLed::On);
    m_LED->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Ignored);
    m_Grid->addWidget(m_LED, m_Row, 0, Qt::AlignVCenter);

    m_Label = new QLabel;
    m_Label->setText(m_Device->getDeviceName());
    m_Label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Ignored);
    m_Grid->addWidget(m_Label, m_Row, 1, Qt::AlignLeft);

    // Serial Button
    m_SerialB = new QPushButton(i18n("Serial"));
    m_SerialB->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Ignored);
    m_SerialB->setToolTip(i18n("Select <b>Serial</b> if your device is connected via Serial to USB adapter."));
    m_Grid->addWidget(m_SerialB, m_Row, 2);
    connect(m_SerialB, &QPushButton::clicked, this, [this]()
    {
        INDI::PropertyView<ISwitch> connectionMode = *(m_Device->getBaseDevice()->getSwitch("CONNECTION_MODE"));
        IUResetSwitch(&connectionMode);
        connectionMode.at(0)->setState(ISS_ON);
        connectionMode.at(1)->setState(ISS_OFF);
        m_Device->getDriverInfo()->getClientManager()->sendNewSwitch(&connectionMode);
    });

    // Network Button
    m_NetworkB = new QPushButton(i18n("Network"));
    m_NetworkB->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Ignored);
    m_NetworkB->setToolTip(i18n("Select <b>Network</b> if your device is connected via Ethernet or WiFi."));
    m_Grid->addWidget(m_NetworkB, m_Row, 3);
    connect(m_NetworkB, &QPushButton::clicked, this, [this]()
    {
        INDI::PropertyView<ISwitch> connectionMode = *(m_Device->getBaseDevice()->getSwitch("CONNECTION_MODE"));
        IUResetSwitch(&connectionMode);
        connectionMode.at(0)->setState(ISS_OFF);
        connectionMode.at(1)->setState(ISS_ON);
        m_Device->getDriverInfo()->getClientManager()->sendNewSwitch(&connectionMode);
    });

    INDI::PropertyView<ISwitch> connectionMode = *(modeProperty->getSwitch());
    if (connectionMode.findWidgetByName("CONNECTION_SERIAL"))
    {
        if (m_Device->getBaseDevice()->getProperty("SYSTEM_PORTS").isValid())
        {
            m_Ports = new QComboBox;
            m_Ports->setEditable(true);
            m_Ports->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Ignored);
            m_Ports->setToolTip(i18n("Select Serial port"));

            INDI::PropertyView<ISwitch> systemPorts = *(m_Device->getBaseDevice()->getSwitch("SYSTEM_PORTS"));
            for (auto &oneSwitch : systemPorts)
                m_Ports->addItem(oneSwitch.getLabel());
            connect(m_Ports, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated), this,
                    [this](int index)
            {
                // Double check property still exists.
                INDI::Property systemPorts = m_Device->getBaseDevice()->getProperty("SYSTEM_PORTS");
                if (systemPorts.isValid())
                {
                    INDI::PropertyView<ISwitch> systemPortsSwitch = *(systemPorts->getSwitch());
                    IUResetSwitch(&systemPortsSwitch);
                    systemPortsSwitch.at(index)->setState(ISS_ON);
                    m_Device->getDriverInfo()->getClientManager()->sendNewSwitch(&systemPortsSwitch);
                }
            });
            // When combo box text changes
            connect(m_Ports->lineEdit(), &QLineEdit::editingFinished, this, [this]()
            {
                INDI::PropertyView<IText> port = *(m_Device->getBaseDevice()->getText("DEVICE_PORT"));
                port.at(0)->setText(m_Ports->lineEdit()->text().toLatin1().constData());
                m_Device->getDriverInfo()->getClientManager()->sendNewText(&port);
            });
        }

        if (m_Device->getBaseDevice()->getProperty("DEVICE_BAUD_RATE").isValid())
        {
            m_BaudRates = new QComboBox;
            m_BaudRates->addItems(BAUD_RATES);
            m_BaudRates->setToolTip(i18n("Select Baud rate"));
            connect(m_BaudRates, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated), this,
                    [this](int index)
            {
                INDI::PropertyView<ISwitch> systemBauds = *(m_Device->getBaseDevice()->getSwitch("DEVICE_BAUD_RATE"));
                IUResetSwitch(&systemBauds);
                systemBauds.at(index)->setState(ISS_ON);
                m_Device->getDriverInfo()->getClientManager()->sendNewSwitch(&systemBauds);
            });
        }
    }
    else
        m_SerialB->setEnabled(false);

    if (connectionMode.findWidgetByName("CONNECTION_TCP"))
    {
        m_HostName = new QLineEdit;
        m_HostName->setPlaceholderText(i18n("Host name or IP address."));
        m_HostName->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Ignored);
        connect(m_HostName, &QLineEdit::editingFinished, this, [this]()
        {
            INDI::PropertyView<IText> server = *(m_Device->getBaseDevice()->getText("DEVICE_ADDRESS"));
            server.at(0)->setText(m_HostName->text().toLatin1().constData());
            m_Device->getDriverInfo()->getClientManager()->sendNewText(&server);
        });

        m_HostPort = new QLineEdit;
        m_HostPort->setPlaceholderText(i18n("Port"));
        connect(m_HostPort, &QLineEdit::editingFinished, this, [this]()
        {
            INDI::PropertyView<IText> server = *(m_Device->getBaseDevice()->getText("DEVICE_ADDRESS"));
            server.at(1)->setText(m_HostPort->text().toLatin1().constData());
            m_Device->getDriverInfo()->getClientManager()->sendNewText(&server);
        });

        m_HostProtocolTCP = new QPushButton("TCP");
        connect(m_HostProtocolTCP, &QPushButton::clicked, this, [this]()
        {
            INDI::PropertyView<ISwitch> connectionType = *(m_Device->getBaseDevice()->getSwitch("CONNECTION_TYPE"));
            IUResetSwitch(&connectionType);
            connectionType.at(0)->setState(ISS_ON);
            m_Device->getDriverInfo()->getClientManager()->sendNewSwitch(&connectionType);
        });

        m_HostProtocolUDP = new QPushButton("UDP");
        connect(m_HostProtocolUDP, &QPushButton::clicked, this, [this]()
        {
            INDI::PropertyView<ISwitch> connectionType = *(m_Device->getBaseDevice()->getSwitch("CONNECTION_TYPE"));
            IUResetSwitch(&connectionType);
            connectionType.at(1)->setState(ISS_ON);
            m_Device->getDriverInfo()->getClientManager()->sendNewSwitch(&connectionType);
        });
    }
    else
        m_NetworkB->setEnabled(false);

    m_ConnectB = new QPushButton;
    m_ConnectB->setFixedSize(QSize(28, 28));
    m_ConnectB->setToolTip(i18n("Connect"));
    m_ConnectB->setIcon(QIcon::fromTheme("network-connect"));
    m_Grid->addWidget(m_ConnectB, m_Row, 8);
    connect(m_ConnectB, &QPushButton::clicked, m_Device, &ISD::GDInterface::Connect);

    m_DisconnectB = new QPushButton;
    m_DisconnectB->setFixedSize(QSize(28, 28));
    m_DisconnectB->setToolTip(i18n("Disconnect"));
    m_DisconnectB->setIcon(QIcon::fromTheme("network-disconnect"));
    m_Grid->addWidget(m_DisconnectB, m_Row, 9);
    connect(m_DisconnectB, &QPushButton::clicked, m_Device, &ISD::GDInterface::Disconnect);

    setConnectionMode(static_cast<ConnectionMode>(connectionMode.findOnSwitchIndex()));

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
void Device::syncGUI()
{
    m_LED->setColor(ColorCode.value(m_Device->getState("CONNECTION")));

    INDI::PropertyView<ISwitch> connectionMode = *(m_Device->getBaseDevice()->getSwitch("CONNECTION_MODE"));

    if (connectionMode.findOnSwitchIndex() == CONNECTION_SERIAL)
    {
        m_SerialB->setStyleSheet(ACTIVE_STYLESHEET);
        m_NetworkB->setStyleSheet(QString());

        if (m_Ports)
        {
            INDI::PropertyView<IText> port = *(m_Device->getBaseDevice()->getText("DEVICE_PORT"));
            m_Ports->setEditText(port.at(0)->getText());
        }

        if (m_BaudRates)
        {
            INDI::PropertyView<ISwitch> systemBauds = *(m_Device->getBaseDevice()->getSwitch("DEVICE_BAUD_RATE"));
            m_BaudRates->setCurrentIndex(systemBauds.findOnSwitchIndex());
        }
    }
    else
    {
        m_SerialB->setStyleSheet(QString());
        m_NetworkB->setStyleSheet(ACTIVE_STYLESHEET);

        if (m_Device->getBaseDevice()->getProperty("DEVICE_ADDRESS").isValid())
        {
            INDI::PropertyView<IText> server = *(m_Device->getBaseDevice()->getText("DEVICE_ADDRESS"));
            m_HostName->setText(server.at(0)->getText());
            m_HostPort->setText(server.at(1)->getText());
        }

        if (m_Device->getBaseDevice()->getProperty("CONNECTION_TYPE").isValid())
        {
            INDI::PropertyView<ISwitch> connectionType = *(m_Device->getBaseDevice()->getSwitch("CONNECTION_TYPE"));
            m_HostProtocolTCP->setStyleSheet(connectionType.findOnSwitchIndex() == 0 ? ACTIVE_STYLESHEET : QString());
            m_HostProtocolUDP->setStyleSheet(connectionType.findOnSwitchIndex() == 1 ? ACTIVE_STYLESHEET : QString());
        }
    }

    if (m_Device->isConnected())
    {
        m_ConnectB->setStyleSheet(ACTIVE_STYLESHEET);
        m_DisconnectB->setStyleSheet(QString());
    }
    else
    {
        m_ConnectB->setStyleSheet(QString());
        m_DisconnectB->setStyleSheet(ACTIVE_STYLESHEET);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
uint8_t Device::systemPortCount() const
{
    if (m_Ports)
        return m_Ports->count();
    else
        return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
void Device::setConnectionMode(ConnectionMode mode)
{
    if (mode == CONNECTION_SERIAL)
    {
        if (m_HostName)
        {
            m_Grid->removeWidget(m_HostName);
            m_HostName->setParent(nullptr);
            m_Grid->removeWidget(m_HostPort);
            m_HostPort->setParent(nullptr);
            m_Grid->removeWidget(m_HostProtocolTCP);
            m_HostProtocolTCP->setParent(nullptr);
            m_Grid->removeWidget(m_HostProtocolUDP);
            m_HostProtocolUDP->setParent(nullptr);
        }

        m_Grid->addWidget(m_Ports, m_Row, 4, 1, 2);
        m_Grid->addWidget(m_BaudRates, m_Row, 6, 1, 2);
        m_Grid->setColumnStretch(4, 2);
    }
    else if (mode == CONNECTION_NETWORK)
    {
        if (m_Ports)
        {
            m_Grid->removeWidget(m_Ports);
            m_Ports->setParent(nullptr);
            m_Grid->removeWidget(m_BaudRates);
            m_BaudRates->setParent(nullptr);
        }

        m_Grid->addWidget(m_HostName, m_Row, 4);
        m_Grid->addWidget(m_HostPort, m_Row, 5);
        m_Grid->addWidget(m_HostProtocolTCP, m_Row, 6);
        m_Grid->addWidget(m_HostProtocolUDP, m_Row, 7);
        m_Grid->setColumnStretch(4, 2);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
void Device::processSwitch(ISwitchVectorProperty *svp)
{
    QString prop = svp->name;
    if (prop == "CONNECTION_MODE")
    {
        setConnectionMode(static_cast<ConnectionMode>(IUFindOnSwitchIndex(svp)));
        syncGUI();
    }
    else if (prop == "CONNECTION_TYPE" || prop == "SYSTEM_PORTS" ||
             prop == "DEVICE_BAUD_RATE")
        syncGUI();
}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
void Device::processText(ITextVectorProperty *tvp)
{
    QString prop = tvp->name;
    if (prop == "DEVICE_PORT")
        syncGUI();
}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
void Device::setConnected()
{
    syncGUI();
}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
void Device::setDisconnected()
{
    syncGUI();
}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
Dialog::Dialog(QWidget *parent) : QDialog(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(this);

    QPushButton *connectAll = new QPushButton(i18n("Connect All"), this);
    buttonBox->addButton(connectAll, QDialogButtonBox::AcceptRole);
    buttonBox->addButton(QDialogButtonBox::Close);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &Dialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &Dialog::reject);

    m_Layout = new QGridLayout;

    mainLayout->addLayout(m_Layout);
    mainLayout->addStretch();
    mainLayout->addWidget(buttonBox);

    setWindowTitle(i18nc("@title:window", "Port Selector"));
}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
void Dialog::addDevice(ISD::GDInterface *device)
{
    auto pos = std::find_if(m_Devices.begin(), m_Devices.end(), [device](std::unique_ptr<Device> &oneDevice)
    {
        return oneDevice->name() == device->getDeviceName();
    });

    // New
    if (pos == m_Devices.end())
    {
        std::unique_ptr<Device> dev(new Device(device, m_Layout, m_Devices.size()));
        m_Devices.push_back(std::move(dev));
    }
    // Existing device
    else
    {
        (*pos)->syncGUI();
    }

}

//////////////////////////////////////////////////////////////////////////////////////////
/// Should Port Selector be shown automatically?
/// If we only have ONE device with ONE port, then no need to bother the user
/// with useless selection.
//////////////////////////////////////////////////////////////////////////////////////////
bool Dialog::shouldShow() const
{
    if (m_Devices.empty())
        return false;

    // If we only have one device with single port, then no need to show dialog
    if (m_Devices.size() == 1 && m_Devices[0]->systemPortCount() == 1)
        return false;

    // Otherwise, show dialog
    return true;
}
}
