/***************************************************************************
                          modcalcdaylength.cpp  -  description
                             -------------------
    begin                : wed jun 12 2002
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

#include "modcalcdaylength.h"
#include "modcalcprec.h"
#include "dms.h"
#include "dmsbox.h"
#include "skyobject.h"
#include "geolocation.h"
#include "kstars.h"
#include "timebox.h"
#include "kssun.h"
#include "ksnumbers.h"
#include "kstarsdatetime.h"
#include "libkdeedu/extdate/extdatetimeedit.h"

#include <kapplication.h> 

modCalcDayLength::modCalcDayLength(QWidget *parentSplit, const char *name) : modCalcDayLengthDlg(parentSplit,name) {
	showCurrentDate();
	initGeo();
	show();
}

modCalcDayLength::~modCalcDayLength() {}

void modCalcDayLength::showCurrentDate (void)
{
	KStarsDateTime dt( KStarsDateTime::currentDateTime() );
	datBox->setDate( dt.date() );
}

void modCalcDayLength::initGeo(void)
{
	KStars *ks = (KStars*) parent()->parent()->parent(); // QSplitter->AstroCalc->KStars
	geoPlace = ks->geo();
	longBox->show( geoPlace->lng() );
	latBox->show( geoPlace->lat() );
}

void modCalcDayLength::getGeoLocation (void)
{
	geoPlace->setLong( longBox->createDms() );
	geoPlace->setLat(  latBox->createDms() );
	geoPlace->setHeight( 0.0);
}

KStarsDateTime modCalcDayLength::getDateTime (void)
{
	return KStarsDateTime( datBox->date() , QTime(8,0,0) );
}

void modCalcDayLength::slotClearCoords(){

	azSetBox->clearFields();
	azRiseBox->clearFields();
	elTransitBox->clearFields();

	// reset to current date
	datBox->setDate(ExtDate::currentDate());

// reset times to 00:00:00
	setTimeBox->clearFields();
	riseTimeBox->clearFields();
	transitTimeBox->clearFields();

//	dayLBox->setTime(time);
	dayLBox->clearFields();
}

QTime modCalcDayLength::lengthOfDay(QTime setQTime, QTime riseQTime){
	QTime dL(0,0,0);
	int dds = riseQTime.secsTo(setQTime);
	QTime dLength = dL.addSecs( dds );

	return dLength;
}

void modCalcDayLength::slotComputePosTime()
{
	long double jd0 = getDateTime().djd();
	getGeoLocation();

	KSNumbers * num = new KSNumbers(jd0);
	KSSun *Sun = new KSSun(((KStars*) parent()->parent()->parent())->data());
	Sun->findPosition(num);

	QTime setQtime = Sun->riseSetTime( jd0 , geoPlace, false );
	QTime riseQtime = Sun->riseSetTime( jd0 , geoPlace, true );
	QTime transitQtime = Sun->transitTime(jd0 , geoPlace);

	dms setAz = Sun->riseSetTimeAz(jd0, geoPlace, false);
	dms riseAz = Sun->riseSetTimeAz(jd0, geoPlace, true);
	dms transAlt = Sun->transitAltitude(jd0, geoPlace);

	if (setQtime.isValid() ) {
		azSetBox->show( setAz );
		elTransitBox->show( transAlt );
		azRiseBox->show( riseAz );

		setTimeBox->showTime( setQtime );
		riseTimeBox->showTime( riseQtime );
		transitTimeBox->showTime( transitQtime );

		QTime dayLQtime = lengthOfDay (setQtime,riseQtime);

		dayLBox->showTime( dayLQtime );
	} else if (transAlt.Degrees() > 0. ) {
		azSetBox->setDMS(i18n("Circumpolar"));
		elTransitBox->show( transAlt );
		azRiseBox->setDMS(i18n("Circumpolar"));

		setTimeBox->showTime( setQtime );
		riseTimeBox->showTime( riseQtime );
		transitTimeBox->showTime( transitQtime );

		dayLBox->setEntry("24:00:00");

	} else if (transAlt.Degrees() < 0. ) {
		azSetBox->setDMS("does not rise");
		elTransitBox->setDMS("does not rise");
		azRiseBox->setDMS("does not rise");

		setTimeBox->clearFields();
		riseTimeBox->clearFields();
		transitTimeBox->clearFields();

		dayLBox->showTime( QTime(0,0,0) );
	}

	delete num;
}

#include "modcalcdaylength.moc"
