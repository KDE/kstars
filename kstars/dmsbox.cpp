/***************************************************************************
                          dmsbox.cpp  -  description
                             -------------------
    begin                : wed Dec 19 2001
    copyright            : (C) 2001-2002 by Pablo de Vicente
    email                : vicente@oan.es
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
#include "dmsbox.h"
#include "dms.h"
#include <qlabel.h>
#include <qhbox.h>
#include <qlineedit.h>
#include <qstring.h>
#include <qwidget.h>

dmsBox::dmsBox(QWidget *parent, const char *name, bool dg, int nc) : QHBox(parent,name) {

	
	QHBox * dBox = new QHBox(parent,name);
	
	degreeName = new QLineEdit( dBox,"HourName");
	minuteName = new QLineEdit( dBox,"MinuteName");
	secondName = new QLineEdit( dBox,"SecondName");
	
	dBox->setSpacing(1);
	setStretchFactor(dBox,0);
	dBox->setMargin(6);

	int nw = nc*10;
	// Deg
	degreeName->setMaxLength(nc);
	degreeName->setMaximumWidth(nw);
	// Min
	minuteName->setMaxLength(2);
	minuteName->setMaximumWidth(20);
	//Sec
	secondName->setMaxLength(5);
	secondName->setMaximumWidth(50);

	dBox->setMaximumWidth(110);

	deg = dg;
}

void dmsBox::showInDegrees (dms d)
{
	setDegree( QString("%1").arg(d.degree(),2) );
	setMinute( QString("%1").arg(d.getArcMin(),2) );
	double seconds = d.getArcSec() + d.getmArcSec()/1000.;
	setSecond( QString("%1").arg(seconds,6,'f',3) );

}

void dmsBox::showInHours (dms d)
{
	setDegree( QString("%1").arg(d.hour(),2) );
	setMinute( QString("%1").arg(d.minute(),2) );
	double seconds = d.second() + d.msecond()/1000.;
	setSecond( QString("%1").arg(seconds,6,'f',3) );

}

void dmsBox::show(dms d)
{
	if (deg)
		showInDegrees(d);
	else
		showInHours(d);
}

dms dmsBox::createDms ()
{
	if (deg) {  	
		double D = (double)abs( degrees() ) + (double)minutes()/60. +
			(double)seconds()/3600.;
		if ( degrees() <0 ) {D = -1.0*D;}
		
		return	dms( D );
	}
	else {
		double H = (double)abs( degrees() ) + (double)minutes()/60. +
			(double)seconds()/3600.;
		if ( degrees() <0)
			H = -1.0*H;
		
		dms h;
		h.setH (H);
		return	h;
	}
}

void dmsBox::clearFields (void) {

	setDegree("");
	setMinute("");
	setSecond("");
}

dmsBox::~dmsBox(){
}
