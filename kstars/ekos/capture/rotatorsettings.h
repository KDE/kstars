/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#pragma once

#include "ui_rotatorsettings.h"
#include "indi/indistd.h"
#include "ekos/capture/capturedeviceadaptor.h"
#include "ekos/auxiliary/rotatorutils.h"
#include "indi/indilistener.h"
#include "ekos/mount/mount.h"
#include "indi/indimount.h"
#include "ekos/ekos.h"

#include <QDialog>

class RotatorSettings : public QDialog, public Ui::RotatorDialog
{
    Q_OBJECT

public:

    explicit RotatorSettings(QWidget *parent);

    void   initRotator(Ekos::CaptureDeviceAdaptor *CaptureDA, QString RotatorName);
    void   updateRotator(double RAngle);
    void   updateGauge(double Angle);
    void   updateGaugeZeroPos(ISD::Mount::PierSide Pierside);
    bool   isRotationEnforced() { return enforceJobPA->isChecked(); }
    void   setRotationEnforced(bool enabled) { enforceJobPA->setChecked(enabled); }
    double getCameraPA() { return CameraPA->value(); }
    void   setCameraPA(double Angle) { CameraPA->setValue(Angle); }
    void   setPAOffset(double value) { CameraOffset->setValue(value);}
    void   refresh(double PAngle);

private:

    void   activateRotator(double Angle);
    void   commitRotatorDirection(bool Reverse);
    void   syncFOV(double PA);

    Ekos::CaptureDeviceAdaptor *m_CaptureDA {nullptr};
    QSharedPointer<RotatorUtils> m_RotatorUtils;

};
