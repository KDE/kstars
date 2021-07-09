/*  Ekos Port Selector
    Copyright (C) 2021 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "portselector.h"

#include "indipropertyswitch.h"
#include "ksnotification.h"
#include "ekos_debug.h"
#include "kspaths.h"
#include "indi/driverinfo.h"
#include "indi/clientmanager.h"
#include "Options.h"

#include <QPushButton>

namespace  Selector
{

QStringList Device::BAUD_RATES = { "9600", "19200", "38400", "57600", "115200", "230400" };
//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
Device::Device(ISD::GDInterface *device, QGridLayout *grid, uint8_t row) :  m_Device(device), m_Grid(grid), m_Row(row)
{
    ColorCode[IPS_IDLE] = Qt::gray;
    ColorCode[IPS_BUSY] = Qt::yellow;
    ColorCode[IPS_OK] = Qt::green;
    ColorCode[IPS_ALERT] = Qt::red;

    connect(m_Device, &ISD::GDInterface::numberUpdated, this, &Device::processNumber);
    connect(m_Device, &ISD::GDInterface::switchUpdated, this, &Device::processSwitch);
    connect(m_Device, &ISD::GDInterface::textUpdated, this, &Device::processText);
    connect(m_Device, &ISD::GDInterface::Connected, this, &Device::setConnected);
    connect(m_Device, &ISD::GDInterface::Disconnected, this, &Device::setDisconnected);

    initGUI();
    syncGUI();
}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
void Device::initGUI()
{
    INDI::Property modeProperty = m_Device->getBaseDevice()->getSwitch("CONNECTION_MODE");
    if (!modeProperty.isValid())
        return;

    const QString pressedStylesheet = "QPushButton:pressed {border: 1px solid #007700; font-weight:bold;}";

    m_LED = new KLed;
    m_Grid->addWidget(m_LED, m_Row, 0, Qt::AlignVCenter);

    m_Label = new QLabel;
    m_Label->setText(m_Device->getDeviceName());
    m_Grid->addWidget(m_Label, m_Row, 1, Qt::AlignLeft);

    // Serial Button
    m_SerialB = new QPushButton(i18n("Serial"));
    m_SerialB->setStyleSheet(pressedStylesheet);
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
    m_NetworkB->setStyleSheet(pressedStylesheet);
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
        m_Ports = new QComboBox;
        m_Ports->setEditable(true);
        m_Ports->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
        INDI::PropertyView<ISwitch> systemPorts = *(m_Device->getBaseDevice()->getSwitch("SYSTEM_PORTS"));
        for (auto &oneSwitch : systemPorts)
            m_Ports->addItem(oneSwitch.getLabel());

        m_BaudRates = new QComboBox;
        m_BaudRates->addItems(BAUD_RATES);
    }
    else
        m_SerialB->setEnabled(false);

    if (connectionMode.findWidgetByName("CONNECTION_TCP"))
    {
        m_HostName = new QLineEdit;
        m_HostName->setPlaceholderText(i18n("Host name or IP address."));

        m_HostPort = new QLineEdit;
        m_HostPort->setPlaceholderText(i18n("Port"));

        m_HostProtocolTCP = new QPushButton("TCP");
        m_HostProtocolTCP->setStyleSheet(pressedStylesheet);
        m_HostProtocolUDP = new QPushButton("UDP");
        m_HostProtocolUDP->setStyleSheet(pressedStylesheet);
    }
    else
        m_NetworkB->setEnabled(false);

    m_ConnectB = new QPushButton;
    m_ConnectB->setFixedSize(QSize(28, 28));
    m_ConnectB->setIcon(QIcon::fromTheme("network-connect"));
    m_Grid->addWidget(m_ConnectB, m_Row, 8);

    m_DisconnectB = new QPushButton;
    m_DisconnectB->setFixedSize(QSize(28, 28));
    m_DisconnectB->setIcon(QIcon::fromTheme("network-disconnect"));
    m_Grid->addWidget(m_DisconnectB, m_Row, 9);

    setConnectionMode(static_cast<ConnectionMode>(connectionMode.findOnSwitchIndex()));

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
        m_SerialB->setDown(true);
        m_NetworkB->setDown(false);

        INDI::PropertyView<ISwitch> systemPorts = *(m_Device->getBaseDevice()->getSwitch("SYSTEM_PORTS"));
        const int portIndex = systemPorts.findOnSwitchIndex();
        if (portIndex >= 0)
            m_Ports->setCurrentIndex(portIndex);
        else
        {
            INDI::PropertyView<IText> port = *(m_Device->getBaseDevice()->getText("DEVICE_PORT"));
            m_Ports->setEditText(port.at(0)->getText());
        }

        INDI::PropertyView<ISwitch> systemBauds = *(m_Device->getBaseDevice()->getSwitch("DEVICE_BAUD_RATE"));
        m_BaudRates->setCurrentIndex(systemBauds.findOnSwitchIndex());
    }
    else
    {
        m_SerialB->setDown(false);
        m_NetworkB->setDown(true);

        INDI::PropertyView<IText> server = *(m_Device->getBaseDevice()->getText("DEVICE_ADDRESS"));
        m_HostName->setText(server.at(0)->getText());
        m_HostPort->setText(server.at(1)->getText());

        INDI::PropertyView<ISwitch> connectionType = *(m_Device->getBaseDevice()->getSwitch("CONNECTION_TYPE"));
        m_HostProtocolTCP->setDown(connectionType.findOnSwitchIndex() == 0);
        m_HostProtocolUDP->setDown(connectionType.findOnSwitchIndex() == 1);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
void Device::setConnectionMode(ConnectionMode mode)
{
    if (mode == CONNECTION_SERIAL)
    {
        m_Grid->removeWidget(m_HostName);
        m_Grid->removeWidget(m_HostPort);
        m_Grid->removeWidget(m_HostProtocolTCP);
        m_Grid->removeWidget(m_HostProtocolUDP);

        m_Grid->addWidget(m_Ports, m_Row, 4, 1, 2);
        m_Grid->addWidget(m_BaudRates, m_Row, 6, 1, 2);
    }
    else if (mode == CONNECTION_NETWORK)
    {
        m_Grid->removeWidget(m_Ports);
        m_Grid->removeWidget(m_BaudRates);

        m_Grid->addWidget(m_HostName, m_Row, 4);
        m_Grid->addWidget(m_HostPort, m_Row, 5);
        m_Grid->addWidget(m_HostProtocolTCP, m_Row, 6);
        m_Grid->addWidget(m_HostProtocolUDP, m_Row, 7);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
void Device::processNumber(INumberVectorProperty *nvp)
{

}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
void Device::processSwitch(ISwitchVectorProperty *svp)
{

}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
void Device::processText(ITextVectorProperty *tvp)
{

}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
void Device::setConnected()
{

}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
void Device::setDisconnected()
{

}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
Dialog::Dialog(QWidget *parent) : QDialog(parent)
{
    m_Layout = new QGridLayout(this);

    setWindowTitle(i18n("Port Selector"));
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
}
