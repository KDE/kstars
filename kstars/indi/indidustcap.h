/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <memory>
#include <QTimer>

#include "indiconcretedevice.h"

namespace ISD
{
/**
 * @class DustCap
 * Handles operation of a remotely controlled dust cover cap.
 *
 * @author Jasem Mutlaq
 */
class DustCap : public ConcreteDevice
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.INDI.DustCap")
        Q_PROPERTY(ISD::DustCap::Status status READ status NOTIFY newStatus)
        Q_PROPERTY(bool canPark READ canPark)
        Q_PROPERTY(bool hasLight READ hasLight)

    public:
        explicit DustCap(GenericDevice *parent);
        virtual ~DustCap() override = default;
        typedef enum
        {
            CAP_IDLE,
            CAP_PARKING,
            CAP_UNPARKING,
            CAP_PARKED,
            CAP_ERROR
        } Status;

        virtual void processSwitch(ISwitchVectorProperty *svp) override;

        Q_SCRIPTABLE virtual bool hasLight();
        Q_SCRIPTABLE virtual bool canPark();
        Q_SCRIPTABLE virtual bool isLightOn();
        // Check if cap is fully parked.
        Q_SCRIPTABLE virtual bool isParked();
        // Check if cap is fully unparked. We need this because we have parking and unparking in progress
        Q_SCRIPTABLE virtual bool isUnParked();

        static const QString getStatusString(Status status, bool translated = true);

    public slots:
        /**
         * @brief SetBrightness Set light box brightness levels if dimmable.
         * @param val Value of brightness level.
         * @return True if operation is successful, false otherwise.
         */
        Q_SCRIPTABLE bool setBrightness(uint16_t val);

        /**
         * @brief SetLightEnabled Turn on/off light
         * @param enable true to turn on, false to turn off
         * @return True if operation is successful, false otherwise.
         */
        Q_SCRIPTABLE bool setLightEnabled(bool enable);

        /**
         * @brief Park Close dust cap
         * @return True if operation is successful, false otherwise.
         */
        Q_SCRIPTABLE bool park();

        /**
         * @brief UnPark Open dust cap
         * @return True if operation is successful, false otherwise.
         */
        Q_SCRIPTABLE bool unPark();

        Q_SCRIPTABLE Status status()
        {
            return m_Status;
        }

    signals:
        void newStatus(ISD::DustCap::Status status);

    private:
        Status m_Status { CAP_IDLE };
        static const QList<const char *> capStates;
};

}

Q_DECLARE_METATYPE(ISD::DustCap::Status)
QDBusArgument &operator<<(QDBusArgument &argument, const ISD::DustCap::Status &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::DustCap::Status &dest);
