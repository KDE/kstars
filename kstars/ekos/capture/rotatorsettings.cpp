/*  Rotator Settings
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#include "rotatorsettings.h"
#include "Options.h"

#include <indicom.h>
#include <basedevice.h>
#include <cmath>

RotatorSettings::RotatorSettings(QWidget *parent) : QDialog(parent)
{
    setupUi(this);

    rotatorGauge->setFormat("%v");
    rotatorGauge->setMinimum(0);
    rotatorGauge->setMaximum(360);

    connect(ticksSlider, SIGNAL(valueChanged(int)), ticksSpin, SLOT(setValue(int)));
    connect(angleSlider, &QSlider::valueChanged, this, [this](int angle){angleSpin->setValue(angle);});

    connect(setTickB, SIGNAL(clicked()), this, SLOT(gotoTicks()));
    connect(setAngleB, SIGNAL(clicked()), this, SLOT(gotoAngle()));

    PAMulSpin->setValue(Options::pAMultiplier());
    PAOffsetSpin->setValue(Options::pAOffset());

    connect(plannedPASpin, &QSpinBox::editingFinished, this, [this]() { enforceRotationCheck->setChecked(true);});
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

    connect(currentRotator, &ISD::GDInterface::propertyDefined, [&](INDI::Property *prop)
        {
            if (!strcmp(prop->getName(), "ABS_ROTATOR_POSITION"))
            {
                INumberVectorProperty *absPos = prop->getNumber();
                setTicksMinMaxStep(static_cast<int32_t>(absPos->np[0].min), static_cast<int32_t>(absPos->np[0].max), static_cast<int32_t>(absPos->np[0].step));
                setCurrentTicks(static_cast<int32_t>(absPos->np[0].value));
            }
            else if (!strcmp(prop->getName(), "ABS_ROTATOR_ANGLE"))
            {
                INumberVectorProperty *absAngle = prop->getNumber();
                setCurrentAngle(absAngle->np[0].value);
            }
        });
}

void RotatorSettings::setTicksMinMaxStep(int32_t min, int32_t max, int32_t step)
{
    ticksSlider->setMinimum(min);
    ticksSlider->setMaximum(max);
    ticksSlider->setSingleStep(step);

    ticksSpin->setMinimum(min);
    ticksSpin->setMaximum(max);
    ticksSpin->setSingleStep(step);

    //plannedTicksSpin->setMinimum(min);
    //plannedTicksSpin->setMaximum(max);
    //plannedTicksSpin->setSingleStep(step);
}

void RotatorSettings::gotoTicks()
{
    int32_t ticks = ticksSpin->value();
    currentRotator->runCommand(INDI_SET_ROTATOR_TICKS, &ticks);
}

void RotatorSettings::gotoAngle()
{
    double angle = angleSpin->value();
    currentRotator->runCommand(INDI_SET_ROTATOR_ANGLE, &angle);
}

void RotatorSettings::setCurrentTicks(int32_t ticks)
{
    ticksEdit->setText(QString::number(ticks));
}

void RotatorSettings::setCurrentAngle(double angle)
{
    angleEdit->setText(QString::number(angle, 'f', 2));
    rawAngle->setText(QString::number(angle, 'f', 2));
    rotatorGauge->setValue(angle);
    updatePA();
}

void RotatorSettings::updatePA()
{
    double PA = rotatorGauge->value() * PAMulSpin->value() + PAOffsetSpin->value();
    while (PA > 180)
        PA -= 180;
    while (PA < -180)
        PA += 180;

    PASpin->setValue(PA);
}
