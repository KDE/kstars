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
#include <kdebug.h>
#include "timeunitbox.h"
#include "timeunitbox.moc"
#include <qpushbutton.h>


TimeUnitBox::TimeUnitBox(QWidget *parent, const char *name, bool daysonly )
	: QVBox( parent, name ) {

	UpButton = new QPushButton( "+", this );
	UpButton->setMaximumWidth( 22 );
	UpButton->setMaximumHeight( 10 );
	DownButton = new QPushButton( "-", this );
	DownButton->setMaximumWidth( 22 );
	DownButton->setMaximumHeight( 10 );

	setDaysOnly( daysonly );

	connect( UpButton, SIGNAL( clicked() ), this, SLOT( increase() ) );
	connect( DownButton, SIGNAL( clicked() ), this, SLOT( decrease() ) );
}

TimeUnitBox::~TimeUnitBox(){
}

void TimeUnitBox::setDaysOnly( bool daysonly ) {
	if ( daysonly ) {
		setMinValue( 1-DAYUNITS );
		setMaxValue( DAYUNITS-1 );
		setValue( 1 ); // Start out with days units
	
		UnitStep[0] = 0;
		UnitStep[1] = 1;
		UnitStep[2] = 5;
		UnitStep[3] = 8;
		UnitStep[4] = 14;
	} else {
		setMinValue( 1-ALLUNITS );
		setMaxValue( ALLUNITS-1 );
		setValue( 1 ); // Start out with seconds units
	
		UnitStep[0] = 0;
		UnitStep[1] = 4;
		UnitStep[2] = 10;
		UnitStep[3] = 16;
		UnitStep[4] = 21;
		UnitStep[5] = 25;
		UnitStep[6] = 28;
		UnitStep[7] = 34;
	}
}

void TimeUnitBox::increase() {
	if ( value() < maxValue() ) {
		setValue( value()+1 );
		emit valueChanged( value() );
	}
}

void TimeUnitBox::decrease() {
	if ( value() > minValue() ) {
		setValue( value()-1 );
		emit valueChanged( value() );
	}
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
