/***************************************************************************
                          jmoontool.h  -  Display overhead view of the solar system
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

#ifndef JMOONTOOL_H
#define JMOONTOOL_H

#include <kdialogbase.h>
#include "kstarsplotwidget.h"

class KStars;

/**@class JMoonTool
	*@short Display the positions of Jupiter's moons as a function of time
	*@version 1.0
	*@author Jason Harris
	*/
class JMoonTool : public KDialogBase
{
Q_OBJECT
public:
	JMoonTool(QWidget *parent = 0, const char *name = 0);
	~JMoonTool();

protected:
	virtual void keyPressEvent( QKeyEvent *e );

private:
	void initPlotObjects();
	KStarsPlotWidget *pw;
	KStars *ksw;
	QString colJp, colIo, colEu, colGn, colCa;
};

#endif
