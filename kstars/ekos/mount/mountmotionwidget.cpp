/*  Widget to control the mount motion.
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include "mountmotionwidget.h"
#include "klocalizedstring.h"

#include <QKeyEvent>

extern const char *libindi_strings_context;

namespace Ekos
{
MountMotionWidget::MountMotionWidget(QWidget *parent)
    : QWidget{parent}
{
    setupUi(this);
    // North West
    northWest->setIcon(QIcon(":/icons/go-northwest"));
    connect(northWest, &QAbstractButton::pressed, this, [this]()
    {
        emit newMotionCommand(0, 0, 0);
    });
    connect(northWest, &QAbstractButton::released, this, [this]()
    {
        emit newMotionCommand(1, 0, 0);
    });

    // North
    north->setIcon(QIcon(":/icons/go-north"));
    connect(north, &QAbstractButton::pressed, this, [this]()
    {
        emit newMotionCommand(0, 0, -1);
    });
    connect(north, &QAbstractButton::released, this, [this]()
    {
        emit newMotionCommand(1, 0, -1);
    });

    // North East
    northEast->setIcon(QIcon(":/icons/go-northeast"));
    connect(northEast, &QAbstractButton::pressed, this, [this]()
    {
        emit newMotionCommand(0, 0, 1);
    });
    connect(northEast, &QAbstractButton::released, this, [this]()
    {
        emit newMotionCommand(1, 0, 1);
    });

    // West
    west->setIcon(QIcon(":/icons/go-west"));
    connect(west, &QAbstractButton::pressed, this, [this]()
    {
        emit newMotionCommand(0, -1, 0);
    });
    connect(west, &QAbstractButton::released, this, [this]()
    {
        emit newMotionCommand(1, -1, 0);
    });

    // Stop
    stop->setIcon(QIcon(":/icons/stop"));
    connect(stop, &QAbstractButton::pressed, this, [this]()
    {
        emit aborted();
    });

    // East
    east->setIcon(QIcon(":/icons/go-east"));
    connect(east, &QAbstractButton::pressed, this, [this]()
    {

        emit newMotionCommand(0, -1, 1);
    });
    connect(east, &QAbstractButton::released, this, [this]()
    {
        emit newMotionCommand(1, -1, 1);
    });

    // South West
    southWest->setIcon(QIcon(":/icons/go-southwest"));
    connect(southWest, &QAbstractButton::pressed, this, [this]()
    {

        emit newMotionCommand(0, 1, 0);
    });
    connect(southWest, &QAbstractButton::released, this, [this]()
    {
        emit newMotionCommand(1, 1, 0);
    });

    // South
    south->setIcon(QIcon(":/icons/go-south"));
    connect(south, &QAbstractButton::pressed, this, [this]()
    {

        emit newMotionCommand(0, 1, -1);
    });
    connect(south, &QAbstractButton::released, this, [this]()
    {
        emit newMotionCommand(1, 1, -1);
    });

    // South East
    southEast->setIcon(QIcon(":/icons/go-southeast"));
    connect(southEast, &QAbstractButton::pressed, this, [this]()
    {
        emit newMotionCommand(0, 1, 1);

    });
    connect(southEast, &QAbstractButton::released, this, [this]()
    {
        emit newMotionCommand(1, 1, 1);
    });

    // Slew Rate
    connect(speedSliderObject, &QSlider::sliderReleased, this, [this]()
    {
        emit newSlewRate(speedSliderObject->value());
    });

    // Up/down Reverse
    connect(upDownCheckObject, &QCheckBox::toggled, this, &MountMotionWidget::updownReversed);

    // Left/Right Reverse
    connect(leftRightCheckObject, &QCheckBox::toggled, this, &MountMotionWidget::leftrightReversed);
}

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
void MountMotionWidget::syncSpeedInfo(INDI::PropertySwitch svp)
{
    if (svp.isValid())
    {
        int index = svp.findOnSwitchIndex();
        speedSliderObject->setEnabled(true);
        speedSliderObject->setMaximum(svp.count() - 1);
        speedSliderObject->setValue(index);

        speedLabelObject->setText(i18nc(libindi_strings_context, svp[index].getLabel()));
        speedLabelObject->setEnabled(true);
    }
    else
    {
        // QtQuick
        speedSliderObject->setEnabled(false);
        speedLabelObject->setEnabled(false);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
void MountMotionWidget::updateSpeedInfo(INDI::PropertySwitch svp)
{
    if (svp.isValid())
    {
        auto index = svp.findOnSwitchIndex();
        speedSliderObject->setProperty("value", index);
        speedLabelObject->setProperty("text", i18nc(libindi_strings_context, svp[index].getLabel()));
    }
}

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
void MountMotionWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;

    switch (event->key())
    {
        case Qt::Key_Up:
        {
            north->setDown(true);
            emit newMotionCommand(0, 0, -1);
            break;
        }
        case Qt::Key_Down:
        {
            south->setDown(true);
            emit newMotionCommand(0, 1, -1);
            break;
        }
        case Qt::Key_Right:
        {
            east->setDown(true);
            emit newMotionCommand(0, -1, 1);
            break;
        }
        case Qt::Key_Left:
        {
            west->setDown(true);
            emit newMotionCommand(0, -1, 0);
            break;
        }
        case Qt::Key_Space:
        {
            break;
        }
    }

    event->ignore();
}

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
void MountMotionWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;

    switch (event->key())
    {
        case Qt::Key_Up:
        {
            north->setDown(false);
            emit newMotionCommand(1, 0, -1);
            break;
        }
        case Qt::Key_Down:
        {
            south->setDown(false);
            emit newMotionCommand(1, 1, -1);
            break;
        }
        case Qt::Key_Right:
        {
            east->setDown(false);
            emit newMotionCommand(1, -1, 1);
            break;
        }
        case Qt::Key_Left:
        {
            west->setDown(false);
            emit newMotionCommand(1, -1, 0);
            break;
        }
        case Qt::Key_Space:
        {
            stop->click();
            break;
        }
    }

    event->ignore();
}

} // namespace
