/*  Ekos guide tool
    Copyright (C) 2012 Alexander Stepanenko

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
    rguider(Ekos::Guide *parent = 0);
    ~rguider();

    void guide( void );
    void set_half_refresh_rate( bool is_half );
    bool is_guiding( void ) const;
    void set_math( cgmath *math );
    void fill_interface( void );

protected slots:
	void onXscaleChanged( int i );
	void onYscaleChanged( int i );
	void onSquareSizeChanged( int index );
	void onThresholdChanged( int i );
	void onInfoRateChanged( double val );
	void onEnableDirRA( int state );
	void onEnableDirDEC( int state );
	void onInputParamChanged();

	void onStartStopButtonClick();

private:
	cgmath *pmath;
    Ekos::Guide *pmain_wnd;

    custom_drawer *pDriftOut;
    cscroll_graph *drift_graph;
	bool is_started;
	bool half_refresh_rate;



private:
    Ui::guiderClass ui;
};

#endif // GUIDER_H
