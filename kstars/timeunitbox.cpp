/***************************************************************************
                          timeunitbox.cpp  -  description
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

#include <stdlib.h>
#include <qpushbutton.h>
#include <kdebug.h>
#include "timeunitbox.h"
#include "timeunitbox.moc"

TimeUnitBox::TimeUnitBox(QWidget *parent, const char *name ) : QSpinBox(parent,name) {
	setValidator( 0 );
	setButtonSymbols( QSpinBox::UpDownArrows );
	setMinValue( 1-NUNITS );
	setMaxValue( NUNITS-1 );
	setLineStep( 1 );
	setWrapping( false );
	setValue( 0 ); //Start out with seconds units

	UnitStep[0] = 0;
	UnitStep[1] = 4;
	UnitStep[2] = 10;
	UnitStep[3] = 16;
	UnitStep[4] = 21;
	UnitStep[5] = 25;
	UnitStep[6] = 28;
	UnitStep[7] = 34;

//Qt 2.x: why is iconSet still NULL here???  Can't set size of the spinbox because
//I have no way of knowing how wide the up/down buttons are going to be.  grrr....
//	if ( downButton()->iconSet()==NULL ) qDebug( "iconset is null" );
//	setMaximumWidth( downButton()->iconSet()->pixmap().width() );

#if ( QT_VERSION >= 300 )
	setMaximumWidth( downRect().width() );
#else
	setMaximumWidth( 20 );
#endif
}

TimeUnitBox::~TimeUnitBox(){
}

int TimeUnitBox::unitValue() {
	int uval;
	if ( value() >= 0 ) uval = UnitStep[ value() ];
	else uval = -1*UnitStep[ abs( value() ) ];
	return uval;
}

int TimeUnitBox::getUnitValue( int val ) {
	if ( val >= 0 ) return UnitStep[ val ];
	else return -1*UnitStep[ abs( val ) ];
}
