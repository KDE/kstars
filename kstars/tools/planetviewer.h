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

#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QPaintEvent>

#include <kdialogbase.h>
#include <kpushbutton.h>

#include "pvplotwidget.h"
#include "planetviewerui.h"
#include "kstarsdatetime.h"

#define AUMAX 48

class KSPlanetBase;

class PlanetViewerUI : public QFrame, public Ui::PlanetViewer {
Q_OBJECT
public:
	PlanetViewerUI(QWidget *parent = 0 );
};

/**@class PlanetViewer
	*@short Display an overhead view of the solar system
	*@version 1.0
	*@author Jason Harris
	*/
class PlanetViewer : public KDialogBase
{
Q_OBJECT
public:
	PlanetViewer(QWidget *parent = 0);
	~PlanetViewer();

	QString centerPlanet() const { return CenterPlanet; }
	void setCenterPlanet( const QString &cp ) { CenterPlanet = cp; }
	
	KPlotObject* planetObject(uint i) const { return planet[i]; }
	QString planetName(uint i) const { return pName[i]; }

protected:
	virtual void keyPressEvent( QKeyEvent *e );
	virtual void paintEvent( QPaintEvent* );

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
	double scale;
	bool isClockRunning;
	QTimer tmr;
	int UpdateInterval[9], LastUpdate[9];
	QString pName[9], pColor[9];
	QString CenterPlanet;

	QList<KSPlanetBase*> PlanetList;

	KPlotObject *ksun;
	KPlotObject *planet[9];
	KPlotObject *planetLabel[9];
};

#endif
