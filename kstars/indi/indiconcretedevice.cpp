/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Handle INDI Standard properties.
*/

#include "indiconcretedevice.h"
#include "clientmanager.h"

namespace ISD
{

uint8_t ConcreteDevice::m_ID = 1;

ConcreteDevice::ConcreteDevice(ISD::GenericDevice *parent) : GDInterface(parent), m_Parent(parent),
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
    connect(parent, &GenericDevice::Disconnected, this, [this]()
    {
        // Invalidate dispatched.
        setProperty("dispatched", QVariant());
        emit Disconnected();
    });

    // Signal --> Slots
    connect(parent, &GenericDevice::propertyDefined, this, [this](INDI::Property value)
    {
        if (m_ReadyTimer)
            m_ReadyTimer->start();
        registerProperty(value);
    });
    connect(parent, &GenericDevice::propertyDeleted, this, &ConcreteDevice::removeProperty);
    connect(parent, &GenericDevice::propertyUpdated, this, &ConcreteDevice::updateProperty);

    // N.B. JM 2024.04.09: Better move signals to bottom to allow internal update slots above to run first
    // and update any status variables before informing the outside world.
    // Signal --> Signal
    connect(parent, &GenericDevice::propertyDefined, this, &ConcreteDevice::propertyDefined);
    connect(parent, &GenericDevice::propertyDeleted, this, &ConcreteDevice::propertyDeleted);
    connect(parent, &GenericDevice::propertyUpdated, this, &ConcreteDevice::propertyUpdated);
}

void ConcreteDevice::registeProperties()
{
    // Register all properties first
    for (auto &oneProperty : m_Parent->getProperties())
        registerProperty(oneProperty);
}

void ConcreteDevice::updateProperty(INDI::Property prop)
{
    switch (prop.getType())
    {
        case INDI_SWITCH:
            processSwitch(prop);
            break;
        case INDI_NUMBER:
            processNumber(prop);
            break;
        case INDI_TEXT:
            processText(prop);
            break;
        case INDI_LIGHT:
            processLight(prop);
            break;
        case INDI_BLOB:
            processBLOB(prop);
        default:
            break;
    }
}

void ConcreteDevice::processProperties()
{
    // Register all properties first
    for (auto &oneProperty : m_Parent->getProperties())
    {
        switch (oneProperty.getType())
        {
            case INDI_SWITCH:
                processSwitch(oneProperty);
                break;
            case INDI_NUMBER:
                processNumber(oneProperty);
                break;
            case INDI_TEXT:
                processText(oneProperty);
                break;
            case INDI_LIGHT:
                processLight(oneProperty);
                break;
            case INDI_BLOB:
                processBLOB(oneProperty);
                break;
            default:
                break;
        }
    }
}

INDI::PropertyNumber ConcreteDevice::getNumber(const QString &name) const
{
    return m_Parent->getBaseDevice().getNumber(name.toLatin1().constData());
}

INDI::PropertyText   ConcreteDevice::getText(const QString &name) const
{
    return m_Parent->getBaseDevice().getText(name.toLatin1().constData());
}

INDI::PropertySwitch ConcreteDevice::getSwitch(const QString &name) const
{
    return m_Parent->getBaseDevice().getSwitch(name.toLatin1().constData());
}

INDI::PropertyLight  ConcreteDevice::getLight(const QString &name) const
{
    return m_Parent->getBaseDevice().getLight(name.toLatin1().constData());
}

INDI::PropertyBlob   ConcreteDevice::getBLOB(const QString &name) const
{
    return m_Parent->getBaseDevice().getBLOB(name.toLatin1().constData());
}

void ConcreteDevice::sendNewProperty(INDI::Property prop)
{
    m_Parent->sendNewProperty(prop);
}

QString ConcreteDevice::getMessage(int id) const
{
    return QString::fromStdString(m_Parent->getBaseDevice().messageQueue(id));
}

INDI::Property ConcreteDevice::getProperty(const QString &name) const
{
    return m_Parent->getProperty(name);
}

const QSharedPointer<DriverInfo> &ConcreteDevice::getDriverInfo() const
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
