/***************************************************************************
                          modcalcsidtime.cpp  -  description
                             -------------------
    begin                : Wed Jan 23 2002
    copyright            : (C) 2002 by Pablo de Vicente
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

#include "ksutils.h"
#include "dmsbox.h"
#include "modcalcsidtime.h"
#include "modcalcsidtime.moc"
#include "kstars.h"

#include <qdatetimeedit.h>
#include <qradiobutton.h>
#include <qdatetime.h>

modCalcSidTime::modCalcSidTime(QWidget *parentSplit, const char *name) : modCalcSidTimeDlg (parentSplit,name) {

	showCurrentTimeAndLong();
	this->show();		
}

modCalcSidTime::~modCalcSidTime(void) {

}

void modCalcSidTime::showCurrentTimeAndLong (void)
{
	KStars *ks = (KStars*) parent()->parent()->parent();
	 // modCalcSidTimeDlg -> QSplitter->AstroCalc->KStars

	showUT( ks->clock()->UTC().time() );
	datBox->setDate( ks->clock()->UTC().date() );

	longBox->show( ks->geo()->lng() );
}

dms modCalcSidTime::computeUTtoST (QTime ut, QDate dt, dms longitude)
{
	QDateTime utdt = QDateTime( dt, ut);
	return KSUtils::UTtoLST( utdt, &longitude);
}

QTime modCalcSidTime::computeSTtoUT (dms st, QDate dt, dms longitude)
{
	QDateTime dtt = QDateTime( dt, QTime());
	return KSUtils::LSTtoUT( st, dtt, &longitude);
}

void modCalcSidTime::showUT( QTime dt )
{
	UtBox->setTime( dt );
}

void modCalcSidTime::showST( dms st )
{
	QTime dt( st.hour(), st.minute(), st.second() );
	StBox->setTime( dt );
}

QTime modCalcSidTime::getUT( void ) 
{
	QTime dt;
	dt = UtBox->time();
	return dt;
}

dms modCalcSidTime::getST( void ) 
{
	QTime dt = StBox->time();
	dms st; st.setH( dt.hour(), dt.minute(), dt.second() );
	return st;
}

QDate modCalcSidTime::getDate( void ) 
{
	QDate dt;
	dt = datBox->date();
	return dt;
}

dms modCalcSidTime::getLongitude( void )
{
	dms longitude;
	longitude = longBox->createDms();
	return longitude;
}

void modCalcSidTime::slotClearFields(){
	datBox->setDate(QDate::currentDate());
	QTime time(0,0,0);
	UtBox->setTime(time);
	StBox->setTime(time);
}

void modCalcSidTime::slotComputeTime(){
	QTime ut;
	dms st;

	QDate dt = getDate();
	dms longitude = getLongitude();

	if(UtRadio->isChecked()) {
		ut = getUT();
		st = computeUTtoST( ut, dt, longitude );
		showST( st );
	} else {
		st = getST();
		ut = computeSTtoUT( st, dt, longitude );
		showUT( ut );
	}

}
