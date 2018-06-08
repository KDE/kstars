/*  Ekos guide tool
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "rcalibration.h"

#include "gmath.h"
#include "vect.h"

#include "kspaths.h"
#include "kstars.h"
#include "Options.h"
#include "../ekosmanager.h"
#include "../guide.h"
#include "../fitsviewer/fitsviewer.h"
#include "../fitsviewer/fitsview.h"

#include <KMessageBox>
#include <KLocalizedString>
#include <KNotifications/KNotification>

#include <time.h>
#include <assert.h>

#undef MIN
#undef MAX

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

internalCalibration::internalCalibration(cgmath *mathObject, Ekos::Guide *parent) : QWidget(parent)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    ui.setupUi(this);

    setWindowTitle(i18n("Calibration"));

    pmath = mathObject;

    calibrationStage = CAL_CAPTURE_IMAGE;

    guideModule = parent;

    is_started      = false;
    axis            = GUIDE_RA;
    auto_drift_time = 5;

    start_x1 = start_y1 = 0;
    end_x1 = end_y1 = 0;
    start_x2 = start_y2 = 0;
    end_x2 = end_y2 = 0;

    ui.spinBox_DriftTime->setVisible(true);
    ui.progressBar->setVisible(false);
    ui.spinBox_ReticleAngle->setMaximum(360);

    // connect ui
    connect(ui.spinBox_ReticleX, SIGNAL(valueChanged(double)), this, SLOT(onReticleXChanged(double)));
    connect(ui.spinBox_ReticleY, SIGNAL(valueChanged(double)), this, SLOT(onReticleYChanged(double)));
    connect(ui.spinBox_ReticleAngle, SIGNAL(valueChanged(double)), this, SLOT(onReticleAngChanged(double)));
    connect(ui.pushButton_StartCalibration, SIGNAL(clicked()), this, SLOT(onStartReticleCalibrationButtonClick()));
    connect(ui.autoModeCheck, SIGNAL(stateChanged(int)), this, SLOT(onEnableAutoMode(int)));
    connect(ui.autoStarCheck, SIGNAL(toggled(bool)), this, SLOT(toggleAutoSquareSize(bool)));
    connect(ui.captureB, SIGNAL(clicked()), this, SLOT(capture()));

    ui.autoModeCheck->setChecked(Options::useAutoMode());
    ui.spinBox_Pulse->setValue(Options::calibrationPulseDuration());

    ui.twoAxisCheck->setChecked(Options::useTwoAxis());
    ui.spinBox_DriftTime->setValue(Options::autoModeIterations());
    ui.autoStarCheck->setChecked(Options::autoStar());
    if (ui.autoStarCheck->isChecked())
    {
        ui.autoSquareSizeCheck->setEnabled(true);
        ui.autoSquareSizeCheck->setChecked(Options::autoSquareSize());
    }

    idleColor.setRgb(200, 200, 200);
    okColor    = Qt::green;
    busyColor  = Qt::yellow;
    alertColor = Qt::red;

    fillInterface();
}

internalCalibration::~internalCalibration()
{
}

void internalCalibration::fillInterface(void)
{
    double rx, ry, rang;

    if (!pmath)
        return;

    pmath->getReticleParameters(&rx, &ry, &rang);

    ui.spinBox_ReticleX->setValue(rx);
    ui.spinBox_ReticleY->setValue(ry);
    ui.spinBox_ReticleAngle->setValue(rang);
    ui.progressBar->setValue(0);

    if (isCalibrating() && ui.autoModeCheck->isChecked())
        ui.progressBar->setVisible(true);
    else
        ui.progressBar->setVisible(false);
}

bool internalCalibration::setVideoParams(int vid_wd, int vid_ht)
{
    if (vid_wd <= 0 || vid_ht <= 0)
        return false;

    ui.spinBox_ReticleX->setMaximum((double)vid_wd);
    ui.spinBox_ReticleY->setMaximum((double)vid_ht);

    return true;
}

void internalCalibration::update_reticle_pos(double x, double y)
{
    if (ui.spinBox_ReticleX->value() == x && ui.spinBox_ReticleY->value() == y)
        return;

    ui.spinBox_ReticleX->setValue(x);
    ui.spinBox_ReticleY->setValue(y);
}

void internalCalibration::setMathObject(cgmath *math)
{
    assert(math);
    pmath = math;

    //pmath->calc_and_set_reticle2( 100, 100, 200, 90,   100, 100, 60, 200);
}

void internalCalibration::onEnableAutoMode(int state)
{
    ui.spinBox_DriftTime->setVisible(state == Qt::Checked);
    ui.progressBar->setVisible(state == Qt::Checked);

    ui.autoStarCheck->setEnabled(state == Qt::Checked);
}

void internalCalibration::onReticleXChanged(double val)
{
    double x, y, ang;

    if (!pmath)
        return;

    pmath->getReticleParameters(&x, &y, &ang);
    pmath->setReticleParameters(val, y, ang);
}

void internalCalibration::onReticleYChanged(double val)
{
    double x, y, ang;

    if (!pmath)
        return;

    pmath->getReticleParameters(&x, &y, &ang);
    pmath->setReticleParameters(x, val, ang);
}

void internalCalibration::onReticleAngChanged(double val)
{
    double x, y, ang;

    if (!pmath)
        return;

    pmath->getReticleParameters(&x, &y, &ang);
    pmath->setReticleParameters(x, y, val);
}

void internalCalibration::onStartReticleCalibrationButtonClick()
{
    // Capture final image
    if (calibrationType == CAL_MANUAL && calibrationStage == CAL_START)
    {
        guideModule->capture();
        return;
    }

    if (calibrationStage > CAL_START)
    {
        stopCalibration();
        return;
    }

    startCalibration();
}

bool internalCalibration::stopCalibration()
{
    if (!pmath)
        return false;

    calibrationStage = CAL_ERROR;

    emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);

    reset();

    return true;
}

bool internalCalibration::startCalibration()
{
    if (!pmath)
        return false;

    if (guideModule->isGuiding())
    {
        guideModule->appendLogText(i18n("Cannot calibrate while autoguiding is active."));
        return false;
    }

    if (calibrationStage != CAL_START && ui.autoStarCheck->isChecked())
    {
        capture();
        return true;
    }

    bool ccdInfo = true, scopeInfo = true;
    QString errMsg;
    double ccd_w, ccd_h, g_aperture, g_focal;

    pmath->getGuiderParameters(&ccd_w, &ccd_h, &g_aperture, &g_focal);

    if (ccd_w == 0 || ccd_h == 0)
    {
        errMsg  = "CCD";
        ccdInfo = false;
    }

    if (g_aperture == 0 || g_focal == 0)
    {
        scopeInfo = false;
        if (ccdInfo == false)
            errMsg += " & Telescope";
        else
            errMsg += "Telescope";
    }

    if (ccdInfo == false || scopeInfo == false)
    {
        KMessageBox::error(this, i18n("%1 info are missing. Please set the values in INDI Control Panel.", errMsg));
        return false;
    }

    if (pmath->getImageView())
        disconnect(pmath->getImageView(), SIGNAL(trackingStarSelected(int,int)), this,
                   SLOT(trackingStarSelected(int,int)));

    ui.captureLED->setColor(okColor);
    ui.selectStarLED->setColor(okColor);

    calibrationStage = CAL_START;
    emit newStatus(Ekos::GUIDE_CALIBRATING);

    // Must reset dec swap before we run any calibration procedure!
    guideModule->setDECSwap(false);
    pmath->setDeclinationSwapEnabled(false);

    pmath->setLostStar(false);
    //pmain_wnd->capture();

    Options::setCalibrationPulseDuration(ui.spinBox_Pulse->value());
    Options::setUseAutoMode(ui.autoModeCheck->isChecked());
    Options::setUseTwoAxis(ui.twoAxisCheck->isChecked());
    Options::setAutoModeIterations(ui.spinBox_DriftTime->value());
    Options::setAutoStar(ui.autoStarCheck->isChecked());
    if (ui.autoStarCheck->isChecked())
        Options::setAutoSquareSize(ui.autoSquareSizeCheck->isChecked());

    // manual
    if (ui.autoModeCheck->checkState() != Qt::Checked)
    {
        calibrateManualReticle();
        return true;
    }

    ui.progressBar->setVisible(true);

    // automatic
    if (ui.twoAxisCheck->checkState() == Qt::Checked)
        calibrateRADECRecticle(false);
    else
        calibrateRADECRecticle(true);

    return true;
}

void internalCalibration::processCalibration()
{
    //if (pmath->get_image())
    //guide_frame->setTrackingBox(QRect(pmath-> square_pos.x, square_pos.y, square_size*2, square_size*2));
    //pmath->get_image()->setTrackingBoxSize(QSize(pmath->get_square_size(), pmath->get_square_size()));

    if (pmath->isStarLost())
    {
        calibrationStage = CAL_ERROR;
        emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);

        ui.startCalibrationLED->setColor(alertColor);
        KMessageBox::error(
            nullptr, i18n("Lost track of the guide star. Try increasing the square size or reducing pulse duration."));
        reset();
        return;
    }

    switch (calibrationType)
    {
        case CAL_NONE:
            break;

        case CAL_MANUAL:
            calibrateManualReticle();
            break;

        case CAL_RA_AUTO:
            calibrateRADECRecticle(true);
            break;

        case CAL_RA_DEC_AUTO:
            calibrateRADECRecticle(false);
            break;
    }
}

bool internalCalibration::isCalibrating()
{
    if (calibrationStage >= CAL_START)
        return true;

    return false;
}

void internalCalibration::reset()
{
    is_started = false;
    ui.pushButton_StartCalibration->setText(i18n("Start"));
    ui.startCalibrationLED->setColor(idleColor);
    ui.progressBar->setVisible(false);
    connect(pmath->getImageView(), SIGNAL(trackingStarSelected(int,int)), this, SLOT(trackingStarSelected(int,int)),
            Qt::UniqueConnection);
}

void internalCalibration::calibrateManualReticle(void)
{
    //----- manual mode ----
    // get start point

    calibrationType = CAL_MANUAL;

    if (!is_started)
    {
        if (ui.twoAxisCheck->checkState() == Qt::Checked)
        {
            ui.pushButton_StartCalibration->setText(i18n("Stop GUIDE_RA"));
        }
        else
        {
            ui.pushButton_StartCalibration->setText(i18n("Stop"));
        }
        guideModule->appendLogText(i18n("Drift scope in RA. Press stop when done."));

        calibrationStage = CAL_START;
        pmath->getStarScreenPosition(&start_x1, &start_y1);

        axis       = GUIDE_RA;
        is_started = true;
    }
    else // get end point and calc orientation
    {
        if (ui.twoAxisCheck->checkState() == Qt::Checked)
        {
            if (axis == GUIDE_RA)
            {
                pmath->getStarScreenPosition(&end_x1, &end_y1);

                start_x2 = end_x1;
                start_y2 = end_y1;

                axis = GUIDE_DEC;

                ui.pushButton_StartCalibration->setText(i18n("Stop GUIDE_DEC"));
                guideModule->appendLogText(i18n("Drift scope in DEC. Press Stop when done."));
                return;
            }
            else
            {
                pmath->getStarScreenPosition(&end_x2, &end_y2);
                bool dec_swap = false;
                // calc orientation
                if (pmath->calculateAndSetReticle2D(start_x1, start_y1, end_x1, end_y1, start_x2, start_y2, end_x2,
                                                    end_y2, &dec_swap))
                {
                    fillInterface();
                    if (dec_swap)
                        guideModule->appendLogText(i18n("DEC swap enabled."));
                    else
                        guideModule->appendLogText(i18n("DEC swap disabled."));

                    guideModule->appendLogText(i18n("Calibration completed."));

                    KNotification::event(QLatin1String("CalibrationSuccessful"),
                                         i18n("Guiding calibration completed successfully"));
                    calibrationStage = CAL_FINISH;

                    emit newStatus(Ekos::GUIDE_CALIBRATION_SUCESS);

                    guideModule->setDECSwap(dec_swap);
                }
                else
                {
                    QMessageBox::warning(this, i18n("Error"), i18n("Calibration rejected. Start drift is too short."),
                                         QMessageBox::Ok);
                    is_started       = false;
                    calibrationStage = CAL_ERROR;
                    emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);
                }
            }
        }
        else
        {
            pmath->getStarScreenPosition(&end_x1, &end_y1);

            if (pmath->calculateAndSetReticle1D(start_x1, start_y1, end_x1, end_y1))
            {
                calibrationStage = CAL_FINISH;
                fillInterface();
                guideModule->appendLogText(i18n("Calibration completed."));

                emit newStatus(Ekos::GUIDE_CALIBRATION_SUCESS);

                KNotification::event(QLatin1String("CalibrationSuccessful"),
                                     i18n("Guiding calibration completed successfully"));
            }
            else
            {
                calibrationStage = CAL_ERROR;

                emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);

                QMessageBox::warning(this, i18n("Error"), i18n("Calibration rejected. Start drift is too short."),
                                     QMessageBox::Ok);
                is_started = false;
                KNotification::event(QLatin1String("CalibrationFailed"),
                                     i18n("Guiding calibration failed with errors"));
            }
        }

        reset();
    }
}

void internalCalibration::calibrateRADECRecticle(bool ra_only)
{
    bool auto_term_ok = false;

    if (!pmath)
        return;

    int pulseDuration = ui.spinBox_Pulse->value();
    int totalPulse    = pulseDuration * auto_drift_time;

    if (ra_only)
        calibrationType = CAL_RA_AUTO;
    else
        calibrationType = CAL_RA_DEC_AUTO;

    switch (calibrationStage)
    {
        case CAL_START:
            //----- automatic mode -----
            auto_drift_time = ui.spinBox_DriftTime->value();

            if (ra_only)
                turn_back_time = auto_drift_time * 2 + auto_drift_time / 2;
            else
                turn_back_time = auto_drift_time * 6;
            iterations = 0;

            ui.progressBar->setMaximum(turn_back_time);

            ui.progressBar->setValue(0);

            ui.pushButton_StartCalibration->setText(i18n("Abort"));

            guideModule->appendLogText(i18n("GUIDE_RA drifting forward..."));

            // get start point
            //pmath->get_star_screen_pos( &start_x1, &start_y1 );

            start_x1 = ui.spinBox_ReticleX->value();
            start_y1 = ui.spinBox_ReticleY->value();

            if (Options::guideLogging())
                qDebug() << "Guide: Start X1 " << start_x1 << " Start Y1 " << start_y1;

            guideModule->sendPulse(RA_INC_DIR, pulseDuration);
            if (Options::guideLogging())
                qDebug() << "Guide: Iteration " << iterations << " Direction: " << RA_INC_DIR
                         << " Duration: " << pulseDuration << " ms.";

            iterations++;

            ui.progressBar->setValue(iterations);

            calibrationStage = CAL_RA_INC;

            ui.startCalibrationLED->setColor(busyColor);

            break;

        case CAL_RA_INC:
            guideModule->sendPulse(RA_INC_DIR, pulseDuration);

            if (Options::guideLogging())
            {
                // Star position resulting from LAST guiding pulse to mount
                double cur_x, cur_y;
                pmath->getStarScreenPosition(&cur_x, &cur_y);
                qDebug() << "Guide: Iteration #" << iterations - 1 << ": STAR " << cur_x << "," << cur_y;

                qDebug() << "Guide: Iteration " << iterations << " Direction: " << RA_INC_DIR
                         << " Duration: " << pulseDuration << " ms.";
            }

            iterations++;
            ui.progressBar->setValue(iterations);

            if (iterations == auto_drift_time)
                calibrationStage = CAL_RA_DEC;

            break;

        case CAL_RA_DEC:
        {
            if (iterations == auto_drift_time)
            {
                pmath->getStarScreenPosition(&end_x1, &end_y1);
                if (Options::guideLogging())
                    qDebug() << "Guide: End X1 " << end_x1 << " End Y1 " << end_y1;

                phi   = pmath->calculatePhi(start_x1, start_y1, end_x1, end_y1);
                ROT_Z = RotateZ(-M_PI * phi / 180.0); // derotates...

                guideModule->appendLogText(i18n("GUIDE_RA drifting reverse..."));
            }

            //----- Z-check (new!) -----
            double cur_x, cur_y;
            pmath->getStarScreenPosition(&cur_x, &cur_y);

            Vector star_pos = Vector(cur_x, cur_y, 0) - Vector(start_x1, start_y1, 0);
            star_pos.y      = -star_pos.y;
            star_pos        = star_pos * ROT_Z;

            if (Options::guideLogging())
                qDebug() << "Guide: Star x pos is " << star_pos.x << " from original point.";

            // start point reached... so exit
            if (star_pos.x < 1.5)
            {
                pmath->performProcessing();
                auto_term_ok = true;
            }

            //----- Z-check end -----

            if (!auto_term_ok)
            {
                if (iterations < turn_back_time)
                {
                    guideModule->sendPulse(RA_DEC_DIR, pulseDuration);

                    if (Options::guideLogging())
                    {
                        // Star position resulting from LAST guiding pulse to mount
                        double cur_x, cur_y;
                        pmath->getStarScreenPosition(&cur_x, &cur_y);
                        qDebug() << "Guide: Iteration #" << iterations - 1 << ": STAR " << cur_x << "," << cur_y;

                        qDebug() << "Guide: Iteration " << iterations << " Direction: " << RA_INC_DIR
                                 << " Duration: " << pulseDuration << " ms.";
                    }

                    iterations++;

                    ui.progressBar->setValue(iterations);
                    break;
                }

                calibrationStage = CAL_ERROR;

                emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);

                if (ui.autoStarCheck->isChecked())
                    guideModule->appendLogText(i18np("GUIDE_RA: Scope cannot reach the start point after %1 iteration. "
                                                     "Possible mount or drive problems...",
                                                     "GUIDE_RA: Scope cannot reach the start point after %1 "
                                                     "iterations. Possible mount or drive problems...",
                                                     turn_back_time));
                else
                    QMessageBox::warning(this, i18n("Warning"),
                                         i18np("GUIDE_RA: Scope cannot reach the start point after %1 iteration. "
                                               "Possible mount or drive problems...",
                                               "GUIDE_RA: Scope cannot reach the start point after %1 iterations. "
                                               "Possible mount or drive problems...",
                                               turn_back_time),
                                         QMessageBox::Ok);

                KNotification::event(QLatin1String("CalibrationFailed"),
                                     i18n("Guiding calibration failed with errors"));
                reset();
                break;
            }

            if (ra_only == false)
            {
                calibrationStage = CAL_DEC_INC;
                start_x2         = cur_x;
                start_y2         = cur_y;

                if (Options::guideLogging())
                    qDebug() << "Guide: Start X2 " << start_x2 << " start Y2 " << start_y2;

                guideModule->sendPulse(DEC_INC_DIR, pulseDuration);

                if (Options::guideLogging())
                {
                    // Star position resulting from LAST guiding pulse to mount
                    double cur_x, cur_y;
                    pmath->getStarScreenPosition(&cur_x, &cur_y);
                    qDebug() << "Guide: Iteration #" << iterations - 1 << ": STAR " << cur_x << "," << cur_y;

                    qDebug() << "Guide: Iteration " << iterations << " Direction: " << RA_INC_DIR
                             << " Duration: " << pulseDuration << " ms.";
                }

                iterations++;
                dec_iterations = 1;
                ui.progressBar->setValue(iterations);
                guideModule->appendLogText(i18n("GUIDE_DEC drifting forward..."));
                break;
            }
            // calc orientation
            if (pmath->calculateAndSetReticle1D(start_x1, start_y1, end_x1, end_y1, totalPulse))
            {
                calibrationStage = CAL_FINISH;
                fillInterface();
                guideModule->appendLogText(i18n("Calibration completed."));

                emit newStatus(Ekos::GUIDE_CALIBRATION_SUCESS);

                ui.startCalibrationLED->setColor(okColor);
                KNotification::event(QLatin1String("CalibrationSuccessful"),
                                     i18n("Guiding calibration completed successfully"));
                //if (ui.autoStarCheck->isChecked())
                //guideModule->selectAutoStar();
            }
            else
            {
                if (ui.autoStarCheck->isChecked())
                    guideModule->appendLogText(i18n("Calibration rejected. Star drift is too short."));
                else
                    QMessageBox::warning(this, i18n("Error"), i18n("Calibration rejected. Star drift is too short."),
                                         QMessageBox::Ok);
                ui.startCalibrationLED->setColor(alertColor);
                calibrationStage = CAL_ERROR;

                emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);

                KNotification::event(QLatin1String("CalibrationFailed"),
                                     i18n("Guiding calibration failed with errors"));
            }

            reset();
            break;
        }

        case CAL_DEC_INC:
            guideModule->sendPulse(DEC_INC_DIR, pulseDuration);

            if (Options::guideLogging())
            {
                // Star position resulting from LAST guiding pulse to mount
                double cur_x, cur_y;
                pmath->getStarScreenPosition(&cur_x, &cur_y);
                qDebug() << "Guide: Iteration #" << iterations - 1 << ": STAR " << cur_x << "," << cur_y;

                qDebug() << "Guide: Iteration " << iterations << " Direction: " << RA_INC_DIR
                         << " Duration: " << pulseDuration << " ms.";
            }

            iterations++;
            dec_iterations++;
            ui.progressBar->setValue(iterations);

            if (dec_iterations == auto_drift_time)
                calibrationStage = CAL_DEC_DEC;

            break;

        case CAL_DEC_DEC:
        {
            if (dec_iterations == auto_drift_time)
            {
                pmath->getStarScreenPosition(&end_x2, &end_y2);
                if (Options::guideLogging())
                    qDebug() << "Guide: End X2 " << end_x2 << " End Y2 " << end_y2;

                phi   = pmath->calculatePhi(start_x2, start_y2, end_x2, end_y2);
                ROT_Z = RotateZ(-M_PI * phi / 180.0); // derotates...

                guideModule->appendLogText(i18n("GUIDE_DEC drifting reverse..."));
            }

            //----- Z-check (new!) -----
            double cur_x, cur_y;
            pmath->getStarScreenPosition(&cur_x, &cur_y);

            //pmain_wnd->appendLogText(i18n("GUIDE_DEC running back...");

            if (Options::guideLogging())
                qDebug() << "Guide: Cur X2 " << cur_x << " Cur Y2 " << cur_y;

            Vector star_pos = Vector(cur_x, cur_y, 0) - Vector(start_x2, start_y2, 0);
            star_pos.y      = -star_pos.y;
            star_pos        = star_pos * ROT_Z;

            if (Options::guideLogging())
                qDebug() << "Guide: start Pos X " << star_pos.x << " from original point.";

            // start point reached... so exit
            if (star_pos.x < 1.5)
            {
                pmath->performProcessing();
                auto_term_ok = true;
            }

            //----- Z-check end -----

            if (!auto_term_ok)
            {
                if (iterations < turn_back_time)
                {
                    guideModule->sendPulse(DEC_DEC_DIR, pulseDuration);

                    if (Options::guideLogging())
                    {
                        // Star position resulting from LAST guiding pulse to mount
                        double cur_x, cur_y;
                        pmath->getStarScreenPosition(&cur_x, &cur_y);
                        qDebug() << "Guide: Iteration #" << iterations - 1 << ": STAR " << cur_x << "," << cur_y;

                        qDebug() << "Guide: Iteration " << iterations << " Direction: " << RA_INC_DIR
                                 << " Duration: " << pulseDuration << " ms.";
                    }

                    iterations++;
                    dec_iterations++;

                    ui.progressBar->setValue(iterations);
                    break;
                }

                calibrationStage = CAL_ERROR;

                emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);

                if (ui.autoStarCheck->isChecked())
                    guideModule->appendLogText(i18np("GUIDE_DEC: Scope cannot reach the start point after %1 "
                                                     "iteration.\nPossible mount or drive problems...",
                                                     "GUIDE_DEC: Scope cannot reach the start point after %1 "
                                                     "iterations.\nPossible mount or drive problems...",
                                                     turn_back_time));
                else
                    QMessageBox::warning(this, i18n("Warning"),
                                         i18np("GUIDE_DEC: Scope cannot reach the start point after %1 "
                                               "iteration.\nPossible mount or drive problems...",
                                               "GUIDE_DEC: Scope cannot reach the start point after %1 "
                                               "iterations.\nPossible mount or drive problems...",
                                               turn_back_time),
                                         QMessageBox::Ok);

                KNotification::event(QLatin1String("CalibrationFailed"),
                                     i18n("Guiding calibration failed with errors"));
                reset();
                break;
            }

            bool swap_dec = false;
            // calc orientation
            if (pmath->calculateAndSetReticle2D(start_x1, start_y1, end_x1, end_y1, start_x2, start_y2, end_x2, end_y2,
                                                &swap_dec, totalPulse))
            {
                calibrationStage = CAL_FINISH;
                fillInterface();
                if (swap_dec)
                    guideModule->appendLogText(i18n("DEC swap enabled."));
                else
                    guideModule->appendLogText(i18n("DEC swap disabled."));
                guideModule->appendLogText(i18n("Calibration completed."));

                emit newStatus(Ekos::GUIDE_CALIBRATION_SUCESS);

                ui.startCalibrationLED->setColor(okColor);
                guideModule->setDECSwap(swap_dec);

                KNotification::event(QLatin1String("CalibrationSuccessful"),
                                     i18n("Guiding calibration completed successfully"));

                //if (ui.autoStarCheck->isChecked())
                //guideModule->selectAutoStar();
            }
            else
            {
                if (ui.autoStarCheck->isChecked())
                    guideModule->appendLogText(i18n("Calibration rejected. Star drift is too short."));
                else
                    QMessageBox::warning(this, i18n("Error"), i18n("Calibration rejected. Star drift is too short."),
                                         QMessageBox::Ok);

                emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);

                ui.startCalibrationLED->setColor(alertColor);
                calibrationStage = CAL_ERROR;
                KNotification::event(QLatin1String("CalibrationFailed"),
                                     i18n("Guiding calibration failed with errors"));
            }

            reset();

            break;
        }

        default:
            break;
    }
}

void internalCalibration::trackingStarSelected(int x, int y)
{
    //int square_size = guide_squares[pmath->getSquareIndex()].size;

    pmath->setReticleParameters(x, y, ui.spinBox_ReticleAngle->value());
    //pmath->moveSquare(x-square_size/(2*pmath->getBinX()), y-square_size/(2*pmath->getBinY()));

    if (calibrationStage == CAL_FINISH)
        return;

    ui.selectStarLED->setColor(okColor);

    calibrationStage = CAL_START;

    ui.pushButton_StartCalibration->setEnabled(true);

    QVector3D starCenter = guideModule->getStarPosition();
    starCenter.setX(x);
    starCenter.setY(y);
    guideModule->setStarPosition(starCenter, true);

    if (ui.autoStarCheck->isChecked())
        startCalibration();
}

void internalCalibration::capture()
{
    if (isCalibrating())
        stopCalibration();

    calibrationStage = CAL_CAPTURE_IMAGE;

    if (guideModule->capture())
    {
        ui.captureLED->setColor(busyColor);
        guideModule->appendLogText(i18n("Capturing image..."));
    }
    else
    {
        ui.captureLED->setColor(alertColor);
        calibrationStage = CAL_ERROR;
        emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);
    }
}

bool internalCalibration::setImageView(FITSView *image)
{
    guideFrame = image;

    switch (calibrationStage)
    {
        case CAL_CAPTURE_IMAGE:
        case CAL_SELECT_STAR:
        {
            guideModule->appendLogText(i18n("Image captured..."));

            ui.captureLED->setColor(okColor);
            calibrationStage = CAL_SELECT_STAR;
            ui.selectStarLED->setColor(busyColor);

            FITSData *image_data = guideFrame->getImageData();

            setVideoParams(image_data->getWidth(), image_data->getHeight());

            if (ui.autoStarCheck->isChecked())
            {
                bool rc = guideModule->selectAutoStar();

                if (rc == false)
                {
                    guideModule->appendLogText(
                        i18n("Failed to automatically select a guide star. Please select a guide star..."));
                    connect(guideFrame, SIGNAL(trackingStarSelected(int,int)), this,
                            SLOT(trackingStarSelected(int,int)), Qt::UniqueConnection);
                    return true;
                }
                else
                    trackingStarSelected(guideModule->getStarPosition().x(), guideModule->getStarPosition().y());
                return false;
            }
            else
            {
                connect(guideFrame, SIGNAL(trackingStarSelected(int,int)), this, SLOT(trackingStarSelected(int,int)),
                        Qt::UniqueConnection);
            }
        }
        break;

        default:
            break;
    }

    return true;
}

void internalCalibration::setCalibrationTwoAxis(bool enable)
{
    ui.twoAxisCheck->setChecked(enable);
}

void internalCalibration::setCalibrationAutoStar(bool enable)
{
    ui.autoStarCheck->setChecked(enable);
}

void internalCalibration::setCalibrationAutoSquareSize(bool enable)
{
    ui.autoSquareSizeCheck->setChecked(enable);
}

void internalCalibration::setCalibrationPulseDuration(int pulseDuration)
{
    ui.spinBox_Pulse->setValue(pulseDuration);
}

void internalCalibration::toggleAutoSquareSize(bool enable)
{
    ui.autoSquareSizeCheck->setEnabled(enable);
}
