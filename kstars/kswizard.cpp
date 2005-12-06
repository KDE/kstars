/***************************************************************************
                          kswizard.cpp  -  description
                             -------------------
    begin                : Wed 28 Jan 2004
    copyright            : (C) 2004 by Jason Harris
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

#include <qfile.h>
#include <qpixmap.h>
#include <qlabel.h>
#include <klineedit.h>
#include <klistbox.h>
#include <kpushbutton.h>

#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "geolocation.h"
#include "dmsbox.h"
#include "telescopewizardprocess.h"
#include "kswizard.h"

WizWelcomeUI::WizWelcomeUI( QWidget *parent ) : QFrame( parent ) {
	setupUi( this );
}

WizLocationUI::WizLocationUI( QWidget *parent ) : QFrame( parent ) {
	setupUi( this );
}

WizDevicesUI::WizDevicesUI( QWidget *parent ) : QFrame( parent ) {
	setupUi( this );
}

WizDownloadUI::WizDownloadUI( QWidget *parent ) : QFrame( parent ) {
	setupUi( this );
}

KSWizard::KSWizard( QWidget *parent )
 : KDialogBase( parent )
{
	ksw = (KStars *)parent;
	GeoID.resize(10000);
	
	QFrame *page = plainPage();
	QVBoxLayout *vlay = new QVBoxLayout( page, 0, 0 );
	wiz = new KSWizardUI( this );
	vlay->addWidget( wiz );

	welcome = new WizWelcomeUI( this );
	location = new WizLocationUI( this );
	devices = new WizDevicesUI( this );
	download = new WizDownloadUI( this );

	wiz->WizardStack->addWidget( welcome );
	wiz->WizardStack->addWidget( location );
	wiz->WizardStack->addWidget( devices );
	wiz->WizardStack->addWidget( download );
	
	//Load images into banner frames.
	QFile imFile;
	QPixmap im = QPixmap();
	
	if ( KSUtils::openDataFile( imFile, "wzstars.png" ) ) {
		imFile.close(); //Just need the filename...
		im.load( imFile.name() );
	}
	welcome->Banner->setPixmap( im );
	
	if ( KSUtils::openDataFile( imFile, "wzgeo.png" ) ) {
		imFile.close(); //Just need the filename...
		im.load( imFile.name() );
	}
	location->Banner->setPixmap( im );
	
	if ( KSUtils::openDataFile( imFile, "wzscope.png" ) ) {
		imFile.close(); //Just need the filename...
		im.load( imFile.name() );
	}
	devices->Banner->setPixmap( im );

	if ( KSUtils::openDataFile( imFile, "wzdownload.png" ) ) {
		imFile.close(); //Just need the filename...
		im.load( imFile.name() );
	}
	download->Banner->setPixmap( im );

	//connect signals/slots
	connect( CityListBox, SIGNAL( selectionChanged() ), this, SLOT( slotChangeCity() ) );
	connect( CityFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
	connect( ProvinceFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
	connect( CountryFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
//Uncomment if we ever need the telescope page...
//	connect( TelescopeWizardButton, SIGNAL( clicked() ), this, SLOT( slotTelescopeSetup() ) );
	connect( DownloadButton, SIGNAL( clicked() ), ksw, SLOT( slotDownload() ) );
	
	//Initialize Geographic Location page
	initGeoPage();
}

//Do NOT delete members of filteredCityList!  They are not created by KSWizard.
KSWizard::~KSWizard() 
{}

void KSWizard::initGeoPage() {
	LongBox->setReadOnly( true );
	LatBox->setReadOnly( true );

	//Populate the CityListBox
	//flag the ID of the current City
	int index(0);
	for (GeoLocation *loc = ksw->data()->geoList.first(); loc; loc = ksw->data()->geoList.next()) {
		CityListBox->insertItem( loc->fullName() );
		filteredCityList.append( loc );
		
		if ( loc->fullName() == ksw->data()->geo()->fullName() ) {
			index = ksw->data()->geoList.at();
			Geo = loc;
		}
	}
	
	//Sort alphabetically
	CityListBox->sort();
	
	//preset to current city
	CityListBox->setCurrentItem( index + 1 );
}

void KSWizard::slotChangeCity() {
	Geo = 0L;
	
	if ( CityListBox->currentItem() >= 0 ) {
		for ( int i=0; i < filteredCityList.size(); ++i ) {
			if ( filteredCityList[i]->fullName() == CityListBox->currentText() ) {
				Geo = filteredCityList[i];
				break;
			}
		}
	}

	LongBox->showInDegrees( Geo->lng() );
	LatBox->showInDegrees( Geo->lat() );
}

void KSWizard::slotFilterCities() {
	CityListBox->clear();
	//Do NOT delete members of filteredCityList!
	while ( ! filteredCityList.isEmpty() ) filteredCityList.takeFirst();

	for (GeoLocation *loc = ksw->data()->geoList.first(); loc; loc = ksw->data()->geoList.next()) {
		QString sc( loc->translatedName() );
		QString ss( loc->translatedCountry() );
		QString sp = "";
		if ( !loc->province().isEmpty() )
			sp = loc->translatedProvince();

		if ( sc.lower().startsWith( CityFilter->text().lower() ) &&
				sp.lower().startsWith( ProvinceFilter->text().lower() ) &&
				ss.lower().startsWith( CountryFilter->text().lower() ) ) {
			CityListBox->insertItem( loc->fullName() );
			filteredCityList.append( loc );
		}
	}
	
	CityListBox->sort();

	if ( CityListBox->firstItem() )  // set first item in list as selected
		CityListBox->setCurrentItem( CityListBox->firstItem() );
}

//Uncomment if we ever need the telescope page...
//void KSWizard::slotTelescopeSetup() {
//	telescopeWizardProcess twiz(ksw);
//	twiz.exec();
//}

#include "kswizard.moc"
