/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#pragma once

#include "ui_rotatorsettings.h"
#include "indi/indimount.h"
#include "qloggingcategory.h"
#include <QDialog>

namespace Ekos
{
class CaptureDeviceAdaptor;
}

class RotatorSettings : public QDialog, public Ui::RotatorDialog
{
    Q_OBJECT

public:

    explicit RotatorSettings(QWidget *parent);

    void   initRotator(const QString &train, const QSharedPointer<Ekos::CaptureDeviceAdaptor> CaptureDA, ISD::Rotator *device);
    void   updateRotator(double RAngle);
    void   updateGauge(double Angle);
    void   updateGaugeZeroPos(ISD::Mount::PierSide Pierside);
    void   updateFlipPolicy(const bool FlipRotationAllowed);
    /* remove enforceJobPA
    // bool   isRotationEnforced() { return enforceJobPA->isChecked(); }
    // void   setRotationEnforced(bool enabled) { enforceJobPA->setChecked(enabled); }
    */
    double getCameraPA() { return CameraPA->value(); }
    void   setCameraPA(double Angle) { CameraPA->setValue(Angle); }
    void   setPAOffset(double value) { CameraOffset->setValue(value);}
    void   refresh(double PAngle);

private:
    // Capture adaptor instance to access functions
    QSharedPointer<Ekos::CaptureDeviceAdaptor> m_CaptureDA;
    // Rotator Device
    ISD::Rotator *m_Rotator = {nullptr};
    void   setFlipPolicy(const int index);
    void   showAlignOptions();
    void   activateRotator(double Angle);
    void   commitRotatorDirection(bool Reverse);
    void   syncFOV(double PA);

signals:
    void   newLog(const QString &text);
};
