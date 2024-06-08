/*
    SPDX-FileCopyrightText: 2024 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mountcontrolpanel.h"
#include "skypoint.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "geolocation.h"
#include "ekos/manager.h"
#include "dialogs/finddialog.h"

#define EQ_BUTTON_ID  0
#define HOR_BUTTON_ID 1
#define HA_BUTTON_ID  2

namespace Ekos
{
MountControlPanel::MountControlPanel(QWidget *parent) : QDialog(parent)
{
    setupUi(this);
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    // forward motion commands
    connect(mountMotion, &MountMotionWidget::newMotionCommand, this, &MountControlPanel::newMotionCommand);
    connect(mountMotion, &MountMotionWidget::newSlewRate, this, &MountControlPanel::newSlewRate);
    connect(mountMotion, &MountMotionWidget::aborted, this, &MountControlPanel::aborted);
    connect(mountMotion, &MountMotionWidget::updownReversed, this, &MountControlPanel::updownReversed);
    connect(mountMotion, &MountMotionWidget::leftrightReversed, this, &MountControlPanel::leftrightReversed);

    // forward J2000 selection to the target widget, which does not have its own selection
    connect(mountPosition, &MountPositionWidget::J2000Enabled, mountTarget, &MountTargetWidget::setJ2000Enabled);

    // forward target commands
    connect(mountTarget, &MountTargetWidget::slew, this, &MountControlPanel::slew);
    connect(mountTarget, &MountTargetWidget::sync, this, &MountControlPanel::sync);

    // PARK
    connect(parkButtonObject, &QPushButton::clicked, this, &MountControlPanel::park);
    // UNPARK
    connect(unparkButtonObject, &QPushButton::clicked, this, &MountControlPanel::unpark);
    // center
    connect(centerButtonObject, &QPushButton::clicked, this, &MountControlPanel::center);

    // ensure that all J2000 attributes are in sync
    setJ2000Enabled(true);
}



/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
void MountControlPanel::setJ2000Enabled(bool enabled)
{
    mountPosition->setJ2000Enabled(enabled);
    mountTarget->setJ2000Enabled(enabled);
}

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
void MountControlPanel::keyPressEvent(QKeyEvent *event)
{
    // forward to sub widget
    mountMotion->keyPressEvent(event);
}

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
void MountControlPanel::keyReleaseEvent(QKeyEvent *event)
{
    // forward to sub widget
    mountMotion->keyReleaseEvent(event);
}

}
