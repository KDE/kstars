/***************************************************************************
                          elts.cpp  -  description
                             -------------------
    begin                : wed nov 17 08:05:11 CET 2002
    copyright            : (C) 2002-2003 by Pablo de Vicente
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
#include "elts.h"
#include "eltscanvas.h"

#include "skypoint.h"
#include "geolocation.h"
#include <qvariant.h>
#include <dmsbox.h>
#include <qdatetimeedit.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qtabwidget.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>
#include "ksutils.h"
#include "kstars.h"
#include "finddialog.h"
#include "locationdialog.h"
#include "geolocation.h"

elts::elts( QWidget* parent)  : 
	KDialogBase( KDialogBase::Plain, i18n( "Elevation vs. Time" ), Close, Close, parent ) 
{

	ks = (KStars*) parent;

	QFrame *page = plainPage();
	
	setMainWidget(page);
	QVBoxLayout *topLayout = new QVBoxLayout( page, 0, spacingHint() );

//	eltsTotalBox = new QGroupBox( this, "eltsBox" );
//	eltsTotalBox->setMinimumWidth( 580 );
//	eltsTotalBox->setMinimumHeight( 510 );
//	eltsTotalBox->setTitle( i18n( "Elevation vs. Time" ) );
//
//	eltsTotalBoxLayout = new QVBoxLayout( eltsTotalBox );
//	eltsTotalBoxLayout->setSpacing( 6 );
//	eltsTotalBoxLayout->setMargin( 11 );

	eltsView = new eltsCanvas( page );

	// each pixel 4 minutes x 30 arcmin 
	// Total size is X: 24x60 = 1440; 1440/4 = 360 + 2*40 (padding) = 440
	//            is Y: 180 + 2*40 (padding) = 260
	eltsView->setFixedSize( 440, 260 );

	ctlTabs = new QTabWidget( page, "DisplayTabs" );
//	ctlTabs->move( 10, 24 );
//	ctlTabs->setMinimumSize( 560, 110 );


	/** Tab for adding individual sources and clearing the plot */
	/** 1st Tab **/

	sourceTab = new QWidget( ctlTabs );
	sourceLayout = new QVBoxLayout( sourceTab, 0, 6 );

	nameLayout = new QHBoxLayout( 0, 2, 9 );
	coordLayout = new QHBoxLayout( 0, 2, 9 );

	nameLabel = new QLabel( sourceTab );
	nameLabel->setText( i18n( "Name:" ) );
	
	nameBox = new QLineEdit( sourceTab);
	
	browseButton = new QPushButton( sourceTab );
	browseButton->setText( i18n( "Browse" ) );
	
	nameLayout->addWidget( nameLabel );
	nameLayout->addWidget( nameBox );
	nameLayout->addSpacing( 12 );
	nameLayout->addWidget( browseButton );
	

	raLabel = new QLabel( sourceTab );
	raLabel->setText( i18n( "RA:" ) );
	
	raBox = new dmsBox( sourceTab , "raBox", FALSE);

	decLabel = new QLabel( sourceTab );
	decLabel->setText( i18n( "Dec:" ) );

	decBox = new dmsBox( sourceTab );

	epochLabel = new QLabel( sourceTab );
	epochLabel->setText( i18n( "Epoch:" ) );

	epochName = new QLineEdit( sourceTab );
	epochName->setMaximumSize( QSize( 60, 32767 ) );

	coordLayout->addWidget( raLabel );
	coordLayout->addWidget( raBox );
	coordLayout->addWidget( decLabel );
	coordLayout->addWidget( decBox );
	coordLayout->addWidget( epochLabel );
	coordLayout->addWidget( epochName );
	
	/** Buttons Layout */

	clearAddLayout = new QHBoxLayout( 0, 0, 6);

	clearButton = new QPushButton( sourceTab );
	clearButton->setMinimumSize( QSize( 80, 0 ) );
	clearButton->setText( i18n( "Clear all" ) );

	QSpacerItem* spacer = new QSpacerItem( 10, 0, QSizePolicy::Expanding, QSizePolicy::Minimum );

	addButton = new QPushButton( sourceTab );
	addButton->setMinimumSize( QSize( 80, 0 ) );
	addButton->setText( i18n( "Add" ) );

	clearAddLayout->addItem( spacer );
	clearAddLayout->addWidget( clearButton );
	clearAddLayout->addWidget( addButton );

	/** Closing all layouts for Tab source*/

//	sourceLayout->addSpacing( 10 );
	sourceLayout->addLayout( nameLayout );
	sourceLayout->addLayout( coordLayout );
	sourceLayout->addLayout( clearAddLayout );

	ctlTabs->insertTab( sourceTab, i18n( "Source" ) );

	/** Tab for Date and Location */
	/** 2nd Tab */

	dateTab = new QWidget( ctlTabs );
	dateLocationLayout = new QVBoxLayout( dateTab, 0, 6, "dateLocationLayout"); 

	longLatLayout = new QHBoxLayout( 0, 2, 9, "longLatLayout"); 

	dateLabel = new QLabel( dateTab );
	dateLabel->setText( i18n( "Date:" ) );
	longLatLayout->addWidget( dateLabel );

	dateBox = new QDateEdit( dateTab );
	longLatLayout->addWidget( dateBox );

	latLabel = new QLabel( dateTab );
	latLabel->setText( i18n( "Lat.:" ) );
	longLatLayout->addWidget( latLabel );

	latBox = new dmsBox( dateTab );
	longLatLayout->addWidget( latBox );

	longLabel = new QLabel( dateTab );
	longLabel->setText( i18n( "Long.:" ) );
	longLatLayout->addWidget( longLabel );

	longBox = new dmsBox( dateTab );
	longLatLayout->addWidget( longBox );

	/* Layout for the button part */

	updateLayout = new QHBoxLayout( 0, 0, 6); 
	QSpacerItem* spacer_2 = new QSpacerItem( 10, 0, QSizePolicy::Expanding, QSizePolicy::Minimum );

	cityButton = new QPushButton( dateTab );
	cityButton->setText( i18n( "Choose City..." ) );
	
	updateButton = new QPushButton( dateTab );
