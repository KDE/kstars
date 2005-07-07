/***************************************************************************
                          timestepbox.cpp  -  description
                             -------------------
    begin                : Sat Apr 13 2002
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
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <kdebug.h>
#include <klocale.h>

#include "timestepbox.h"

TimeStepBox::TimeStepBox( QWidget *parent, const char* name, bool daysonly )
	: QFrame( parent, name ) {

	timeBox = new TimeSpinBox( this, "timebox", daysonly );
	unitBox = new TimeUnitBox( this, "unitbox", daysonly );

	QToolTip::add( timeBox, i18n( "Adjust time step" ) );
	QToolTip::add( unitBox, i18n( "Adjust time step units" ) );

	QWhatsThis::add( this, i18n( "Set the timescale for the simulation clock.  A setting of \"1 sec\" means the clock advances in real-time, keeping up perfectly with your CPU clock.  Higher values make the simulation clock run faster, lower values make it run slower.  Negative values make it run backwards."
"\n\n"
"There are two pairs of up/down buttons.  The left pair will cycle through all available timesteps in sequence.  Since there are a large number of timesteps, the right pair is provided to skip to the next higher/lower unit of time.  For example, if the timescale is currently \"1 min\", the right up button will make it \"1 hour\", and the right down button will make it \"1 sec\"" ) );
	hlay = new QHBoxLayout( this, 2, 0 );
	hlay->addWidget( timeBox );
	hlay->addWidget( unitBox );
	hlay->activate();

	timeBox->setValue( 4 ); //real-time

	connect( unitBox, SIGNAL( valueChanged( int ) ), this, SLOT( changeUnits() ) );
	connect( timeBox, SIGNAL( valueChanged( int ) ), this, SLOT( syncUnits( int ) ) );
	connect( timeBox, SIGNAL( scaleChanged( float ) ), this, SIGNAL( scaleChanged( float ) ) );

}

void TimeStepBox::changeUnits( void ) {
	timeBox->setValue( unitBox->unitValue() );
}

void TimeStepBox::syncUnits( int tstep ) {
	int i;
	for ( i=unitbox()->maxValue(); i>=unitbox()->minValue(); --i )
		if ( abs(tstep) >= unitBox->getUnitValue( i ) ) break;

//don't want setValue to trigger changeUnits()...
	disconnect( unitBox, SIGNAL( valueChanged( int ) ), this, SLOT( changeUnits() ) );
	unitBox->setValue( tstep < 0 ? -i : i );
	connect( unitBox, SIGNAL( valueChanged( int ) ), this, SLOT( changeUnits() ) );
}

void TimeStepBox::setDaysOnly( bool daysonly ) {
	tsbox()->setDaysOnly( daysonly );
	unitbox()->setDaysOnly( daysonly );
}

#include "timestepbox.moc"
