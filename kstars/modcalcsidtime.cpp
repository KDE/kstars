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
	QDateTime dt = QDateTime::currentDateTime();
	datBox->setDate( dt.date() );
	showUT( dt.time() );

	KStars *ks = (KStars*) parent()->parent()->parent();
	 // modCalcSidTimeDlg -> QSplitter->AstroCalc->KStars

	longBox->show( ks->geo()->lng() );
}

QTime modCalcSidTime::computeUTtoST (QTime ut, QDate dt, dms longitude)
{
	QTime lst;

	QDateTime utdt = QDateTime( dt, ut);
	lst = KSUtils::UTtoLST( utdt, &longitude);
	return lst;
}

QTime modCalcSidTime::computeSTtoUT (QTime st, QDate dt, dms longitude)
{
	QTime ut;

	QDateTime dtst = QDateTime( dt, st);
	ut = KSUtils::LSTtoUT( dtst, &longitude);
	return ut;
}

void modCalcSidTime::showUT ( QTime dt )
{
	UtBox->setTime( dt );
}

void modCalcSidTime::showST ( QTime dt )
{
	StBox->setTime( dt );
}

QTime modCalcSidTime::getUT (void) 
{
	QTime dt;
	dt = UtBox->time();
	return dt;
}

QTime modCalcSidTime::getST (void) 
{
	QTime dt;
	dt = StBox->time();
	return dt;
}

QDate modCalcSidTime::getDate (void) 
{
	QDate dt;
	dt = datBox->date();
	return dt;
}

dms modCalcSidTime::getLongitude (void)
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
	QTime ut, st;

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
