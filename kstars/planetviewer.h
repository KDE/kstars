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
#include "plotwidget.h"

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

public slots:
	void slotZoomIn();
	void slotZoomOut();

protected:
	virtual void paintEvent( QPaintEvent *e );
	virtual void keyPressEvent( QKeyEvent *e );

private:
	void initPlotObjects();
	PlotWidget *pw;
};

#endif
