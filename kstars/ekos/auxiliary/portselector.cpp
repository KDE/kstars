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
#include "Options.h"

#include <QPushButton>

namespace  Selector
{

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
Device::Device(ISD::GDInterface *device) :  m_Device(device)
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

    m_Layout = new QHBoxLayout;
    m_LED = new KLed;
    m_Label = new QLabel;
    m_Label->setText(m_Device->getDeviceName());

    m_ConnectB = new QPushButton;
    m_DisconnectB = new QPushButton;

    m_Layout->addWidget(m_LED);
    m_Layout->addWidget(m_Label);

    INDI::PropertyView<ISwitch> connectionMode = *(modeProperty->getSwitch());
    const bool isSerial = connectionMode.findOnSwitch()->isNameMatch("CONNECTION_SERIAL");
    if (connectionMode.findWidgetByName("CONNECTION_SERIAL"))
    {
        m_SerialB = new QPushButton(i18n("Serial"));
        m_Layout->addWidget(m_SerialB);

        m_Ports = new QComboBox;
        m_BaudRates = new QComboBox;
    }
    if (connectionMode.findWidgetByName("CONNECTION_TCP"))
    {
        m_NetworkB = new QPushButton(i18n("Network"));
        m_NetworkB->setDown(!isSerial);
        m_Layout->addWidget(m_NetworkB);

        m_HostName = new QLineEdit;
        m_HostName->setPlaceholderText(i18n("Host name or IP address."));

        m_HostPort = new QLineEdit;
        m_HostPort->setPlaceholderText(i18n("Port"));

        m_HostProtocolTCP = new QPushButton("TCP");
        m_HostProtocolUDP = new QPushButton("UDP");
    }

    if (m_SerialB)
    {
        m_Layout->addWidget(m_SerialB);
        m_Layout->addWidget(m_Ports);
        m_Layout->addWidget(m_BaudRates);
    }
    if (m_NetworkB)
    {
        m_Layout->addWidget(m_NetworkB);
        m_Layout->addWidget(m_HostName);
        m_Layout->addWidget(m_HostPort);
        m_Layout->addWidget(m_HostProtocolTCP);
        m_Layout->addWidget(m_HostProtocolUDP);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
void Device::syncGUI()
{
    INDI::Property modeProperty = m_Device->getBaseDevice()->getSwitch("CONNECTION_MODE");
    if (!modeProperty.isValid())
        return;

    m_LED->setColor(ColorCode.value(m_Device->getState("CONNECTION")));

    INDI::PropertyView<ISwitch> connectionMode = *(modeProperty->getSwitch());
    const bool isSerial = connectionMode.findOnSwitch()->isNameMatch("CONNECTION_SERIAL");

    m_Ports->setHidden(!isSerial);
    m_BaudRates->setHidden(!isSerial);

    m_HostName->setHidden(isSerial);
    m_HostPort->setHidden(isSerial);
    m_HostProtocolTCP->setHidden(isSerial);
    m_HostProtocolUDP->setHidden(isSerial);
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
    m_Layout = new QVBoxLayout(this);
}

//////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////
void Dialog::addDevice(ISD::GDInterface *device)
{
    std::unique_ptr<Device> dev(new Device(device));
    m_Layout->addLayout(dev->rowLayout());
    m_Devices.push_back(std::move(dev));

}
}
