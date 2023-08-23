/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
                            2022 Toni Schriber

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/******************************************************************************************************
* In 'rotatorGauge' and 'paGauge' all angles are displayed in viewing direction and positiv CCW.
*******************************************************************************************************/

#include "rotatorsettings.h"
#include "Options.h"
#include "fov.h"
#include "kstarsdata.h"

#include <indicom.h>
#include <basedevice.h>
#include <cmath>


RotatorSettings::RotatorSettings(QWidget *parent) : QDialog(parent)
{
    setupUi(this);

    connect(RotatorUtils::Instance(), &RotatorUtils::changedPierside, this, &RotatorSettings::updateGaugeZeroPos);

    // -- Parameter -> ui file

    // -- Camera position angle
    CameraPA->setKeyboardTracking(false);
    connect(CameraPA, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,  [ = ](double PAngle)
    {
        double RAngle = RotatorUtils::Instance()->calcRotatorAngle(PAngle);
        RotatorAngle->setValue(RAngle);
        syncFOV(PAngle);
        activateRotator(RAngle);
        CameraPASlider->setSliderPosition(PAngle * 100); // Prevent rounding to integer
    });
    connect(CameraPASlider, &QSlider::sliderReleased, this, [this]()
    {
        CameraPA->setValue(CameraPASlider->sliderPosition() / 100.0); // Set position angle
    });
    connect(CameraPASlider, &QSlider::valueChanged, this, [this](int PAngle100)
    {
        double PAngle = PAngle100 / 100;
        paGauge->setValue(-(PAngle)); // Preview cameraPA in gauge
        syncFOV(PAngle); // Preview FOV
    });

    // -- Options
    // enforceJobPA -> header file
    connect(reverseDirection,  &QCheckBox::toggled, this, [ = ](bool toggled)
    {
        commitRotatorDirection(toggled);
    });

    // Rotator Gauge
    rotatorGauge->setFormat("R");   // dummy format
    rotatorGauge->setMinimum(-360); // display in viewing direction
    rotatorGauge->setMaximum(0);

    // Polar Angle Gauge
    paGauge->setFormat("P");        // dummy format
    paGauge->setMinimum(-181);      // display in viewing direction
    paGauge->setMaximum(181);

    // Angle Ruler
    paRuler->plotLayout()->clear();
    QCPPolarAxisAngular *angularAxis = new QCPPolarAxisAngular(paRuler);
    angularAxis->removeRadialAxis(angularAxis->radialAxis());
    QColor TransparentBlack(0, 0, 0, 100);
    QPen Pen(TransparentBlack, 3);
    angularAxis->setBasePen(Pen);
    angularAxis->setTickPen(Pen);
    angularAxis->setSubTickPen(Pen);
    angularAxis->setTickLabels(false);
    angularAxis->setTickLength(10, 10);
    angularAxis->setSubTickLength(5, 5);
    paRuler->plotLayout()->addElement(0, 0, angularAxis);
    paRuler->setBackground(Qt::GlobalColor::transparent);  // transparent background part 1
    paRuler->setAttribute(Qt::WA_OpaquePaintEvent, false); // transparent background part 2
    angularAxis->grid()->setAngularPen(QPen(Qt::GlobalColor::transparent)); // no grid
    paRuler->replot();

    // Parameter Interface
    CameraPA->setMaximum(180.00);   // uniqueness of angle (-180 = 180)
    CameraPA->setMinimum(-179.99);
    RotatorAngle->setButtonSymbols(QAbstractSpinBox::NoButtons);
    CameraOffset->setValue(Options::pAOffset());
    CameraOffset->setButtonSymbols(QAbstractSpinBox::NoButtons);
    MountPierside->setCurrentIndex(ISD::Mount::PIER_UNKNOWN);
    MountPierside->setDisabled(true);  // only show pierside for information
}

