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

MagnitudeSpinBox::MagnitudeSpinBox( QWidget * parent )
        : QDoubleSpinBox(parent)
{
     setRange(0, 10);
     setValue(0);
     setDecimals(1);
     setSingleStep(0.1);
}

MagnitudeSpinBox::MagnitudeSpinBox( double minValue, double maxValue,
                                    QWidget * parent )
        : QDoubleSpinBox( parent )
{
    setRange(minValue, maxValue);
    setValue(minValue);
    setDecimals(1);
    setSingleStep(0.1);
}
