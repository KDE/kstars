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
#include "widgets/dmsbox.h"
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

KSWizardUI::KSWizardUI( QWidget *parent ) : QFrame( parent ) {
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
	connect( location->CityListBox, SIGNAL( selectionChanged() ), this, SLOT( slotChangeCity() ) );
	connect( location->CityFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
	connect( location->ProvinceFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
	connect( location->CountryFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
	connect( devices->TelescopeWizardButton, SIGNAL( clicked() ), this, SLOT( slotTelescopeSetup() ) );
	connect( download->DownloadButton, SIGNAL( clicked() ), ksw, SLOT( slotDownload() ) );
	
	//Initialize Geographic Location page
	initGeoPage();
}

//Do NOT delete members of filteredCityList!  They are not created by KSWizard.
KSWizard::~KSWizard() 
{}

void KSWizard::initGeoPage() {
	location->LongBox->setReadOnly( true );
	location->LatBox->setReadOnly( true );

	//Populate the CityListBox
	//flag the ID of the current City
	int index(0);
	foreach ( GeoLocation *loc, ksw->data()->geoList ) {
		location->CityListBox->insertItem( loc->fullName() );
		filteredCityList.append( loc );
		
		if ( loc->fullName() == ksw->data()->geo()->fullName() ) {
			index = ksw->data()->geoList.indexOf( loc );
			Geo = loc;
		}
	}
	
	//Sort alphabetically
	location->CityListBox->sort();
	
	//preset to current city
	location->CityListBox->setCurrentItem( index + 1 );
}

void KSWizard::slotChangeCity() {
	Geo = 0L;
	
	if ( location->CityListBox->currentItem() >= 0 ) {
		for ( int i=0; i < filteredCityList.size(); ++i ) {
			if ( filteredCityList[i]->fullName() == location->CityListBox->currentText() ) {
				Geo = filteredCityList[i];
				break;
			}
		}
	}

	location->LongBox->showInDegrees( Geo->lng() );
	location->LatBox->showInDegrees( Geo->lat() );
}

void KSWizard::slotFilterCities() {
	location->CityListBox->clear();
	//Do NOT delete members of filteredCityList!
	while ( ! filteredCityList.isEmpty() ) filteredCityList.takeFirst();

	foreach ( GeoLocation *loc, ksw->data()->geoList ) {
		QString sc( loc->translatedName() );
		QString ss( loc->translatedCountry() );
		QString sp = "";
		if ( !loc->province().isEmpty() )
			sp = loc->translatedProvince();

		if ( sc.lower().startsWith( location->CityFilter->text().lower() ) &&
				sp.lower().startsWith( location->ProvinceFilter->text().lower() ) &&
				ss.lower().startsWith( location->CountryFilter->text().lower() ) ) {
			location->CityListBox->insertItem( loc->fullName() );
			filteredCityList.append( loc );
		}
	}
	
	location->CityListBox->sort();

	if ( location->CityListBox->firstItem() )  // set first item in list as selected
		location->CityListBox->setCurrentItem( location->CityListBox->firstItem() );
}

void KSWizard::slotTelescopeSetup() {
	telescopeWizardProcess twiz(ksw);
	twiz.exec();
}

#include "kswizard.moc"
