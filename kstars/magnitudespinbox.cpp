/***************************************************************************
                          magnitudespinbox.cpp  -  description
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

#include <magnitudespinbox.h>

MagnitudeSpinBox::MagnitudeSpinBox( int minValue, int maxValue, QWidget * parent , const char * name )
	: QSpinBox ( minValue, maxValue, 1 /* step */, parent, name)
{
	// otherwise a decimal point cannot be entered. One could also try a float validator,
	// but that was too difficult tonight.
	setValidator(0);

	// another simplification: Only accept values >=0. That means that negative magnitudes cannot be entered.
	// That should not matter too much as the number of stars with a magnitude < 0 (i.e. brighter that ma 0.0)
	// is very limited.
  if ( minValue < 0 ) {
		setMinValue(0);   	
	}
}

QString MagnitudeSpinBox::mapValueToText( int value )
{
	return QString("%1.%2").arg(value/10).arg(value%10);
}

int MagnitudeSpinBox::mapTextToValue( bool* ok )
{
	return int(text().toFloat()*10);
}
