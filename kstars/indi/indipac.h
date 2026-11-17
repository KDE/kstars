/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <memory>
#include <QTimer>

#include <KLocalizedString>

#include "indiconcretedevice.h"

namespace ISD
{
/**
 * @class PAC
 * Handles operation of a remotely controlled Polar Alignment Corrector (PAC) device.
 *
 * The PAC interface allows clients to:
 * - Set the measured azimuth and altitude alignment errors (ALIGNMENT_CORRECTION_ERROR)
 * - Start or abort an automated polar alignment correction (ALIGNMENT_CORRECTION)
 * - Monitor correction progress (ALIGNMENT_CORRECTION_STATUS)
 * - Manually nudge the mount in azimuth or altitude (PAC_MANUAL_ADJUSTMENT)
 *
 * @author Jasem Mutlaq
 */
class PAC : public ConcreteDevice
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.INDI.PAC")
        Q_PROPERTY(ISD::PAC::Status status READ status NOTIFY newStatus)

    public:
        explicit PAC(GenericDevice *parent);
        virtual ~PAC() override = default;

        typedef enum
        {
            PAC_IDLE,
            PAC_CORRECTING,
            PAC_CORRECTED,
            PAC_ERROR
        } Status;

        virtual void processSwitch(INDI::Property prop) override;
        virtual void processNumber(INDI::Property prop) override;
        virtual void processLight(INDI::Property prop) override;

        static const QString getStatusString(Status status, bool translated = true);

    public Q_SLOTS:
        /**
         * @brief startCorrection Start an automated polar alignment correction.
         * The PAC moves the polar axis by the opposite of the error, so the error is removed.
         * @param azError Azimuth error in degrees, as measured by the polar alignment assistant
         * (positive = polar axis to the right of the pole as seen facing the pole, i.e. East in the
         * northern hemisphere).
         * @param altError Altitude error in degrees (positive = polar axis too high).
         * @return True if the command was sent successfully, false otherwise.
         */
        Q_SCRIPTABLE bool startCorrection(double azError, double altError);

        /**
         * @brief abortCorrection Abort an in-progress polar alignment correction.
         * @return True if the command was sent successfully, false otherwise.
         */
        Q_SCRIPTABLE bool abortCorrection();

        /**
         * @brief moveAZ Manually nudge the azimuth axis.
         * @param degrees Signed step in degrees: positive = East, negative = West.
         * @return True if the command was sent successfully, false otherwise.
         */
        Q_SCRIPTABLE bool moveAZ(double degrees);

        /**
         * @brief moveALT Manually nudge the altitude axis.
         * @param degrees Signed step in degrees: positive = North, negative = South.
         * @return True if the command was sent successfully, false otherwise.
         */
        Q_SCRIPTABLE bool moveALT(double degrees);

        Q_SCRIPTABLE Status status()
        {
            return m_Status;
        }

    Q_SIGNALS:
        void newStatus(ISD::PAC::Status status);

    private:
        Status m_Status { PAC_IDLE };
        static const QList<KLocalizedString> pacStates;
};

}

Q_DECLARE_METATYPE(ISD::PAC::Status)
QDBusArgument &operator<<(QDBusArgument &argument, const ISD::PAC::Status &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::PAC::Status &dest);
