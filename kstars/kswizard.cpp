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
#include "kswizardui.h"
#include "kswizard.h"

KSWizard::KSWizard( QWidget *parent, const char *name )
 : KSWizardUI( parent, name )
{
	ksw = (KStars *)parent;
	GeoID.resize(10000);
	
	//Removing telescope page for now...
	removePage( page(2) );
	
	//Remove Download page if KDE < 3.2.90
	#if ( ! KDE_IS_VERSION( 3, 2, 90 ) ) 
	removePage( page(3) );
	#endif
	
	//each page should have a finish button
	for ( unsigned int i=0; i<((unsigned int) pageCount()); ++i ) {
		setFinishEnabled( page(i), true );
	}

	//Disable "Next" Button on last page
	setNextEnabled( page( pageCount() - 1 ), false );

	//Load images into banner frames.
	QFile imFile;
	QPixmap im = QPixmap();
	
	if ( KSUtils::openDataFile( imFile, "wzstars.png" ) ) {
		imFile.close(); //Just need the filename...
		im.load( imFile.name() );
	}
	Banner1->setPixmap( im );
	
	if ( KSUtils::openDataFile( imFile, "wzgeo.png" ) ) {
		imFile.close(); //Just need the filename...
		im.load( imFile.name() );
	}
	Banner2->setPixmap( im );
	
//Uncomment if we ever need the telescope page...
//	if ( KSUtils::openDataFile( imFile, "wzscope.png" ) ) {
//		imFile.close(); //Just need the filename...
//		im.load( imFile.name() );
//	}
//	Banner3->setPixmap( im );

	//Only load the download page banner if KDE >= 3.2.90
	#if ( KDE_IS_VERSION( 3, 2, 90 ) )
	if ( KSUtils::openDataFile( imFile, "wzdownload.png" ) ) {
		imFile.close(); //Just need the filename...
		im.load( imFile.name() );
	}
	Banner4->setPixmap( im );
	#endif

	//connect signals/slots
	connect( CityListBox, SIGNAL( selectionChanged() ), this, SLOT( slotChangeCity() ) );
	connect( CityFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
	connect( ProvinceFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
	connect( CountryFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
//Uncomment if we ever need the telescope page...
//	connect( TelescopeWizardButton, SIGNAL( clicked() ), this, SLOT( slotTelescopeSetup() ) );
	#if ( KDE_IS_VERSION( 3, 2, 90 ) )
	connect( DownloadButton, SIGNAL( clicked() ), ksw, SLOT( slotDownload() ) );
	#endif
	
	//Initialize Geographic Location page
	filteredCityList.setAutoDelete( false );
	initGeoPage();
}

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
		for (GeoLocation *loc = filteredCityList.first(); loc; loc = filteredCityList.next()) {
			if ( loc->fullName() == CityListBox->currentText() ) {
				Geo = loc;
				break;
			}
		}
	}

	LongBox->showInDegrees( Geo->lng() );
	LatBox->showInDegrees( Geo->lat() );
}

void KSWizard::slotFilterCities() {
	CityListBox->clear();
	filteredCityList.clear();

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
