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
#include <KLocalizedString>

#include "scroll_graph.h"
#include "gmath.h"
#include "fitsviewer/fitsview.h"
#include "../ekosmanager.h"
#include "kstars.h"

#include "Options.h"

#define DRIFT_GRAPH_WIDTH	300
#define DRIFT_GRAPH_HEIGHT	300
#define MAX_DITHER_RETIRES  20

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
    is_subframed = false;

    lost_star_try=0;       

	ui.comboBox_SquareSize->clear();
	for( i = 0;guide_squares[i].size != -1;++i )
		ui.comboBox_SquareSize->addItem( QString().setNum( guide_squares[i].size ) );

	ui.comboBox_ThresholdAlg->clear();
	for( i = 0;guide_square_alg[i].idx != -1;++i )
		ui.comboBox_ThresholdAlg->addItem( QString( guide_square_alg[i].name ) );

	// connect ui
	connect( ui.spinBox_XScale, 		SIGNAL(valueChanged(int)),	this, SLOT(onXscaleChanged(int)) );
	connect( ui.spinBox_YScale, 		SIGNAL(valueChanged(int)),	this, SLOT(onYscaleChanged(int)) );
	connect( ui.comboBox_SquareSize, 	SIGNAL(activated(int)),    this, SLOT(onSquareSizeChanged(int)) );
	connect( ui.comboBox_ThresholdAlg, 	SIGNAL(activated(int)),    this, SLOT(onThresholdChanged(int)) );
	connect( ui.spinBox_GuideRate, 		SIGNAL(valueChanged(double)), this, SLOT(onInfoRateChanged(double)) );
	connect( ui.checkBox_DirRA, 		SIGNAL(stateChanged(int)), 	this, SLOT(onEnableDirRA(int)) );
	connect( ui.checkBox_DirDEC, 		SIGNAL(stateChanged(int)), 	this, SLOT(onEnableDirDEC(int)) );
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
    ui.spinBox_AOLimit->setValue(Options::aOLimit());

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

void rguider::set_ao(bool enable)
{
    ui.spinBox_AOLimit->setEnabled(enable);
}

double rguider::get_ao_limit()
{
    return ui.spinBox_AOLimit->value();
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

    ui.l_RecommendedGain->setText( xi18n("P: %1", QString().setNum(cgmath::precalc_proportional_gain(Options::guidingRate()), 'f', 2 )) );
    ui.spinBox_GuideRate->setValue( Options::guidingRate() );

	// info params...
	ui.l_Focal->setText( str.setNum( (int)info_params.focal) );
	ui.l_Aperture->setText( str.setNum( (int)info_params.aperture) );
	ui.l_FbyD->setText( QString().setNum( info_params.focal_ratio, 'f', 1) );
	str = QString().setNum(info_params.fov_wd, 'f', 1) + 'x' + QString().setNum(info_params.fov_ht, 'f', 1);
	ui.l_FOV->setText( str );

    ui.checkBox_DirRA->setChecked( Options::enableRAGuide() );
    ui.checkBox_DirDEC->setChecked( Options::enableDECGuide() );

    ui.spinBox_PropGainRA->setValue( Options::rAPropotionalGain() );
    ui.spinBox_PropGainDEC->setValue( Options::dECPropotionalGain()  );

    ui.spinBox_IntGainRA->setValue( Options::rAIntegralGain() );
    ui.spinBox_IntGainDEC->setValue( Options::dECIntegralGain() );

    ui.spinBox_DerGainRA->setValue( Options::rADerivativeGain() );
    ui.spinBox_DerGainDEC->setValue( Options::dECDerivativeGain() );

    ui.spinBox_MaxPulseRA->setValue( Options::rAMaximumPulse() );
    ui.spinBox_MaxPulseDEC->setValue( Options::dECMaximumPulse() );

    ui.spinBox_MinPulseRA->setValue( Options::rAMinimumPulse() );
    ui.spinBox_MinPulseDEC->setValue( Options::dECMinimumPulse() );

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

    ui.l_RecommendedGain->setText( xi18n("P: %1", QString().setNum(pmath->precalc_proportional_gain(in_params->guiding_rate), 'f', 2 )) );
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
    ui.kfcg_guideSubFrame->setEnabled(targetChip->canSubframe());
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
        Options::setAOLimit(ui.spinBox_AOLimit->value());
        Options::setGuidingRate(ui.spinBox_GuideRate->value());
        Options::setEnableRAGuide(ui.checkBox_DirRA->isChecked());
        Options::setEnableDECGuide(ui.checkBox_DirDEC->isChecked());
        Options::setRAPropotionalGain(ui.spinBox_PropGainRA->value());
        Options::setDECPropotionalGain(ui.spinBox_PropGainDEC->value());
        Options::setRAIntegralGain(ui.spinBox_IntGainRA->value());
        Options::setDECIntegralGain(ui.spinBox_IntGainDEC->value());
        Options::setRADerivativeGain(ui.spinBox_DerGainRA->value());
        Options::setDECDerivativeGain(ui.spinBox_DerGainDEC->value());
        Options::setRAMaximumPulse(ui.spinBox_MaxPulseRA->value());
        Options::setDECMaximumPulse(ui.spinBox_MaxPulseDEC->value());
        Options::setRAMinimumPulse(ui.spinBox_MinPulseRA->value());
        Options::setDECMinimumPulse(ui.spinBox_MinPulseDEC->value());

        if (pimage)
            disconnect(pimage, SIGNAL(guideStarSelected(int,int)), 0, 0);

		drift_graph->reset_data();
        ui.pushButton_StartStop->setText( xi18n("Stop") );
        pmain_wnd->appendLogText(xi18n("Autoguiding started."));
		pmath->start();
        lost_star_try=0;
		is_started = true;
        useRapidGuide = ui.kfcg_useRapidGuide->isChecked();
        if (useRapidGuide)
            pmain_wnd->startRapidGuide();

        emit autoGuidingToggled(true, ui.kcfg_useDither->isChecked());

        pmain_wnd->setSuspended(false);

        if (ui.kfcg_guideSubFrame->isEnabled() && ui.kfcg_guideSubFrame->isChecked() && is_subframed == false)
            first_frame = true;

        capture();
	}
	// stop
	else
	{
        if (pimage)
            connect(pimage, SIGNAL(guideStarSelected(int,int)), this, SLOT(guideStarSelected(int,int)));
        ui.pushButton_StartStop->setText( xi18n("Start Autoguide") );
        pmain_wnd->appendLogText(xi18n("Autoguiding stopped."));
		pmath->stop();

        targetChip->abortExposure();

        if (useRapidGuide)
            pmain_wnd->stopRapidGuide();

        emit autoGuidingToggled(false, ui.kcfg_useDither->isChecked());

        isDithering = false;
		is_started = false;
	}
}

