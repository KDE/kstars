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
#include "kstars.h"
#include "kstarsdata.h"
#include "geolocation.h"
#include "libkdeedu/extdate/extdatetimeedit.h"

#include <qdatetimeedit.h>  //need for QTimeEdit
#include <qradiobutton.h>
#include <klineedit.h>
#include <klocale.h>
#include <kglobal.h>

#include "kstarsdatetime.h"

#define MJD0 2400000.5

modCalcJD::modCalcJD(QWidget *parentSplit, const char *name) : modCalcJdDlg(parentSplit,name) {
	
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
	
	julianDay = getDateTime().djd();
	showJd( julianDay );

	modjulianDay = julianDay - MJD0;
	showMjd(modjulianDay);
}

void modCalcJD::computeFromMjd (void)
{
	long double julianDay, modjulianDay;

	modjulianDay = KGlobal::locale()->readNumber( MjdName->text() );
	julianDay = MJD0 + modjulianDay;
	showJd( julianDay );
	computeFromJd();
}

void modCalcJD::computeFromJd (void)
{
	long double julianDay, modjulianDay;
	julianDay = KGlobal::locale()->readNumber( JdName->text() );
	KStarsDateTime dt( julianDay );

	datBox->setDate( dt.date() );
	timBox->setTime( dt.time() );

	modjulianDay = julianDay - MJD0;
	showMjd( modjulianDay );
}


void modCalcJD::slotClearTime (void)
{
	JdName->setText ("");
	MjdName->setText ("");
	datBox->setDate( ExtDate::currentDate() );
	timBox->setTime(QTime(0,0,0));
}

void modCalcJD::showCurrentTime (void)
{
	KStars *ks = (KStars*) parent()->parent()->parent();

	KStarsDateTime dt = ks->data()->geo()->LTtoUT( KStarsDateTime::currentDateTime() );
	datBox->setDate( dt.date() );
	timBox->setTime( dt.time() );
	computeFromCalendar();
}

KStarsDateTime modCalcJD::getDateTime (void)
{
	return KStarsDateTime( datBox->date() , timBox->time() );
}

void modCalcJD::showJd(long double julianDay)
{
	JdName->setText(KGlobal::locale()->formatNumber( (double)julianDay, 5 ) );
}

void modCalcJD::showMjd(long double modjulianDay)
{
	MjdName->setText(KGlobal::locale()->formatNumber( (double)modjulianDay, 5 ) );
}
