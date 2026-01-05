/*  Widget to slew or sync to a position.
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include "ui_mounttargetwidget.h"
#include "skypoint.h"

#include <QObject>
#include <QWidget>

namespace Ekos
{
class MountTargetWidget : public QWidget, public Ui::MountTargetWidget
{
        Q_OBJECT
    public:
        explicit MountTargetWidget(QWidget *parent = nullptr);

        bool processCoords(dms &ra, dms &de);

        /**
         * @brief setTargetPosition set the target position, coordinates in JNow
         */
        void setTargetPosition(SkyPoint *position);
        void setTargetName(const QString &name);

        // target coord conversions for displaying
        bool raDecToAzAlt(QString qsRA, QString qsDec);
        bool raDecToHaDec(QString qsRA);
        bool azAltToRaDec(QString qsAz, QString qsAlt);
        bool azAltToHaDec(QString qsAz, QString qsAlt);
        bool haDecToRaDec(QString qsHA);
        bool haDecToAzAlt(QString qsHA, QString qsDec);

        void setJ2000Enabled(bool enabled);

        SkyPoint *currentTarget()
        {
            return m_currentTarget;
        }

    private:
        void processSlew();
        void processSync();
        void findTarget();
        void updateTargetDisplay(int id = -1, SkyPoint *target = nullptr);

        bool updateTarget();
        SkyPoint *m_currentTarget {nullptr};
        bool m_isJ2000 {false};

        /**
         * @brief Helper function to update  coordinates of a sky point from its JNow coordinates
         * @param coords sky point with correct JNow values in RA and DEC
         * @param updateJ2000 update the J2000 coordinates
         * @param updateHorizontal update the ALT/AZ coordinates
         */
        void updateJ2000Coordinates(SkyPoint *coords, bool updateJ2000 = true, bool updateHorizontal = true);

    signals:
        void sync(double RA, double DE);
        void slew(double RA, double DE);
        void newTarget();

};

} // namespace
