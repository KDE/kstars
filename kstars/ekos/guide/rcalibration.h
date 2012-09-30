/*  Ekos guide tool
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef RETICLE_CALIBRATION_H
#define RETICLE_CALIBRATION_H

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
    rcalibration(Ekos::Guide *parent = 0);
    ~rcalibration();

    enum CalibrationStage { CAL_CAPTURE_IMAGE, CAL_SELECT_STAR, CAL_FINISH, CAL_ERROR, CAL_START, CAL_RA_INC, CAL_RA_DEC, CAL_DEC_INC, CAL_DEC_DEC };
    enum CalibrationType { CAL_NONE, CAL_MANUAL, CAL_RA_AUTO, CAL_RA_DEC_AUTO };

    bool set_video_params( int vid_wd, int vid_ht );
    void update_reticle_pos( double x, double y );
    void set_math( cgmath *math );
    void set_ccd(ISD::CCD *ccd);

    void set_image(FITSImage *image);

    bool is_calibrating();
    bool is_finished() { return calibrationStage == CAL_FINISH; }
    void process_calibration();

    void reset();

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


private:

    void select_auto_star(FITSImage *image);
	void fill_interface( void );
	void calibrate_reticle_manual( void );
    void calibrate_reticle_by_ra_dec( bool ra_only ); // 1 or 2-axis calibration

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
