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
#include <qradiobutton.h>
#include <qstring.h>


modCalcAzel::modCalcAzel(QWidget *parentSplit, const char *name) : modCalcAzelDlg (parentSplit,name) {

	showCurrentDateTime();
 	initGeo();
	showLongLat();
	this->show();
}

modCalcAzel::~modCalcAzel(){
    delete geoPlace;
}

SkyPoint modCalcAzel::getEquCoords (void)
{
	dms raCoord, decCoord;

	raCoord = raBox->createDms(FALSE);
	decCoord = decBox->createDms();

	SkyPoint sp = SkyPoint (raCoord, decCoord);

	return sp;
}

SkyPoint modCalcAzel::getHorCoords (void)
{
	dms azCoord, elCoord;

	azCoord = azBox->createDms();
	elCoord = elBox->createDms();

	SkyPoint sp = SkyPoint();

	sp.setAz(azCoord);
	sp.setAlt(elCoord);

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

	julianDay = KSUtils::UTtoJD( getQDateTime() );

	return julianDay;
}

double modCalcAzel::getEpoch (QString eName)
{

	double epoch = eName.toDouble();

	return epoch;
}

dms modCalcAzel::getLongitude(void)
{
	dms longitude;
	longitude = longBox->createDms();
	return longitude;
}

dms modCalcAzel::getLatitude(void)
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

void modCalcAzel::showEquCoords ( SkyPoint sp, long double jd )
{
	raBox->show( sp.ra(), FALSE );
	decBox->show( sp.dec() );
	showEpoch(jd);
}

void modCalcAzel::showEpoch ( long double jd )
{
	double epochN = 0.;
	epochN = KSUtils::JdToEpoch(jd);
//	Localization
//	epochName->setText(KGlobal::locale()->formatNumber(epochN,3));
	epochName->setText( QString("%1").arg(epochN, 0, 'f', 2));
	
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
	long double jd0 = KSUtils::epochToJd ( epoch0 );

	dms lgt = getLongitude();
	dms LST = KSUtils::UTtoLST( getQDateTime(), &lgt );

	SkyPoint sp;

	if(radioApCoords->isChecked()) {

		sp = getEquCoords();
		sp.apparentCoord(jd0, jd);
		dms lat(getLatitude());
		sp.EquatorialToHorizontal( &LST, &lat );
		showHorCoords( sp );

	} else {

		sp = getHorCoords();
		dms lat(getLatitude());
		sp.HorizontalToEquatorial( &LST, &lat );
		showEquCoords( sp, jd );
	}

}
