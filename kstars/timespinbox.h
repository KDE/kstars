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
#define TSB_NVALUES 14

/**
	*A spinbox for adjusting the timestep used by kstars.  It displays decimal
	*values like MagnitudeSpinBox, but it only takes certain specific values:
	*-100, -50, -20, -10, -5, -2, -1, 1, 2, 5, 10, 20, 50, 100
	*However, the user may manually enter any float value they wish.
  *@author Jason Harris
	*@version 0.9
  */

class TimeSpinBox : public QSpinBox  {
Q_OBJECT
public:
	/**Constructor*/
	TimeSpinBox( QWidget *parent, const char *name=0 );

	/**Destructor (empty)*/
	~TimeSpinBox() {}

	/**Overloaded function to set displayed string based on the value of the internal counter
		*@param value the spinbox value to be translated to a displayed string
		*@returns the string to display for the value given as an argument
		*/
	virtual QString mapValueToText( int value );
	virtual int mapTextToValue( bool *ok);

	signals:
		void scaleChanged(float s);

	public slots:
		void changeScale(float s);

protected slots:
	void setStepSize(int x);
	void reportChange(int x);

private:
	float floatValue;

};

#endif
