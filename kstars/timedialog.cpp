/***************************************************************************
                          timedialog.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 11 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include <klocale.h>

#include <tqlabel.h>
#include <tqpushbutton.h>
#include <tqspinbox.h>
#include <tqlayout.h>

#include "timedialog.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "simclock.h"
#include "libkdeedu/extdate/extdatepicker.h"

TimeDialog::TimeDialog( const KStarsDateTime &now, TQWidget* parent )
    : KDialogBase( KDialogBase::Plain, i18n( "set clock to a new time", "Set Time" ), Ok|Cancel, Ok, parent )
{
	ksw = (KStars*) parent;
	TQFrame *page = plainPage();

	vlay = new TQVBoxLayout( page, 2, 2 );
	hlay = new TQHBoxLayout( 2 ); //this layout will be added to the VLayout
	dPicker = new ExtDatePicker( page );
	dPicker->setDate( now.date() );

	HourBox = new TQSpinBox( page, "HourBox" );
	TQFont Box_font(  HourBox->font() );
	Box_font.setBold( TRUE );
	HourBox->setFont( Box_font );
	HourBox->setWrapping( TRUE );
	HourBox->setMaxValue( 23 );
	HourBox->setButtonSymbols( TQSpinBox::PlusMinus );
	HourBox->setValue( now.time().hour() );

	TextLabel1 = new TQLabel( page, "TextLabel1" );
	TextLabel1->setText( " :" );
	TextLabel1->setFont( Box_font );
	
	MinuteBox = new TQSpinBox( page, "MinuteBox" );
	TQFont MinuteBox_font(  MinuteBox->font() );
	MinuteBox->setFont( Box_font );
	MinuteBox->setWrapping( TRUE );
	MinuteBox->setMaxValue( 59 );
	MinuteBox->setButtonSymbols( TQSpinBox::PlusMinus );
	MinuteBox->setValue( now.time().minute() );
	
	TextLabel1_2 = new TQLabel( page, "TextLabel1_2" );
	TextLabel1_2->setFont( Box_font );
	
	SecondBox = new TQSpinBox( page, "SecondBox" );
	SecondBox->setFont( Box_font );
	SecondBox->setMaxValue( 59 );
	SecondBox->setWrapping( TRUE );
	SecondBox->setButtonSymbols( TQSpinBox::PlusMinus );
	SecondBox->setValue( now.time().second() );
	
	NowButton = new TQPushButton( page, "NowButton" );
	NowButton->setText( i18n( "Now"  ) );
	NowButton->setFont( Box_font );
	
	vlay->addWidget( dPicker, 0, 0 );
	vlay->addLayout( hlay, 0 );
	
	hlay->addWidget( HourBox, 0, 0 );
	hlay->addWidget( TextLabel1, 0, 0 );
	hlay->addWidget( MinuteBox, 0, 0 );
	hlay->addWidget( TextLabel1_2, 0, 0 );
	hlay->addWidget( SecondBox, 0, 0 );
	hlay->addWidget( NowButton );
	
	vlay->activate();
	
	TQObject::connect( this, TQT_SIGNAL( okClicked() ), this, TQT_SLOT( accept() ));
	TQObject::connect( this, TQT_SIGNAL( cancelClicked() ), this, TQT_SLOT( reject() ));
	TQObject::connect( NowButton, TQT_SIGNAL( clicked() ), this, TQT_SLOT( setNow() ));
	TQObject::connect( HourBox, TQT_SIGNAL( valueChanged( int ) ), this, TQT_SLOT( HourPrefix( int ) ));
	TQObject::connect( MinuteBox, TQT_SIGNAL( valueChanged( int ) ), this, TQT_SLOT( MinutePrefix( int ) ));
	TQObject::connect( SecondBox, TQT_SIGNAL( valueChanged( int ) ), this, TQT_SLOT( SecondPrefix( int ) ));
}

//Add handler for Escape key to close window
//Use keyReleaseEvent because keyPressEvents are already consumed
//by the ExtDatePicker.
void TimeDialog::keyReleaseEvent( TQKeyEvent *kev ) {
	switch( kev->key() ) {
		case Key_Escape:
		{
			close();
			break;
		}

		default: { kev->ignore(); break; }
	}
}

void TimeDialog::setNow( void )
{
  KStarsDateTime dt( KStarsDateTime::currentDateTime() );

  dPicker->setDate( dt.date() );
  TQTime t = dt.time();

  HourBox->setValue( t.hour() );
  MinuteBox->setValue( t.minute() );
  SecondBox->setValue( t.second() );
}

void TimeDialog::HourPrefix( int value ) {
	HourBox->setPrefix( "" );
	if ( value < 10 ) HourBox->setPrefix( "0" );
}

void TimeDialog::MinutePrefix( int value ) {
	MinuteBox->setPrefix( "" );
	if ( value < 10 ) MinuteBox->setPrefix( "0" );
}

void TimeDialog::SecondPrefix( int value ) {
	SecondBox->setPrefix( "" );
	if ( value < 10 ) SecondBox->setPrefix( "0" );
}

TQTime TimeDialog::selectedTime( void ) {
	TQTime t( HourBox->value(), MinuteBox->value(), SecondBox->value() );
	return t;
}

ExtDate TimeDialog::selectedDate( void ) {
	ExtDate d( dPicker->date() );
	return d;
}

KStarsDateTime TimeDialog::selectedDateTime( void ) {
	return KStarsDateTime( selectedDate(), selectedTime() );
}

#include "timedialog.moc"
