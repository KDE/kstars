/***************************************************************************
                          modcalcprec.cpp  -  description
                             -------------------
    begin                : Sun Jan 27 2002
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

#include "modcalcprec.h"
#include "modcalcprec.moc"
#include "dms.h"
#include "dmsbox.h"
#include "skypoint.h"
#include "ksutils.h"
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

modCalcPrec::modCalcPrec(QWidget *parentSplit, const char *name) : QVBox(parentSplit,name) {

	rightBox = new QVBox (parentSplit);

	QGroupBox * InputBox = new QGroupBox (rightBox);
	InputBox->setTitle( i18n("Original Coordinates") );
	
	QHBox * buttonBox = new QHBox(InputBox);
	
	QPushButton * Compute = new QPushButton( i18n( "Compute" ), buttonBox);
	QPushButton * Clear = new QPushButton( i18n( "Clear" ), buttonBox );
	
	Compute->setFixedHeight(25);
	Compute->setMaximumWidth(100);
	
	Clear->setFixedHeight(25);
	Clear->setMaximumWidth(100);
	
	QVBoxLayout * D0Lay = new QVBoxLayout( InputBox);
	QHBox * ra0dec0Box = new QHBox(InputBox);
	
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

	QString e0 = showCurrentEpoch();
	epoch0Name->setText(e0);

	epoch0Box->setMargin(6);
	epoch0Box->setSpacing(6);
	
	buttonBox->setMargin(10);
	D0Lay->setMargin(14);
	D0Lay->setSpacing(4);
	D0Lay->addWidget(buttonBox);
	D0Lay->addWidget(ra0dec0Box);

	//Output

	QGroupBox * outputBox = new QGroupBox (rightBox);
	outputBox->setTitle( i18n ("Precessed Coordinates") );

	QVBoxLayout * D1Lay = new QVBoxLayout( outputBox);
	QHBox * rafdecfBox = new QHBox(outputBox);
	
	QLabel * rafLabel = new QLabel(rafdecfBox,"raLabel");
	rafLabel->setText(i18n("Right ascention","RA:"));
	rafBox = new dmsBox(rafdecfBox,"raBox",FALSE);

	QLabel * decfLabel = new QLabel(rafdecfBox,"decLabel");
	decfLabel->setText(i18n("Declination","DEC:"));
	decfBox = new dmsBox(rafdecfBox,"decBox");
	
	QHBox * epochfBox = new QHBox(rafdecfBox);	
	QLabel * epochfLabel = new QLabel( epochfBox, "Epoch" );
	epochfLabel->setText( i18n(" Epoch:") );
	epochfName = new QLineEdit( epochfBox );
	epochfName->setMaximumWidth(70);

	epochfName->setText( "2000.0" );

	epochfBox->setMargin(6);
	epochfBox->setSpacing(6);
	
	D1Lay->setMargin(14);
	D1Lay->addWidget(rafdecfBox);

	QSpacerItem * downSpacer = new QSpacerItem(400,500);
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


	connect( Compute, SIGNAL(clicked() ), this, SLOT( slotComputeCoords() ) ) ;
	connect( Clear, SIGNAL(clicked() ), this, SLOT( slotClearCoords() ) ) ;
	
}

modCalcPrec::~modCalcPrec(){
	delete rightBox;
}

SkyPoint modCalcPrec::getEquCoords (void) {
	dms raCoord, decCoord;

	raCoord = ra0Box->createDms();
	decCoord = dec0Box->createDms();

	SkyPoint sp = SkyPoint (raCoord, decCoord);

	return sp;
}

QString modCalcPrec:: showCurrentEpoch () {

	double epoch = setCurrentEpoch();
	QString eName = QString("%1").arg(epoch,7,'f',2);

	return eName;
}

double modCalcPrec::setCurrentEpoch () {

	QDateTime dt = QDateTime::currentDateTime();
	long double jd = KSUtils::UTtoJulian( dt );
	double epoch = JdtoEpoch(jd);

	return epoch;
}

double modCalcPrec::getEpoch (QString eName) {
	
	double epoch = eName.toDouble();

	return epoch;
}

void modCalcPrec::slotClearCoords (void) {
	
	ra0Box->clearFields();
	dec0Box->clearFields();
	rafBox->clearFields();
	decfBox->clearFields();
	epoch0Name->setText("");
	epochfName->setText("");
	
}

void modCalcPrec::slotComputeCoords (void) {

	SkyPoint sp;
	
	sp = getEquCoords();
	double epoch0 = getEpoch( epoch0Name->text() );
	double epochf = getEpoch( epochfName->text() );
	sp = precess( sp.ra0(), sp.dec0(), epoch0, epochf );
	showEquCoords( sp );

}

void modCalcPrec::showEquCoords ( SkyPoint sp ) {

	rafBox->show( sp.ra() );
	decfBox->show( sp.dec() );
	
}

double modCalcPrec::JdtoEpoch (long double jd) {

	long double Jd2000 = 2451545.00;
	return ( 2000.0 - ( Jd2000 - jd ) / 365.2425);
}

long double modCalcPrec::epochToJd (double epoch) {

	double yearsTo2000 = 2000.0 - epoch;

	if (epoch == 1950.0) {
		return 2433282.4235;
	} else if ( epoch == 2000.0 ) {
		return J2000;
	} else {
		return ( J2000 - yearsTo2000 * 365.2425 );
	}

}

SkyPoint modCalcPrec::precess (dms RA0, dms Dec0, double epoch0, double epochf) {

	double cosRA0, sinRA0, cosDec0, sinDec0;
	dms RA, Dec;
        double v[3], s[3]; 

	long double jd0 = epochToJd ( epoch0 );
	long double jdf = epochToJd ( epochf );

	RA0.SinCos( sinRA0, cosRA0 );
	Dec0.SinCos( sinDec0, cosDec0 );

	//Need to first precess to J2000.0 coords

	if ( jd0 != J2000 ) {
	
	//v is a column vector representing input coordinates.

		KSNumbers *num = new KSNumbers (jd0);

		v[0] = cosRA0*cosDec0; 
		v[1] = sinRA0*cosDec0;
		v[2] = sinDec0;

	//s is the product of P1 and v; s represents the
	//coordinates precessed to J2000
		for ( unsigned int i=0; i<3; ++i ) {
			s[i] = num->p1( 0, i )*v[0] + num->p1( 1, i )*v[1] + 
				num->p1( 2, i )*v[2];
		}
		delete num;

	} else {

		//Input coords already in J2000, set s accordingly.
		s[0] = cosRA0*cosDec0;
		s[1] = sinRA0*cosDec0;
		s[2] = sinDec0; 
	}
      
	KSNumbers *num = new KSNumbers (jdf);
      
	//Multiply P2 and s to get v, the vector representing the new coords.

	for ( unsigned int i=0; i<3; ++i ) {
		v[i] = num->p2( 0, i )*s[0] + num->p2( 1, i )*s[1] + 
		num->p2( 2, i )*s[2];
	}
	delete num;

	//Extract RA, Dec from the vector:

//	RA.setRadians( atan( v[1]/v[0] ) );
	RA.setRadians( atan2( v[1],v[0] ) );
	Dec.setRadians( asin( v[2] ) );
	
	//resolve ambiguity of atan()

//	if ( v[0] < 0.0 ) {
//		RA.setD( RA.Degrees() + 180.0 );
//	} else if( v[1] < 0.0 ) {
//		RA.setD( RA.Degrees() + 360.0 );
//	}

	if (RA.Degrees() < 0.0 )
		RA.setD( RA.Degrees() + 360.0 );

	SkyPoint sPoint = SkyPoint (RA, Dec);

	return sPoint;
}
