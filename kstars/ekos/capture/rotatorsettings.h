/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#pragma once

#include "ui_rotatorsettings.h"
#include "ekos/mount/mount.h"

#include <QDialog>

class RotatorSettings : public QDialog, public Ui::RotatorDialog
{
    Q_OBJECT

public:
    explicit RotatorSettings(QWidget *parent);

    void setTicksMinMaxStep(int32_t min, int32_t max, int32_t step);

    // Current Raw Rotator Angle
    double currentRawAngle() { return rawAngleOut->text().toDouble();}
    void setCurrentRawAngle(double value);

    // Target Raw Rotator Angle
    double targetRawAngle() { return rawAngleSpin->value();}
    void setTargetRawAngle(double value) { rawAngleSlider->setValue(value);}

    // Should we set PA in the job sequence?
    bool isRotationEnforced() { return enforceRotationCheck->isChecked(); }
    void setRotationEnforced(bool enabled) { enforceRotationCheck->setChecked(enabled); }

    // Target Position Angle
    double targetPositionAngle() { return positionAngleSpin->value(); }
    void setTargetPositionAngle(double value) {positionAngleSlider->setValue(value);}

    // Current Position Angle
    double currentPositionAngle() { return positionAngleOut->text().toDouble(); }
    void setCurrentPositionAngle(double value) {positionAngleOut->setText(QString::number(value, 'f', 2));}

    // Pier side
    void setCurrentPierSide(ISD::Mount::PierSide side);
    double adjustedOffset();

    void refresh();

protected slots:
    void updatePA();
    void syncPA(double PA);

private:
    ISD::Mount::PierSide m_PierSide {ISD::Mount::PIER_UNKNOWN};
    bool m_RawAngleSynced {false}, m_PositionAngleSynced {false};

};
