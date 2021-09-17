/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indi/indistd.h"
#include "indi/indicap.h"

#include <QtDBus>

namespace Ekos
{
/**
 * @class DustCap
 * @short Supports basic DustCap functions (open/close) and optionally control flat light
 *
 * @author Jasem Mutlaq
 * @version 1.0
 */
class DustCap : public QObject
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.DustCap")
        Q_PROPERTY(ISD::DustCap::Status status READ status NOTIFY newStatus)
        Q_PROPERTY(ISD::ParkStatus parkStatus READ parkStatus NOTIFY newParkStatus)
        Q_PROPERTY(bool canPark READ canPark)

    public:
        DustCap();
        virtual ~DustCap() override = default;

        /**
         * @defgroup DustCapDBusInterface Ekos DBus Interface - DustCap Interface
         * Ekos::DustCap interface provides basic DustCap operations.
         */

        /*@{*/

        /**
         * DBUS interface function.
         * If dust cap can park/unpark or is it just a light source?
         * @return True if park is supported, false otherwise
         */
        Q_SCRIPTABLE bool canPark();

        /**
         * DBUS interface function.
         * Park / Close dust cap
         * @return True if operation started/successful, false otherwise
         */
        Q_SCRIPTABLE bool park();

        /**
         * DBUS interface function.
         * UnPark / Open dust cap
         * @return True if operation started/successful, false otherwise
         */
        Q_SCRIPTABLE bool unpark();

        /**
         * DBUS interface function.
         * hasLight: Does the dust cap have a flat light source?
         * @return True if there if flat light, false otherwise
         */
        Q_SCRIPTABLE bool hasLight();

        /**
         * DBUS interface function.
         * setLightEnabled: Turn on/off light box
         * @param enable If true, turn light on, otherwise turn light off
         * @return True if operation started/successful, false otherwise
         */
        Q_SCRIPTABLE bool setLightEnabled(bool enable);

        /**
         * DBUS interface function.
         * SetLight: Set light source brightness level
         * @return True if operation started/successful, false otherwise
         */
        Q_SCRIPTABLE bool setBrightness(uint16_t val);

        /**
         * DBUS interface function.
         * Get the dome park status
         */
        Q_SCRIPTABLE ISD::ParkStatus parkStatus()
        {
            return m_ParkStatus;
        }

        /** @}*/

        /**
         * @brief setDustCap set the DustCap device
         * @param newDustCap pointer to DustCap device.
         */
        void setDustCap(ISD::GDInterface *newDustCap);

        void removeDevice(ISD::GDInterface *device);

        ISD::DustCap::Status status()
        {
            return currentDustCap->status();
        }

    signals:
        void newStatus(ISD::DustCap::Status status);
        void newParkStatus(ISD::ParkStatus status);
        void lightToggled(bool enabled);
        void lightIntensityChanged(int value);
        void ready();

    private:
        void processProp(INDI::Property prop);
        void processSwitch(ISwitchVectorProperty *svp);
        void processNumber(INumberVectorProperty *nvp);
        // Devices needed for DustCap operation
        ISD::DustCap *currentDustCap { nullptr };
        ISD::ParkStatus m_ParkStatus { ISD::PARK_UNKNOWN };
        bool m_LightEnabled { false };
        uint16_t m_lightIntensity { 0 };
};
}