void RotatorSettings::initRotator(const QString &train, Ekos::CaptureDeviceAdaptor *CaptureDA, QString RName)
{
    RotatorUtils::Instance()->initRotatorUtils(train);
    // Setting name
    RotatorName->setText(RName);
    // Setting angle & gauge
    m_CaptureDA = CaptureDA;
    RotatorAngle->setValue(CaptureDA->getRotatorAngle());

    // Need to block signal to prevent any cascade effect this change was not triggered by user input.
    CameraPA->blockSignals(true);
    CameraPA->setValue(RotatorUtils::Instance()->calcCameraAngle(RotatorAngle->value(), false));
    CameraPA->blockSignals(false);
    updateGaugeZeroPos(RotatorUtils::Instance()->getMountPierside());
}

void RotatorSettings::updateRotator(double RAngle)
{
    RotatorAngle->setValue(RAngle);
    double PAngle = RotatorUtils::Instance()->calcCameraAngle(RAngle, false);
    // Need to block signal to prevent any cascade effect this change was not triggered by user input.
    CameraPA->blockSignals(true);
    CameraPA->setValue(PAngle);
    CameraPA->blockSignals(false);
    updateGauge(RAngle);
}

void RotatorSettings::updateGauge(double RAngle)
{
    rotatorGauge->setValue(-RAngle); // display in viewing direction
    CurrentRotatorAngle->setText(QString::number(RAngle, 'f', 2));
    paGauge->setValue(-(RotatorUtils::Instance()->calcCameraAngle(RAngle, false)));
}

void RotatorSettings::updateGaugeZeroPos(ISD::Mount::PierSide Pierside)
{
    if (Pierside == ISD::Mount::PIER_UNKNOWN)
    {
        MountPierside->setStyleSheet("QComboBox {border: 1px solid red;}");
    }
    else
    {
        MountPierside->setStyleSheet("QComboBox {}");
    }
    MountPierside->setCurrentIndex(Pierside);
    if (Pierside == ISD::Mount::PIER_WEST)
    {
        rotatorGauge->setNullPosition(QRoundProgressBar::PositionTop);
    }
    else if (Pierside == ISD::Mount::PIER_EAST)
    {
        rotatorGauge->setNullPosition(QRoundProgressBar::PositionBottom);
    }
    double RAngle = RotatorAngle->value();
    CameraPA->blockSignals(true);
    CameraPA->setValue(RotatorUtils::Instance()->calcCameraAngle(RAngle, false));
    CameraPA->blockSignals(false);
    updateGauge(RAngle);
}

void RotatorSettings::activateRotator(double Angle)
{
    m_CaptureDA->setRotatorAngle(Angle);
}

void RotatorSettings::commitRotatorDirection(bool Reverse)
{
    m_CaptureDA->reverseRotator(Reverse);
}

void RotatorSettings::refresh(double PAngle) // Call from setAlignResults() in Module Capture
{
    CameraPA->setValue(PAngle);
    syncFOV(PAngle);
    CameraOffset->setValue(Options::pAOffset());
}

void RotatorSettings::syncFOV(double PA)
{
    for (auto oneFOV : KStarsData::Instance()->getTransientFOVs())
    {
        // Only change the PA for the sensor FOV
        if (oneFOV->objectName() == "sensor_fov")
        {
            // Make sure that it is always displayed
            if (!Options::showSensorFOV())
            {
                Options::setShowSensorFOV(true);
                oneFOV->setProperty("visible", true);
            }

            // JM 2020-10-15
            // While we have the correct Position Angle
            // Because Ekos reads frame TOP-BOTTOM instead of the BOTTOM-TOP approach
            // used by astrometry, the PA is always 180 degree off. To avoid confusion to the user
            // the PA is drawn REVERSED to show the *expected* frame. However, the final PA is
            // the "correct" PA as expected by astrometry.
            //double drawnPA = PA >= 0 ? (PA - 180) : (PA + 180);
            oneFOV->setPA(PA);
            break;
        }
    }
}

