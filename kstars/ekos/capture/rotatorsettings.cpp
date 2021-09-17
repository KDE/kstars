/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include "rotatorsettings.h"
#include "Options.h"
#include "fov.h"
#include "kstarsdata.h"

#include <indicom.h>
#include <basedevice.h>
#include <cmath>

RotatorSettings::RotatorSettings(QWidget *parent) : QDialog(parent)
{
    setupUi(this);

    rotatorGauge->setFormat("%v");
    rotatorGauge->setMinimum(0);
    rotatorGauge->setMaximum(360);

    connect(angleSlider, &QSlider::valueChanged, this, [this](int angle)
    {
        angleSpin->setValue(angle);
    });

    connect(setAngleB, SIGNAL(clicked()), this, SLOT(gotoAngle()));

    PAMulSpin->setValue(Options::pAMultiplier());
    PAOffsetSpin->setValue(Options::pAOffset());

    connect(setPAB, SIGNAL(clicked()), this, SLOT(setPA()));

    syncFOVPA->setChecked(Options::syncFOVPA());
    connect(syncFOVPA, &QCheckBox::toggled, [this](bool toggled)
    {
        Options::setSyncFOVPA(toggled);
        if (toggled) syncPA(targetPASpin->value());
    });

    connect(enforceRotationCheck, SIGNAL(toggled(bool)), targetPASpin, SLOT(setEnabled(bool)));
    connect(targetPASpin, SIGNAL(valueChanged(double)), this, SLOT(syncPA(double)));
    connect(PAMulSpin, &QSpinBox::editingFinished, this, [this]()
    {
        Options::setPAMultiplier(PAMulSpin->value());
        updatePA();
    }
           );
    connect(PAOffsetSpin, &QSpinBox::editingFinished, this, [this]()
    {
        Options::setPAOffset(PAOffsetSpin->value());
        updatePA();
    });
}

void RotatorSettings::setRotator(ISD::GDInterface *rotator)
{
    currentRotator = rotator;

    connect(currentRotator, &ISD::GDInterface::propertyDefined, [&](INDI::Property * prop)
    {
        if (prop->isNameMatch("ABS_ROTATOR_ANGLE"))
        {
            auto absAngle = prop->getNumber();
            setCurrentAngle(absAngle->at(0)->getValue());
        }
    });
}


void RotatorSettings::gotoAngle()
{
    double angle = angleSpin->value();
    currentRotator->runCommand(INDI_SET_ROTATOR_ANGLE, &angle);
}

void RotatorSettings::setCurrentAngle(double angle)
{
    angleEdit->setText(QString::number(angle, 'f', 3));
    rawAngle->setText(QString::number(angle, 'f', 3));
    rotatorGauge->setValue(angle);
    updatePA();
}

void RotatorSettings::updatePA()
{
    double PA = rotatorGauge->value() * PAMulSpin->value() + PAOffsetSpin->value();
    // Limit PA to -180 to +180
    if (PA > 180)
        PA -= 360;
    if (PA < -180)
        PA += 360;

    //PASpin->setValue(PA);
    PAOut->setText(QString::number(PA, 'f', 3));
}

void RotatorSettings::refresh()
{
    PAMulSpin->setValue(Options::pAMultiplier());
    PAOffsetSpin->setValue(Options::pAOffset());
    updatePA();
}

void RotatorSettings::syncPA(double PA)
{
    if (syncFOVPA->isChecked())
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
                double drawnPA = PA >= 0 ? (PA - 180) : (PA + 180);
                oneFOV->setPA(drawnPA);
                break;
            }
        }
    }
}

void RotatorSettings::setPA()
{
    // PA = RawAngle * Multiplier + Offset
    double rawAngle = (PASpin->value() - PAOffsetSpin->value()) / PAMulSpin->value();
    // Get raw angle (0 to 360) from PA (-180 to +180)
    if (rawAngle < 0)
        rawAngle += 360;
    else if (rawAngle > 360)
        rawAngle -= 360;

    //angleEdit->setText(QString::number(rawAngle, 'f', 3));
    angleSpin->setValue(rawAngle);
    gotoAngle();
}
