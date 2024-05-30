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

    // Coordinate Button Group
    targetRATextObject->setUnits(dmsBox::HOURS);
    coordinateButtonGroup->setId(equatorialCheckObject, EQ_BUTTON_ID);
    coordinateButtonGroup->setId(horizontalCheckObject, HOR_BUTTON_ID);
    coordinateButtonGroup->setId(haEquatorialCheckObject, HA_BUTTON_ID);
    connect(coordinateButtonGroup, &QButtonGroup::idPressed, this, &MountControlPanel::updateTargetLabels);

    // GOTO
    connect(gotoButtonObject, &QPushButton::clicked, this, &MountControlPanel::processSlew);

    // SYNC
    connect(syncButtonObject, &QPushButton::clicked, this, &MountControlPanel::processSync);

    // PARK
    connect(parkButtonObject, &QPushButton::clicked, this, &MountControlPanel::park);

    // UNPARK
    connect(unparkButtonObject, &QPushButton::clicked, this, &MountControlPanel::unpark);

    // Find
    connect(findButtonObject, &QPushButton::clicked, this, &MountControlPanel::findTarget);

    // center
    connect(centerButtonObject, &QPushButton::clicked, this, &MountControlPanel::center);

    // Up/down Reverse
    connect(upDownCheckObject, &QCheckBox::toggled, this, &MountControlPanel::updownReversed);

    // Left/Right Reverse
    connect(leftRightCheckObject, &QCheckBox::toggled, this, &MountControlPanel::leftrightReversed);

    raValueObject->setTextInteractionFlags(Qt::TextSelectableByMouse);
    deValueObject->setTextInteractionFlags(Qt::TextSelectableByMouse);
    azValueObject->setTextInteractionFlags(Qt::TextSelectableByMouse);
    altValueObject->setTextInteractionFlags(Qt::TextSelectableByMouse);
    haValueObject->setTextInteractionFlags(Qt::TextSelectableByMouse);
    zaValueObject->setTextInteractionFlags(Qt::TextSelectableByMouse);
}

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
bool MountControlPanel::processCoords(dms &ra, dms &de)
{
    bool raOK = false, deOK = false;

    if (equatorialCheckObject->property("checked").toBool())
    {
        ra = targetRATextObject->createDms(&raOK);
        de = targetDETextObject->createDms(&deOK);
    }

    if (horizontalCheckObject->property("checked").toBool())
    {
        dms az = targetRATextObject->createDms(&raOK);
        dms at = targetDETextObject->createDms(&deOK);
        SkyPoint target;
        target.setAz(az);
        target.setAlt(at);
        target.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());
        ra = target.ra();
        de = target.dec();
    }

    if (haEquatorialCheckObject->property("checked").toBool())
    {
        dms ha = targetRATextObject->createDms(&raOK);
        de = targetDETextObject->createDms(&deOK);
        dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
        ra = (lst - ha + dms(360.0)).reduce();
    }

    // If J2000 was checked and the Mount is _not_ already using native J2000 coordinates
    // then we need to convert J2000 to JNow. Otherwise, we send J2000 as is.
    if (j2000CheckObject->property("checked").toBool() && m_isJ2000 == false)
    {
        // J2000 ---> JNow
        SkyPoint J2000Coord(ra, de);
        J2000Coord.setRA0(ra);
        J2000Coord.setDec0(de);
        J2000Coord.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());

        ra = J2000Coord.ra();
        de = J2000Coord.dec();
    }

    return raOK && deOK;
}

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
void MountControlPanel::updateTargetLabels(int id)
{
    switch (id)
    {
        case HOR_BUTTON_ID:
            targetRALabel->setText("AZ:");
            targetDECLabel->setText("AL:");
            targetRATextObject->setUnits(dmsBox::HOURS);
            break;
        case HA_BUTTON_ID:
            targetRALabel->setText("HA:");
            targetDECLabel->setText("DE:");
            targetRATextObject->setUnits(dmsBox::HOURS);
            break;
        default:
            targetRALabel->setText("RA:");
            targetDECLabel->setText("DE:");
            targetRATextObject->setUnits(dmsBox::DEGREES);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
void MountControlPanel::processSlew()
{
    dms ra, de;
    if (processCoords(ra, de))
        emit slew(ra.Hours(), de.Degrees());
}

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
void MountControlPanel::processSync()
{
    dms ra, de;
    if (processCoords(ra, de))
        emit sync(ra.Hours(), de.Degrees());
}

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
void MountControlPanel::findTarget()
{
    if (FindDialog::Instance()->execWithParent(Ekos::Manager::Instance()) == QDialog::Accepted)
    {
        auto object = FindDialog::Instance()->targetObject();
        if (object != nullptr)
        {
            auto const data = KStarsData::Instance();
            auto o = object->clone();
            o->updateCoords(data->updateNum(), true, data->geo()->lat(), data->lst(), false);

            equatorialCheckObject->setProperty("checked", true);

            targetTextObject->setProperty("text", o->name());

            if (jnowCheckObject->property("checked").toBool())
            {
                targetRATextObject->setText(o->ra().toHMSString());
                targetDETextObject->setText(o->dec().toDMSString());
            }
            else
            {
                targetRATextObject->setText(o->ra0().toHMSString());
                targetDETextObject->setText(o->dec0().toDMSString());
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////
void MountControlPanel::keyPressEvent(QKeyEvent *event)
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
void MountControlPanel::keyReleaseEvent(QKeyEvent *event)
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

}
