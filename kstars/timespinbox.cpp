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

#include <qlineedit.h>
#include <klocale.h>
#include "timespinbox.h"

TimeSpinBox::TimeSpinBox( QWidget *parent, const char *name )
  : QSpinBox ( 0, 9, 1 /* step */, parent, name ){

	setValidator(0);
	setSuffix( i18n( " sec" ) );
	setButtonSymbols( QSpinBox::PlusMinus );
	editor()->setReadOnly( true );

	Values[0]=0.1; Values[1]=0.25; Values[2]=0.5; Values[3]=1.; Values[4]=2.;
	Values[5]=5.; Values[6]=10.; Values[7]=20.; Values[8]=50.; Values[9]=100.;
	setValue(3);
}

QString TimeSpinBox::mapValueToText( int index ) {
	return QString("%1").arg(TimeSpinBox::Values[index]);
	
}
