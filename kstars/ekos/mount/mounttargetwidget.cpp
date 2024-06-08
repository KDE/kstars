/*  Widget to slew or sync to a position.
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mounttargetwidget.h"
#include "kstarsdatetime.h"
#include "kstarsdata.h"
#include "kstars.h"
#include "ekos/manager.h"
#include "dialogs/finddialog.h"

#define EQ_BUTTON_ID  0
#define HOR_BUTTON_ID 1
#define HA_BUTTON_ID  2

namespace Ekos
{
MountTargetWidget::MountTargetWidget(QWidget *parent)
    : QWidget{parent}
{
    setupUi(this);

    // Coordinate Button Group
    targetRATextObject->setUnits(dmsBox::HOURS);
    coordinateButtonGroup->setId(equatorialCheckObject, EQ_BUTTON_ID);
    coordinateButtonGroup->setId(horizontalCheckObject, HOR_BUTTON_ID);
    coordinateButtonGroup->setId(haEquatorialCheckObject, HA_BUTTON_ID);
    connect(coordinateButtonGroup, &QButtonGroup::idPressed, [this](int id)
    {
        updateTargetDisplay(id);
    });

    // coordinate data changes
    connect(targetRATextObject, &dmsBox::editingFinished, this, &MountTargetWidget::updateTarget);
    connect(targetDETextObject, &dmsBox::editingFinished, this, &MountTargetWidget::updateTarget);

    // GOTO
    connect(gotoButtonObject, &QPushButton::clicked, this, &MountTargetWidget::processSlew);
    // SYNC
    connect(syncButtonObject, &QPushButton::clicked, this, &MountTargetWidget::processSync);
    // Find
    connect(findButtonObject, &QPushButton::clicked, this, &MountTargetWidget::findTarget);
}

bool MountTargetWidget::processCoords(dms &ra, dms &de)
{
    // do nothing if no target is set
    if (currentTarget.isNull())
        return false;

    ra = currentTarget->ra();
    de = currentTarget->dec();

    return true;
}

void MountTargetWidget::setTargetPosition(SkyPoint *target)
{
    // we assume that JNow is set, update all other coordinates
    updateJ2000Coordinates(target);
    currentTarget.reset(target);
    updateTargetDisplay();
}

void MountTargetWidget::setTargetName(const QString &name)
{
    targetTextObject->setText(name);
}

bool MountTargetWidget::raDecToAzAlt(QString qsRA, QString qsDec)
{
    dms RA, Dec;

    if (!RA.setFromString(qsRA, false) || !Dec.setFromString(qsDec, true))
        return false;

    SkyPoint targetCoord(RA, Dec);

    targetCoord.EquatorialToHorizontal(KStarsData::Instance()->lst(),
                                       KStarsData::Instance()->geo()->lat());

    targetRATextObject->setProperty("text", targetCoord.alt().toDMSString());
    targetDETextObject->setProperty("text", targetCoord.az().toDMSString());

    return true;
}

bool MountTargetWidget::raDecToHaDec(QString qsRA)
{
    dms RA;

    if (!RA.setFromString(qsRA, false))
        return false;

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());

    dms HA = (lst - RA + dms(360.0)).reduce();

    QChar sgn('+');
    if (HA.Hours() > 12.0)
    {
        HA.setH(24.0 - HA.Hours());
        sgn = '-';
    }

    targetRATextObject->setProperty("text", QString("%1%2").arg(sgn).arg(HA.toHMSString()));

    return true;
}

bool MountTargetWidget::azAltToRaDec(QString qsAz, QString qsAlt)
{
    dms Az, Alt;

    if (!Az.setFromString(qsAz, true) || !Alt.setFromString(qsAlt, true))
        return false;

    SkyPoint targetCoord;
    targetCoord.setAz(Az);
    targetCoord.setAlt(Alt);

    targetCoord.HorizontalToEquatorial(KStars::Instance()->data()->lst(),
                                       KStars::Instance()->data()->geo()->lat());

    targetRATextObject->setProperty("text", targetCoord.ra().toHMSString());
    targetDETextObject->setProperty("text", targetCoord.dec().toDMSString());

    return true;
}

bool MountTargetWidget::azAltToHaDec(QString qsAz, QString qsAlt)
{
    dms Az, Alt;

    if (!Az.setFromString(qsAz, true) || !Alt.setFromString(qsAlt, true))
        return false;

    SkyPoint targetCoord;
    targetCoord.setAz(Az);
    targetCoord.setAlt(Alt);

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());

    targetCoord.HorizontalToEquatorial(&lst, KStars::Instance()->data()->geo()->lat());

    dms HA = (lst - targetCoord.ra() + dms(360.0)).reduce();

    QChar sgn('+');
    if (HA.Hours() > 12.0)
    {
        HA.setH(24.0 - HA.Hours());
        sgn = '-';
    }

    targetRATextObject->setProperty("text", QString("%1%2").arg(sgn).arg(HA.toHMSString()));
    targetDETextObject->setProperty("text", targetCoord.dec().toDMSString());

    return true;
}

bool MountTargetWidget::haDecToRaDec(QString qsHA)
{
    dms HA;

    if (!HA.setFromString(qsHA, false))
        return false;

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    dms RA = (lst - HA + dms(360.0)).reduce();

    targetRATextObject->setProperty("text", RA.toHMSString());

    return true;
}

bool MountTargetWidget::haDecToAzAlt(QString qsHA, QString qsDec)
{
    dms HA, Dec;

    if (!HA.setFromString(qsHA, false) || !Dec.setFromString(qsDec, true))
        return false;

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    dms RA = (lst - HA + dms(360.0)).reduce();

    SkyPoint targetCoord;
    targetCoord.setRA(RA);
    targetCoord.setDec(Dec);

    targetCoord.EquatorialToHorizontal(&lst, KStars::Instance()->data()->geo()->lat());

    targetRATextObject->setProperty("text", targetCoord.alt().toDMSString());
    targetDETextObject->setProperty("text", targetCoord.az().toDMSString());

    return true;
}

void MountTargetWidget::setJ2000Enabled(bool enabled)
{
    m_isJ2000 = enabled;
    updateTargetDisplay();
}

void MountTargetWidget::processSlew()
{
    dms ra, de;
    if (processCoords(ra, de))
        emit slew(ra.Hours(), de.Degrees());
}

void MountTargetWidget::processSync()
{
    dms ra, de;
    if (processCoords(ra, de))
        emit sync(ra.Hours(), de.Degrees());
}

void MountTargetWidget::findTarget()
{
    if (FindDialog::Instance()->execWithParent(Ekos::Manager::Instance()) == QDialog::Accepted)
    {
        auto object = FindDialog::Instance()->targetObject();
        if (object != nullptr)
        {
            auto const data = KStarsData::Instance();
            auto o = object->clone();
            o->updateCoords(data->updateNum(), true, data->geo()->lat(), data->lst(), false);
            o->EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

            equatorialCheckObject->setProperty("checked", true);

            targetTextObject->setProperty("text", o->name());
            currentTarget.reset(o);
            updateTargetDisplay();
        }
    }

}

void MountTargetWidget::updateTargetDisplay(int id, SkyPoint *target)
{
    // use current target if none is set
    if (target == nullptr)
        target = currentTarget.get();
    if (id < 0)
        id = coordinateButtonGroup->checkedId();

    switch (id)
    {
        case HOR_BUTTON_ID:
            targetRALabel->setText("AL:");
            targetDECLabel->setText("AZ:");
            targetRATextObject->setUnits(dmsBox::DEGREES);
            if (target != nullptr)
            {
                targetRATextObject->setText(target->alt().toDMSString());
                targetDETextObject->setText(target->az().toDMSString());
            }
            break;
        case HA_BUTTON_ID:
            targetRALabel->setText("HA:");
            targetDECLabel->setText("DE:");
            targetRATextObject->setUnits(dmsBox::HOURS);
            if (target != nullptr)
            {
                dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
                dms HA = (lst - target->ra() + dms(360.0)).reduce();

                QChar sgn('+');
                if (HA.Hours() > 12.0)
                {
                    HA.setH(24.0 - HA.Hours());
                    sgn = '-';
                }

                targetRATextObject->setProperty("text", QString("%1%2").arg(sgn).arg(HA.toHMSString()));
                targetDETextObject->setProperty("text", dms(90 - target->altRefracted().Degrees()).toDMSString());
            }
            break;
        case EQ_BUTTON_ID:
            targetRALabel->setText("RA:");
            targetDECLabel->setText("DE:");
            targetRATextObject->setUnits(dmsBox::HOURS);
            if (target != nullptr)
            {
                if (m_isJ2000)
                {
                    targetRATextObject->setText(target->ra0().toHMSString());
                    targetDETextObject->setText(target->dec0().toDMSString());
                }
                else
                {
                    targetRATextObject->setText(target->ra().toHMSString());
                    targetDETextObject->setText(target->dec().toDMSString());
                }
            }
            break;
        default:
            // do nothing
            break;
    }
}

bool MountTargetWidget::updateTarget()
{
    bool raOK = false, deOK = false;
    SkyPoint *currentCoords = nullptr;
    if (horizontalCheckObject->property("checked").toBool())
    {
        dms at = targetRATextObject->createDms(&raOK);
        dms az = targetDETextObject->createDms(&deOK);
        if (raOK == false || deOK == false)
            return false;

        currentCoords = new SkyPoint();
        currentCoords->setAz(az);
        currentCoords->setAlt(at);
        currentCoords->HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());
        // calculate J2000 coordinates
        updateJ2000Coordinates(currentCoords, true, false);
    }
    else if (haEquatorialCheckObject->property("checked").toBool())
    {
        dms de = targetDETextObject->createDms(&deOK);
        dms ha = targetRATextObject->createDms(&raOK);
        if (raOK == false || deOK == false)
            return false;

        currentCoords = new SkyPoint();
        currentCoords->setDec(de);
        dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
        currentCoords->setRA((lst - ha + dms(360.0)).reduce());
        // calculate J2000 coordinates
        updateJ2000Coordinates(currentCoords);
    }
    else
    {
        dms ra = targetRATextObject->createDms(&raOK);
        dms de = targetDETextObject->createDms(&deOK);
        if (raOK == false || deOK == false)
            return false;

        currentCoords = new SkyPoint();
        // set both JNow and J2000 coordinates
        if (m_isJ2000)
        {
            currentCoords->setRA0(ra);
            currentCoords->setDec0(de);
            // update JNow coordinates
            currentCoords->apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());
        }
        else
        {
            currentCoords->setRA(ra);
            currentCoords->setDec(de);
            // calculate J2000 coordinates
            updateJ2000Coordinates(currentCoords);
        }
        currentCoords->EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
    }

    currentTarget.reset(currentCoords);
    updateTargetDisplay();
    return true;
}

void MountTargetWidget::updateJ2000Coordinates(SkyPoint * coords, bool updateJ2000, bool updateHorizontal)
{
    // do nothing for empty coordinates
    if (coords == nullptr)
        return;

    if (updateJ2000)
    {
        SkyPoint J2000Coord(coords->ra(), coords->dec());
        J2000Coord.catalogueCoord(KStars::Instance()->data()->ut().djd());
        coords->setRA0(J2000Coord.ra());
        coords->setDec0(J2000Coord.dec());
    }

    if (updateHorizontal)
        coords->EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
}

} // namespace
