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

#if (QT_VERSION < 300)
#include "timebox.h"
#else
#include <qdatetimeedit.h>
#endif

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

//#include <kapplication.h> ..already included in modcalcapcoord.h

modCalcApCoord::modCalcApCoord(QWidget *parentSplit, const char *name) : QWidget(parentSplit,name) {

	rightBox = new QWidget (parentSplit);
	QVBoxLayout * rightBoxLayout = new QVBoxLayout( rightBox, 12, 6);

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
// Date and Time

	QHBox * datetimeBox = new QHBox(InputBox,"datetimeBox");

	QHBox * d0Box = new QHBox(datetimeBox,"datetimeBox");
	d0Box->setMaximumWidth(100);

	QLabel * timeLabel = new QLabel(d0Box,"timeLabel");
	timeLabel->setText( i18n( "Universal Time","UT:") );
#if (QT_VERSION < 300)
	timBox = new timeBox(d0Box,"timeBox");
#else
	timBox = new QTimeEdit(d0Box,"timeBox");
#endif

	QHBox * d1Box = new QHBox(datetimeBox,"datetimeBox");
	d1Box->setMaximumWidth(140);

	QLabel * dateLabel = new QLabel(d1Box,"dateLabel");
	dateLabel->setText( i18n( "Date:") );
#if (QT_VERSION < 300)
	datBox = new timeBox(d1Box,"dateBox",FALSE);
#else
	datBox = new QDateEdit(d1Box,"dateBox");
#endif

	showCurrentTime();

	buttonBox->setMargin(10);
	
	D00Lay->setMargin(14);
	D00Lay->setSpacing(4);
	D00Lay->addWidget(buttonBox);
	D00Lay->addWidget(datetimeBox);

// RA and DEC

	QGroupBox * origCoordBox = new QGroupBox (rightBox);
	origCoordBox->setTitle( i18n("Original Coordinates") );
	
	QVBoxLayout * D0Lay = new QVBoxLayout( origCoordBox);
	QHBox * ra0dec0Box = new QHBox(origCoordBox);
	
	QLabel * ra0Label = new QLabel(ra0dec0Box,"raLabel");
	ra0Label->setText(i18n("Right ascention","RA:"));
	ra0Box = new dmsBox(ra0dec0Box,"raBox",FALSE);

	QLabel * dec0Label = new QLabel(ra0dec0Box,"decLabel");
	dec0Label->setText(i18n("Declination","DEC:"));
	dec0Box = new dmsBox(ra0dec0Box,"decBox");
	
	QHBox * epoch0Box = new QHBox(ra0dec0Box);	
	QLabel * epoch0Label = new QLabel( epoch0Box, "Epoch" );
	epoch0Label->setText( i18n(" Epoch:") );
	epoch0Name = new QLineEdit( epoch0Box );
	epoch0Name->setMaximumWidth(70);

	epoch0Name->setText( "2000.0" );

	epoch0Box->setMargin(6);
	epoch0Box->setSpacing(6);
	
	D0Lay->setMargin(14);
	D0Lay->setSpacing(4);
	D0Lay->addWidget(ra0dec0Box);

	//Output

	QGroupBox * outputBox = new QGroupBox (rightBox);
	outputBox->setTitle( i18n ("Apparent Coordinates") );

	QVBoxLayout * D1Lay = new QVBoxLayout( outputBox);
	QHBox * rafdecfBox = new QHBox(outputBox);
	
	QLabel * rafLabel = new QLabel(rafdecfBox,"raLabel");
	rafLabel->setText(i18n("Right ascention","RA:"));
	rafBox = new dmsBox(rafdecfBox,"raBox",FALSE);

	QLabel * decfLabel = new QLabel(rafdecfBox,"decLabel");
	decfLabel->setText(i18n("Declination","DEC:"));
	decfBox = new dmsBox(rafdecfBox,"decBox");
	
	D1Lay->setMargin(14);
	D1Lay->addWidget(rafdecfBox);

	QSpacerItem * downSpacer = new QSpacerItem(400,80);

	rightBoxLayout->addWidget(InputBox);
	rightBoxLayout->addWidget(origCoordBox);
	rightBoxLayout->addWidget(outputBox);
	rightBoxLayout->addItem(downSpacer);
	
	rightBox->setMaximumWidth(550);
	rightBox->setMinimumWidth(400);
	rightBox->show();
//
// slots


	connect( Compute, SIGNAL(clicked() ), this, SLOT( slotComputeCoords() ) ) ;
	connect( Clear, SIGNAL(clicked() ), this, SLOT( slotClearCoords() ) ) ;
	
}

modCalcApCoord::~modCalcApCoord(){
	delete rightBox;
}

SkyPoint modCalcApCoord::getEquCoords (void) {
	dms raCoord, decCoord;

	raCoord = ra0Box->createDms();
	decCoord = dec0Box->createDms();

	SkyPoint sp = SkyPoint (raCoord, decCoord);

	return sp;
}

void modCalcApCoord::showCurrentTime (void)
{
	QDateTime dt = QDateTime::currentDateTime();

#if (QT_VERSION < 300)
	datBox->showDate( dt.date() );
	timBox->showTime( dt.time() );
#else
	datBox->setDate( dt.date() );
	timBox->setTime( dt.time() );
#endif

}

QDateTime modCalcApCoord::getQDateTime (void)
{
#if (QT_VERSION < 300)
	QDateTime dt ( datBox->createDate() , timBox->createTime() );
#else
	QDateTime dt ( datBox->date() , timBox->time() );
#endif

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
	rafBox->show( sp.ra() );
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
#if (QT_VERSION < 300)
	datBox->clearFields();
	timBox->clearFields();
#else
	datBox->setDate(QDate::currentDate());
	timBox->setTime(QTime(0,0,0));
#endif
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
