/***************************************************************************
                          timeunitbox.h  -  description
                             -------------------
    begin                : Sat Apr 27 2002
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

#ifndef TIMEUNITBOX_H
#define TIMEUNITBOX_H

#include <qvbox.h>
#include <qpushbutton.h>
#include <qwidget.h>

#define NUNITS 8

/**TimeUnitBox is a QSpinBox which is just wide enough to show the up/down buttons only, and not
	*the QLineEdit.  These buttons are used to perform large steps in the TimeSpinBox, which
	*is paired with TimeUnitBox to form the composite widget: TimeStepBox.
	*@short provides second set of up/down buttons for TimeStepBox.
  *@author Jason Harris
	*@version 0.9
  */

class TimeUnitBox : public QVBox {
   Q_OBJECT
public: 
	TimeUnitBox(QWidget *parent=0, const char *name=0);
	~TimeUnitBox();
	/**@returns the value of UnitStep for the current spinbox value() */
	int unitValue();

	/**@short the same as unitValue, except you can get the UnitStep for any value, not just the current one.
		*@returns the value of UnitStep for the index value given as an argument.
		*/
	int getUnitValue( int );

	void setValue( int value ) { Value = value; }
	int value() const { return Value; }

signals:
	void valueChanged(int);

private slots:
	void increase();
	void decrease();

private:
	void setMinValue( int minValue ) { MinimumValue = minValue; }
	void setMaxValue( int maxValue ) { MaximumValue = maxValue; }

	int minValue() const { return MinimumValue; }
	int maxValue() const { return MaximumValue; }

	QPushButton *UpButton, *DownButton;
	int MinimumValue, MaximumValue, Value, UnitStep[ NUNITS ];
};

#endif
