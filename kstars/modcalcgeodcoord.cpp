/***************************************************************************
                          modcalcgeodcoord.cpp  -  description
                             -------------------
    begin                : Tue Jan 15 2002
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

#include "modcalcgeodcoord.h"
#include "modcalcgeodcoord.moc"
#include "dms.h"
#include "dmsbox.h"
#include "geolocation.h"
#include "kstars.h"

#include <qradiobutton.h>
#include <qstring.h>
#include <qcheckbox.h>
#include <qtextstream.h>

#include <kcombobox.h>
#include <klineedit.h>
#include <kapplication.h>
#include <knumvalidator.h>
#include <klocale.h>
#include <klineedit.h>
#include <kfiledialog.h>
#include <kmessagebox.h>


modCalcGeodCoord::modCalcGeodCoord(QWidget *parentSplit, const char *name) : modCalcGeodCoordDlg(parentSplit,name) {

	static const char *ellipsoidList[] = {
    "IAU76", "GRS80", "MERIT83", "WGS84", "IERS89"};

	spheRadio->setChecked(TRUE);
	ellipsoidBox->insertStrList (ellipsoidList,5);
	geoPlace = new GeoLocation();
	showLongLat();
	setEllipsoid(0);
	this->show();

}

modCalcGeodCoord::~modCalcGeodCoord(){
}

void modCalcGeodCoord::showLongLat(void)
{

	KStars *ks = (KStars*) parent()->parent()->parent(); // QSplitter->AstroCalc->KStars
	lonGeoBox->show( ks->geo()->lng() );
	latGeoBox->show( ks->geo()->lat() );
	altGeoBox->setText( QString("0.0") );
}

void modCalcGeodCoord::setEllipsoid(int index) {

	geoPlace->changeEllipsoid(index);

}

void modCalcGeodCoord::getCartGeoCoords (void)
{

	geoPlace->setXPos( KGlobal::locale()->readNumber(xGeoName->text())*1000. );
	geoPlace->setYPos( KGlobal::locale()->readNumber(yGeoName->text())*1000. );
	geoPlace->setZPos( KGlobal::locale()->readNumber(zGeoName->text())*1000. );
	//geoPlace->setXPos( xGeoName->text().toDouble()*1000. );
	//geoPlace->setYPos( yGeoName->text().toDouble()*1000. );
	//geoPlace->setZPos( zGeoName->text().toDouble()*1000. );
}

void modCalcGeodCoord::getSphGeoCoords (void)
{
	geoPlace->setLong( lonGeoBox->createDms() );
	geoPlace->setLat(  latGeoBox->createDms() );
	geoPlace->setHeight( altGeoBox->text().toDouble() );
}

void modCalcGeodCoord::slotClearGeoCoords (void)
{

	geoPlace->setLong( 0.0 );
	geoPlace->setLat(  0.0 );
	geoPlace->setHeight( 0.0 );
	latGeoBox->clearFields();
	lonGeoBox->clearFields();
	//showSpheGeoCoords();
	//showCartGeoCoords();

}

void modCalcGeodCoord::slotComputeGeoCoords (void)
{

	if(cartRadio->isChecked()) {
		getCartGeoCoords();
		showSpheGeoCoords();
	} else {
		getSphGeoCoords();
		showCartGeoCoords();
	}

}

void modCalcGeodCoord::showSpheGeoCoords(void)
{
	lonGeoBox->show( geoPlace->lng() );
	latGeoBox->show( geoPlace->lat() );
	altGeoBox->setText(QString("%1").arg( geoPlace->height() ,11,'f',3));
}

void modCalcGeodCoord::showCartGeoCoords(void)
{

	xGeoName->setText( KGlobal::locale()->formatNumber( geoPlace->xPos()/1000. ,6));
	yGeoName->setText( KGlobal::locale()->formatNumber( geoPlace->yPos()/1000. ,6));
	zGeoName->setText( KGlobal::locale()->formatNumber( geoPlace->zPos()/1000. ,6));
//	xGeoName->setText(QString("%1").arg( geoPlace->xPos()/1000. ,11,'f',6));
//	yGeoName->setText(QString("%1").arg( geoPlace->yPos()/1000. ,11,'f',6));
//	zGeoName->setText(QString("%1").arg( geoPlace->zPos()/1000. ,11,'f',6));
}

void geoCheck() {

}

void modCalcGeodCoord::slotLongCheckedBatch(){

	if ( longCheckBatch->isChecked() ) {
		longBoxBatch->setEnabled( false );
		geoCheck();
	} else {
		longBoxBatch->setEnabled( true );
	}
}

void modCalcGeodCoord::slotLatCheckedBatch(){

	if ( latCheckBatch->isChecked() ) {
		latBoxBatch->setEnabled( false );
		geoCheck();
	} else {
		latBoxBatch->setEnabled( true );
	}
}

void modCalcGeodCoord::slotElevCheckedBatch(){

	if ( elevCheckBatch->isChecked() ) {
		elevBoxBatch->setEnabled( false );
		geoCheck();
	} else {
		elevBoxBatch->setEnabled( true );
	}
}

void modCalcGeodCoord::slotXCheckedBatch(){

	if ( xCheckBatch->isChecked() ) {
		xBoxBatch->setEnabled( false );
		geoCheck();
	} else {
		xBoxBatch->setEnabled( true );
	}
}

void modCalcGeodCoord::slotYCheckedBatch(){

	if ( yCheckBatch->isChecked() ) {

		yBoxBatch->setEnabled( false );
		geoCheck();
	} else {
		yBoxBatch->setEnabled( true );
	}
}

void modCalcGeodCoord::slotZCheckedBatch(){

	if ( zCheckBatch->isChecked() ) {
		zBoxBatch->setEnabled( false );
		geoCheck();
	} else {
		zBoxBatch->setEnabled( true );
	}
}
