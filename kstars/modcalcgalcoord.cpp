/***************************************************************************
                          modcalcgal.cpp  -  description
                             -------------------
    begin                : Thu Jan 17 2002
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

#include "dms.h"
#include "dmsbox.h"
#include "modcalcgalcoord.h"
#include "modcalcgalcoord.moc"
#include <qwidget.h>
#include <qlabel.h>
#include <qvbox.h>
#include <qgroupbox.h>
#include <qradiobutton.h>
#include <qlineedit.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qstring.h>
#include <qbuttongroup.h>
#include <qcombobox.h>
#include <klocale.h>

#include <qglobal.h>
#if (QT_VERSION <= 299)
#include <kapp.h>
#else
#include <kapplication.h>
#endif


modCalcGalCoord::modCalcGalCoord(QWidget *parentSplit, const char *name) : QVBox(parentSplit,name) {
	
	rightBox = new QVBox (parentSplit);

	QButtonGroup * InputBox = new QButtonGroup (rightBox);
	InputBox->setTitle( i18n("Input Selection") );
	
	equRadio = new QRadioButton( i18n( "Equatorial" ), InputBox );
	galRadio = new QRadioButton( i18n( "Galactic" ), InputBox );
	
	equRadio->setChecked(TRUE);
	
	QPushButton * Compute = new QPushButton( i18n( "Compute" ), InputBox );
	QPushButton * Clear = new QPushButton( i18n( "Clear" ), InputBox );

// Layout for the Radio Buttons Box
	
	QVBoxLayout * InputLay = new QVBoxLayout(InputBox);
	QHBoxLayout * hlay = new QHBoxLayout();
	QHBoxLayout * hlay2 = new QHBoxLayout();
	
	InputLay->setMargin(14);

	hlay->setSpacing(20);
	hlay->setMargin(6);
	hlay2->setMargin(6);
		
	Compute->setFixedHeight(25);
	Compute->setMaximumWidth(100);
	
	Clear->setFixedHeight(25);
	Clear->setMaximumWidth(100);
		
	InputLay->addLayout (hlay);
	InputLay->addLayout (hlay2);
	
	hlay2->addWidget (Compute);
	hlay2->addWidget (Clear);
	
	hlay->addWidget ( equRadio);
	hlay->addWidget ( galRadio);

	// Input for galactic coords

	QGroupBox * galBox = new QGroupBox (rightBox);
	galBox->setTitle( i18n("Galactic Coordinates"));
	
	QLabel * lgLabel = new QLabel( galBox );
	lgLabel->setText( "l:" );
	lgName = new QLineEdit( galBox );

	QLabel * bgLabel = new QLabel( galBox );
	bgLabel->setText( "b:" );
	bgName = new QLineEdit( galBox );

	QHBoxLayout * galLay = new QHBoxLayout( galBox );
	galLay->setSpacing(6);
	galLay->setMargin(14);
	
	QHBoxLayout * lgLay = new QHBoxLayout();
	lgLay->setSpacing(6);
	lgLay->setMargin(3);
	
	QHBoxLayout * bgLay = new QHBoxLayout();
	bgLay->setSpacing(6);
	bgLay->setMargin(3);
	
	lgLay->addWidget ( lgLabel);
	lgLay->addWidget ( lgName);
	bgLay->addWidget ( bgLabel);
	bgLay->addWidget ( bgName);
	
	galLay->addLayout (lgLay );
	galLay->addLayout (bgLay );

// Input for Equatorial coords.

	QGroupBox * equBox = new QGroupBox (rightBox);
	equBox->setTitle( i18n("Equatorial Coordinates"));
	
	QVBoxLayout * D0Lay = new QVBoxLayout( equBox );
	QHBox * radecBox = new QHBox(equBox);
	
	QLabel * raLabel = new QLabel(radecBox);
	raLabel->setText(i18n("Right ascention","RA:"));
	raBox = new dmsBox(radecBox,"raBox",FALSE);

	QLabel * decLabel = new QLabel(radecBox);
	decLabel->setText(i18n("Declination","DEC:"));
	decBox = new dmsBox(radecBox,"decBox");
	
//	QHBox * epochBox = new QHBox(equBox);	
	QHBox * epochBox = new QHBox(radecBox);	

	QLabel * epochLabel = new QLabel( epochBox );
	epochLabel->setText( i18n(" Epoch:") );
	epochName = new QLineEdit( epochBox );
	epochName->setText("2000.0");
	epochBox->setMargin(6);
	epochBox->setSpacing(6);
	
	D0Lay->setMargin(14);
	D0Lay->addWidget(radecBox);
//	D0Lay->addWidget(epochBox,0);

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

modCalcGalCoord::~modCalcGalCoord() {
	delete rightBox;
}

void modCalcGalCoord::getGalCoords (void) {

	galLong = dms( lgName->text().toDouble() );
	galLat = dms( bgName->text().toDouble() );
	getEpoch();
}

void modCalcGalCoord::getEquCoords (void) {

	raCoord = raBox->createDms();
	decCoord = decBox->createDms();
	getEpoch();
}

void modCalcGalCoord::getEpoch (void) {
	epoch = epochName->text().toDouble();

}

void modCalcGalCoord::slotClearCoords (void) {
	
	raBox->clearFields();
	decBox->clearFields();
	lgName->setText("");
	bgName->setText("");
	
}

void modCalcGalCoord::slotComputeCoords (void) {
	
	if(galRadio->isChecked()) {
		getGalCoords();
//		checkEpoch();
		GalToEqu();
		showEquCoords();
	} else {
		getEquCoords();
//		checkEpoch();
		EquToGal();
		showGalCoords();
	}

}

void modCalcGalCoord::showEquCoords(void) {
	raBox->show( raCoord );
	decBox->show( decCoord );
}

void modCalcGalCoord::showGalCoords(void) {
	lgName->setText(QString("%1").arg( galLong.Degrees() ,11,'f',6));
	bgName->setText(QString("%1").arg( galLat.Degrees() ,11,'f',6));
}

void modCalcGalCoord::GalToEqu(void) {

	double a = 123.0;
	double c = 0.213802833;
	dms b, galLong_a;
	double sinb, cosb, singLat, cosgLat, tangLat, singLong_a, cosgLong_a;

	b.setD(27.4);
	tangLat = tan( galLat.radians() );


	galLat.SinCos(singLat,cosgLat);
	dms( galLong.Degrees()-a ).SinCos(singLong_a,cosgLong_a);
	b.SinCos(sinb,cosb);
	
	raCoord.setRadians(c + atan2(singLong_a,cosgLong_a*sinb-tangLat*cosb) );
	raCoord = raCoord.reduce();
//	raHourCoord = dms( raCoord.Hours() );
	
	decCoord.setRadians( asin(singLat*sinb+cosgLat*cosb*cosgLong_a) );
}

void modCalcGalCoord::EquToGal(void) {
	double a = 192.25;
	double c = 5.288347634;
	dms b;
	double sinb, cosb, sina_RA, cosa_RA, sinDEC, cosDEC, tanDEC;

	b.setD(27.4);
	tanDEC = tan( decCoord.radians() );
	
	b.SinCos(sinb,cosb);
	dms( a - raCoord.Degrees() ).SinCos(sina_RA,cosa_RA);
	decCoord.SinCos(sinDEC,cosDEC);

	galLong.setRadians( c - atan2( sina_RA, cosa_RA*sinb-tanDEC*cosb) );
	galLong = galLong.reduce();

	galLat.setRadians( asin(sinDEC*sinb+cosDEC*cosb*cosa_RA) );

}
