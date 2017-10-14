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

    void setCurrentAngle(double angle);

    bool isRotationEnforced() { return enforceRotationCheck->isChecked(); }
    void setRotationEnforced(bool enabled) { enforceRotationCheck->setChecked(enabled); }

    double getTargetRotationPA() { return targetPASpin->value(); }
    void setTargetRotationPA(double value) { targetPASpin->setValue(value); }
    double getTargetAngle() { return angleSpin->value(); }
    double getCurrentRotationPA() { return PAOut->text().toDouble(); }

    void setPAMultiplier(double value) { PAMulSpin->setValue(value);}
    void setPAOffset(double value) { PAOffsetSpin->setValue(value);}

    void refresh();

protected slots:
    void gotoAngle();
    void updatePA();
    void setPA();
    void syncPA(double PA);

private:
    ISD::GDInterface *currentRotator { nullptr };    
};