//	updateButton->setMinimumSize( QSize( 80, 0 ) );
	updateButton->setText( i18n( "Update" ) );
	
	updateLayout->addItem( spacer_2 );
	updateLayout->addWidget( cityButton );
	updateLayout->addWidget( updateButton );
	
	/** Closing layouts the date/location tab */

	dateLocationLayout->addLayout( longLatLayout );
	dateLocationLayout->addLayout( updateLayout );

	ctlTabs->insertTab( dateTab, i18n( "Date && Location" ) );

//	eltsTotalBoxLayout->addSpacing( 10 );
	topLayout->addWidget( eltsView );
	topLayout->addWidget( ctlTabs );

	showCurrentDate();
	//initGeo();
	showLongLat();
//	initVars();

	connect( browseButton, SIGNAL( clicked() ), this, SLOT( slotBrowseObject() ) );
	connect( cityButton, SIGNAL( clicked() ), this, SLOT( slotChooseCity() ) );
	connect( updateButton, SIGNAL(clicked() ), this, SLOT( slotUpdateDateLoc() ) );
	connect( clearButton, SIGNAL(clicked() ), this, SLOT( slotClear() ) );
	connect( addButton, SIGNAL(clicked() ), this, SLOT( slotAddSource() ) );
}


/*  
 *  Destroys the object and frees any allocated resources
 */
elts::~elts()
{
    // no need to delete child widgets, Qt does it all for us
}

//QSize elts::sizeHint() const
//{
//	  return QSize(580,560);
//}

void elts::slotAddSource(void) {
	
	// There is a bug here. If one puts a window on top of
	// this one the source curve will disappear.
	newsource = true;
	eltsView->repaint(false);
	newsource = false;
}

void elts::slotClear(void) {
	eltsView->erase();
}

void elts::slotUpdateDateLoc(void) {
	eltsView->repaint();
}

void elts::slotBrowseObject(void) {
	FindDialog fd(ks);
	if ( fd.exec() == QDialog::Accepted ) {
		SkyObject *obj = fd.currentItem()->objName()->skyObject();
		raBox->showInHours( obj->ra() );
		decBox->showInDegrees( obj->dec() );
		nameBox->setText( obj->translatedName() );
	} 
}

void elts::slotChooseCity(void) {
	LocationDialog ld(ks);
	if ( ld.exec() == QDialog::Accepted ) {
		int ii = ld.getCityIndex();
		if ( ii >= 0 ) {
			GeoLocation *geo = ks->data()->geoList.at(ii);
			latBox->showInDegrees( geo->lat() );
			longBox->showInDegrees( geo->lng() );
		}
	}
}

SkyPoint elts::getEquCoords (void)
{
	dms raCoord, decCoord;

	raCoord = raBox->createDms();
	decCoord = decBox->createDms();

	SkyPoint sp = SkyPoint (raCoord, decCoord);

	return sp;
}

void elts::showCurrentDate (void)
{
	QDateTime dt = QDateTime::currentDateTime();
	dateBox->setDate( dt.date() );
}

QDateTime elts::getQDate (void)
{
	QDateTime dt ( dateBox->date(),QTime(0,0,0) );
	return dt;
}

dms elts::getLongitude (void)
{
	dms longitude;
	longitude = longBox->createDms();
	return longitude;
}

dms elts::getLatitude (void)
{
	dms latitude;
	latitude = latBox->createDms();
	return latitude;
}

void elts::initGeo(void)
{
	geoPlace = new GeoLocation( ks->geo() );
}

void elts::showLongLat(void)
{
	longBox->show( ks->geo()->lng() );
	latBox->show( ks->geo()->lat() );
}

long double elts::computeJdFromCalendar (void)
{
	long double julianDay;

	julianDay = KSUtils::UTtoJulian( getQDate() );

	return julianDay;
}

double elts::getEpoch (QString eName)
{

	double epoch = eName.toDouble();

	return epoch;
}

long double elts::epochToJd (double epoch)
{

	double yearsTo2000 = 2000.0 - epoch;

	if (epoch == 1950.0) {
		return 2433282.4235;
	} else if ( epoch == 2000.0 ) {
		return J2000;
	} else {
		return ( J2000 - yearsTo2000 * 365.2425 );
	}

}


SkyPoint elts::appCoords() {

	long double jd = computeJdFromCalendar();
	double epoch0 = getEpoch( epochName->text() );
	long double jd0 = epochToJd ( epoch0 );


	SkyPoint sp;
	sp = getEquCoords();

	sp.apparentCoord(jd0, jd);

	return sp;
}

