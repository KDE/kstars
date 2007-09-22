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

#ifndef JMOONTOOL_H_
#define JMOONTOOL_H_

#include <kdialog.h>

class KStars;
class QColor;
class KPlotWidget;

/**@class JMoonTool
	*@short Display the positions of Jupiter's moons as a function of time
	*@version 1.0
	*@author Jason Harris
	*/
class JMoonTool : public KDialog
{
    Q_OBJECT
public:
    JMoonTool(QWidget *parent = 0);
    ~JMoonTool();

protected:
    virtual void keyPressEvent( QKeyEvent *e );

private:
    void initPlotObjects();
    KPlotWidget *pw;
    KStars *ksw;
    QColor colJp, colIo, colEu, colGn, colCa;
};

#endif
