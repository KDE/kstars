/***************************************************************************
                          timespinbox.h  -  K Desktop Planetarium
                             -------------------
    begin                : Tue Sep 4 2001
    copyright            : (C) 2001 by Jason Harris
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




#ifndef TIMESPINBOX_H
#define TIMESPINBOX_H

#include <qspinbox.h>

/**
	*A spinbox for adjusting the timestep used by kstars.  It displays decimal
  *values like MagnitudeSpinBox, but it only takes certain specific values:
  *0.1, 0.25, 0.5, 1, 2, 5, 10, 20, 50, 100
  *@author Jason Harris
  */

class TimeSpinBox : public QSpinBox  {
public: 
	TimeSpinBox( QWidget *parent, const char *name=0 );
	~TimeSpinBox();
	virtual QString mapValueToText( int value );

	float Values[10];
};

#endif
