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

#include "magnitudespinbox.h"

MagnitudeSpinBox::MagnitudeSpinBox( QWidget * parent , const char * name )
	: KDoubleNumInput( 0.0, 10.0, 0.0, 0.1 /*step*/, 1 /*precision*/, parent, name)
{
}

MagnitudeSpinBox::MagnitudeSpinBox( double minValue, double maxValue,
	QWidget * parent , const char * name )
	: KDoubleNumInput( minValue, maxValue, minValue, 0.1 /* step */, 1, parent, name)
{
}
