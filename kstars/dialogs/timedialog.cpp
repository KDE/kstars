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


#include "timedialog.h"

#include <klocale.h>
#include <QPushButton>
#include <kdatepicker.h>

#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QTimeEdit>

#include "kstarsdatetime.h"
#include "kstarsdata.h"
#include "simclock.h"
#include "geolocation.h"

TimeDialog::TimeDialog( const KStarsDateTime &now, GeoLocation *_geo, QWidget *parent, bool UTCFrame )
        : KDialog( parent ), geo( _geo )
{
    UTCNow = UTCFrame;

    QFrame *page = new QFrame(this);
    setMainWidget( page );
    if( UTCNow )
        setCaption( xi18nc( "set clock to a new time", "Set UTC Time" ) );
    else
        setCaption( xi18nc( "set clock to a new time", "Set Time" ) );
    setButtons( KDialog::Ok|KDialog::Cancel );

    vlay = new QVBoxLayout( page );
    vlay->setMargin( 2 );
    vlay->setSpacing( 2 );
    hlay = new QHBoxLayout(); //this layout will be added to the VLayout
    hlay->setSpacing( 2 );

    dPicker = new KDatePicker( now.date(), page );
    tEdit = new QTimeEdit( now.time(), page );
    NowButton = new QPushButton( page );
    NowButton->setObjectName( "NowButton" );
    NowButton->setText( UTCNow ? xi18n( "UTC Now"  ) : xi18n( "Now"  ) );

    vlay->addWidget( dPicker, 0, 0 );
    vlay->addLayout( hlay, 0 );

    hlay->addWidget( tEdit );
    hlay->addWidget( NowButton );

    vlay->activate();

    QObject::connect( this, SIGNAL( okClicked() ), this, SLOT( accept() ));
    QObject::connect( this, SIGNAL( cancelClicked() ), this, SLOT( reject() ));
    QObject::connect( NowButton, SIGNAL( clicked() ), this, SLOT( setNow() ));
}

//Add handler for Escape key to close window
//Use keyReleaseEvent because keyPressEvents are already consumed
//by the KDatePicker.
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
    KStarsDateTime dt( KStarsDateTime::currentDateTime( KDateTime::Spec::UTC() ) );
    if ( ! UTCNow )
        dt = geo->UTtoLT( dt );

    dPicker->setDate( dt.date() );
    tEdit->setTime( dt.time() );
}

QTime TimeDialog::selectedTime( void ) {
    return tEdit->time();
}

QDate TimeDialog::selectedDate( void ) {
    return dPicker->date();
}

KStarsDateTime TimeDialog::selectedDateTime( void ) {
    return KStarsDateTime( selectedDate(), selectedTime() );
}

#include "timedialog.moc"
