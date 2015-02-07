/*  Ekos guide tool
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef RCALIBRATION_H
#define RCALIBRATION_H

#include "gmath.h"
#include "../guide.h"
#include "ui_rcalibration.h"

typedef struct
{
	bool two_axis;
	bool auto_mode;
	int  dift_time;
	int  frame_count;
}calibrationparams_t;



class rcalibration: public QWidget
{
    Q_OBJECT

public:
    explicit rcalibration(Ekos::Guide *parent = 0);
    ~rcalibration();

    enum CalibrationStage { CAL_CAPTURE_IMAGE, CAL_SELECT_STAR, CAL_FINISH, CAL_ERROR, CAL_START, CAL_RA_INC, CAL_RA_DEC, CAL_DEC_INC, CAL_DEC_DEC };
    enum CalibrationType { CAL_NONE, CAL_MANUAL, CAL_RA_AUTO, CAL_RA_DEC_AUTO };


    bool setVideoParams( int vid_wd, int vid_ht );
    void update_reticle_pos( double x, double y );
    void setMathObject( cgmath *math );
    void setCalibrationOptions(bool useTwoAxis, bool autoCalibration, bool useDarkFrame);
    void setCalibrationParams(int boxSize, int pulseDuration);
    //void set_ccd(ISD::CCD *ccd);

    void setImage(FITSView *image);

    double getReticleAngle() { return ui.spinBox_ReticleAngle->value();}

    bool isCalibrating();
    bool isAutoCalibration() { return ui.autoCalibrationCheck->isChecked(); }
    bool isCalibrationComplete() { return (calibrationStage == CAL_FINISH || calibrationStage == CAL_ERROR); }
    bool isCalibrationSuccessful() { return (calibrationStage == CAL_FINISH); }
    bool useDarkFrame() { return ui.darkFrameCheck->isChecked(); }
    void processCalibration();
    CalibrationStage getCalibrationStage() { return calibrationStage; }
    void reset();

    bool startCalibration();
    bool stopCalibration();

protected slots:
	void onSquareSizeChanged( int index );
	void onEnableAutoMode( int state );
	void onReticleXChanged( double val );
	void onReticleYChanged( double val );
	void onReticleAngChanged( double val );
	void onStartReticleCalibrationButtonClick();

public slots:
    void capture();
    void guideStarSelected(int x, int y);

signals:
    void calibrationCompleted(bool);

private:

    QPair<double,double> selectAutoStar(FITSView *image);
    void fillInterface( void );
    void calibrateManualReticle( void );
    void calibrateRADECRecticle( bool ra_only ); // 1 or 2-axis calibration

	bool is_started;
	
	calibrationparams_t calibration_params;
	int  axis;
	int  auto_drift_time;
    int turn_back_time;
	double start_x1, start_y1;
	double end_x1, end_y1;
	double start_x2, start_y2;
	double end_x2, end_y2;
    int iterations, dec_iterations;
    double phi;
    Matrix ROT_Z;

	cgmath *pmath;
    Ekos::Guide *pmain_wnd;

    QColor idleColor, okColor, busyColor, alertColor;

    CalibrationStage calibrationStage;
    CalibrationType  calibrationType;


    Ui::rcalibrationClass ui;

};


#endif // RETICLE_CALIBRATION_H
