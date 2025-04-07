/*  Widget to control the mount motion.
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_mountmotionwidget.h"
#include "indipropertyswitch.h"

#include <QWidget>

namespace Ekos
{
class MountMotionWidget  : public QWidget, public Ui::MountMotionWidget
{
        Q_OBJECT

        friend class MountControlPanel;

    public:
        explicit MountMotionWidget(QWidget *parent = nullptr);

        void syncSpeedInfo(INDI::PropertySwitch svp);
        void updateSpeedInfo(INDI::PropertySwitch svp);

    protected:
        void keyPressEvent(QKeyEvent *event) override;
        void keyReleaseEvent(QKeyEvent *event) override;

    signals:
        void newMotionCommand(int command, int NS, int WE);
        void newSlewRate(int rate);
        void aborted();
        void updownReversed(bool enable);
        void leftrightReversed(bool enable);

};

} // namespace
