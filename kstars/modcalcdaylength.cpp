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
#include "timebox.h"
#include "skyobject.h"
#include "ksutils.h"
#include "kssun.h"
#include "geolocation.h"
#include "kstars.h"
#include "ksnumbers.h"

#include <qwidget.h>
#include <qlabel.h>
#include <qvbox.h>
#include <qgroupbox.h>
#include <qlineedit.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qstring.h>
#include <qdatetime.h>
#include <klocale.h>

#include <kapplication.h>

modCalcDayLength::modCalcDayLength(QWidget *parentSplit, const char *name) : QVBox(parentSplit,name) {

	rightBox = new QVBox (parentSplit);

//  Buttons 
//
	QGroupBox * InputBox = new QGroupBox (rightBox);
	InputBox->setTitle( i18n("Input Data") );
	
	QVBoxLayout * D00Lay = new QVBoxLayout( InputBox );

	QHBox * buttonBox = new QHBox(InputBox);
	
	QPushButton * Compute = new QPushButton( i18n( "Compute" ), buttonBox);
	QPushButton * Clear = new QPushButton( i18n( "Clear" ), buttonBox );
	
	Compute->setFixedHeight(25);
	Compute->setMaximumWidth(100);
	
	Clear->setFixedHeight(25);
	Clear->setMaximumWidth(100);
//
// Geo coords

	QHBox * geoBox = new QHBox(InputBox);

	QHBox * l0Box = new QHBox(geoBox);
	l0Box->setMaximumWidth(150);

	QLabel * longLabel = new QLabel(l0Box);
	longLabel->setText( i18n( "Geographical Longitude","Longitude:") );
	longBox = new dmsBox(l0Box,"LongBox");

	QHBox * l1Box = new QHBox(geoBox);
	l1Box->setMaximumWidth(150);

	QLabel * latLabel = new QLabel(l1Box);
	latLabel->setText( i18n( "Geographical Latitude","Latitude:") );
	latBox = new dmsBox(l1Box,"LatBox");

// Date

	QHBox * dateBox = new QHBox(InputBox);
	dateBox->setMaximumWidth(120);

	QLabel * dateLabel = new QLabel(dateBox);
	dateLabel->setText( i18n( "Date:") );
	datBox = new timeBox(dateBox,"dateBox",FALSE);

	showCurrentDate();
 	initGeo();
	showLongLat();

	buttonBox->setMargin(10);
	
	D00Lay->setMargin(14);
	D00Lay->setSpacing(4);
	D00Lay->addWidget(buttonBox);
	D00Lay->addWidget(geoBox);
	D00Lay->addWidget(dateBox);

// Results

	QGroupBox * resultsBox = new QGroupBox (rightBox);
	resultsBox->setTitle( i18n("Positions and times") );

	QVBoxLayout * D0Lay = new QVBoxLayout( resultsBox );

	QGridLayout * DGLay = new QGridLayout(3, 4);
	
	QLabel * riseTimeLabel = new QLabel(resultsBox);
	riseTimeLabel->setText(i18n("Sunrise:"));
	riseTimeBox = new timeBox(resultsBox,"riseTimeBox");

	QLabel * azRiseLabel = new QLabel(resultsBox);
	azRiseLabel->setText(i18n("Azimuth:"));
	azRiseBox = new dmsBox(resultsBox,"azRiseBox");

	QLabel * transitTimeLabel = new QLabel(resultsBox);
	transitTimeLabel->setText(i18n("Noon:"));
	transitTimeBox = new timeBox(resultsBox,"noonTimeBox");

	QLabel * elTransitLabel = new QLabel(resultsBox);
	elTransitLabel->setText(i18n("Elevation:"));
	elTransitBox = new dmsBox(resultsBox,"elTransitBox");

	QLabel * setTimeLabel = new QLabel(resultsBox);
	setTimeLabel->setText(i18n("Sunset:"));
	setTimeBox = new timeBox(resultsBox,"setTimeBox");

	QLabel * azSetLabel = new QLabel(resultsBox);
	azSetLabel->setText(i18n("Azimuth:"));
	azSetBox = new dmsBox(resultsBox,"azSetBox");

	
	DGLay->addWidget(riseTimeLabel,0,0);
	DGLay->addWidget(riseTimeBox,0,1);

	DGLay->addWidget(azRiseLabel,0,2);
	DGLay->addWidget(azRiseBox,0,3);

	DGLay->addWidget(transitTimeLabel,1,0);
	DGLay->addWidget(transitTimeBox,1,1);
	
	DGLay->addWidget(elTransitLabel,1,2);
	DGLay->addWidget(elTransitBox,1,3);

	DGLay->addWidget(setTimeLabel,2,0);
	DGLay->addWidget(setTimeBox,2,1);

	DGLay->addWidget(azSetLabel,2,2);
	DGLay->addWidget(azSetBox,2,3);

	DGLay->setMargin(14);
	DGLay->setSpacing(4);
 	
  QHBox * dLBox = new QHBox(resultsBox);
  dLBox->setMaximumWidth(150);

	QLabel * dayLLabel = new QLabel(dLBox);
	dayLLabel->setText(i18n("Day length:"));
	dayLBox = new timeBox(dLBox,"daylBox");

	D0Lay->setMargin(14);
	D0Lay->setSpacing(4);

	D0Lay->addLayout(DGLay);
	D0Lay->addWidget(dLBox);

//  D0Lay->activate();

	QSpacerItem * downSpacer = new QSpacerItem(400,300);
	QVBox * noBox = new QVBox (rightBox);
	QVBoxLayout * D2Lay = new QVBoxLayout( noBox);
	D2Lay->addItem(downSpacer);
	
	rightBox->setMaximumWidth(550);
	rightBox->setMinimumWidth(400);
	rightBox->setMargin(14);
	rightBox->setSpacing(7);
	rightBox->show();
//
// slots


	connect( Compute, SIGNAL(clicked() ), this, SLOT( slotComputePosTime() ) ) ;
	connect( Clear, SIGNAL(clicked() ), this, SLOT( slotClearCoords() ) ) ;
	
}