void rguider::capture()
{
    if (ui.kfcg_guideSubFrame->isChecked() && is_subframed == false)
    {
        int x,y,w,h, square_size, binx, biny;
        targetChip->getBinning(&binx, &biny);
        pmath->get_reticle_params(&ret_x, &ret_y, &ret_angle);
        square_size = pmath->get_square_size();
        x = (ret_x - square_size)*binx;
        y = (ret_y - square_size)*biny;
        w=square_size*2*binx;
        h=square_size*2*biny;

        int minX, maxX, minY, maxY, minW, maxW, minH, maxH;
        targetChip->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH);

        is_subframed = true;

        if (x<minX)
            x=minX;
        if (y<minY)
            y=minY;
        if ((w+x)>maxW)
            w=maxW-x;
        if ((h+y)>maxH)
            h=maxH-y;

        pmath->set_video_params(w, h);

        targetChip->setFrame(x, y, w, h);

        guideStarSelected(w/2, h/2);
    }
    else if (ui.kfcg_guideSubFrame->isChecked() == false/* && is_subframed == true*/)
    {
        is_subframed = false;
        //targetChip->setFrame(fx, fy, fw, fh);
        targetChip->resetFrame();
    }

    pmain_wnd->capture();
}

void rguider::onSetDECSwap(bool enable)
{
    pmain_wnd->setDECSwap(enable);
}

void rguider::set_dec_swap(bool enable)
{
    ui.swapCheck->disconnect(this);
    ui.swapCheck->setChecked(enable);
    connect(ui.swapCheck, SIGNAL(toggled(bool)), this, SLOT(set_dec_swap(bool)));
}

