/***************************************************************************
                          modcalcapcoord.cpp  -  description
                             -------------------
    begin                : Wed Apr 10 2002
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

#include "modcalcapcoord.h"
#include "modcalcapcoord.moc"
#include "modcalcprec.h"
#include "dms.h"
#include "dmsbox.h"
#include "skypoint.h"
#include "ksutils.h"
#include <klineedit.h>
#include <qdatetimeedit.h>
#include <klocale.h>

//#include <kapplication.h> ..already included in modcalcapcoord.h

modCalcApCoord::modCalcApCoord(QWidget *parentSplit, const char *name) : modCalcApCoordDlg(parentSplit,name) {

	showCurrentTime();
	this->show();

}

modCalcApCoord::~modCalcApCoord(){
}

SkyPoint modCalcApCoord::getEquCoords (void) {
	dms raCoord, decCoord;

	raCoord = ra0Box->createDms(FALSE);
	decCoord = dec0Box->createDms();

	SkyPoint sp = SkyPoint (raCoord, decCoord);

	return sp;
}

void modCalcApCoord::showCurrentTime (void)
{
	QDateTime dt = QDateTime::currentDateTime();

	datBox->setDate( dt.date() );
	timBox->setTime( dt.time() );
}

QDateTime modCalcApCoord::getQDateTime (void)
{
	QDateTime dt ( datBox->date() , timBox->time() );

	return dt;
}

long double modCalcApCoord::computeJdFromCalendar (void)
{
	long double julianDay;

	julianDay = KSUtils::UTtoJulian( getQDateTime() );

	return julianDay;
}

double modCalcApCoord::getEpoch (QString eName) {
	
	double epoch = eName.toDouble();

	return epoch;
}

void modCalcApCoord::showEquCoords ( SkyPoint sp ) {
	rafBox->show( sp.ra() , FALSE);
	decfBox->show( sp.dec() );
}

long double modCalcApCoord::epochToJd (double epoch) {

	double yearsTo2000 = 2000.0 - epoch;

	if (epoch == 1950.0) {
		return 2433282.4235;
	} else if ( epoch == 2000.0 ) {
		return J2000;
	} else {
		return ( J2000 - yearsTo2000 * 365.2425 );
	}

}

void modCalcApCoord::slotClearCoords(){
 	
	ra0Box->clearFields();
	dec0Box->clearFields();
	rafBox->clearFields();
	decfBox->clearFields();
	epoch0Name->setText("");
	datBox->setDate(QDate::currentDate());
	timBox->setTime(QTime(0,0,0));
}

void modCalcApCoord::slotComputeCoords(){

	long double jd = computeJdFromCalendar();
	double epoch0 = getEpoch( epoch0Name->text() );
	long double jd0 = epochToJd ( epoch0 );
	
	SkyPoint sp;
	sp = getEquCoords();

	sp.apparentCoord(jd0, jd);
	showEquCoords( sp );

}
