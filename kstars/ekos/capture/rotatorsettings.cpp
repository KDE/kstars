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
#include "ekos/manager.h"
#include "indi/indirotator.h"
#include <indicom.h>
#include <basedevice.h>
#include <cmath>
#include "capture.h"
#include "ekos/align/align.h"
#include "ekos/capture/capturedeviceadaptor.h"
#include "ekos/align/opsalign.h"
#include "ekos/auxiliary/rotatorutils.h"

#include "ekos_capture_debug.h"

RotatorSettings::RotatorSettings(QWidget *parent) : QDialog(parent)
{
    setupUi(this);

    connect(this, &RotatorSettings::newLog, Ekos::Manager::Instance()->captureModule(), &Ekos::Capture::appendLogText);
    connect(RotatorUtils::Instance(), &RotatorUtils::changedPierside, this, &RotatorSettings::updateGaugeZeroPos);
    connect(FlipPolicy, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RotatorSettings::setFlipPolicy);
    connect(AlignOptions,  &QPushButton::clicked, this, &RotatorSettings::showAlignOptions);

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

    // Position Angle Gauge
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

void RotatorSettings::initRotator(const QString &train, const QSharedPointer<Ekos::CaptureDeviceAdaptor> CaptureDA,
                                  ISD::Rotator *device)
{
    m_CaptureDA = CaptureDA;
    RotatorUtils::Instance()->initRotatorUtils(train);

    m_Rotator = device;
    RotatorName->setText(m_Rotator->getDeviceName());
    updateFlipPolicy(Options::astrometryFlipRotationAllowed());
    // Give getState() a second
    QTimer::singleShot(1000, [ = ]
    {
        if (m_CaptureDA->getRotatorAngleState() < IPS_BUSY)
        {
            double RAngle = m_CaptureDA->getRotatorAngle();
            updateRotator(RAngle);
            updateGaugeZeroPos(RotatorUtils::Instance()->getMountPierside());
            qCInfo(KSTARS_EKOS_CAPTURE()) << "Rotator Settings: Initial raw angle is" << RAngle << ".";
            emit newLog(i18n("Initial rotator angle %1° is read in successfully.", RAngle));
        }
        else
            qCWarning(KSTARS_EKOS_CAPTURE()) << "Rotator Settings: Reading initial raw angle failed.";
    });
}

void RotatorSettings::updateRotator(double RAngle)
{
    RotatorAngle->setValue(RAngle);
    double PAngle = RotatorUtils::Instance()->calcCameraAngle(RAngle, false);
    CameraPA->blockSignals(true); // Prevent reaction coupling via user input
    CameraPA->setValue(PAngle);
    CameraPA->blockSignals(false);
    CameraPASlider->setSliderPosition(PAngle * 100); // Prevent rounding to integer
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
    double RAngle = 0;
    if (Pierside == ISD::Mount::PIER_UNKNOWN)
        MountPierside->setStyleSheet("QComboBox {border: 1px solid red;}");
    else
        MountPierside->setStyleSheet("QComboBox {}");
    MountPierside->setCurrentIndex(Pierside);
    if (Pierside == ISD::Mount::PIER_WEST)
        rotatorGauge->setNullPosition(QRoundProgressBar::PositionTop);
    else if (Pierside == ISD::Mount::PIER_EAST)
        rotatorGauge->setNullPosition(QRoundProgressBar::PositionBottom);
    if (Options::astrometryFlipRotationAllowed()) // Preserve rotator raw angle
        RAngle = RotatorAngle->value();
    else // Preserve camera position angle
    {
        RAngle = RotatorUtils::Instance()->calcRotatorAngle(CameraPA->value());
        activateRotator(RAngle);
    }
    updateGauge(RAngle);
    updateRotator(RAngle);
}

void RotatorSettings::setFlipPolicy(const int index)
{
    Ekos::OpsAlign::FlipPriority Priority = static_cast<Ekos::OpsAlign::FlipPriority>(index);
    Ekos::OpsAlign *AlignOptionsModule = Ekos::Manager::Instance()->alignModule()->getAlignOptionsModule();
    if (AlignOptionsModule)
        AlignOptionsModule->setFlipPolicy(Priority);
}

void RotatorSettings::updateFlipPolicy(const bool FlipRotationAllowed)
{
    int i = -1;
    if (FlipRotationAllowed)
        i = static_cast<int>(Ekos::OpsAlign::FlipPriority::ROTATOR_ANGLE);
    else
        i = static_cast<int>(Ekos::OpsAlign::FlipPriority::POSITION_ANGLE);
    FlipPolicy->blockSignals(true); // Prevent reaction coupling
    FlipPolicy->setCurrentIndex(i);
    FlipPolicy->blockSignals(false);
}

void RotatorSettings::showAlignOptions()
{
    KConfigDialog * alignSettings = KConfigDialog::exists("alignsettings");
    if (alignSettings)
    {
        alignSettings->setEnabled(true);
        alignSettings->show();
    }
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

