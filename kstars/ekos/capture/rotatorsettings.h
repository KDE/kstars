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
    explicit RotatorSettings(QWidget *parent);

    void setRotator(ISD::GDInterface *rotator);

    void setTicksMinMaxStep(int32_t min, int32_t max, int32_t step);

    void setCurrentTicks(int32_t ticks);
    void setCurrentAngle(double angle);

    bool isRotationEnforced() { return enforceRotationCheck->isChecked(); }
    void setRotationEnforced(bool enabled) { enforceRotationCheck->setChecked(enabled); }

    double getTargetRotationPA() { return plannedPASpin->value(); }
    void setTargetRotationPA(double value) { plannedPASpin->setValue(value); }
    double getTargetAngle() { return angleSpin->value(); }
    double getCurrentRotationPA() { return PASpin->value(); }

    void setPAMultiplier(double value) { PAMulSpin->setValue(value);}
    void setPAOffset(double value) { PAOffsetSpin->setValue(value);}

    int32_t getCurrentRotationTicks() { return ticksEdit->text().toInt(); }



protected slots:
    void gotoTicks();
    void gotoAngle();
    void updatePA();

private:
    ISD::GDInterface *currentRotator { nullptr };    
};
