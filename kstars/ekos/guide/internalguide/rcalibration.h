/*  Ekos guide tool
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "gmath.h"

#include "../guide.h"
#include "ui_rcalibration.h"
#include "fitsviewer/fitsview.h"

#include <QPointer>

typedef struct
{
    bool two_axis;
    bool auto_mode;
    int dift_time;
    int frame_count;
} calibrationparams_t;

class internalCalibration : public QWidget
{
    Q_OBJECT

  public:
    explicit internalCalibration(cgmath *mathObject, Ekos::Guide *parent = 0);
    ~internalCalibration();

    bool setVideoParams(int vid_wd, int vid_ht);
    void update_reticle_pos(double x, double y);
    void setMathObject(cgmath *math);

    void setCalibrationTwoAxis(bool enable);
    void setCalibrationAutoStar(bool enable);
    void setCalibrationAutoSquareSize(bool enable);
    void setCalibrationDarkFrame(bool enable);
    void setCalibrationPulseDuration(int pulseDuration);

    // 2015-09-05 return false in case of auto star selection because we don't want the guide module to do any processing
    // otherwise return true
    bool setImageView(FITSView *image);

    double getReticleAngle() { return ui.spinBox_ReticleAngle->value(); }

    bool isCalibrating();
    bool isCalibrationComplete() { return (calibrationStage == CAL_FINISH || calibrationStage == CAL_ERROR); }
    bool isCalibrationSuccessful() { return (calibrationStage == CAL_FINISH); }

    bool useAutoStar() { return ui.autoStarCheck->isChecked(); }
    bool useAutoSquareSize() { return ui.autoSquareSizeCheck->isChecked(); }
    bool useTwoAxis() { return ui.twoAxisCheck->isChecked(); }

    void processCalibration();
    CalibrationStage getCalibrationStage() { return calibrationStage; }
    void reset();

    bool startCalibration();
    bool stopCalibration();

  protected slots:
    void onEnableAutoMode(int state);
    void onReticleXChanged(double val);
    void onReticleYChanged(double val);
    void onReticleAngChanged(double val);
    void onStartReticleCalibrationButtonClick();
    void toggleAutoSquareSize(bool enable);

  public slots:
    void capture();
    void trackingStarSelected(int x, int y);

  signals:
    void newStatus(Ekos::GuideState state);

  private:
    Ui::rcalibrationClass ui;
};
