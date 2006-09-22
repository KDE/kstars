/***************************************************************************
                          calcframe.h  -  description
                             -------------------
    begin                : Sat 21 Sept 2006
    copyright            : (C) 2006 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef CALCFRAME_H
#define CALCFRAME_H

#include <QFrame>

/**@class CalcFrame is a QFrame with a frameShown() signal.
	*@author Jason Harris
	*@version 1.0
	*/
class CalcFrame : public QFrame {
	Q_OBJECT
public:
	CalcFrame( QWidget *parent=0, const char *name=0 );
	~CalcFrame() {}
	
signals:
	void frameShown();
	
protected:
	void showEvent( QShowEvent * ) { emit frameShown(); }
};

#endif
