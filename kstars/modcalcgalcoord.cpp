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
#include <qradiobutton.h>
#include <klocale.h>
#include <klineedit.h>
#include <kapplication.h>


modCalcGalCoord::modCalcGalCoord(QWidget *parentSplit, const char *name) : modCalcGalCoordDlg(parentSplit,name) {
	
	equRadio->setChecked(TRUE);
	this->show();
}

modCalcGalCoord::~modCalcGalCoord() {
}

void modCalcGalCoord::getGalCoords (void) {

	galLong = lgBox->createDms();
	galLat = bgBox->createDms();
	getEpoch();
}

void modCalcGalCoord::getEquCoords (void) {

	raCoord = raBox->createDms(FALSE);
	decCoord = decBox->createDms();
	getEpoch();
}

void modCalcGalCoord::getEpoch (void) {
	epoch = epochName->text().toDouble();

}

void modCalcGalCoord::slotClearCoords (void) {
	
	raBox->clearFields();
	decBox->clearFields();
	lgBox->clearFields();
	bgBox->clearFields();
	
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
	raBox->show( raCoord , FALSE);
	decBox->show( decCoord );
}

void modCalcGalCoord::showGalCoords(void) {
	lgBox->show( galLong );
	bgBox->show( galLat );
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
