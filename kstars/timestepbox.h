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

/**TimeStepBox consists of a TimeSpinBox, coupled with a second QSpinBox.
	*The second spinbox is sized so that only the up/down buttons are visible.
	*These buttons skip timesteps to the next time units, to allow the user
	*to more quickly navigate through the 82 possible time steps.
	*@short Composite spinbox for specifying a time step.
  *@author Jason Harris
	*@version 0.9
  */

class TimeStepBox : public QFrame  {
Q_OBJECT
public:
/**Constructor. */
	TimeStepBox( QWidget *parent=0, const char* name=0 );
/**Destructor. (empty)*/
	~TimeStepBox() {}

/**@returns a pointer to the child TimeSpinBox */
	TimeSpinBox* tsbox() const { return timeBox; }

/**@returns a pointer to the child QSpinBox (which performs the large unit steps) */
	QSpinBox* unitbox() const { return unitBox; }
signals:
	void scaleChanged( float );
private slots:
/**Set the TimeSpinBox value according to the current UnitBox value.
	*This is connected to the UnitBox valueChanged() Signal. */
	void changeUnits( int );

/**Make sure the current UnitBox value represents the correct units for the
	*current TimeBox value. This slot is connected to the TimeBox valueChanged() Slot. */
	void syncUnits( int );
private:
	int UnitStep[NUNITS];
	QHBoxLayout *hlay;
	TimeSpinBox *timeBox;
	QSpinBox *unitBox;
};

#endif
