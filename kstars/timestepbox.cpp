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
#include <kdebug.h>
#include <klocale.h>

#include "timestepbox.h"

TimeStepBox::TimeStepBox( QWidget *parent, const char* name )
	: QFrame( parent, name ) {

	timeBox = new TimeSpinBox( this );
	unitBox = new TimeUnitBox( this );

	QToolTip::add( timeBox, i18n( "Adjust time step" ) );
	QToolTip::add( unitBox, i18n( "Adjust time step units" ) );

	hlay = new QHBoxLayout( this, 2, 0 );
	hlay->addWidget( timeBox );
	hlay->addWidget( unitBox );
	hlay->activate();

	setMaximumWidth( unitBox->width() + timeBox->width() );

	connect( unitBox, SIGNAL( valueChanged( int ) ), this, SLOT( changeUnits() ) );
	connect( timeBox, SIGNAL( valueChanged( int ) ), this, SLOT( syncUnits( int ) ) );
	connect( timeBox, SIGNAL( scaleChanged( float ) ), this, SIGNAL( scaleChanged( float ) ) );

}

void TimeStepBox::changeUnits( void ) {
	timeBox->setValue( unitBox->unitValue() );
}

void TimeStepBox::syncUnits( int tstep ) {
	int i;
	for ( i=NUNITS-1; i>-NUNITS; --i )
		if ( abs(tstep) >= unitBox->getUnitValue( i ) ) break;

//don't want setValue to trigger changeUnits()...
	disconnect( unitBox, SIGNAL( valueChanged( int ) ), this, SLOT( changeUnits() ) );
	unitBox->setValue( tstep < 0 ? -i : i );
	connect( unitBox, SIGNAL( valueChanged( int ) ), this, SLOT( changeUnits() ) );
}

#include "timestepbox.moc"
