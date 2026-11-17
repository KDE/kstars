/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <basedevice.h>
#include <QtDBus/qdbusmetatype.h>

#include "indipac.h"
#include "pacadaptor.h"
#include "ksnotification.h"

namespace ISD
{

const QList<KLocalizedString> PAC::pacStates = { ki18n("Idle"), ki18n("Correcting"), ki18n("Corrected"), ki18n("Error") };

PAC::PAC(GenericDevice *parent): ConcreteDevice(parent)
{
    qRegisterMetaType<ISD::PAC::Status>("ISD::PAC::Status");
    qDBusRegisterMetaType<ISD::PAC::Status>();

    new PACAdaptor(this);
    m_DBusObjectPath = QString("/KStars/INDI/PAC/%1").arg(getID());
    QDBusConnection::sessionBus().registerObject(m_DBusObjectPath, this);
}

void PAC::processSwitch(INDI::Property prop)
{
    auto svp = prop.getSwitch();
    // PAC_ABORT_MOTION is the only switch exposed by the new PACInterface.
    // Its state going to IPS_OK means the abort command was accepted.
    if (svp->isNameMatch("PAC_ABORT_MOTION"))
    {
        if (svp->getState() == IPS_OK)
        {
            if (m_Status != PAC_IDLE)
            {
                m_Status = PAC_IDLE;
                Q_EMIT newStatus(m_Status);
            }
        }
    }
}

void PAC::processNumber(INDI::Property prop)
{
    auto nvp = prop.getNumber();
    // PAC_MANUAL_ADJUSTMENT state transitions drive the correction status:
    //   IPS_BUSY  – one or both axes are still moving  → PAC_CORRECTING
    //   IPS_OK    – all axes finished successfully      → PAC_CORRECTED
    //   IPS_ALERT – an axis encountered an error        → PAC_ERROR
    if (nvp->isNameMatch("PAC_MANUAL_ADJUSTMENT"))
    {
        Status currentStatus = m_Status;

        switch (nvp->getState())
        {
            case IPS_BUSY:
                currentStatus = PAC_CORRECTING;
                break;

            case IPS_OK:
                currentStatus = PAC_CORRECTED;
                KSNotification::event(QLatin1String("IndiServerMessage"), i18n("Polar alignment correction completed"),
                                      KSNotification::Observatory, KSNotification::Info);
                break;

            case IPS_ALERT:
                currentStatus = PAC_ERROR;
                KSNotification::event(QLatin1String("IndiServerMessage"), i18n("Polar alignment correction error"),
                                      KSNotification::Observatory, KSNotification::Alert);
                break;

            case IPS_IDLE:
                currentStatus = PAC_IDLE;
                break;
        }

        if (currentStatus != m_Status)
        {
            m_Status = currentStatus;
            Q_EMIT newStatus(m_Status);
        }
    }
}

void PAC::processLight(INDI::Property prop)
{
    // The updated PACInterface no longer exposes any light properties.
    Q_UNUSED(prop)
}

bool PAC::startCorrection(double azError, double altError)
{
    // The PACInterface uses PAC_MANUAL_ADJUSTMENT to trigger axis movement, with both azimuth and
    // altitude steps written together in a single property send. The steps are moves of the polar
    // axis as seen facing the pole: azimuth > 0 moves it right (East in the northern hemisphere),
    // altitude > 0 raises it. The errors give the position of the axis relative to the pole with the
    // same signs, so the correction that removes them is their negative. This matches the vendor
    // software of the Avalon UPAS and the MLAstro RPA, which move against the measured error.
    auto manualNP = getNumber("PAC_MANUAL_ADJUSTMENT");
    if (!manualNP)
        return false;

    auto azNP  = manualNP.findWidgetByName("MANUAL_AZ_STEP");
    auto altNP = manualNP.findWidgetByName("MANUAL_ALT_STEP");
    if (!azNP || !altNP)
        return false;

    azNP->setValue(-azError);
    altNP->setValue(-altError);
    sendNewProperty(manualNP);

    // A driver may finish a correction without ever reporting Busy, e.g. when every axis
    // is below its minimum move. Mark the correction as started now, so the following Ok
    // is always a status change and listeners see the correction complete.
    if (m_Status != PAC_CORRECTING)
    {
        m_Status = PAC_CORRECTING;
        Q_EMIT newStatus(m_Status);
    }

    return true;
}

bool PAC::abortCorrection()
{
    // The updated PACInterface exposes PAC_ABORT_MOTION to cancel ongoing axis movement.
    auto abortSP = getSwitch("PAC_ABORT_MOTION");
    if (!abortSP)
        return false;

    auto abortSW = abortSP.findWidgetByName("ABORT");
    if (!abortSW)
        return false;

    abortSP.reset();
    abortSW->setState(ISS_ON);
    sendNewProperty(abortSP);

    return true;
}

bool PAC::moveAZ(double degrees)
{
    auto manualNP = getNumber("PAC_MANUAL_ADJUSTMENT");
    if (!manualNP)
        return false;

    auto azNP = manualNP.findWidgetByName("MANUAL_AZ_STEP");
    if (!azNP)
        return false;

    azNP->setValue(degrees);
    sendNewProperty(manualNP);

    return true;
}

bool PAC::moveALT(double degrees)
{
    auto manualNP = getNumber("PAC_MANUAL_ADJUSTMENT");
    if (!manualNP)
        return false;

    auto altNP = manualNP.findWidgetByName("MANUAL_ALT_STEP");
    if (!altNP)
        return false;

    altNP->setValue(degrees);
    sendNewProperty(manualNP);

    return true;
}

const QString PAC::getStatusString(PAC::Status status, bool translated)
{
    return translated ? pacStates[status].toString() : pacStates[status].untranslatedText();
}

}

QDBusArgument &operator<<(QDBusArgument &argument, const ISD::PAC::Status &source)
{
    argument.beginStructure();
    argument << static_cast<int>(source);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::PAC::Status &dest)
{
    int a;
    argument.beginStructure();
    argument >> a;
    argument.endStructure();
    dest = static_cast<ISD::PAC::Status>(a);
    return argument;
}