modCalcDayLength::~modCalcDayLength(){
	delete rightBox;
}

void modCalcDayLength::showCurrentDate (void)
{
	QDateTime dt = QDateTime::currentDateTime();

	datBox->showDate( dt.date() );

}

void modCalcDayLength::initGeo(void)
{
	KStars *ks = (KStars*)kapp->mainWidget();
	geoPlace = new GeoLocation( ks->geo() );
}

void modCalcDayLength::showLongLat(void)
{

	KStars *ks = (KStars*)kapp->mainWidget();
	longBox->show( ks->geo()->lng() );
	latBox->show( ks->geo()->lat() );
}

void modCalcDayLength::getGeoLocation (void)
{
	geoPlace->setLong( longBox->createDms() );
	geoPlace->setLat(  latBox->createDms() );
	geoPlace->setHeight( 0.0);

}

QDateTime modCalcDayLength::getQDateTime (void)
{
	QDateTime dt ( datBox->createDate() , QTime(8,0,0) );

	return dt;
}

long double modCalcDayLength::computeJdFromCalendar (void)
{
	long double julianDay;

	julianDay = KSUtils::UTtoJulian( getQDateTime() );

	return julianDay;
}

void modCalcDayLength::slotClearCoords(){
 	
	azSetBox->clearFields();
	azRiseBox->clearFields();
	elTransitBox->clearFields();

	setTimeBox->clearFields();
	riseTimeBox->clearFields();
	transitTimeBox->clearFields();
	
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

	long double jd0 = computeJdFromCalendar();
	
	KSNumbers * num = new KSNumbers(jd0);
	KSSun *Sun = new KSSun();
	Sun->findPosition(num);

	QTime setQtime = Sun->setTime(jd0 , geoPlace);
	QTime riseQtime = Sun->riseTime(jd0 , geoPlace);
	QTime transitQtime = Sun->transitTime(jd0 , geoPlace);

	dms setAz = Sun->riseSetTimeAz(jd0, geoPlace, false);
	dms riseAz = Sun->riseSetTimeAz(jd0, geoPlace, true);
	dms transAlt = Sun->transitAltitude(jd0, geoPlace);

	azSetBox->show( setAz );
	elTransitBox->show( transAlt );
	azRiseBox->show( riseAz );

	setTimeBox->showTime( setQtime );
	riseTimeBox->showTime( riseQtime );
	transitTimeBox->showTime( transitQtime );

	QTime dayLQtime = lengthOfDay (setQtime,riseQtime);

	dayLBox->showTime( dayLQtime );

	delete num;

}
