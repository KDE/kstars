/***************************************************************************
                          planetviewer.h  -  Display overhead view of the solar system
                             -------------------
    begin                : Sun May 25 2003
    copyright            : (C) 2003 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef PLANETVIEWER_H
#define PLANETVIEWER_H

#include <kdialogbase.h>
#include <kpushbutton.h>
#include <tqtimer.h>

#include "kstarsplotwidget.h"
#include "planetviewerui.h"
#include "kstarsdatetime.h"
#include "planetcatalog.h"

/**@class PlanetViewer
	*@short Display an overhead view of the solar system
	*@version 1.0
	*@author Jason Harris
	*/
class PlanetViewer : public KDialogBase
{
Q_OBJECT
public:
	PlanetViewer(TQWidget *parent = 0, const char *name = 0);
	~PlanetViewer();

	TQString centerPlanet() const { return CenterPlanet; }
	void setCenterPlanet( const TQString &cp ) { CenterPlanet = cp; }
	
	KPlotObject* planetObject(uint i) const { return planet[i]; }
	TQString planetName(uint i) const { return pName[i]; }

protected:
	virtual void keyPressEvent( TQKeyEvent *e );
	virtual void paintEvent( TQPaintEvent* );

private slots:
	void initPlotObjects();
	void tick();
	void setTimeScale(float);
	void slotChangeDate( const ExtDate &d );
	void slotRunClock();
	void slotToday();

private:
	void updatePlanets();
	
	PlanetViewerUI *pw;
	KStarsDateTime ut;
	PlanetCatalog PCat;
	double scale;
	bool isClockRunning;
	TQTimer tmr;
	int UpdateInterval[9], LastUpdate[9];
	TQString pName[9], pColor[9];
	TQString CenterPlanet;

	KPlotObject *ksun;
	KPlotObject *planet[9];
	KPlotObject *planetLabel[9];
};

class PVPlotWidget : public KStarsPlotWidget
{
Q_OBJECT
public:
	PVPlotWidget( double x1, double x2, double y1, double y2, 
			TQWidget *parent=0, const char *name=0 );
	PVPlotWidget( TQWidget *parent=0, const char *name=0 );
	~PVPlotWidget();

public slots:
	void slotZoomIn();
	void slotZoomOut();

signals:
	void doubleClicked( double, double );

protected:
	virtual void keyPressEvent( TQKeyEvent *e );
	virtual void mousePressEvent( TQMouseEvent *e );
	virtual void mouseMoveEvent( TQMouseEvent *e );
	virtual void mouseReleaseEvent( TQMouseEvent * );
	virtual void mouseDoubleClickEvent( TQMouseEvent *e );
	virtual void wheelEvent( TQWheelEvent *e );

private:
	bool mouseButtonDown;
	int oldx, oldy;
	PlanetViewer *pv;
};

#endif
