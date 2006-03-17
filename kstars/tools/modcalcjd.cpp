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

//#include <q3datetimeedit.h>  //need for QTimeEdit
#include <qradiobutton.h>
#include <klineedit.h>
#include <klocale.h>
#include <kglobal.h>

#include "kstarsdatetime.h"

#define MJD0 2400000.5

modCalcJD::modCalcJD(QWidget *parentSplit) : QFrame(parentSplit) {
	setupUi(this);
	showCurrentTime();
	show();
}

modCalcJD::~modCalcJD(void)
{
}

void modCalcJD::slotComputeTime (void)
{
	
	if(DateRadio->isChecked()) {
		computeFromCalendar();
	} else if (JdRadio->isChecked()) {
		JDBox->setFocus();
		computeFromJd();
	} else if (MjdRadio->isChecked()) {
		ModJDBox->setFocus();
		computeFromMjd();
	}
}

void modCalcJD::computeFromCalendar (void)
{
	long double julianDay, modjulianDay;
	
	julianDay = getDateTime().djd();
	showJd( julianDay );

	modjulianDay = julianDay - MJD0;
	showMjd(modjulianDay);
}

void modCalcJD::computeFromMjd (void)
{
	long double julianDay, modjulianDay;

	modjulianDay = KGlobal::locale()->readNumber( ModJDBox->text() );
	julianDay = MJD0 + modjulianDay;
	showJd( julianDay );
	computeFromJd();
}

void modCalcJD::computeFromJd (void)
{
	long double julianDay, modjulianDay;
	julianDay = KGlobal::locale()->readNumber( JDBox->text() );
	KStarsDateTime dt( julianDay );

	DateTimeBox->setDateTime( dt );

	modjulianDay = julianDay - MJD0;
	showMjd( modjulianDay );
}


void modCalcJD::slotClearTime (void)
{
	JDBox->setText (QString());
	ModJDBox->setText (QString());
	DateTimeBox->setDateTime( ExtDateTime::currentDateTime() );
}

void modCalcJD::showCurrentTime (void)
{
	KStarsDateTime dt = KStarsDateTime::currentDateTime();
	DateTimeBox->setDateTime( dt );
}

KStarsDateTime modCalcJD::getDateTime (void)
{
	return KStarsDateTime( DateTimeBox->dateTime() );
}

void modCalcJD::showJd(long double julianDay)
{
	JDBox->setText(KGlobal::locale()->formatNumber( (double)julianDay, 5 ) );
}

void modCalcJD::showMjd(long double modjulianDay)
{
	ModJDBox->setText(KGlobal::locale()->formatNumber( (double)modjulianDay, 5 ) );
}
