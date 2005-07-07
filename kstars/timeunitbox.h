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
#include <qwidget.h>

#define ALLUNITS 8
#define DAYUNITS 5

/**@class TimeUnitBox 
	*A pair of buttons, arranged one above the other, labeled "+"/"-".  These buttons 
	*are to be paired with the TimeSpinBox widget.  Their function is to provide 
	*a way to cycle through the possible time steps using larger intervals than the up/down
	*buttons of the TimeSpinBox.  For example, say the Spinbox currently shows a timestep of 
	*"1 sec".  Increasing the timestep with the spinbox up-button will change it to
	*"2 sec", while using the "+" button of this widget will change it to "1 min".
	*
	*The idea is that these "outer" buttons always change to the next even unit of time.
	*
	*@note this widget is not to be used on its own; it is combined with the TimeSpinBox 
	*widget to form the TimeStepBox composite widget. 
	*@short provides a second set of up/down buttons for TimeStepBox.
	*@author Jason Harris
	*@version 1.0
	*/

class QPushButton;

class TimeUnitBox : public QVBox {
   Q_OBJECT
public:
	/**Constructor*/
	TimeUnitBox(QWidget *parent=0, const char *name=0, bool daysonly = false);
	/**Destructor (empty)*/
	~TimeUnitBox();
	/**@return the value of UnitStep for the current spinbox value() */
	int unitValue();

	/**@short the same as unitValue, except you can get the UnitStep for any value, not just the current one.
		*@return the value of UnitStep for the index value given as an argument.
		*/
	int getUnitValue( int );

	/**Set the value which describes which time-unit is displayed in the TimeSpinBox.
		*@p value the new value
		*/
	void setValue( int value ) { Value = value; }
	/**@return the internal value describing the time-unit of the TimeSpinBox.
		*/
	int value() const { return Value; }

	/**Set the minimum value for the internal time-unit value
		*/
	void setMinValue( int minValue ) { MinimumValue = minValue; }
	/**Set the maximum value for the internal time-unit value
		*/
	void setMaxValue( int maxValue ) { MaximumValue = maxValue; }

	/**@return the minimum value for the internal time-unit value
		*/
	int minValue() const { return MinimumValue; }
	/**@return the maximum value for the internal time-unit value
		*/
	int maxValue() const { return MaximumValue; }

	bool daysOnly() const { return DaysOnly; }
	void setDaysOnly( bool daysonly );

signals:
	void valueChanged(int);

private slots:
	/**Increment the internal time-unit value
		*/
	void increase();
	/**Decrement the internal time-unit value
		*/
	void decrease();

private:
	bool DaysOnly;
	QPushButton *UpButton, *DownButton;
	int MinimumValue, MaximumValue, Value, UnitStep[ ALLUNITS ];
};

#endif
