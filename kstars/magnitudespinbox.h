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

/**@class MagnitudeSpinBox
	*A special spinbox for entering magnitude values.
	*This class now inherits KDoubleNumInput instead of QSpinBox
	*@short a custom spinbox for magnitude (float) values.
	*@author Heiko Evermann
	*@version 1.0
	*/

#include <knuminput.h>

class QWidget;

class MagnitudeSpinBox  : public KDoubleNumInput
{
	public:

/**Default Constructor.
	*/
	MagnitudeSpinBox( QWidget* parent = 0, const char* name = 0);

/**Constructor.  Set minimum and maximum values for the spinbox.
	*/
	MagnitudeSpinBox( double minValue, double maxValue, QWidget* parent = 0, const char* name = 0);
};


#endif
