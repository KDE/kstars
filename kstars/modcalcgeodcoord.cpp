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
#include <klocale.h>
#include <knumvalidator.h>

#if (QT_VERSION < 300)
#include <kapp.h>
#else
#include <kapplication.h>
#endif

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
	cartBox->setTitle( i18n("Cartesian Coordinates"));

	QLabel * XLabel = new QLabel( cartBox );
	XLabel->setText( i18n("X coordinate","X (km):") );
	xGeoName = new QLineEdit( cartBox, "XGeoName" );
        xGeoName->setValidator( new KIntValidator( xGeoName ) );

	QLabel * YLabel = new QLabel( cartBox );
	YLabel->setText( i18n("Y coordinate","Y (km):") );
	yGeoName = new QLineEdit( cartBox, "YGeoName" );
        yGeoName->setValidator( new KIntValidator( yGeoName ) );

	QLabel * ZLabel = new QLabel( cartBox );
	ZLabel->setText( i18n("Z coordinate","Z (km):") );
	zGeoName = new QLineEdit( cartBox, "ZGeoName" );
        zGeoName->setValidator( new KIntValidator( zGeoName ) );


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
	spheBox->setTitle( i18n("Geographic Coordinates"));

	QVBoxLayout * D0Lay = new QVBoxLayout( spheBox );
	QHBox * lonlatBox = new QHBox(spheBox,"datetimeBox");

	QLabel * lonLabel = new QLabel( lonlatBox );
	lonLabel->setText( i18n("Geographic Longitude","Long.:"));
	lonGeoBox = new dmsBox(lonlatBox,"LongGeoBox",TRUE);

	QLabel * latLabel = new QLabel( lonlatBox );
	latLabel->setText( i18n("Geographic Latitude","Lat.:"));
	latGeoBox = new dmsBox(lonlatBox,"LatGeoBox",TRUE);

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

	showLongLat();
	setEllipsoid(0);

// slots

	rightBox->setMaximumWidth(550);
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

void modCalcGeodCoord::showLongLat(void)
{

	KStars *ks = (KStars*) parent()->parent()->parent(); // QSplitter->AstroCalc->KStars
	lonGeoBox->show( ks->geo()->lng() );
	latGeoBox->show( ks->geo()->lat() );
	altGeoName->setText( QString("0.0") );
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

	xGeoName->setText( KGlobal::locale()->formatNumber( geoPlace->xPos()/1000. ,6));
	yGeoName->setText( KGlobal::locale()->formatNumber( geoPlace->yPos()/1000. ,6));
	zGeoName->setText( KGlobal::locale()->formatNumber( geoPlace->zPos()/1000. ,6));
//	xGeoName->setText(QString("%1").arg( geoPlace->xPos()/1000. ,11,'f',6));
//	yGeoName->setText(QString("%1").arg( geoPlace->yPos()/1000. ,11,'f',6));
//	zGeoName->setText(QString("%1").arg( geoPlace->zPos()/1000. ,11,'f',6));
}
