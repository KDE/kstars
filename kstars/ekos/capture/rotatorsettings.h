/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
