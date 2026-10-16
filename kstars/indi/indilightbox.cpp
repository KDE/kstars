/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <basedevice.h>
#include <QtDBus/qdbusmetatype.h>
#include "indilightbox.h"
#include "lightboxadaptor.h"
#include "ksnotification.h"

Q_DECLARE_METATYPE(ISD::LightBox::LightStatus)

namespace ISD
{

const QList<KLocalizedString> LightBox::lightStates =
{
    ki18n("Off"), ki18n("On"), ki18n("Busy"), ki18n("Error"), ki18n("Unknown")
};

LightBox::LightBox(GenericDevice *parent) : ConcreteDevice(parent)
{
    qRegisterMetaType<ISD::LightBox::LightStatus>("ISD::LightBox::LightStatus");
    qDBusRegisterMetaType<ISD::LightBox::LightStatus>();

    new LightBoxAdaptor(this);
    m_DBusObjectPath = QString("/KStars/INDI/LightBox/%1").arg(getID());
    QDBusConnection::sessionBus().registerObject(m_DBusObjectPath, this);
}

void LightBox::processSwitch(INDI::Property prop)
{
    auto svp = prop.getSwitch();
    if (svp->isNameMatch("FLAT_LIGHT_CONTROL"))
    {
        LightStatus newStatus = LIGHT_OFF;

        // Check for error state first
        if (svp->getState() == IPS_ALERT)
            newStatus = LIGHT_ERROR;
        else if (svp->getState() == IPS_BUSY)
            newStatus = LIGHT_BUSY;
        else
        {
            auto lightON = svp->findWidgetByName("FLAT_LIGHT_ON");
            if (lightON && lightON->getState() == ISS_ON)
                newStatus = LIGHT_ON;
        }

        if (newStatus != m_LightStatus)
        {
            m_LightStatus = newStatus;
            Q_EMIT newLightStatus(m_LightStatus);
        }
    }
}

void LightBox::processNumber(INDI::Property prop)
{
    auto nvp = prop.getNumber();
    if (nvp->isNameMatch("FLAT_LIGHT_INTENSITY"))
    {
        m_Brightness = static_cast<uint16_t>(nvp->at(0)->getValue());
    }
}

const QString LightBox::getStatusString(LightBox::LightStatus status, bool translated)
{
    return translated ? lightStates[status].toString() : lightStates[status].untranslatedText();
}

bool LightBox::isLightOn()
{
    auto lightSP = getSwitch("FLAT_LIGHT_CONTROL");
    if (!lightSP)
        return false;

    auto lightON = lightSP.findWidgetByName("FLAT_LIGHT_ON");
    if (!lightON)
        return false;

    return lightON->getState() == ISS_ON;
}

bool LightBox::setLightEnabled(bool enable)
{
    auto lightSP = getSwitch("FLAT_LIGHT_CONTROL");

    if (!lightSP)
        return false;

    auto lightON  = lightSP.findWidgetByName("FLAT_LIGHT_ON");
    auto lightOFF = lightSP.findWidgetByName("FLAT_LIGHT_OFF");

    if (!lightON || !lightOFF)
        return false;

    lightSP.reset();

    if (enable)
        lightON->setState(ISS_ON);
    else
        lightOFF->setState(ISS_ON);

    sendNewProperty(lightSP);
    // The light state is unknown until the driver confirms it. Report it as busy until then,
    // so that the outdated state is not taken for the current one.
    m_LightStatus = LIGHT_BUSY;

    return true;
}

bool LightBox::setBrightness(uint16_t val)
{
    auto lightIntensity = getNumber("FLAT_LIGHT_INTENSITY");
    if (!lightIntensity)
        return false;

    lightIntensity[0].setValue(val);
    sendNewProperty(lightIntensity);
    return true;
}


}

QDBusArgument &operator<<(QDBusArgument &argument, const ISD::LightBox::LightStatus &source)
{
    argument.beginStructure();
    argument << static_cast<int>(source);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::LightBox::LightStatus &dest)
{
    int a;
    argument.beginStructure();
    argument >> a;
    argument.endStructure();
    dest = static_cast<ISD::LightBox::LightStatus>(a);
    return argument;
}
