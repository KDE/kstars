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
#include <klocale.h>
#include <qtooltip.h>

#include "timestepbox.h"

TimeStepBox::TimeStepBox( QWidget *parent, const char* name )
	: QFrame( parent, name ) {
	timeBox = new TimeSpinBox( this );

	unitBox = new QSpinBox( -NUNITS, NUNITS, 1, this );
	unitBox->setMaximumWidth( unitBox->downRect().width() );
	unitBox->setValidator( 0 );
	unitBox->setButtonSymbols( QSpinBox::UpDownArrows );
//editor() is protected...if we want this, we'll have to subclass QSpinBox again
//	unitBox->editor()->setReadOnly( true );
	unitBox->setValue( 0 ); //Start out with seconds units

	QToolTip::add( timeBox, i18n( "Adjust Time Step" ) );
	QToolTip::add( unitBox, i18n( "Adjust Time Step Units" ) );

	hlay = new QHBoxLayout( this, 2, 0 );
	hlay->addWidget( timeBox );
	hlay->addWidget( unitBox );
	hlay->activate();

	UnitStep[0] = 0;
	UnitStep[1] = 4;
	UnitStep[2] = 10;
	UnitStep[3] = 16;
	UnitStep[4] = 21;
	UnitStep[5] = 25;
	UnitStep[6] = 28;
	UnitStep[7] = 34;

	connect( unitBox, SIGNAL( valueChanged( int ) ), this, SLOT( changeUnits( int ) ) );
	connect( timeBox, SIGNAL( valueChanged( int ) ), this, SLOT( syncUnits( int ) ) );
	connect( timeBox, SIGNAL( scaleChanged( float ) ), this, SIGNAL( scaleChanged( float ) ) );

}

void TimeStepBox::changeUnits( int value ) {
	timeBox->setValue( value >= 0 ? UnitStep[ value ] : -1*UnitStep[ abs(value) ] );
}

void TimeStepBox::syncUnits( int tstep ) {
	unsigned int i;
	for ( i=NUNITS-1; i>=0; --i )
		if ( abs(tstep) >= UnitStep[i] ) break;

//don't want setValue to trigger changeUnits()...
	disconnect( unitBox, SIGNAL( valueChanged( int ) ), this, SLOT( changeUnits( int ) ) );
	unitBox->setValue( tstep < 0 ? -i : i );
	connect( unitBox, SIGNAL( valueChanged( int ) ), this, SLOT( changeUnits( int ) ) );
}

#include "timestepbox.moc"
