#ifndef magnitudespinbox_h
#define magnitudespinbox_h

/***************************************************************************
                          magnitudespinbox.h  -  description
                             -------------------
    begin                : Thu Jul 26 2001
    copyright            : (C) 2001 by Heiko Evermann
    email                : heiko@evermann.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


/**A special spinbox for entering magnitude values.
	*Screen representation has one digit after the decimal point
	*valueChanged still has integer as result. Therefore a mapping is
	*used : 0.0 => 0
	*       1.0 => 10 etc.
	*
	*see QT-documentation of class QSpinBox
	*file:/usr/lib/qt2/doc/html/qspinbox.html
	*@short a custom spinbox for magnitude (float) values.
	*@author Heiko Evermann
	*@version 0.9
	*/

#include <qspinbox.h>

class QWidget;

class MagnitudeSpinBox  : public QSpinBox
{
	public:

/**Constructor.  Set minimum and maximum values for the spinbox.
	*/
	MagnitudeSpinBox( int minValue, int maxValue, QWidget* parent = 0, const char* name = 0);

/**Overridden function to set the displayed string according to the
	*internal int value of the spinbox.
	*@param value the internal value to interpret
	*@returns the QString to display in the spinbox (a floating-point magnitude that is equal to value/10)
	*/
	virtual QString mapValueToText( int value );

/**Overridden function to set the internal int value of the spinbox according to the
	*string displayed.
	*@param ok not used (?)	
	*@returns the internal value as parsed from the displayed string.
	*/
	virtual int mapTextToValue( bool* ok );
};


#endif
