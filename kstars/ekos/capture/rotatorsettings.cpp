/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include "rotatorsettings.h"
#include "Options.h"
#include "fov.h"
#include "kstarsdata.h"
#include "ekos/auxiliary/solverutils.h"

#include <indicom.h>
#include <basedevice.h>
#include <cmath>

RotatorSettings::RotatorSettings(QWidget *parent) : QDialog(parent)
{
    setupUi(this);

    rotatorGauge->setFormat("%v");
    rotatorGauge->setMinimum(0);
    rotatorGauge->setMaximum(360);

    connect(rawAngleSlider, &QSlider::valueChanged, this, [this](int angle)
    {
        rawAngleSpin->setValue(angle);
    });

    connect(positionAngleSlider, &QSlider::valueChanged, this, [this](int angle)
    {
        positionAngleSpin->setValue(angle);
    });

    connect(positionAngleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RotatorSettings::syncPA);
}


void RotatorSettings::setCurrentRawAngle(double value)
{
    rawAngleOut->setText(QString::number(value, 'f', 3));
    rotatorGauge->setValue(value);
    if (m_RawAngleSynced == false)
    {
        rawAngleSlider->setValue(value);
        m_RawAngleSynced = true;
    }
    updatePA();
}

double RotatorSettings::adjustedOffset()
{
    auto offset = Options::pAOffset();
    auto calibrationPierSide = static_cast<ISD::Mount::PierSide>(Options::pAPierSide());
    if (calibrationPierSide != ISD::Mount::PIER_UNKNOWN &&
            m_PierSide != ISD::Mount::PIER_UNKNOWN &&
            calibrationPierSide != m_PierSide)
        offset = range360(offset + 180);
    return offset;
}

void RotatorSettings::updatePA()
{
    // 1. PA = (RawAngle * Multiplier) - Offset
    // 2. Offset = (RawAngle * Multiplier) - PA
    // 3. RawAngle = (Offset + PA) / Multiplier
    double PA = SolverUtils::rangePA((rotatorGauge->value() * Options::pAMultiplier()) - adjustedOffset());
    positionAngleOut->setText(QString::number(PA, 'f', 3));
    if (m_PositionAngleSynced == false)
    {
        positionAngleSlider->setValue(PA);
        m_PositionAngleSynced = true;
    }
}

void RotatorSettings::refresh()
{
    updatePA();
}

void RotatorSettings::setCurrentPierSide(ISD::Mount::PierSide side)
{
    m_PierSide = side;
    updatePA();
};

void RotatorSettings::syncPA(double PA)
{
    for (auto oneFOV : KStarsData::Instance()->getTransientFOVs())
    {
        // Only change the PA for the sensor FOV
        if (oneFOV->objectName() == "sensor_fov")
        {
            // Make sure that it is always displayed
            if (!Options::showSensorFOV())
            {
                Options::setShowSensorFOV(true);
                oneFOV->setProperty("visible", true);
            }

            // JM 2020-10-15
            // While we have the correct Position Angle
            // Because Ekos reads frame TOP-BOTTOM instead of the BOTTOM-TOP approach
            // used by astrometry, the PA is always 180 degree off. To avoid confusion to the user
            // the PA is drawn REVERSED to show the *expected* frame. However, the final PA is
            // the "correct" PA as expected by astrometry.
            //double drawnPA = PA >= 0 ? (PA - 180) : (PA + 180);
            oneFOV->setPA(PA);
            break;
        }
    }
}
