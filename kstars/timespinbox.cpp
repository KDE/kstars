/***************************************************************************
                          timespinbox.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Tue Sep 4 2001
    copyright            : (C) 2001 by Jason Harris
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

#include <qlineedit.h>

#include <kdebug.h>
#include <klocale.h>

#include "timespinbox.h"

TimeSpinBox::TimeSpinBox( QWidget *parent, const char *name )
  : QSpinBox ( -10000, 10000, 10 /* step */, parent, name ),
	floatValue(1.0)
{

	setValidator(0);
	setSuffix( i18n( "abbreviation for seconds of time", " sec" ) );
	setButtonSymbols( QSpinBox::PlusMinus );
	editor()->setReadOnly( false );

	setValue(10);

	connect(this,  SIGNAL( valueChanged( int ) ), this, SLOT( setStepSize(int) ));
	connect(this,  SIGNAL( valueChanged( int ) ), this, SLOT( reportChange(int) ));
}

void TimeSpinBox::setStepSize(int x) {
	int ss = 1;
	x = abs(x)/10;
	while (x) {
		x /= 10;
		ss *= 10;
	}
	if (ss == x) ss /= 10;

	setLineStep(ss);
}

QString TimeSpinBox::mapValueToText( int value ) {
	return QString("%1.%2").arg(value/10).arg(value%10);
}

int TimeSpinBox::mapTextToValue( bool* ok ) {
	*ok = true;
	return int(text().toFloat()*10);
}

void TimeSpinBox::reportChange( int x) {
	float newval = float(x) / 10.0;
	if (newval != floatValue) {
		floatValue =  newval;
		kdDebug() << "reporting change to value = " << floatValue << endl;
		emit scaleChanged(floatValue);
	}
}

void TimeSpinBox::changeScale(float x) {
	if (x != floatValue) {
		int i = int(x * 10);
		setValue(i);
	}
}

#include "timespinbox.moc"
