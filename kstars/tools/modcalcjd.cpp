/***************************************************************************
                          modcalcjd.cpp  -  description
                             -------------------
    begin                : Tue Jan 15 2002
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

#include "modcalcjd.h"
#include "modcalcjd.moc"
#include "libkdeedu/extdate/extdatetimeedit.h"

#include <qdatetimeedit.h>  //need for QTimeEdit
#include <qradiobutton.h>
#include <klineedit.h>
#include <klocale.h>
#include <kglobal.h>

#include "ksutils.h"

//#include <kapplication.h> ...already included in modcalcjd.h

modCalcJD::modCalcJD(QWidget *parentSplit, const char *name) : modCalcJdDlg(parentSplit,name) {
	
	showCurrentTime();
	this->show();
}

modCalcJD::~modCalcJD(void)
{
}

void modCalcJD::slotComputeTime (void)
{
	
	if(DateRadio->isChecked()) {
		computeFromCalendar();
	} else if (JdRadio->isChecked()) {
		JdName->setFocus();
		computeFromJd();
	} else if (MjdRadio->isChecked()) {
		MjdName->setFocus();
		computeFromMjd();
	}

}

void modCalcJD::computeFromCalendar (void)
{
	long double julianDay, modjulianDay;
	
	julianDay = KSUtils::UTtoJD( getExtDateTime() );
	showJd(julianDay);

	modjulianDay = julianDay - 2400000.5;
	showMjd(modjulianDay);
}

void modCalcJD::computeFromMjd (void)
{
	long double julianDay, modjulianDay;

	modjulianDay = KGlobal::locale()->readNumber( MjdName->text() );
	julianDay = 	2400000.5 + modjulianDay;
	showJd(julianDay);
	computeFromJd();
	
}
void modCalcJD::computeFromJd (void)
{
	long double julianDay, modjulianDay;

	ExtDateTime dt;

	julianDay = KGlobal::locale()->readNumber( JdName->text() );
	dt = KSUtils::JDtoUT( julianDay );

	datBox->setDate( dt.date() );
	timBox->setTime( dt.time() );

	modjulianDay = julianDay - 2400000.5;
	showMjd(modjulianDay);
}


void modCalcJD::slotClearTime (void)
{
	JdName->setText ("");
	MjdName->setText ("");
	datBox->setDate(ExtDate::currentDate());
	timBox->setTime(QTime(0,0,0));
}

void modCalcJD::showCurrentTime (void)
{
	ExtDateTime dt = ExtDateTime::currentDateTime();
	datBox->setDate( dt.date() );
	timBox->setTime( dt.time() );
}

ExtDateTime modCalcJD::getExtDateTime (void)
{
	ExtDateTime dt ( datBox->date() , timBox->time() );
	return dt;
}

void modCalcJD::showJd(long double julianDay)
{
	JdName->setText(KGlobal::locale()->formatNumber( (double)julianDay, 5 ) );
}

void modCalcJD::showMjd(long double modjulianDay)
{
	MjdName->setText(KGlobal::locale()->formatNumber( (double)modjulianDay, 5 ) );
}
