/*  Ekos guide tool
    Copyright (C) 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef GUIDER_H
#define GUIDER_H

#include <QtGui>
#include "common.h"
#include "ui_guider.h"
#include "scroll_graph.h"
#include "gmath.h"

#include "../guide.h"


class rguider : public QWidget
{
    Q_OBJECT

public:
    explicit rguider(Ekos::Guide *parent = 0);
    ~rguider();

    void guide( void );
    void start();
    void abort();
    bool dither();
    void set_half_refresh_rate( bool is_half );
    bool is_guiding( void ) const;
    void set_math( cgmath *math );
    void set_ao(bool enable);
    void fill_interface( void );
    void set_image(FITSView *image);
    void set_ready(bool enable) { is_ready = enable;}
    void set_target_chip(ISD::CCDChip *chip);
    bool isRapidGuide() { return useRapidGuide;}
    bool is_dithering() { return isDithering; }
    double get_ao_limit();    

public slots:
    void set_dec_swap(bool enable);

protected slots:
	void onXscaleChanged( int i );
	void onYscaleChanged( int i );
	void onSquareSizeChanged( int index );
	void onThresholdChanged( int i );
	void onInfoRateChanged( double val );
	void onEnableDirRA( int state );
	void onEnableDirDEC( int state );
	void onInputParamChanged();
    void onRapidGuideChanged(bool enable);
    void capture();
    void guideStarSelected(int x, int y);
	void onStartStopButtonClick();
    void onSetDECSwap(bool enable);

signals:
    void ditherComplete();
    void ditherToggled(bool);
    void autoGuidingToggled(bool, bool);

private:
	cgmath *pmath;
    Ekos::Guide *pmain_wnd;

    custom_drawer *pDriftOut;
    cscroll_graph *drift_graph;

    FITSView *pimage;
	bool is_started;
    bool is_ready;
    bool is_subframed;
    bool first_frame;
	bool half_refresh_rate;
    int lost_star_try;
    bool useRapidGuide;
    ISD::CCDChip *targetChip;
    int fx,fy,fw,fh;
    double ret_x, ret_y, ret_angle;
    bool isDithering;

private:
    Ui::guiderClass ui;
};

#endif // GUIDER_H
