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


// a special spinbox for entering magnitude values.
// Screen representation has one digit after the decimal point
// valueChanged still has integer as result. Therefore a mapping is
// used : 0.0 => 0
//        1.0 => 10 etc.
//
// see QT-documentation of class QSpinBox
// file:/usr/lib/qt2/doc/html/qspinbox.html

#include <qspinbox.h>

class QWidget;

class MagnitudeSpinBox  : public QSpinBox
{
	public:	
	MagnitudeSpinBox( int minValue, int maxValue, QWidget* parent = 0, const char* name = 0);
	
	virtual QString mapValueToText( int value );
	virtual int mapTextToValue( bool* ok );
};

