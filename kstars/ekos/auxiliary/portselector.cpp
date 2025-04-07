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
Device::Device(const QSharedPointer<ISD::GenericDevice> &device, QGridLayout *grid,
               uint8_t row) :  m_Name(device->getDeviceName()), m_Device(device), m_Grid(grid), m_Row(row)
{
    ColorCode[IPS_IDLE] = Qt::gray;
    ColorCode[IPS_OK] = Qt::green;
    ColorCode[IPS_BUSY] = Qt::yellow;
    ColorCode[IPS_ALERT] = Qt::red;

    connect(m_Device.get(), &ISD::GenericDevice::propertyUpdated, this, &Device::updateProperty);

    if (initGUI())
        syncGUI();
}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
bool Device::initGUI()
{
    INDI::PropertySwitch connectionMode = m_Device->getBaseDevice().getSwitch("CONNECTION_MODE");
    if (!connectionMode.isValid())
        return false;

    m_LED = new KLed;
    m_LED->setLook(KLed::Sunken);
    m_LED->setState(KLed::On);
    m_LED->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Ignored);
    m_Grid->addWidget(m_LED, m_Row, 0, Qt::AlignVCenter);

    m_Label = new QLabel;
    m_Label->setText(m_Name);
    m_Label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Ignored);
    m_Grid->addWidget(m_Label, m_Row, 1, Qt::AlignLeft);

    // Serial Button
    m_SerialB = new QPushButton(i18n("Serial"));
    m_SerialB->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Ignored);
    m_SerialB->setToolTip(i18n("Select <b>Serial</b> if your device is connected via Serial to USB adapter."));
    m_Grid->addWidget(m_SerialB, m_Row, 2);
    connect(m_SerialB, &QPushButton::clicked, this, [this]()
    {
        INDI::PropertySwitch connectionMode = m_Device->getBaseDevice().getSwitch("CONNECTION_MODE");
        connectionMode.reset();
        connectionMode[0].setState(ISS_ON);
        m_Device->sendNewProperty(connectionMode);
    });

    // Network Button
    m_NetworkB = new QPushButton(i18n("Network"));
    m_NetworkB->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Ignored);
    m_NetworkB->setToolTip(i18n("Select <b>Network</b> if your device is connected via Ethernet or WiFi."));
    m_Grid->addWidget(m_NetworkB, m_Row, 3);
    connect(m_NetworkB, &QPushButton::clicked, this, [this]()
    {
        INDI::PropertySwitch connectionMode = m_Device->getBaseDevice().getSwitch("CONNECTION_MODE");
        connectionMode.reset();
        connectionMode[1].setState(ISS_ON);
        m_Device->sendNewProperty(connectionMode);
    });

    if (connectionMode.findWidgetByName("CONNECTION_SERIAL"))
    {
        if (m_Device->getBaseDevice().getProperty("SYSTEM_PORTS").isValid())
        {
            m_Ports = new QComboBox;
            m_Ports->setEditable(true);
            m_Ports->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Ignored);
            m_Ports->setToolTip(i18n("Select Serial port"));

            INDI::PropertySwitch systemPorts = m_Device->getBaseDevice().getSwitch("SYSTEM_PORTS");
            for (auto &oneSwitch : systemPorts)
                m_Ports->addItem(oneSwitch.getLabel());
            connect(m_Ports, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated), this,
                    [this](int index)
            {
                // Double check property still exists.
                INDI::PropertySwitch systemPorts = m_Device->getBaseDevice().getProperty("SYSTEM_PORTS");
                if (systemPorts.isValid())
                {
                    systemPorts.reset();
                    systemPorts[index].setState(ISS_ON);
                    m_Device->sendNewProperty(systemPorts);
                }
            });
            // When combo box text changes
            connect(m_Ports->lineEdit(), &QLineEdit::editingFinished, this, [this]()
            {
                INDI::PropertyText port = m_Device->getBaseDevice().getText("DEVICE_PORT");
                port[0].setText(m_Ports->lineEdit()->text().toLatin1().constData());
                m_Device->sendNewProperty(port);
            });
        }

        if (m_Device->getBaseDevice().getProperty("DEVICE_BAUD_RATE").isValid())
        {
            m_BaudRates = new QComboBox;
            m_BaudRates->addItems(BAUD_RATES);
            m_BaudRates->setToolTip(i18n("Select Baud rate"));
            connect(m_BaudRates, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated), this,
                    [this](int index)
            {
                INDI::PropertySwitch systemBauds = m_Device->getBaseDevice().getSwitch("DEVICE_BAUD_RATE");
                systemBauds.reset();
                systemBauds[index].setState(ISS_ON);
                m_Device->sendNewProperty(systemBauds);
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
            INDI::PropertyText server = m_Device->getBaseDevice().getText("DEVICE_ADDRESS");
            server[0].setText(m_HostName->text().toLatin1().constData());
            m_Device->sendNewProperty(server);
        });

        m_HostPort = new QLineEdit;
        m_HostPort->setPlaceholderText(i18n("Port"));
        connect(m_HostPort, &QLineEdit::editingFinished, this, [this]()
        {
            auto server = m_Device->getProperty("DEVICE_ADDRESS").getText();
            server->at(1)->setText(m_HostPort->text().toLatin1().constData());
            m_Device->sendNewProperty(server);
        });

        m_HostProtocolTCP = new QPushButton("TCP");
        connect(m_HostProtocolTCP, &QPushButton::clicked, this, [this]()
        {
            auto connectionType = m_Device->getProperty("CONNECTION_TYPE").getSwitch();
            connectionType->reset();
            connectionType->at(0)->setState(ISS_ON);
            m_Device->sendNewProperty(connectionType);
        });

        m_HostProtocolUDP = new QPushButton("UDP");
        connect(m_HostProtocolUDP, &QPushButton::clicked, this, [this]()
        {
            auto connectionType = m_Device->getProperty("CONNECTION_TYPE").getSwitch();
            connectionType->reset();
            connectionType->at(1)->setState(ISS_ON);
            m_Device->sendNewProperty(connectionType);
        });
    }
    else
        m_NetworkB->setEnabled(false);

    m_ConnectB = new QPushButton;
    m_ConnectB->setFixedSize(QSize(28, 28));
    m_ConnectB->setToolTip(i18n("Connect"));
    m_ConnectB->setIcon(QIcon::fromTheme("network-connect"));
    m_Grid->addWidget(m_ConnectB, m_Row, 8);
    connect(m_ConnectB, &QPushButton::clicked, m_Device.get(), &ISD::GenericDevice::Connect);

    m_DisconnectB = new QPushButton;
    m_DisconnectB->setFixedSize(QSize(28, 28));
    m_DisconnectB->setToolTip(i18n("Disconnect"));
    m_DisconnectB->setIcon(QIcon::fromTheme("network-disconnect"));
    m_Grid->addWidget(m_DisconnectB, m_Row, 9);
    connect(m_DisconnectB, &QPushButton::clicked, m_Device.get(), &ISD::GenericDevice::Disconnect);

    setConnectionMode(static_cast<ConnectionMode>(connectionMode.findOnSwitchIndex()));

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
void Device::syncGUI()
{
    // Check if initGUI failed before?
    if (m_LED == nullptr)
    {
        // Try to initiGUI again
        initGUI();

        // If it failed again, return.
        if (m_LED == nullptr)
            return;
    }

    m_LED->setColor(ColorCode.value(m_Device->getState("CONNECTION")));

    auto connectionMode = m_Device->getProperty("CONNECTION_MODE").getSwitch();

    if (connectionMode->findOnSwitchIndex() == CONNECTION_SERIAL)
    {
        m_ActiveConnectionMode = CONNECTION_SERIAL;
        m_SerialB->setStyleSheet(ACTIVE_STYLESHEET);
        m_NetworkB->setStyleSheet(QString());

        if (m_Ports)
        {
            auto port = m_Device->getProperty("DEVICE_PORT").getText();
            m_Ports->setEditText(port->at(0)->getText());
        }

        if (m_BaudRates)
        {
            auto systemBauds = m_Device->getProperty("DEVICE_BAUD_RATE").getSwitch();
            m_BaudRates->setCurrentIndex(systemBauds->findOnSwitchIndex());
        }
    }
    else
    {
        m_ActiveConnectionMode = CONNECTION_NETWORK;
        m_SerialB->setStyleSheet(QString());
        m_NetworkB->setStyleSheet(ACTIVE_STYLESHEET);

        if (m_Device->getProperty("DEVICE_ADDRESS").isValid())
        {
            auto server = m_Device->getProperty("DEVICE_ADDRESS").getText();
            m_HostName->setText(server->at(0)->getText());
            m_HostPort->setText(server->at(1)->getText());
        }

        if (m_Device->getProperty("CONNECTION_TYPE").isValid())
        {
            auto connectionType = m_Device->getBaseDevice().getSwitch("CONNECTION_TYPE").getSwitch();
            m_HostProtocolTCP->setStyleSheet(connectionType->findOnSwitchIndex() == 0 ? ACTIVE_STYLESHEET : QString());
            m_HostProtocolUDP->setStyleSheet(connectionType->findOnSwitchIndex() == 1 ? ACTIVE_STYLESHEET : QString());
        }
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
    m_ActiveConnectionMode = mode;

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
void Device::updateProperty(INDI::Property prop)
{
    if (prop.isNameMatch("CONNECTION_MODE"))
    {
        auto svp = prop.getSwitch();
        setConnectionMode(static_cast<ConnectionMode>(svp->findOnSwitchIndex()));
        syncGUI();
    }
    else if (prop.isNameMatch("CONNECTION_TYPE") || prop.isNameMatch("SYSTEM_PORTS") ||
             prop.isNameMatch("DEVICE_BAUD_RATE") || prop.isNameMatch("DEVICE_PORT"))
        syncGUI();
    else if (prop.isNameMatch("CONNECTION") && m_ConnectB)
    {
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
#ifdef Q_OS_MACOS
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
void Dialog::addDevice(const QSharedPointer<ISD::GenericDevice> &device)
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
///
//////////////////////////////////////////////////////////////////////////////////////////
void Dialog::removeDevice(const QString &name)
{
    m_Devices.erase(std::remove_if(m_Devices.begin(), m_Devices.end(), [name](const auto & oneDevice)
    {
        return oneDevice->name() == name;
    }), m_Devices.end());
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

    uint32_t systemPortCount = 0;
    uint32_t serialDevices = 0;
    uint32_t networkDevices = 0;

    for (const auto &oneDevice : m_Devices)
    {
        if (oneDevice->systemPortCount() > 0)
            systemPortCount = oneDevice->systemPortCount();

        if (oneDevice->activeConnectionMode() == CONNECTION_SERIAL)
            serialDevices++;
        else if (oneDevice->activeConnectionMode() == CONNECTION_NETWORK)
            networkDevices++;
    }

    // If we just have a single serial device with single port, no need to toggle port selector.
    // If have have one or more network devices, we must show the dialog for the first time.
    if (networkDevices == 0 && serialDevices <= 1 && systemPortCount <= 1)
        return false;

    // Otherwise, show dialog
    return true;
}
}
