/*  Ekos guide tool
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "rcalibration.h"
#include "Options.h"

#include <unistd.h>
#include <time.h>
#include <assert.h>

#include <KMessageBox>

#include "gmath.h"
#include "vect.h"

#include "../guide.h"
#include "../fitsviewer/fitsviewer.h"
#include "../fitsviewer/fitsview.h"

#undef MIN
#undef MAX

#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

rcalibration::rcalibration(Ekos::Guide *parent)
    : QWidget(parent)
{
    int i;

	ui.setupUi(this);

    setWindowTitle(i18n("Calibration"));

	pmath = NULL;

    calibrationStage = CAL_CAPTURE_IMAGE;

    pmain_wnd = parent;

	is_started = false;
    axis = GUIDE_RA;
    auto_drift_time = 5;
	
	start_x1 = start_y1 = 0;
	end_x1 = end_y1 = 0;
	start_x2 = start_y2 = 0;
	end_x2 = end_y2 = 0;

    ui.spinBox_Pulse->setValue( Options::calibrationPulseDuration());
    ui.checkBox_AutoMode->setChecked( Options::useAutoMode() );
    ui.checkBox_TwoAxis->setChecked( Options::useTwoAxis());
    ui.checkBox_DarkFrame->setChecked(Options::useDarkFrame());
    ui.spinBox_DriftTime->setValue( Options::autoModeIterations() );

    ui.spinBox_DriftTime->setVisible( true );
    ui.progressBar->setVisible( false );
	ui.spinBox_ReticleAngle->setMaximum( 360 );

	for( i = 0;guide_squares[i].size != -1;++i )
		ui.comboBox_SquareSize->addItem( QString().setNum( guide_squares[i].size ) );

    ui.comboBox_SquareSize->setCurrentIndex(1);

	// connect ui
	connect( ui.comboBox_SquareSize, 	SIGNAL(activated(int)),    		this, SLOT(onSquareSizeChanged(int)) );
	connect( ui.spinBox_ReticleX, 		SIGNAL(valueChanged(double)),	this, SLOT(onReticleXChanged(double)) );
	connect( ui.spinBox_ReticleY, 		SIGNAL(valueChanged(double)),	this, SLOT(onReticleYChanged(double)) );
	connect( ui.spinBox_ReticleAngle,	SIGNAL(valueChanged(double)),	this, SLOT(onReticleAngChanged(double)) );
    connect( ui.pushButton_StartCalibration, SIGNAL(clicked()), 		this, SLOT(onStartReticleCalibrationButtonClick()) );
	connect( ui.checkBox_AutoMode, 		SIGNAL(stateChanged(int)), 		this, SLOT(onEnableAutoMode(int)) );

    connect (ui.checkBox_DarkFrame, SIGNAL(toggled(bool)), pmain_wnd, SLOT(setUseDarkFrame(bool)));

    connect( ui.captureB, SIGNAL(clicked()), this, SLOT(capture()));

    idleColor.setRgb(200,200,200);
    okColor = Qt::green;
    busyColor = Qt::yellow;
    alertColor = Qt::red;

    fill_interface();
}


rcalibration::~rcalibration()
{

}

void rcalibration::fill_interface( void )
{
 double rx, ry, rang;

 	if( !pmath )
 		return;

 	pmath->get_reticle_params( &rx, &ry, &rang );

 	ui.comboBox_SquareSize->setCurrentIndex( pmath->get_square_index() );
 	ui.spinBox_ReticleX->setValue( rx );
 	ui.spinBox_ReticleY->setValue( ry );
 	ui.spinBox_ReticleAngle->setValue( rang );
 	ui.progressBar->setValue( 0 );

    if (is_calibrating() && ui.checkBox_AutoMode->isChecked())
        ui.progressBar->setVisible(true);
    else
        ui.progressBar->setVisible(false);

}


bool rcalibration::set_video_params( int vid_wd, int vid_ht )
{
	if( vid_wd <= 0 || vid_ht <= 0 )
		return false;

	ui.spinBox_ReticleX->setMaximum( (double)vid_wd );
	ui.spinBox_ReticleY->setMaximum( (double)vid_ht );

 return true;
}


void rcalibration::update_reticle_pos( double x, double y )
{
  	if( ui.spinBox_ReticleX->value() == x && ui.spinBox_ReticleY->value() == y )
  		return;

    ui.spinBox_ReticleX->setValue( x );
	ui.spinBox_ReticleY->setValue( y );
}

void rcalibration::set_math( cgmath *math )
{
	assert( math );
	pmath = math;

	//pmath->calc_and_set_reticle2( 100, 100, 200, 90,   100, 100, 60, 200);
}


void rcalibration::onSquareSizeChanged( int index )
{
	if( !pmath )
		return;

    pmath->resize_square( index );


}


void rcalibration::onEnableAutoMode( int state )
{
	ui.spinBox_DriftTime->setVisible( state == Qt::Checked );
	ui.progressBar->setVisible( state == Qt::Checked );
}


void rcalibration::onReticleXChanged( double val )
{
 double x, y, ang;

	if( !pmath )
		return;

	pmath->get_reticle_params( &x, &y, &ang );
	pmath->set_reticle_params( val, y, ang );



}


void rcalibration::onReticleYChanged( double val )
{
 double x, y, ang;

	if( !pmath )
		return;

	pmath->get_reticle_params( &x, &y, &ang );
	pmath->set_reticle_params( x, val, ang );
}


void rcalibration::onReticleAngChanged( double val )
{
 double x, y, ang;

	if( !pmath )
		return;

	pmath->get_reticle_params( &x, &y, &ang );
	pmath->set_reticle_params( x, y, val );
}


void rcalibration::onStartReticleCalibrationButtonClick()
{

    if (!pmath)
        return;

    if (calibrationStage > CAL_START)
    {
        reset();
        return;
    }

    bool ccdInfo=true, scopeInfo=true;
    QString errMsg;
    double ccd_w, ccd_h, g_aperture, g_focal;

    pmath->get_guider_params(&ccd_w, &ccd_h, &g_aperture, &g_focal);

    if (ccd_w == 0 || ccd_h == 0)
    {
        errMsg = "CCD";
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
            return;
    }

    if (pmath->get_image())
        disconnect(pmath->get_image(), SIGNAL(guideStarSelected(int,int)), this, SLOT(guideStarSelected(int, int)));

    calibrationStage = CAL_START;

    pmath->set_lost_star(false);
    pmain_wnd->capture();

    Options::setCalibrationPulseDuration(ui.spinBox_Pulse->value());
    Options::setUseAutoMode(ui.checkBox_AutoMode->isChecked());
    Options::setUseTwoAxis(ui.checkBox_TwoAxis->isChecked());
    Options::setUseDarkFrame(ui.checkBox_DarkFrame->isChecked());
    Options::setAutoModeIterations(ui.spinBox_DriftTime->value());

	// manual
	if( ui.checkBox_AutoMode->checkState() != Qt::Checked )
	{
		calibrate_reticle_manual();
		return;
	}

    ui.progressBar->setVisible(true);

    pmain_wnd->setUseDarkFrame(ui.checkBox_DarkFrame->isChecked());

	// automatic
	if( ui.checkBox_TwoAxis->checkState() == Qt::Checked )
        calibrate_reticle_by_ra_dec(false);
	else
        calibrate_reticle_by_ra_dec(true);
}

void rcalibration::process_calibration()
{
    if (pmath->get_image())
        pmath->get_image()->setGuideBoxSize(pmath->get_square_size());

    if (pmath->is_lost_star())
    {
        calibrationStage = CAL_ERROR;
        ui.startCalibrationLED->setColor(alertColor);
        KMessageBox::error(NULL, i18n("Lost track of the guide star. Try increasing the square size or reducing pulse duration."));
        reset();
        return;
    }

    switch (calibrationType)
    {
        case CAL_NONE:
        break;

        case CAL_MANUAL:
            calibrate_reticle_manual();
            break;

        case CAL_RA_AUTO:
            calibrate_reticle_by_ra_dec(true);
            break;

        case CAL_RA_DEC_AUTO:
            calibrate_reticle_by_ra_dec(false);
            break;
    }
}

bool rcalibration::is_calibrating()
{
    if (calibrationStage >= CAL_START)
        return true;

    return false;
}

void rcalibration::reset()
{
    is_started = false;
    ui.pushButton_StartCalibration->setText( i18n("Start") );
    ui.startCalibrationLED->setColor(idleColor);
    calibrationStage = CAL_FINISH;
    ui.progressBar->setVisible(false);
    connect(pmath->get_image(), SIGNAL(guideStarSelected(int,int)), this, SLOT(guideStarSelected(int, int)));

}

void rcalibration::calibrate_reticle_manual( void )
{
	//----- manual mode ----
	// get start point

    calibrationType = CAL_MANUAL;

	if( !is_started )
	{
		if( ui.checkBox_TwoAxis->checkState() == Qt::Checked )
		{
            ui.pushButton_StartCalibration->setText( i18n("Stop GUIDE_RA") );
		}
		else
		{
            ui.pushButton_StartCalibration->setText( i18n("Stop") );
		}
        pmain_wnd->appendLogText(i18n("Drift scope in RA. Press stop when done."));

		pmath->get_star_screen_pos( &start_x1, &start_y1 );
		
        axis = GUIDE_RA;
		is_started = true;
	}
	else	// get end point and calc orientation
	{
		if( ui.checkBox_TwoAxis->checkState() == Qt::Checked )
		{
            if( axis == GUIDE_RA )
			{
				pmath->get_star_screen_pos( &end_x1, &end_y1 );
				
				start_x2 = end_x1;
				start_y2 = end_y1;
				
                axis = GUIDE_DEC;
				
                ui.pushButton_StartCalibration->setText( i18n("Stop GUIDE_DEC") );
                pmain_wnd->appendLogText(i18n("Drift scope in DEC. Press Stop when done."));
				return;
			}
			else
			{
				pmath->get_star_screen_pos( &end_x2, &end_y2 );	
                bool dec_swap=false;
				// calc orientation
                if( pmath->calc_and_set_reticle2( start_x1, start_y1, end_x1, end_y1, start_x2, start_y2, end_x2, end_y2, &dec_swap ) )
				{
					fill_interface();
                    if (dec_swap)
                        pmain_wnd->appendLogText(i18n("DEC swap enabled."));
                    else
                        pmain_wnd->appendLogText(i18n("DEC swap disabled."));

                    pmain_wnd->appendLogText(i18n("Calibration completed."));
                    calibrationStage = CAL_FINISH;

                   pmain_wnd->setDECSwap(dec_swap);
				}
				else
				{
                    QMessageBox::warning( this, i18n("Error"), i18n("Calibration rejected. Start drift is too short."), QMessageBox::Ok );
                    calibrationStage = CAL_ERROR;
				}
			}
		}
		else
		{
			pmath->get_star_screen_pos( &end_x1, &end_y1 );

			if( pmath->calc_and_set_reticle( start_x1, start_y1, end_x1, end_y1 ) )
			{
                calibrationStage = CAL_FINISH;
				fill_interface();
                pmain_wnd->appendLogText(i18n("Calibration completed."));
			}
			else
			{
                calibrationStage = CAL_ERROR;
                QMessageBox::warning( this, i18n("Error"), i18n("Calibration rejected. Start drift is too short."), QMessageBox::Ok );
			}
		}

        reset();

	}
}

void rcalibration::calibrate_reticle_by_ra_dec( bool ra_only )
{
 bool auto_term_ok = false;


    if( !pmath )
        return;

    int pulseDuration = ui.spinBox_Pulse->value();
    int totalPulse    = pulseDuration * auto_drift_time;

    if (ra_only)
        calibrationType = CAL_RA_AUTO;
    else
        calibrationType = CAL_RA_DEC_AUTO;

    switch(calibrationStage)
    {

        case CAL_START:
            //----- automatic mode -----
            auto_drift_time = ui.spinBox_DriftTime->value();

            if (ra_only)
                turn_back_time = auto_drift_time*2 + auto_drift_time/2;
            else
                turn_back_time = auto_drift_time*6;
            iterations = 0;

            ui.progressBar->setMaximum( turn_back_time );

            ui.progressBar->setValue( 0 );

            ui.pushButton_StartCalibration->setText(i18n("Abort"));

            pmain_wnd->appendLogText(i18n("GUIDE_RA Drifting..."));

            // get start point
            //pmath->get_star_screen_pos( &start_x1, &start_y1 );

            start_x1 = ui.spinBox_ReticleX->value();
            start_y1 = ui.spinBox_ReticleY->value();

            //qDebug() << "Start X1 " << start_x1 << " Start Y1 " << start_y1 << endl;

            pmain_wnd->do_pulse( RA_INC_DIR, pulseDuration );

            iterations++;

            ui.progressBar->setValue( iterations );

            calibrationStage = CAL_RA_INC;

            ui.startCalibrationLED->setColor(busyColor);

            break;

        case CAL_RA_INC:
            pmain_wnd->do_pulse( RA_INC_DIR, pulseDuration );
            iterations++;
            ui.progressBar->setValue( iterations );

            //qDebug() << "Iteration " << iterations << " and auto drift time is " << auto_drift_time << endl;

            if (iterations == auto_drift_time)
                calibrationStage = CAL_RA_DEC;

            break;

        case CAL_RA_DEC:
        {
            if (iterations == auto_drift_time)
            {
                pmath->get_star_screen_pos( &end_x1, &end_y1 );
                //qDebug() << "End X1 " << end_x1 << " End Y1 " << end_y1 << endl;

                phi = pmath->calc_phi( start_x1, start_y1, end_x1, end_y1 );
                ROT_Z = RotateZ( -M_PI*phi/180.0 ); // derotates...

                pmain_wnd->appendLogText(i18n("Running..."));

            }

            //----- Z-check (new!) -----
            double cur_x, cur_y;
            pmath->get_star_screen_pos( &cur_x, &cur_y );

            //qDebug() << "Cur X1 " << cur_x << " Cur Y1 " << cur_y << endl;

            Vector star_pos = Vector( cur_x, cur_y, 0 ) - Vector( start_x1, start_y1, 0 );
            star_pos.y = -star_pos.y;
            star_pos = star_pos * ROT_Z;

           //qDebug() << "Star x pos is " << star_pos.x << endl;

            // start point reached... so exit
            if( star_pos.x < 1.5 )
            {
                pmath->do_processing();
                auto_term_ok = true;
            }

            //----- Z-check end -----

        if( !auto_term_ok )
        {
            if (iterations < turn_back_time)
            {
                pmain_wnd->do_pulse( RA_DEC_DIR, pulseDuration );
                iterations++;

                ui.progressBar->setValue( iterations );
                break;
            }

            calibrationStage = CAL_ERROR;
            QMessageBox::warning( this, i18n("Warning"), i18np("GUIDE_RA: Scope cannot reach the start point after %1 iteration.\nPossible mount or drive problems...", "GUIDE_RA: Scope cannot reach the start point after %1 iterations.\nPossible mount or drive problems...", turn_back_time), QMessageBox::Ok );
            reset();
            break;
        }

        if (ra_only == false)
        {
            calibrationStage = CAL_DEC_INC;
            start_x2 = cur_x;
            start_y2 = cur_y;

           // qDebug() << "Start X2 " << start_x2 << " start Y2 " << start_y2 << endl;

            pmain_wnd->do_pulse( DEC_INC_DIR, pulseDuration );
            iterations++;
            dec_iterations = 1;
            ui.progressBar->setValue( iterations );
            pmain_wnd->appendLogText(i18n("GUIDE_DEC drifting..."));
            break;
        }
        // calc orientation
        if( pmath->calc_and_set_reticle( start_x1, start_y1, end_x1, end_y1, totalPulse) )
        {
            calibrationStage = CAL_FINISH;
            fill_interface();
            pmain_wnd->appendLogText(i18n("Calibration completed."));
            ui.startCalibrationLED->setColor(okColor);

        }
        else
        {
            QMessageBox::warning( this, i18n("Error"), i18n("Calibration rejected. Star drift is too short."), QMessageBox::Ok );
            ui.startCalibrationLED->setColor(alertColor);
            calibrationStage = CAL_ERROR;
        }

        reset();
        break;
        }

    case CAL_DEC_INC:
        pmain_wnd->do_pulse( DEC_INC_DIR, pulseDuration );
        iterations++;
        dec_iterations++;
        ui.progressBar->setValue( iterations );

        //qDebug() << "Iteration " << iterations << " and auto drift time is " << auto_drift_time << endl;

        if (dec_iterations == auto_drift_time)
            calibrationStage = CAL_DEC_DEC;

        break;

    case CAL_DEC_DEC:
    {
        if (dec_iterations == auto_drift_time)
        {
            pmath->get_star_screen_pos( &end_x2, &end_y2 );
            //qDebug() << "End X2 " << end_x2 << " End Y2 " << end_y2 << endl;

            phi = pmath->calc_phi( start_x2, start_y2, end_x2, end_y2 );
            ROT_Z = RotateZ( -M_PI*phi/180.0 ); // derotates...

            pmain_wnd->appendLogText(i18n("Running..."));
        }

        //----- Z-check (new!) -----
        double cur_x, cur_y;
        pmath->get_star_screen_pos( &cur_x, &cur_y );

        //pmain_wnd->appendLogText(i18n("GUIDE_DEC running back...");

        //qDebug() << "Cur X1 " << cur_x << " Cur Y1 " << cur_y << endl;

        Vector star_pos = Vector( cur_x, cur_y, 0 ) - Vector( start_x2, start_y2, 0 );
        star_pos.y = -star_pos.y;
        star_pos = star_pos * ROT_Z;

        //qDebug() << "start Pos X " << star_pos.x << endl;

        // start point reached... so exit
        if( star_pos.x < 1.5 )
        {
            pmath->do_processing();
            auto_term_ok = true;
        }

        //----- Z-check end -----

    if( !auto_term_ok )
    {
        if (iterations < turn_back_time)
        {
            pmain_wnd->do_pulse( DEC_DEC_DIR, pulseDuration );
            iterations++;
            dec_iterations++;

            ui.progressBar->setValue( iterations );
            break;
        }

        calibrationStage = CAL_ERROR;
        QMessageBox::warning( this, i18n("Warning"), i18np("GUIDE_DEC: Scope cannot reach the start point after %1 iteration.\nPossible mount or drive problems...", "GUIDE_DEC: Scope cannot reach the start point after %1 iterations.\nPossible mount or drive problems...", turn_back_time), QMessageBox::Ok );
        reset();
        break;
    }

    bool swap_dec=false;
    // calc orientation
    if( pmath->calc_and_set_reticle2( start_x1, start_y1, end_x1, end_y1, start_x2, start_y2, end_x2, end_y2, &swap_dec, totalPulse ) )
    {
        calibrationStage = CAL_FINISH;
        fill_interface();
        if (swap_dec)
            pmain_wnd->appendLogText(i18n("DEC swap enabled."));
        else
            pmain_wnd->appendLogText(i18n("DEC swap disabled."));
        pmain_wnd->appendLogText(i18n("Calibration completed."));
        ui.startCalibrationLED->setColor(okColor);

        pmain_wnd->setDECSwap(swap_dec);

    }
    else
    {
        QMessageBox::warning( this, i18n("Error"), i18n("Calibration rejected. Star drift is too short."), QMessageBox::Ok );
        ui.startCalibrationLED->setColor(alertColor);
        calibrationStage = CAL_ERROR;
    }

    reset();

    break;
    }


    default:
            break;

   }

}

void rcalibration::guideStarSelected(int x, int y)
{
    int square_size = guide_squares[pmath->get_square_index()].size;

    pmath->set_reticle_params(x, y, ui.spinBox_ReticleAngle->value());
    pmath->move_square(x-square_size/2, y-square_size/2);

    update_reticle_pos(x, y);

    if (calibrationStage == CAL_FINISH)
        return;

    ui.selectStarLED->setColor(okColor);

    calibrationStage = CAL_START;

    ui.pushButton_StartCalibration->setEnabled(true);
}

void rcalibration::capture()
{
    calibrationStage = CAL_CAPTURE_IMAGE;

    if (pmain_wnd->capture())
    {
         ui.captureLED->setColor(busyColor);
         pmain_wnd->appendLogText(i18n("Capturing image..."));
    }
    else
    {
        ui.captureLED->setColor(alertColor);
        calibrationStage = CAL_ERROR;
    }
}

void rcalibration::set_image(FITSView *image)
{

    if (image == NULL)
        return;

    switch (calibrationStage)
    {
        case CAL_CAPTURE_IMAGE:
        case CAL_SELECT_STAR:
          {
            pmath->resize_square(pmath->get_square_index());
            pmain_wnd->appendLogText(i18n("Image captured..."));

            ui.captureLED->setColor(okColor);
            calibrationStage = CAL_SELECT_STAR;
            ui.selectStarLED->setColor(busyColor);

            FITSImage *image_data = image->getImageData();

            set_video_params(image_data->getWidth(), image_data->getHeight());

            select_auto_star(image);

            connect(image, SIGNAL(guideStarSelected(int,int)), this, SLOT(guideStarSelected(int, int)));

         }
            break;

        default:
            break;
    }
}

void rcalibration::select_auto_star(FITSView *image)
{
    int maxVal=-1;
    Edge *guideStar = NULL;

    FITSImage *image_data = image->getImageData();
    image_data->findStars();


    foreach(Edge *center, image_data->getStarCenters())
    {
        if (center->val > maxVal)
        {
            guideStar = center;
            maxVal = center->val;

        }

    }

    if (guideStar != NULL)
        image->setGuideSquare(guideStar->x, guideStar->y);
}

