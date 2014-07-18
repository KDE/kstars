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

#include "timeunitbox.h"


#include <cstdlib>

#include <QPushButton>
#include <QVBoxLayout>

#include <QDebug>

static const char * const up_arrow[] = {
"22 11 4 1",
"       c None",
".      c #000000",
"+      c #808080",
"@      c #BFBFBF",
"                      ",
"                      ",
"          ..          ",
"         .++.         ",
"        .+  @.        ",
"       .+    @.       ",
"      .+      @.      ",
"     .+        @.     ",
"    .+          @.    ",
"    +            @    ",
"                      "};

static const char * const down_arrow[] = {
"22 11 4 1",
"       c None",
".      c #808080",
"+      c #BFBFBF",
"@      c #000000",
"                      ",
"    .            +    ",
"    @.          +@    ",
"     @.        +@     ",
"      @.      +@      ",
"       @.    +@       ",
"        @.  +@        ",
"         @..@         ",
"          @@          ",
"                      ",
"                      "};

TimeUnitBox::TimeUnitBox(QWidget *parent, bool daysonly )
        : QWidget( parent ) {

    QVBoxLayout *vlay = new QVBoxLayout(this);
    vlay->setMargin(0);
    vlay->setSpacing(0);

    UpButton = new QPushButton( QPixmap(up_arrow), "", this );
    UpButton->setMaximumWidth( 26 );
    UpButton->setMaximumHeight( 13 );
    DownButton = new QPushButton( QPixmap(down_arrow), "", this );
    DownButton->setMaximumWidth( 26 );
    DownButton->setMaximumHeight( 13 );

    vlay->addWidget( UpButton );
    vlay->addWidget( DownButton );
    //	setLayout( vlay );

    setDaysOnly( daysonly );

    connect( UpButton, SIGNAL( clicked() ), this, SLOT( increase() ) );
    connect( DownButton, SIGNAL( clicked() ), this, SLOT( decrease() ) );
}

TimeUnitBox::~TimeUnitBox(){
}

void TimeUnitBox::setDaysOnly( bool daysonly ) {
    if ( daysonly ) {
        setMinimum( 1-DAYUNITS );
        setMaximum( DAYUNITS-1 );
        setValue( 1 ); // Start out with days units

        UnitStep[0] = 0;
        UnitStep[1] = 1;
        UnitStep[2] = 5;
        UnitStep[3] = 8;
        UnitStep[4] = 14;
    } else {
        setMinimum( 1-ALLUNITS );
        setMaximum( ALLUNITS-1 );
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
