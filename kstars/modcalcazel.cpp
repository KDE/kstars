/***************************************************************************
                          modcalcazel.cpp  -  description
                             -------------------
    begin                : sáb oct 26 2002
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

#include "modcalcazel.h"

#include "modcalcazel.moc"
#include "dms.h"
#include "dmsbox.h"
#include "skypoint.h"
#include "geolocation.h"
#include "ksutils.h"
#include "kstars.h"

#include <qdatetimeedit.h>


modCalcAzel::modCalcAzel(QWidget *parentSplit, const char *name) : modCalcAzelDlg (parentSplit,name) {

	showCurrentDateTime();
 	initGeo();
	showLongLat();
	this->show();
}

modCalcAzel::~modCalcAzel(){

}

SkyPoint modCalcAzel::getEquCoords (void)
{
	dms raCoord, decCoord;

	raCoord = raBox->createDms();
	decCoord = decBox->createDms();

	SkyPoint sp = SkyPoint (raCoord, decCoord);

	return sp;
}

void modCalcAzel::showCurrentDateTime (void)
{

	QDateTime dt = QDateTime::currentDateTime();

	datBox->setDate( dt.date() );
	timBox->setTime( dt.time() );

}

QDateTime modCalcAzel::getQDateTime (void)
{

	QDateTime dt ( datBox->date() , timBox->time() );

	return dt;
}

long double modCalcAzel::computeJdFromCalendar (void)
{
	long double julianDay;

	julianDay = KSUtils::UTtoJulian( getQDateTime() );

	return julianDay;
}

double modCalcAzel::getEpoch (QString eName)
{

	double epoch = eName.toDouble();

	return epoch;
}

dms modCalcAzel::getLongitude (void)
{
	dms longitude;
	longitude = longBox->createDms();
	return longitude;
}

dms modCalcAzel::getLatitude (void)
{
	dms latitude;
	latitude = latBox->createDms();
	return latitude;
}

void modCalcAzel::getGeoLocation (void)
{
	geoPlace->setLong( longBox->createDms() );
	geoPlace->setLat(  latBox->createDms() );
	geoPlace->setHeight( 0.0);

}

void modCalcAzel::initGeo(void)
{
	KStars *ks = (KStars*) parent()->parent()->parent(); // QSplitter->AstroCalc->KStars
	geoPlace = new GeoLocation( ks->geo() );
}



void modCalcAzel::showLongLat(void)
{

	KStars *ks = (KStars*) parent()->parent()->parent(); // QSplitter->AstroCalc->KStars
	longBox->show( ks->geo()->lng() );
	latBox->show( ks->geo()->lat() );
}

void modCalcAzel::showHorCoords ( SkyPoint sp )
{

	azBox->show( sp.az() );
	elBox->show( sp.alt() );

}

long double modCalcAzel::epochToJd (double epoch)
{

	double yearsTo2000 = 2000.0 - epoch;

	if (epoch == 1950.0) {
		return 2433282.4235;
	} else if ( epoch == 2000.0 ) {
		return J2000;
	} else {
		return ( J2000 - yearsTo2000 * 365.2425 );
	}

}

dms modCalcAzel::DateTimetoLST (QDateTime utdt, dms longitude)
{

	QTime lst = KSUtils::UTtoLST( utdt, &longitude);
	dms LSTh = QTimeToDMS (lst);
	
	return LSTh;
}

dms modCalcAzel::QTimeToDMS(QTime qtime) {

	dms tt;
	tt.setH(qtime.hour(), qtime.minute(), qtime.second());
	tt.reduce();

	return tt;
}

void modCalcAzel::slotClearCoords()
{

	raBox->clearFields();
	decBox->clearFields();
	azBox->clearFields();
	elBox->clearFields();
	epochName->setText("");

	datBox->setDate(QDate::currentDate());
	timBox->setTime(QTime(0,0,0));

}

void modCalcAzel::slotComputeCoords()
{

	long double jd = computeJdFromCalendar();
	double epoch0 = getEpoch( epochName->text() );
	long double jd0 = epochToJd ( epoch0 );

	dms LSTh = DateTimetoLST (getQDateTime() , getLongitude() );
	
	SkyPoint sp;
	sp = getEquCoords();

	sp.apparentCoord(jd0, jd);
	sp.EquatorialToHorizontal( &LSTh, &(dms(getLatitude())) );
	showHorCoords( sp );

}
