/***************************************************************************
                          timestepbox.h  -  description
                             -------------------
    begin                : Sat Apr 13 2002
    copyright            : (C) 2002 by Jason Harris
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

#ifndef TIMESTEPBOX_H
#define TIMESTEPBOX_H

#include <qframe.h>
#include <qlayout.h>
#include "timespinbox.h"

#define NUNITS 8

/**
  *@author Jason Harris
  */

class TimeStepBox : public QFrame  {
Q_OBJECT
public:
	TimeStepBox( QWidget *parent=0, const char* name=0 );
	~TimeStepBox() {}
	TimeSpinBox* tsbox() const { return timeBox; }
	QSpinBox* unitbox() const { return unitBox; }
signals:
	void scaleChanged( float );
private slots:
	void changeUnits( int );
	void syncUnits( int );
private:
	int UnitStep[NUNITS];
	QHBoxLayout *hlay;
	TimeSpinBox *timeBox;
	QSpinBox *unitBox;
};

#endif
