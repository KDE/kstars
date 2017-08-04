/*  Rotator Settings
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#include "rotatorsettings.h"

#include <QDebug>

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
    rotatorGauge->setValue(angle);
}
