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
#include "dms.h"
#include "dmsbox.h"
#include "geolocation.h"
#include <qwidget.h>
#include <qvbox.h>
#include <qgroupbox.h>
#include <qradiobutton.h>
#include <qlineedit.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qstring.h>
#include <qbuttongroup.h>
#include <qcombobox.h>
#include <kapp.h>
#include <klocale.h>

modCalcGeodCoord::modCalcGeodCoord(QWidget *parentSplit, const char *name) : QVBox(parentSplit,name) {
	
	static const char *ellipsoidList[] = {
    "IAU76", "GRS80", "MERIT83", "WGS84", "IERS89"};
	
	rightBox = new QVBox (parentSplit);

	QButtonGroup * InputBox = new QButtonGroup (rightBox);
	InputBox->setTitle( i18n("Input Selection") );
	
	cartRadio = new QRadioButton( i18n( "Cartesian" ), InputBox );
	spheRadio = new QRadioButton( i18n( "Geographic" ), InputBox );
	
	spheRadio->setChecked(TRUE);
	
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
		
	InputLay->addLayout (hlay, 0);
	InputLay->addLayout (hlay2, 0);
	
	hlay2->addWidget (Compute, 0, 0);
	hlay2->addWidget (Clear, 0, 0);
	
	hlay->addWidget ( cartRadio, 0, 0);
	hlay->addWidget ( spheRadio, 0, 0);

	// Input for XYZ

	QGroupBox * cartBox = new QGroupBox (rightBox);
	cartBox->setTitle( i18n("Cartesian coordinates"));
	
	QLabel * XLabel = new QLabel( cartBox );
	XLabel->setText( i18n(" X coordinate in meters","X (m):") );
	xGeoName = new QLineEdit( cartBox, "XGeoName" );

	QLabel * YLabel = new QLabel( cartBox );
	YLabel->setText( i18n(" Y coordinate","Y (m):") );
	yGeoName = new QLineEdit( cartBox, "YGeoName" );

	QLabel * ZLabel = new QLabel( cartBox );
	ZLabel->setText( i18n(" > coordinate","Z (m):") );
	zGeoName = new QLineEdit( cartBox, "ZGeoName" );
			
	QVBoxLayout * cartLay = new QVBoxLayout( cartBox );
	cartLay->setSpacing(6);
	cartLay->setMargin(14);
	
	QHBoxLayout * xLay = new QHBoxLayout();
	xLay->setSpacing(6);
	xLay->setMargin(3);
	
	QHBoxLayout * yLay = new QHBoxLayout();
	yLay->setSpacing(6);
	yLay->setMargin(3);
	
	QHBoxLayout * zLay = new QHBoxLayout();
	zLay->setSpacing(6);
	zLay->setMargin(3);
	
	xLay->addWidget ( XLabel );
	xLay->addWidget ( xGeoName );
	yLay->addWidget ( YLabel );
	yLay->addWidget ( yGeoName );
	zLay->addWidget ( ZLabel );
	zLay->addWidget ( zGeoName );
	
	cartLay->addLayout (xLay);
	cartLay->addLayout (yLay);
	cartLay->addLayout (zLay);

// Input for Spherical coords.

	QGroupBox * spheBox = new QGroupBox (rightBox);
	spheBox->setTitle( i18n("Geographic coordinates"));
	
	QVBoxLayout * D0Lay = new QVBoxLayout( spheBox );
	QHBox * lonlatBox = new QHBox(spheBox,"datetimeBox");
	
	QLabel * lonLabel = new QLabel( lonlatBox );
	lonLabel->setText( i18n("Geographic Longitude","Long.:"));
	lonGeoBox = new dmsBox(lonlatBox,"LongGeoBox",TRUE,4);

	QLabel * latLabel = new QLabel( lonlatBox );
	latLabel->setText( i18n("Geographic Latitude","Lat.:"));
	latGeoBox = new dmsBox(lonlatBox,"LatGeoBox",TRUE,4);
	
	QHBox * altGeoBox = new QHBox(lonlatBox);	
	QLabel * AltLabel = new QLabel( altGeoBox, "Alt" );
	AltLabel->setText( i18n(" Altitude","Alt:") );
	altGeoName = new QLineEdit( altGeoBox );
	altGeoBox->setMargin(6);
	altGeoBox->setSpacing(6);
	
	QComboBox *ellipsoidBox = new QComboBox( FALSE, spheBox);
	ellipsoidBox->insertStrList (ellipsoidList,5);
	
	ellipsoidBox->setFixedHeight(25);
	ellipsoidBox->setMaximumWidth(100);
	
	D0Lay->setMargin(14);
	D0Lay->addWidget(lonlatBox);
//	D0Lay->addWidget(altGeoBox);
	D0Lay->addWidget(ellipsoidBox,0);

// GeoLocation object
	
	geoPlace = new GeoLocation();
		
// slots

	rightBox->setMaximumWidth(450);
	rightBox->setMinimumWidth(400);
	rightBox->setMargin(14);
	rightBox->setSpacing(7);
	rightBox->show();

	connect( Compute, SIGNAL(clicked() ), this, SLOT( slotComputeGeoCoords() ) ) ;
	connect( Clear, SIGNAL(clicked() ), this, SLOT( slotClearGeoCoords() ) ) ;
	connect( ellipsoidBox, SIGNAL(activated(int)), SLOT(setEllipsoid(int)) );
	
}

modCalcGeodCoord::~modCalcGeodCoord(){
	delete rightBox;
}

void modCalcGeodCoord::setEllipsoid(int index) {

	geoPlace->changeEllipsoid(index);

}

void modCalcGeodCoord::getCartGeoCoords (void)
{
	geoPlace->setXPos( xGeoName->text().toDouble() );
	geoPlace->setYPos( yGeoName->text().toDouble() );
	geoPlace->setZPos( zGeoName->text().toDouble() );
}

void modCalcGeodCoord::getSphGeoCoords (void)
{
	geoPlace->setLong( lonGeoBox->createDms() );
	geoPlace->setLat(  latGeoBox->createDms() );
	geoPlace->setHeight( altGeoName->text().toDouble() );
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
	altGeoName->setText(QString("%1").arg( geoPlace->height() ,11,'f',3));
}

void modCalcGeodCoord::showCartGeoCoords(void)
{
	xGeoName->setText(QString("%1").arg( geoPlace->xPos() ,11,'f',3));
	yGeoName->setText(QString("%1").arg( geoPlace->yPos() ,11,'f',3));
	zGeoName->setText(QString("%1").arg( geoPlace->zPos() ,11,'f',3));
}