void rguider::guide( void )
{
 static int maxPulseCounter=0;
 const cproc_out_params *out;
 QString str;
 uint32_t tick = 0;
 double drift_x = 0, drift_y = 0;


 	 assert( pmath );

     if (first_frame)
     {
        int x,y,w,h,binx, biny;
        targetChip->getFrame(&x, &y, &w, &h);
        targetChip->getBinning(&binx, &biny);
        int square_size = pmath->get_square_size();
        double ret_x,ret_y,ret_angle;
        pmath->get_reticle_params(&ret_x, &ret_y, &ret_angle);
        pmath->move_square((w-square_size)/(2*binx), (h-square_size)/(2*biny));
        pmath->set_reticle_params(w/(2*binx), h/(2*biny), ret_angle);
        first_frame = false;
     }

	 // calc math. it tracks square
	 pmath->do_processing();

     if(!is_started )
	 	 return;

     if (pmath->is_lost_star() && ++lost_star_try > 2)
     {
         onStartStopButtonClick();
         KMessageBox::error(NULL, xi18n("Lost track of the guide star. Try increasing the square size and check the mount."));
         return;
     }
     else
         lost_star_try = 0;

	 // do pulse
	 out = pmath->get_out_params();

     if (out->pulse_length[GUIDE_RA] == ui.spinBox_MaxPulseRA->value() || out->pulse_length[GUIDE_DEC] == ui.spinBox_MaxPulseDEC->value())
        maxPulseCounter++;
     else
         maxPulseCounter=0;

     if (maxPulseCounter > 3)
     {
         pmain_wnd->appendLogText(xi18n("Lost track of the guide star. Aborting guiding..."));
         abort();
         maxPulseCounter=0;
     }

     pmain_wnd->do_pulse( out->pulse_dir[GUIDE_RA], out->pulse_length[GUIDE_RA], out->pulse_dir[GUIDE_DEC], out->pulse_length[GUIDE_DEC] );

     if (isDithering)
         return;

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
        pmain_wnd->appendLogText(xi18n("You must stop auto guiding before changing this setting."));
        return;
    }

    useRapidGuide = enable;

    if (useRapidGuide)
    {
        pmain_wnd->appendLogText(xi18n("Rapid Guiding is enabled. Guide star will be determined automatically by the CCD driver. No frames are sent to Ekos unless explicitly enabled by the user in the CCD driver settings."));
    }
    else
        pmain_wnd->appendLogText(xi18n("Rapid Guiding is disabled."));

}

void rguider::start()
{
    if (is_started == false)
        onStartStopButtonClick();

}

void rguider::abort(bool silence)
{
    if (is_started == true)
    {
        onStartStopButtonClick();

        if (silence)
            return;

        if (Options::playGuideAlarm())
                KStars::Instance()->ekosManager()->playError();
    }
}

bool rguider::dither()
{
    static Vector target_pos;
    static unsigned int retries=0;

    if (ui.kcfg_useDither->isChecked() == false)
        return false;

    double cur_x, cur_y, ret_angle;
    pmath->get_reticle_params(&cur_x, &cur_y, &ret_angle);
    pmath->get_star_screen_pos( &cur_x, &cur_y );
    Matrix ROT_Z = pmath->get_ROTZ();

    //qDebug() << "Star Pos X " << cur_x << " Y " << cur_y << endl;

    if (isDithering == false)
    {
        retries =0;
        targetChip->abortExposure();
        double ditherPixels = ui.kcfg_ditherPixels->value();
        int polarity = (rand() %2 == 0) ? 1 : -1;
        double angle = ((double) rand() / RAND_MAX) * M_PI/2.0;
        double diff_x = ditherPixels * cos(angle);
        double diff_y = ditherPixels * sin(angle);

        isDithering = true;

        if (pmath->get_dec_swap())
            diff_y *= -1;

        if (polarity > 0)
             target_pos = Vector( cur_x, cur_y, 0 ) + Vector( diff_x, diff_y, 0 );
        else
            target_pos = Vector( cur_x, cur_y, 0 ) - Vector( diff_x, diff_y, 0 );

        //qDebug() << "Target Pos X " << target_pos.x << " Y " << target_pos.y << endl;

        pmath->set_reticle_params(target_pos.x, target_pos.y, ret_angle);

        guide();

        pmain_wnd->capture();

        return true;
    }

    if (isDithering == false)
        return false;

    Vector star_pos = Vector( cur_x, cur_y, 0 ) - Vector( target_pos.x, target_pos.y, 0 );
    star_pos.y = -star_pos.y;
    star_pos = star_pos * ROT_Z;

    //qDebug() << "Diff star X " << star_pos.x << " Y " << star_pos.y << endl;

    if (fabs(star_pos.x) < 1 && fabs(star_pos.y) < 1)
    {
        pmath->set_reticle_params(cur_x, cur_y, ret_angle);

        isDithering = false;

        emit ditherComplete();
    }
    else
    {
        if (++retries > MAX_DITHER_RETIRES)
        {
            isDithering = false;
            return false;
        }

        guide();
    }

    pmain_wnd->capture();

    return true;
}




