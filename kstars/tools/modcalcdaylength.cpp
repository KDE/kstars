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

#include "skyobject.h"
#include "geolocation.h"
#include "kstars.h"
#include "kssun.h"
#include "ksmoon.h"
#include "ksnumbers.h"
#include "kstarsdatetime.h"
#include "locationdialog.h"
#include "widgets/dmsbox.h"
#include "widgets/timebox.h"
#include "libkdeedu/extdate/extdatetimeedit.h"

modCalcDayLength::modCalcDayLength(QWidget *parentSplit) 
: QFrame(parentSplit) {
	setupUi(this);
	showCurrentDate();
	initGeo();
	slotComputeAlmanac();

	connect( Date, SIGNAL(dateChanged(const ExtDate&)), this, SLOT(slotComputeAlmanac() ) );
	connect( Location, SIGNAL( clicked() ), this, SLOT( slotLocation() ) );

	show();
}

modCalcDayLength::~modCalcDayLength() {}

void modCalcDayLength::showCurrentDate (void)
{
	KStarsDateTime dt( KStarsDateTime::currentDateTime() );
	Date->setDate( dt.date() );
}

void modCalcDayLength::initGeo(void)
{
	KStars *ks = (KStars*) topLevelWidget()->parent();
	geoPlace = ks->geo();
	Location->setText( geoPlace->fullName() );
}

QTime modCalcDayLength::lengthOfDay(QTime setQTime, QTime riseQTime){
	QTime dL(0,0,0);
	int dds = riseQTime.secsTo(setQTime);
	QTime dLength = dL.addSecs( dds );

	return dLength;
}

void modCalcDayLength::slotLocation() {
	KStars *ks = (KStars*) topLevelWidget()->parent();
	LocationDialog ld(ks);

	if ( ld.exec() == QDialog::Accepted ) {
		GeoLocation *newGeo = ld.selectedCity();
		if ( newGeo ) {
			geoPlace = newGeo;
			Location->setText( geoPlace->fullName() );
		}
	}

	slotComputeAlmanac();
}

void modCalcDayLength::slotComputeAlmanac()
{
	long double jd0 = KStarsDateTime(Date->date(), QTime(8,0,0)).djd();
	KSNumbers * num = new KSNumbers(jd0);

	//Sun
	KSSun *Sun = new KSSun( ((KStars*) topLevelWidget()->parent())->data() );	Sun->findPosition(num);

	QTime ssTime = Sun->riseSetTime( jd0 , geoPlace, false );
	QTime srTime = Sun->riseSetTime( jd0 , geoPlace, true );
	QTime stTime = Sun->transitTime(jd0 , geoPlace);

	dms ssAz = Sun->riseSetTimeAz(jd0, geoPlace, false);
	dms srAz = Sun->riseSetTimeAz(jd0, geoPlace, true);
	dms stAlt = Sun->transitAltitude(jd0, geoPlace);

	//In most cases, the Sun will rise and set:
	if ( ssTime.isValid() ) {
		SunSetAz->setText( ssAz.toDMSString() );
		SunTransitAlt->setText( stAlt.toDMSString() );
		SunRiseAz->setText( srAz.toDMSString() );

		SunSet->setText( ssTime.toString("HH:mm") );
		SunRise->setText( srTime.toString("HH:mm") );
		SunTransit->setText( stTime.toString("HH:mm") );

		QTime daylength = lengthOfDay(ssTime,srTime);
		DayLength->setText( daylength.toString("HH:mm") );

	//...but not always!
	} else if ( stAlt.Degrees() > 0. ) {
		SunSetAz->setText( i18n("Circumpolar") );
		SunTransitAlt->setText( stAlt.toDMSString() );
		SunRiseAz->setText( i18n("Circumpolar") );

		SunSet->setText( "--:--" );
		SunRise->setText( "--:--" );
		SunTransit->setText( stTime.toString("HH:mm") );

		DayLength->setText("24:00");

	} else if (stAlt.Degrees() < 0. ) {
		SunSetAz->setText( i18n("Does not rise") );
		SunTransitAlt->setText( stAlt.toDMSString() );
		SunRiseAz->setText( i18n("Does not set") );

		SunSet->setText( "--:--" );
		SunRise->setText( "--:--" );
		SunTransit->setText( stTime.toString("HH:mm") );


		DayLength->setText( "00:00" );
	}

	//Moon
	KSMoon *Moon = new KSMoon( ((KStars*) topLevelWidget()->parent())->data() );
	Moon->findPosition(num);
	Moon->findPhase(Sun);

	QTime msTime = Moon->riseSetTime( jd0 , geoPlace, false );
	QTime mrTime = Moon->riseSetTime( jd0 , geoPlace, true );
	QTime mtTime = Moon->transitTime(jd0 , geoPlace);

	dms msAz = Moon->riseSetTimeAz(jd0, geoPlace, false);
	dms mrAz = Moon->riseSetTimeAz(jd0, geoPlace, true);
	dms mtAlt = Moon->transitAltitude(jd0, geoPlace);

	//In most cases, the Moon will rise and set:
	if ( msTime.isValid() ) {
		MoonSetAz->setText( msAz.toDMSString() );
		MoonTransitAlt->setText( mtAlt.toDMSString() );
		MoonRiseAz->setText( mrAz.toDMSString() );

		MoonSet->setText( msTime.toString("HH:mm") );
		MoonRise->setText( mrTime.toString("HH:mm") );
		MoonTransit->setText( mtTime.toString("HH:mm") );

	//...but not always!
	} else if ( mtAlt.Degrees() > 0. ) {
		MoonSetAz->setText( i18n("Circumpolar") );
		MoonTransitAlt->setText( mtAlt.toDMSString() );
		MoonRiseAz->setText( i18n("Circumpolar") );

		MoonSet->setText( "--:--" );
		MoonRise->setText( "--:--" );
		MoonTransit->setText( mtTime.toString("HH:mm") );

	} else if ( mtAlt.Degrees() < 0. ) {
		MoonSetAz->setText( i18n("Does not rise") );
		MoonTransitAlt->setText( mtAlt.toDMSString() );
		MoonRiseAz->setText( i18n("Does not set") );

		MoonSet->setText( "--:--" );
		MoonRise->setText( "--:--" );
		MoonTransit->setText( mtTime.toString("HH:mm") );
	}

	LunarPhase->setText( Moon->phaseName()+" ("+QString::number( int( 100*Moon->illum() ) )+"%)" );


	delete num;
	delete Sun;
	delete Moon;
}

#include "modcalcdaylength.moc"
