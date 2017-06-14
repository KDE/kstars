/*  Rotator Settings
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#pragma once

#include "ui_rotatorsettings.h"

#include "indi/indistd.h"

#include <QDialog>

class RotatorSettings : public QDialog, public Ui::RotatorDialog
{
    Q_OBJECT
public:
    RotatorSettings(QWidget *parent);

    void setRotator(ISD::GDInterface *rotator);

    void setTicksMinMaxStep(int32_t min, int32_t max, int32_t step);

    void setCurrentTicks(int32_t ticks);

    bool isRotationEnforced() { return enforceRotationCheck->isChecked(); }
    void setRotationEnforced(bool enabled) { enforceRotationCheck->setChecked(enabled); }

    int32_t getTargetRotationTicks() { return plannedTicksSpin->value(); }
    void setTargetRotationTicks(int32_t value) { plannedTicksSpin->setValue(value); }
    double getTargetAngle() { return angleSpin->value(); }

    int32_t getCurrentRotationTicks() { return ticksEdit->text().toInt(); }


protected slots:
    void gotoTicks();
    void gotoAngle();

private:
    ISD::GDInterface *currentRotator=nullptr;
    double ticksPerDegree=0;
};
