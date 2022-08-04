/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Handle INDI Standard properties.
*/

#include "indiconcretedevice.h"
#include "clientmanager.h"

namespace ISD

{
ConcreteDevice::ConcreteDevice(GenericDevice *parent) : GDInterface(parent), m_Parent(parent),
    m_Name(parent->getDeviceName())
{
    // Signal --> Signal
    connect(parent, &GenericDevice::Connected, this, [this]()
    {
        m_ReadyTimer.reset(new QTimer(this));
        m_ReadyTimer->setInterval(250);
        m_ReadyTimer->setSingleShot(true);
        connect(m_ReadyTimer.get(), &QTimer::timeout, this, &ConcreteDevice::ready);
        emit Connected();
    });
    connect(parent, &GenericDevice::Disconnected, this, &ConcreteDevice::Disconnected);

    // Signal --> Signal
    connect(parent, &GenericDevice::propertyDefined, this, &ConcreteDevice::propertyDefined);
    connect(parent, &GenericDevice::propertyDeleted, this, &ConcreteDevice::propertyDeleted);

    // Signal --> Signal
    connect(parent, &GenericDevice::switchUpdated, this, &ConcreteDevice::switchUpdated);
    connect(parent, &GenericDevice::textUpdated, this, &ConcreteDevice::textUpdated);
    connect(parent, &GenericDevice::numberUpdated, this, &ConcreteDevice::numberUpdated);
    connect(parent, &GenericDevice::lightUpdated, this, &ConcreteDevice::lightUpdated);
    connect(parent, &GenericDevice::BLOBUpdated, this, &ConcreteDevice::BLOBUpdated);

    // Signal --> Slots
    connect(parent, &GenericDevice::propertyDefined, this, [this](INDI::Property value)
    {
        if (m_ReadyTimer)
            m_ReadyTimer->start();
        registerProperty(value);
    });
    connect(parent, &GenericDevice::propertyDeleted, this, &ConcreteDevice::removeProperty);
    connect(parent, &GenericDevice::switchUpdated, this, &ConcreteDevice::processSwitch);
    connect(parent, &GenericDevice::textUpdated, this, &ConcreteDevice::processText);
    connect(parent, &GenericDevice::numberUpdated, this, &ConcreteDevice::processNumber);
    connect(parent, &GenericDevice::lightUpdated, this, &ConcreteDevice::processLight);
}

void ConcreteDevice::registeProperties()
{
    // Register all properties first
    for (auto &oneProperty : m_Parent->getProperties())
        registerProperty(oneProperty);
}

void ConcreteDevice::processProperties()
{
    // Register all properties first
    for (auto &oneProperty : m_Parent->getProperties())
    {
        switch (oneProperty.getType())
        {
            case INDI_SWITCH:
                processSwitch(oneProperty.getSwitch());
                break;
            case INDI_NUMBER:
                processNumber(oneProperty.getNumber());
                break;
            case INDI_TEXT:
                processText(oneProperty.getText());
                break;
            case INDI_LIGHT:
                processLight(oneProperty.getLight());
                break;
            default:
                break;
        }
    }
}

INDI::PropertyView<INumber> *ConcreteDevice::getNumber(const QString &name) const
{
    return m_Parent->getBaseDevice()->getNumber(name.toLatin1().constData());
}

INDI::PropertyView<IText>   *ConcreteDevice::getText(const QString &name) const
{
    return m_Parent->getBaseDevice()->getText(name.toLatin1().constData());
}

INDI::PropertyView<ISwitch> *ConcreteDevice::getSwitch(const QString &name) const
{
    return m_Parent->getBaseDevice()->getSwitch(name.toLatin1().constData());
}

INDI::PropertyView<ILight>  *ConcreteDevice::getLight(const QString &name) const
{
    return m_Parent->getBaseDevice()->getLight(name.toLatin1().constData());
}

INDI::PropertyView<IBLOB>   *ConcreteDevice::getBLOB(const QString &name) const
{
    return m_Parent->getBaseDevice()->getBLOB(name.toLatin1().constData());
}

void ConcreteDevice::sendNewText(ITextVectorProperty *pp)
{
    m_Parent->getClientManager()->sendNewText(pp);
}

QString ConcreteDevice::getMessage(int id) const
{
    return QString::fromStdString(m_Parent->getBaseDevice()->messageQueue(id));
}


void ConcreteDevice::sendNewText(const char *deviceName, const char *propertyName, const char *elementName,
                                 const char *text)
{
    m_Parent->getClientManager()->sendNewText(deviceName, propertyName, elementName, text);
}


void ConcreteDevice::sendNewNumber(INumberVectorProperty *pp)
{
    m_Parent->getClientManager()->sendNewNumber(pp);
}

void ConcreteDevice::sendNewNumber(const char *deviceName, const char *propertyName, const char *elementName, double value)
{
    m_Parent->getClientManager()->sendNewNumber(deviceName, propertyName, elementName, value);
}

void ConcreteDevice::sendNewSwitch(ISwitchVectorProperty *pp)
{
    m_Parent->getClientManager()->sendNewSwitch(pp);
}

void ConcreteDevice::sendNewSwitch(const char *deviceName, const char *propertyName, const char *elementName)
{
    m_Parent->getClientManager()->sendNewSwitch(deviceName, propertyName, elementName);
}

INDI::Property ConcreteDevice::getProperty(const QString &name) const
{
    return m_Parent->getProperty(name);
}

DriverInfo * ConcreteDevice::getDriverInfo() const
{
    return m_Parent->getDriverInfo();
}

bool ConcreteDevice::setConfig(INDIConfig tConfig)
{
    return m_Parent->setConfig(tConfig);
}

Properties ConcreteDevice::getProperties() const
{
    return m_Parent->getProperties();
}

bool ConcreteDevice::getMinMaxStep(const QString &propName, const QString &elementName, double *min, double *max,
                                   double *step) const
{
    return m_Parent->getMinMaxStep(propName, elementName, min, max, step);
}

IPState ConcreteDevice::getState(const QString &propName) const
{
    return m_Parent->getState(propName);
}

IPerm ConcreteDevice::getPermission(const QString &propName) const
{
    return m_Parent->getPermission(propName);
}

}
