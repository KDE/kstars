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
#include "kstarsplotwidget.h"

class PVPlotWidget : public KStarsPlotWidget
{
Q_OBJECT
public:
	PVPlotWidget( double x1, double x2, double y1, double y2, 
			QWidget *parent=0, const char *name=0 );
	~PVPlotWidget();

public slots:
	void slotZoomIn();
	void slotZoomOut();

protected:
	virtual void keyPressEvent( QKeyEvent *e );
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
	PlanetViewer(QWidget *parent = 0, const char *name = 0);
	~PlanetViewer();

protected:
	virtual void keyPressEvent( QKeyEvent *e );
	virtual void paintEvent( QPaintEvent* );

private slots:
	void initPlotObjects();

private:
	PVPlotWidget *pw;
};

#endif
