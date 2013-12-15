/*  Ekos guide tool
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "guider.h"

#include <math.h>
#include <stdlib.h>
#include <assert.h>

#include <KMessageBox>

#include "scroll_graph.h"
#include "gmath.h"

#include "fitsviewer/fitsview.h"

#include "Options.h"

#define DRIFT_GRAPH_WIDTH	300
#define DRIFT_GRAPH_HEIGHT	300
#define MAX_DITHER_RETIRES  3

rguider::rguider(Ekos::Guide *parent)
    : QWidget(parent)
{
 int i;

	ui.setupUi(this);

    pmain_wnd = parent;

    pimage = NULL;

    targetChip = NULL;

    useRapidGuide = false;
    first_frame = false;

    lost_star_try=0;

	ui.comboBox_SquareSize->clear();
	for( i = 0;guide_squares[i].size != -1;++i )
		ui.comboBox_SquareSize->addItem( QString().setNum( guide_squares[i].size ) );

	ui.comboBox_ThresholdAlg->clear();
	for( i = 0;guide_square_alg[i].idx != -1;++i )
		ui.comboBox_ThresholdAlg->addItem( QString( guide_square_alg[i].name ) );

	ui.spinBox_AccFramesRA->setMaximum( MAX_ACCUM_CNT );
	ui.spinBox_AccFramesDEC->setMaximum( MAX_ACCUM_CNT );

	// connect ui
	connect( ui.spinBox_XScale, 		SIGNAL(valueChanged(int)),	this, SLOT(onXscaleChanged(int)) );
	connect( ui.spinBox_YScale, 		SIGNAL(valueChanged(int)),	this, SLOT(onYscaleChanged(int)) );
	connect( ui.comboBox_SquareSize, 	SIGNAL(activated(int)),    this, SLOT(onSquareSizeChanged(int)) );
	connect( ui.comboBox_ThresholdAlg, 	SIGNAL(activated(int)),    this, SLOT(onThresholdChanged(int)) );
	connect( ui.spinBox_GuideRate, 		SIGNAL(valueChanged(double)), this, SLOT(onInfoRateChanged(double)) );
	connect( ui.checkBox_DirRA, 		SIGNAL(stateChanged(int)), 	this, SLOT(onEnableDirRA(int)) );
	connect( ui.checkBox_DirDEC, 		SIGNAL(stateChanged(int)), 	this, SLOT(onEnableDirDEC(int)) );
	connect( ui.spinBox_AccFramesRA, 	SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );
	connect( ui.spinBox_AccFramesDEC, 	SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );
	connect( ui.spinBox_PropGainRA, 	SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );
	connect( ui.spinBox_PropGainDEC, 	SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );
	connect( ui.spinBox_IntGainRA, 		SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );
	connect( ui.spinBox_IntGainDEC, 	SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );
	connect( ui.spinBox_DerGainRA, 		SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );
	connect( ui.spinBox_DerGainDEC, 	SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );
	connect( ui.spinBox_MaxPulseRA, 	SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );
	connect( ui.spinBox_MaxPulseDEC, 	SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );
	connect( ui.spinBox_MinPulseRA, 	SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );
	connect( ui.spinBox_MinPulseDEC, 	SIGNAL(editingFinished()), this, SLOT(onInputParamChanged()) );
    connect( ui.kfcg_useRapidGuide,     SIGNAL(toggled(bool)), this, SLOT(onRapidGuideChanged(bool)));
    connect( ui.kfcg_guideSubFrame,     SIGNAL(toggled(bool)), this, SLOT(onSubFrameClick(bool)));

    connect(ui.captureB, SIGNAL(clicked()), this, SLOT(capture()));

	connect( ui.pushButton_StartStop, SIGNAL(clicked()), this, SLOT(onStartStopButtonClick()) );

    connect( ui.kcfg_useDither, SIGNAL(toggled(bool)), this, SIGNAL(ditherToggled(bool)));


	pmath = NULL;

	// init drift widget
    pDriftOut = new custom_drawer( ui.frame_Graph );
	pDriftOut->move( ui.frame_Graph->frameWidth(), ui.frame_Graph->frameWidth() );
	pDriftOut->setAttribute( Qt::WA_NoSystemBackground, true );
    ui.frame_Graph->setAttribute( Qt::WA_NoSystemBackground, true );

	drift_graph = new cscroll_graph( this, DRIFT_GRAPH_WIDTH, DRIFT_GRAPH_HEIGHT );
	drift_graph->set_visible_ranges( DRIFT_GRAPH_WIDTH, 60 );

    pDriftOut->set_source( drift_graph->get_buffer(), NULL );
	drift_graph->on_paint();

	ui.frame_Graph->resize( DRIFT_GRAPH_WIDTH + 2*ui.frame_Graph->frameWidth(), DRIFT_GRAPH_HEIGHT + 2*ui.frame_Graph->frameWidth() );

	// not UI vars
	is_started = false;
    is_ready   = false;
	half_refresh_rate = false;
    isDithering = false;

    ui.kcfg_useDither->setChecked(Options::useDither());
    ui.kcfg_ditherPixels->setValue(Options::ditherPixels());

}

rguider::~rguider()
{
    delete pDriftOut;
	delete drift_graph;
}


void rguider::set_half_refresh_rate( bool is_half )
{
	half_refresh_rate = is_half;
}


bool rguider::is_guiding( void ) const
{
	return is_started;
}


void rguider::set_math( cgmath *math )
{
	assert( math );
	pmath = math;
}


void rguider::fill_interface( void )
{
 const cproc_in_params *in_params;
 const cproc_out_params *out_params;
 info_params_t	info_params;
 QString str;
 int rx, ry;


	assert( pmath );

	info_params = pmath->get_info_params();
	in_params   = pmath->get_in_params();
	out_params  = pmath->get_out_params();

	drift_graph->get_visible_ranges( &rx, &ry );
	ui.spinBox_XScale->setValue( rx / drift_graph->get_grid_N() );
	ui.spinBox_YScale->setValue( ry / drift_graph->get_grid_N() );

	ui.comboBox_SquareSize->setCurrentIndex( pmath->get_square_index() );
	ui.comboBox_ThresholdAlg->setCurrentIndex( pmath->get_square_algorithm_index() );


    ui.l_RecommendedGain->setText( i18n("P: %1", QString().setNum(cgmath::precalc_proportional_gain(in_params->guiding_rate), 'f', 2 )) );


	ui.spinBox_GuideRate->setValue( in_params->guiding_rate );

	// info params...
	ui.l_Focal->setText( str.setNum( (int)info_params.focal) );
	ui.l_Aperture->setText( str.setNum( (int)info_params.aperture) );
	ui.l_FbyD->setText( QString().setNum( info_params.focal_ratio, 'f', 1) );
	str = QString().setNum(info_params.fov_wd, 'f', 1) + 'x' + QString().setNum(info_params.fov_ht, 'f', 1);
	ui.l_FOV->setText( str );

    ui.checkBox_DirRA->setChecked( in_params->enabled[GUIDE_RA] );
    ui.checkBox_DirDEC->setChecked( in_params->enabled[GUIDE_DEC] );

    ui.spinBox_AccFramesRA->setValue( (int)in_params->accum_frame_cnt[GUIDE_RA] );
    ui.spinBox_AccFramesDEC->setValue( (int)in_params->accum_frame_cnt[GUIDE_DEC] );

    ui.spinBox_PropGainRA->setValue( in_params->proportional_gain[GUIDE_RA] );
    ui.spinBox_PropGainDEC->setValue( in_params->proportional_gain[GUIDE_DEC] );

    ui.spinBox_IntGainRA->setValue( in_params->integral_gain[GUIDE_RA] );
    ui.spinBox_IntGainDEC->setValue( in_params->integral_gain[GUIDE_DEC] );

    ui.spinBox_DerGainRA->setValue( in_params->derivative_gain[GUIDE_RA] );
    ui.spinBox_DerGainDEC->setValue( in_params->derivative_gain[GUIDE_DEC] );

    ui.spinBox_MaxPulseRA->setValue( in_params->max_pulse_length[GUIDE_RA] );
    ui.spinBox_MaxPulseDEC->setValue( in_params->max_pulse_length[GUIDE_DEC] );

    ui.spinBox_MinPulseRA->setValue( in_params->min_pulse_length[GUIDE_RA] );
    ui.spinBox_MinPulseDEC->setValue( in_params->min_pulse_length[GUIDE_DEC] );


    ui.l_DeltaRA->setText(QString().setNum(out_params->delta[GUIDE_RA], 'f', 2) );
    ui.l_DeltaDEC->setText(QString().setNum(out_params->delta[GUIDE_DEC], 'f', 2) );

    ui.l_PulseRA->setText(QString().setNum(out_params->pulse_length[GUIDE_RA]) );
    ui.l_PulseDEC->setText(QString().setNum(out_params->pulse_length[GUIDE_DEC]) );

    ui.l_ErrRA->setText( QString().setNum(out_params->sigma[GUIDE_RA]) );
    ui.l_ErrDEC->setText( QString().setNum(out_params->sigma[GUIDE_DEC]) );

}


void rguider::onXscaleChanged( int i )
{
 int rx, ry;

 	drift_graph->get_visible_ranges( &rx, &ry );
 	drift_graph->set_visible_ranges( i*drift_graph->get_grid_N(), ry );

 	// refresh if not started
 	if( !is_started )
 	{
 		drift_graph->on_paint();
 //		pDriftOut->update();
 	}

}


void rguider::onYscaleChanged( int i )
{
 int rx, ry;

	drift_graph->get_visible_ranges( &rx, &ry );
	drift_graph->set_visible_ranges( rx, i*drift_graph->get_grid_N() );

	// refresh if not started
	if( !is_started )
	{
		drift_graph->on_paint();
//		pDriftOut->update();
	}
}


void rguider::onSquareSizeChanged( int index )
{
	if( pmath )
		pmath->resize_square( index );
}


void rguider::onThresholdChanged( int index )
{
	if( pmath )
		pmath->set_square_algorithm( index );
}



// params changing stuff
void rguider::onInfoRateChanged( double val )
{
 cproc_in_params *in_params;


	if( !pmath )
		return;

	in_params = pmath->get_in_params();

	in_params->guiding_rate = val;

    ui.l_RecommendedGain->setText( i18n("P: %1", QString().setNum(pmath->precalc_proportional_gain(in_params->guiding_rate), 'f', 2 )) );
}


void rguider::onEnableDirRA( int state )
{
 cproc_in_params *in_params;

 	if( !pmath )
  		return;

 	in_params = pmath->get_in_params();
    in_params->enabled[GUIDE_RA] = (state == Qt::Checked);
}


void rguider::onEnableDirDEC( int state )
{
 cproc_in_params *in_params;

 	if( !pmath )
   		return;

  	in_params = pmath->get_in_params();
    in_params->enabled[GUIDE_DEC] = (state == Qt::Checked);
}


void rguider::onInputParamChanged()
{
 QObject *obj;
 QSpinBox *pSB;
 QDoubleSpinBox *pDSB;
 cproc_in_params *in_params;


 	if( !pmath )
 		return;

 	obj = sender();

 	in_params = pmath->get_in_params();

	if( (pSB = dynamic_cast<QSpinBox *>(obj)) )
	{
		if( pSB == ui.spinBox_AccFramesRA )
            in_params->accum_frame_cnt[GUIDE_RA] = pSB->value();
		else
		if( pSB == ui.spinBox_AccFramesDEC )
            in_params->accum_frame_cnt[GUIDE_DEC] = pSB->value();
		else
		if( pSB == ui.spinBox_MaxPulseRA )
            in_params->max_pulse_length[GUIDE_RA] = pSB->value();
		else
		if( pSB == ui.spinBox_MaxPulseDEC )
            in_params->max_pulse_length[GUIDE_DEC] = pSB->value();
		else
		if( pSB == ui.spinBox_MinPulseRA )
            in_params->min_pulse_length[GUIDE_RA] = pSB->value();
		else
		if( pSB == ui.spinBox_MinPulseDEC )
            in_params->min_pulse_length[GUIDE_DEC] = pSB->value();
	}
	else
	if( (pDSB = dynamic_cast<QDoubleSpinBox *>(obj)) )
	{
		if( pDSB == ui.spinBox_PropGainRA )
            in_params->proportional_gain[GUIDE_RA] = pDSB->value();
		else
		if( pDSB == ui.spinBox_PropGainDEC )
            in_params->proportional_gain[GUIDE_DEC] = pDSB->value();
		else
		if( pDSB == ui.spinBox_IntGainRA )
            in_params->integral_gain[GUIDE_RA] = pDSB->value();
		else
		if( pDSB == ui.spinBox_IntGainDEC )
            in_params->integral_gain[GUIDE_DEC] = pDSB->value();
		else
		if( pDSB == ui.spinBox_DerGainRA )
            in_params->derivative_gain[GUIDE_RA] = pDSB->value();
		else
		if( pDSB == ui.spinBox_DerGainDEC )
            in_params->derivative_gain[GUIDE_DEC] = pDSB->value();
	}

}



void rguider::set_target_chip(ISD::CCDChip *chip)
{
    targetChip = chip;
    targetChip->getFrame(&fx, &fy, &fw, &fh);

}

void rguider::onSubFrameClick(bool enable)
{
    if (targetChip == NULL)
        return;

    if (enable)
    {
        int x,y,w,h, square_size, binx, biny;
        targetChip->getBinning(&binx, &biny);
        pmath->get_reticle_params(&ret_x, &ret_y, &ret_angle);
        square_size = pmath->get_square_size();
        x = ret_x - square_size*2*binx ;
        y = ret_y - square_size*2*biny;
        w=square_size*4*binx;
        h=square_size*4*biny;

        first_frame = true;

        if (x<0)
            x=0;
        if (y<0)
            y=0;
        if (w>fw)
            w=fw;
        if (h>fh)
            h=fh;

        pmath->set_video_params(w, h);

        targetChip->setFrame(x, y, w, h);
    }
    else
    {
        first_frame = false;
        targetChip->setFrame(fx, fy, fw, fh);
    }

}

// processing stuff
void rguider::onStartStopButtonClick()
{
 	assert( pmath );
    assert (targetChip);

	// start
	if( !is_started )
	{

        Options::setUseDither(ui.kcfg_useDither->isChecked());
        Options::setDitherPixels(ui.kcfg_ditherPixels->value());

        if (pimage)
            disconnect(pimage, SIGNAL(guideStarSelected(int,int)), 0, 0);

		drift_graph->reset_data();
        ui.pushButton_StartStop->setText( i18n("Stop") );
        pmain_wnd->appendLogText(i18n("Autoguiding started."));
		pmath->start();
        lost_star_try=0;
		is_started = true;
        if (useRapidGuide)
            pmain_wnd->startRapidGuide();

        emit autoGuidingToggled(true, ui.kcfg_useDither->isChecked());

        pmain_wnd->capture();
	}
	// stop
	else
	{
        if (pimage)
            connect(pimage, SIGNAL(guideStarSelected(int,int)), this, SLOT(guideStarSelected(int,int)));
        ui.pushButton_StartStop->setText( i18n("Start Autoguide") );
        pmain_wnd->appendLogText(i18n("Autoguiding stopped."));
		pmath->stop();

        targetChip->abortExposure();

        if (useRapidGuide)
            pmain_wnd->stopRapidGuide();

        emit autoGuidingToggled(false, ui.kcfg_useDither->isChecked());

		is_started = false;
	}
}

void rguider::capture()
{
    pmain_wnd->capture();
}

void rguider::guide( void )
{
 const cproc_out_params *out;
 QString str;
 uint32_t tick = 0;
 double drift_x = 0, drift_y = 0;


 	 assert( pmath );

     if (first_frame && ui.kfcg_guideSubFrame->isChecked())
     {
        int x,y,w,h;
        targetChip->getFrame(&x, &y, &w, &h);
        int square_size = pmath->get_square_size();
        double ret_x,ret_y,ret_angle;
        pmath->get_reticle_params(&ret_x, &ret_y, &ret_angle);
        pmath->move_square((w-square_size)/2, (h-square_size)/2);
        pmath->set_reticle_params(w/2, h/2, ret_angle);
        first_frame = false;
     }

	 // calc math. it tracks square
	 pmath->do_processing();

     if(!is_started )
	 	 return;

     if (pmath->is_lost_star() && ++lost_star_try > 2)
     {
         onStartStopButtonClick();
         KMessageBox::error(NULL, i18n("Lost track of the guide star. Try increasing the square size and check the mount."));
         return;
     }
     else
         lost_star_try = 0;

	 // do pulse
	 out = pmath->get_out_params();


     //qDebug() << "Guide is sending pulse command now ... " << endl;
     pmain_wnd->do_pulse( out->pulse_dir[GUIDE_RA], out->pulse_length[GUIDE_RA], out->pulse_dir[GUIDE_DEC], out->pulse_length[GUIDE_DEC] );
     //qDebug() << "#######################################" << endl;


	 pmath->get_star_drift( &drift_x, &drift_y );

	 drift_graph->add_point( drift_x, drift_y );

	 tick = pmath->get_ticks();


	 if( tick & 1 )
	 {
		 // draw some params in window
         ui.l_DeltaRA->setText(str.setNum(out->delta[GUIDE_RA], 'f', 2) );
         ui.l_DeltaDEC->setText(str.setNum(out->delta[GUIDE_DEC], 'f', 2) );

         ui.l_PulseRA->setText(str.setNum(out->pulse_length[GUIDE_RA]) );
         ui.l_PulseDEC->setText(str.setNum(out->pulse_length[GUIDE_DEC]) );

         ui.l_ErrRA->setText( str.setNum(out->sigma[GUIDE_RA], 'g', 3));
         ui.l_ErrDEC->setText( str.setNum(out->sigma[GUIDE_DEC], 'g', 3 ));
	 }

	 // skip half frames
	 if( half_refresh_rate && (tick & 1) )
	 	return;

     drift_graph->on_paint();
     pDriftOut->update();

}

void rguider::set_image(FITSView *image)
{
    pimage = image;

    if (is_ready && pimage && is_started == false)
        connect(pimage, SIGNAL(guideStarSelected(int,int)), this, SLOT(guideStarSelected(int, int)));
}

void rguider::guideStarSelected(int x, int y)
{
    int square_size = guide_squares[pmath->get_square_index()].size;

    pmath->set_reticle_params(x, y, pmain_wnd->getReticleAngle());
    pmath->move_square(x-square_size/2, y-square_size/2);

    disconnect(pimage, SIGNAL(guideStarSelected(int,int)), this, SLOT(guideStarSelected(int, int)));

}

void rguider::onRapidGuideChanged(bool enable)
{
    if (is_started)
    {
        pmain_wnd->appendLogText(i18n("You must stop auto guiding before changing this setting."));
        return;
    }

    useRapidGuide = enable;

    if (useRapidGuide)
    {
        pmain_wnd->appendLogText(i18n("Rapid Guiding is enabled. Guide star will be determined automatically by the CCD driver. No frames are sent to Ekos unless explicitly enabled by the user in the CCD driver settings."));
    }
    else
        pmain_wnd->appendLogText(i18n("Rapid Guiding is disabled."));

}

void rguider::start()
{
    if (is_started == false)
        onStartStopButtonClick();

}

void rguider::abort()
{
    if (is_started == true)
        onStartStopButtonClick();
}

bool rguider::dither()
{
    static Vector target_pos;
    static unsigned int retries=0;

    if (ui.kcfg_useDither->isChecked() == false)
        return false;

    double cur_x, cur_y, angle;
    pmath->get_reticle_params(&cur_x, &cur_y, &angle);
    pmath->get_star_screen_pos( &cur_x, &cur_y );
    Matrix ROT_Z = pmath->get_ROTZ();

    kDebug() << "Star Pos X " << cur_x << " Y " << cur_y << endl;

    if (isDithering == false)
    {
        retries =0;
        pmath->set_preview_mode(true);
        targetChip->abortExposure();
        double ditherPixels = ui.kcfg_ditherPixels->value();
        int polarity = (rand() %2 == 0) ? 1 : -1;
        double angle = ((double) rand() / RAND_MAX) * M_PI/2.0;
        double x_msec = ditherPixels * cos(angle) * pmath->get_dither_rate(0);
        double y_msec = ditherPixels * sin(angle) * pmath->get_dither_rate(1);

        kDebug() << "Rate X " << pmath->get_dither_rate(0) << " Rate Y " << pmath->get_dither_rate(1) << endl;

        if (x_msec < 0)
            x_msec = ui.spinBox_MinPulseRA->value();
        if (y_msec < 0)
            y_msec = ui.spinBox_MinPulseDEC->value();

        isDithering = true;

        if (polarity > 0)
        {
            target_pos = Vector( cur_x, cur_y, 0 ) + Vector( ditherPixels * cos(angle), ditherPixels * sin(angle), 0 );
            pmain_wnd->do_pulse(RA_INC_DIR, x_msec, DEC_INC_DIR, y_msec);
        }
        else
        {
            target_pos = Vector( cur_x, cur_y, 0 ) - Vector( ditherPixels * cos(angle), ditherPixels * sin(angle), 0 );
            pmain_wnd->do_pulse(RA_DEC_DIR, x_msec, DEC_DEC_DIR, y_msec);
        }

        //target_pos.y = -target_pos.y;
        target_pos = target_pos * ROT_Z;

        kDebug() << "Target Pos X " << target_pos.x << " Y " << target_pos.y << endl;

        return true;
    }

    if (isDithering == false)
        return false;

    Vector star_pos = Vector( cur_x, cur_y, 0 ) - Vector( target_pos.x, target_pos.y, 0 );
    star_pos.y = -star_pos.y;
    star_pos = star_pos * ROT_Z;

    kDebug() << "Diff star X " << star_pos.x << " Y " << star_pos.y << endl;

    if (fabs(star_pos.x) < 1 && fabs(star_pos.y) < 1)
    {
        pmath->set_preview_mode(false);
        pmath->set_reticle_params(cur_x, cur_y, angle);

        isDithering = false;

        emit ditherComplete();
    }
    else if (++retries > MAX_DITHER_RETIRES)
        return false;

    pmain_wnd->capture();

    return true;
}




