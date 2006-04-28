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

#include <qlabel.h>
#include <qpushbutton.h>
#include <qspinbox.h>
#include <qlayout.h>
//Added by qt3to4:
#include <QVBoxLayout>
#include <Q3Frame>
#include <QHBoxLayout>
#include <QKeyEvent>

#include "timedialog.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "simclock.h"
#include "libkdeedu/extdate/extdatepicker.h"

TimeDialog::TimeDialog( const KStarsDateTime &now, QWidget *parent )
	: KDialog( parent, i18nc( "set clock to a new time", "Set Time" ), 
		KDialog::Ok|KDialog::Cancel )
{
	QFrame *page = new QFrame(this);
	setMainWidget( page );

	vlay = new QVBoxLayout( page );
	vlay->setMargin( 2 );
	vlay->setSpacing( 2 );
	hlay = new QHBoxLayout(); //this layout will be added to the VLayout
	hlay->setSpacing( 2 );
	dPicker = new ExtDatePicker( page );
	dPicker->setDate( now.date() );

	HourBox = new QSpinBox( page );
	HourBox->setObjectName( "HourBox" );
	QFont Box_font(  HourBox->font() );
	Box_font.setBold( true );
	HourBox->setFont( Box_font );
	HourBox->setWrapping( true );
	HourBox->setMaximum( 23 );
	HourBox->setButtonSymbols( QSpinBox::PlusMinus );
	HourBox->setValue( now.time().hour() );

	TextLabel1 = new QLabel( page );
	TextLabel1->setObjectName( "TextLabel1" );
	TextLabel1->setText( " :" );
	TextLabel1->setFont( Box_font );
	
	MinuteBox = new QSpinBox( page );
	MinuteBox->setObjectName( "MinuteBox" );
	QFont MinuteBox_font(  MinuteBox->font() );
	MinuteBox->setFont( Box_font );
	MinuteBox->setWrapping( true );
	MinuteBox->setMaximum( 59 );
	MinuteBox->setButtonSymbols( QSpinBox::PlusMinus );
	MinuteBox->setValue( now.time().minute() );
	
	TextLabel1_2 = new QLabel( page );
	TextLabel1_2->setObjectName( "TextLabel1_2" );
	TextLabel1_2->setFont( Box_font );
	
	SecondBox = new QSpinBox( page );
	SecondBox->setObjectName( "SecondBox" );
	SecondBox->setFont( Box_font );
	SecondBox->setMaximum( 59 );
	SecondBox->setWrapping( true );
	SecondBox->setButtonSymbols( QSpinBox::PlusMinus );
	SecondBox->setValue( now.time().second() );
	
	NowButton = new QPushButton( page );
	NowButton->setObjectName( "NowButton" );
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
	
	QObject::connect( this, SIGNAL( okClicked() ), this, SLOT( accept() ));
	QObject::connect( this, SIGNAL( cancelClicked() ), this, SLOT( reject() ));
	QObject::connect( NowButton, SIGNAL( clicked() ), this, SLOT( setNow() ));
	QObject::connect( HourBox, SIGNAL( valueChanged( int ) ), this, SLOT( HourPrefix( int ) ));
	QObject::connect( MinuteBox, SIGNAL( valueChanged( int ) ), this, SLOT( MinutePrefix( int ) ));
	QObject::connect( SecondBox, SIGNAL( valueChanged( int ) ), this, SLOT( SecondPrefix( int ) ));
}

//Add handler for Escape key to close window
//Use keyReleaseEvent because keyPressEvents are already consumed
//by the ExtDatePicker.
void TimeDialog::keyReleaseEvent( QKeyEvent *kev ) {
	switch( kev->key() ) {
		case Qt::Key_Escape:
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
  QTime t = dt.time();

  HourBox->setValue( t.hour() );
  MinuteBox->setValue( t.minute() );
  SecondBox->setValue( t.second() );
}

void TimeDialog::HourPrefix( int value ) {
	HourBox->setPrefix( QString() );
	if ( value < 10 ) HourBox->setPrefix( "0" );
}

void TimeDialog::MinutePrefix( int value ) {
	MinuteBox->setPrefix( QString() );
	if ( value < 10 ) MinuteBox->setPrefix( "0" );
}

void TimeDialog::SecondPrefix( int value ) {
	SecondBox->setPrefix( QString() );
	if ( value < 10 ) SecondBox->setPrefix( "0" );
}

QTime TimeDialog::selectedTime( void ) {
	QTime t( HourBox->value(), MinuteBox->value(), SecondBox->value() );
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
