/***************************************************************************
                          modcalcvizequinox.cpp  -  description
                             -------------------
    begin                : Thu 22 Feb 2007
    copyright            : (C) 2007 by Jason Harris
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

#include <math.h> //fabs()
#include <KPlotPoint>
#include "modcalcvizequinox.h"

#include "dms.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "ksnumbers.h"
#include "kssun.h"
#include "widgets/dmsbox.h"
#include "libkdeedu/extdate/extdatetimeedit.h"
#include "kplotobject.h"
#include "kplotaxis.h"
#include "kplotwidget.h"

modCalcEquinox::modCalcEquinox(QWidget *parentSplit)
	: QFrame(parentSplit), dSpring(), dSummer(), dAutumn(), dWinter() {
	setupUi(this);

	connect( Year, SIGNAL( valueChanged(int) ), this, SLOT( slotCompute() ) );

	KStars *ks = (KStars*) topLevelWidget()->parent();

	Plot->axis(KPlotWidget::LeftAxis)->setLabel( i18n("Sun's Declination") );
	Plot->setTopPadding( 40 );
	//Don't draw Top & Bottom axes; custom axes drawn as plot objects
	Plot->axis(KPlotWidget::BottomAxis)->setVisible( false );
	Plot->axis(KPlotWidget::TopAxis)->setVisible( false );

	long double jd0 = KStarsDateTime( ExtDate(Year->value(), 1, 1), QTime(0,0,0) ).djd(); 
	for ( int imonth=0; imonth < 12; imonth++ ) {
		KStarsDateTime kdt( ExtDate(Year->value(), imonth+1, 1), QTime(0,0,0) );
		DMonth[imonth] = kdt.djd() - jd0;
	}

	//This will call slotCompute():
	Year->setValue( ks->data()->lt().date().year() );

	show();
}

modCalcEquinox::~modCalcEquinox(){
}

double modCalcEquinox::dmonth(int i) {
	if ( i>=0 && i<12 )
		return DMonth[i];
	else
		return 0.0;
}

void modCalcEquinox::slotCompute()
{
	KStarsDateTime tSpring, tSummer, tAutumn, tWinter, tSpring2;

	KStars *ks = (KStars*) topLevelWidget()->parent();
	KSSun Sun( ks->data() );
	int year0 = Year->value();

	Plot->removeAllPlotObjects();

	//Add the celestial equator, just a single line bisecting the plot horizontally
	KPlotObject *ce = new KPlotObject( ks->data()->colorScheme()->colorNamed( "EqColor" ), KPlotObject::Lines, 2.0 );
	ce->addPoint( 0.0, 0.0 );
	ce->addPoint( 366.0, 0.0 );
	Plot->addPlotObject( ce );

	//Add Ecliptic.  This is more complicated than simply incrementing the 
	//ecliptic longitude, because we want the x-axis to be time, not RA.
	//For each day in the year, compute the Sun's position.
	KPlotObject *ecl = new KPlotObject( ks->data()->colorScheme()->colorNamed( "EclColor" ), KPlotObject::Lines, 2 );
	ecl->setLinePen( QPen( ecl->pen().color(), 4 ) );

	KStarsDateTime dt( ExtDate(year0, 1, 1), QTime(0,0,0) );
	long double jd0 = dt.djd(); //save JD on Jan 1st
	Plot->setLimits( 1.0, double(dt.date().daysInYear()), -30.0, 30.0 );

	//Add top and bottom axis lines, and custom tickmarks at each month
	addDateAxes();

	for ( int i=1; i<=dt.date().daysInYear(); i++ ) {
		KSNumbers num( dt.djd() );
		Sun.findPosition( &num );
		ecl->addPoint( double(i), Sun.dec()->Degrees() );

		dt = dt.addDays( 1 );
	}
	Plot->addPlotObject( ecl );

	dSpring = findEquinox( true, ecl ); 
	dSummer = findSolstice( true );
	dAutumn = findEquinox( false, ecl );
	dWinter = findSolstice( false );

	//Add vertical dotted lines at times of the equinoxes and solstices
	KPlotObject *poSpring = new KPlotObject( Qt::white, KPlotObject::Lines, 1 );
	poSpring->setLinePen( QPen( Qt::white, 1.0, Qt::DotLine ) );
	poSpring->addPoint( dSpring.djd()-jd0, Plot->dataRect().top() );
	poSpring->addPoint( dSpring.djd()-jd0, Plot->dataRect().bottom() );
	Plot->addPlotObject( poSpring );
	KPlotObject *poSummer = new KPlotObject( Qt::white, KPlotObject::Lines, 1 );
	poSummer->setLinePen( QPen( Qt::white, 1.0, Qt::DotLine ) );
	poSummer->addPoint( dSummer.djd()-jd0, Plot->dataRect().top() );
	poSummer->addPoint( dSummer.djd()-jd0, Plot->dataRect().bottom() );
	Plot->addPlotObject( poSummer );
	KPlotObject *poAutumn = new KPlotObject( Qt::white, KPlotObject::Lines, 1 );
	poAutumn->setLinePen( QPen( Qt::white, 1.0, Qt::DotLine ) );
	poAutumn->addPoint( dAutumn.djd()-jd0, Plot->dataRect().top() );
	poAutumn->addPoint( dAutumn.djd()-jd0, Plot->dataRect().bottom() );
	Plot->addPlotObject( poAutumn );
	KPlotObject *poWinter = new KPlotObject( Qt::white, KPlotObject::Lines, 1 );
	poWinter->setLinePen( QPen( Qt::white, 1.0, Qt::DotLine ) );
	poWinter->addPoint( dWinter.djd()-jd0, Plot->dataRect().top() );
	poWinter->addPoint( dWinter.djd()-jd0, Plot->dataRect().bottom() );
	Plot->addPlotObject( poWinter );
}

//Add custom top/bottom axes with tickmarks for each month
void modCalcEquinox::addDateAxes() {
	KPlotObject *poTopAxis = new KPlotObject( Qt::white, KPlotObject::Lines, 1 );
	poTopAxis->addPoint( 0.0, Plot->dataRect().bottom() ); //y-axis is reversed!
	poTopAxis->addPoint( 366.0, Plot->dataRect().bottom() );
	Plot->addPlotObject( poTopAxis );

	KPlotObject *poBottomAxis = new KPlotObject( Qt::white, KPlotObject::Lines, 1 );
	poBottomAxis->addPoint( 0.0, Plot->dataRect().top() + 0.02 );
	poBottomAxis->addPoint( 366.0, Plot->dataRect().top() + 0.02 );
	Plot->addPlotObject( poBottomAxis );

	//Tick mark for each month
	long double jd0 = KStarsDateTime( ExtDate(Year->value(), 1, 1), QTime(0,0,0) ).djd(); 
	for ( int imonth=0; imonth<12; imonth++ ) {
		KPlotObject *poMonth = new KPlotObject( Qt::white, KPlotObject::Lines, 1 );
		poMonth->addPoint( dmonth(imonth), Plot->dataRect().top() );
		poMonth->addPoint( dmonth(imonth), Plot->dataRect().top() + 1.4 );
		Plot->addPlotObject( poMonth );
		poMonth = new KPlotObject( Qt::white, KPlotObject::Lines, 1 );
		poMonth->addPoint( dmonth(imonth), Plot->dataRect().bottom() );
		poMonth->addPoint( dmonth(imonth), Plot->dataRect().bottom() - 1.4 );
		Plot->addPlotObject( poMonth );
	}
}

KStarsDateTime modCalcEquinox::findEquinox( bool Spring, KPlotObject *ecl ) {
	//Interpolate to find the moment when the Sun crosses the equator in March
	int month = 3;
	if ( ! Spring ) month = 9;
	int i = ExtDate( Year->value(), month, 16 ).dayOfYear();
	double dec1, dec2;
	dec2 = ecl->points()[i]->y();
	do {
		++i;
		dec1 = dec2;
		dec2 = ecl->points()[i]->y();
	} while ( dec1*dec2 > 0.0 ); //when dec1*dec2<0.0, we bracket the zero

	double x1 = ecl->points()[i-1]->x();
	double x2 = ecl->points()[i]->x();
	double d = fabs(dec2 - dec1);
	double f = 1.0 - fabs(dec2)/d; //fractional distance of the zero, from point1 to point2 

	KStarsDateTime dt0( ExtDate( Year->value(), 1, 1 ), QTime(0,0,0) );
	KStarsDateTime dt = dt0.addSecs( 86400.0*(x1-1 + f*(x2-x1)) ); 
	return dt;
}

KStarsDateTime modCalcEquinox::findSolstice( bool Summer ) {
	//Find the moment when the Sun reaches maximum declination
	//First find three points which bracket the maximum (i.e., x2 > x1,x3)
	//Start at June 16th, which will always be approaching the solstice

	KStars *ks = (KStars*) topLevelWidget()->parent();
	long double jd1,jd2,jd3,jd4;
	double y1(0.0),y2(0.0),y3(0.0), y4(0.0);
	int month = 6;
	if ( ! Summer ) month = 12;

	jd3 = KStarsDateTime( ExtDate( Year->value(), month, 16 ), QTime(0,0,0) ).djd();
	KSNumbers num( jd3 );
	KSSun Sun( ks->data() );
	Sun.findPosition( &num );
	y3 = Sun.dec()->Degrees();
		
	int sgn = 1;
	if ( ! Summer ) sgn = -1; //find minimum if the winter solstice is sought

	do {
		jd3 += 1.0;
		num.updateValues( jd3 );
		Sun.findPosition( &num );
		y1 = y2;
		y2 = y3;
		Sun.findPosition( &num );
		y3 = Sun.dec()->Degrees();
	} while ( y3*sgn > y2*sgn );

	//Ok, now y2 is larger(smaller) than both y3 and y1.
	jd2 = jd3 - 1.0;
	jd1 = jd3 - 2.0;

	//Choose a new starting jd2 that follows the golden ratio:
	// a/b = 1.618; a+b = 2...a = 0.76394
	jd2 = jd1 + 0.76394;
	num.updateValues( jd2 );
	Sun.findPosition( &num );
	y2 = Sun.dec()->Degrees();

	while ( jd3 - jd1 > 0.0005 ) { //sub-minute pecision
		jd4 = jd1 + jd3 - jd2;

		num.updateValues( jd4 );
		Sun.findPosition( &num );
		y4 = Sun.dec()->Degrees();

		if ( y4*sgn > y2*sgn ) { //make jd4 the new center
			if ( jd4 > jd2 ) {
				jd1 = jd2;
				y1 = y2;
				jd2 = jd4;
				y2 = y4;
			} else {
				jd3 = jd2;
				y3 = y2;
				jd2 = jd4;
				y2 = y4;
			}
		} else { //make jd4 a new endpoint
			if ( jd4 > jd2 ) {
				jd3 = jd4;
				y3 = y4;
			} else {
				jd1 = jd4;
				y1 = y4;
			}
		}
	}

	return KStarsDateTime( jd2 );
}

#include "modcalcvizequinox.moc"
