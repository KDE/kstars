/*  Rotator Settings
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "rotatorsettings.h"

#include <indicom.h>

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

    connect(plannedTicksSpin, &QSpinBox::editingFinished, this, [this]() { enforceRotationCheck->setChecked(true);});
}

void RotatorSettings::setRotator(ISD::GDInterface *rotator)
{
    currentRotator = rotator;
}

void RotatorSettings::setTicksMinMaxStep(int32_t min, int32_t max, int32_t step)
{
    ticksSlider->setMinimum(min);
    ticksSlider->setMaximum(max);
    ticksSlider->setSingleStep(step);

    ticksSpin->setMinimum(min);
    ticksSpin->setMaximum(max);
    ticksSpin->setSingleStep(step);

    plannedTicksSpin->setMinimum(min);
    plannedTicksSpin->setMaximum(max);
    plannedTicksSpin->setSingleStep(step);

    ticksPerDegree = max/360.0;
}

void RotatorSettings::gotoTicks()
{
    int32_t ticks = ticksSpin->value();
    currentRotator->runCommand(INDI_SET_ROTATOR, &ticks);
}

void RotatorSettings::gotoAngle()
{
    double a=angleSpin->value();
    double b=rotatorGauge->value();
    double d=fabs(a-b);
    double r=(d > 180) ? 360 - d : d;
    int sign = (a - b >= 0 && a - b <= 180) || (a - b <=-180 && a- b>= -360) ? 1 : -1;
    r *= sign;

    double newTarget = (r+b) * ticksPerDegree;
    if (newTarget < ticksSpin->minimum())
        newTarget -= ticksSpin->minimum();
    else if (newTarget > ticksSpin->maximum())
        newTarget -= ticksSpin->maximum();

    ticksSpin->setValue(newTarget);

    gotoTicks();
}

void RotatorSettings::setCurrentTicks(int32_t ticks)
{
    ticksEdit->setText(QString::number(ticks));
    double angle = range360(ticks / ticksPerDegree);
    angleEdit->setText(QString::number(angle, 'f', 2));

    rotatorGauge->setValue(angle);
}
