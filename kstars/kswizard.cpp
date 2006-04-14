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

#include <QFile>
#include <QStackedWidget>
#include <QPixmap>

#include <kdialog.h>
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

KSWizard::KSWizard( QWidget *parent )
 : KDialog( parent, i18n("KStars Startup Wizard"), 
		KDialog::User1|KDialog::User2|KDialog::Ok|KDialog::Cancel, 0, 
		KGuiItem( i18n("&Next") + QString(" >"), QString(), 
			i18n("Go to next Wizard page") ),
		KGuiItem( QString("< ") + i18n("&Back"), QString(), 
			i18n("Go to previous Wizard page") ) )
{
	ksw = (KStars *)parent;
	GeoID.resize(10000);
	
	wizardStack = new QStackedWidget( this );
	setMainWidget( wizardStack );

	welcome  = new WizWelcomeUI( wizardStack );
	location = new WizLocationUI( wizardStack );
	devices  = new WizDevicesUI( wizardStack );
	download = new WizDownloadUI( wizardStack );

	wizardStack->addWidget( welcome );
	wizardStack->addWidget( location );
	wizardStack->addWidget( devices );
	wizardStack->addWidget( download );
	wizardStack->setCurrentWidget( welcome );

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
	connect( this, SIGNAL( user1Clicked() ), this, SLOT( slotNextPage() ) );
	connect( this, SIGNAL( user2Clicked() ), this, SLOT( slotPrevPage() ) );
	connect( location->CityListBox, SIGNAL( selectionChanged() ), this, SLOT( slotChangeCity() ) );
	connect( location->CityFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
	connect( location->ProvinceFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
	connect( location->CountryFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
	connect( devices->TelescopeWizardButton, SIGNAL( clicked() ), this, SLOT( slotTelescopeSetup() ) );
	connect( download->DownloadButton, SIGNAL( clicked() ), ksw, SLOT( slotDownload() ) );
	
	//Disable Back button
	user2Button()->setEnabled( false );

	//Initialize Geographic Location page
	initGeoPage();
}

//Do NOT delete members of filteredCityList!  They are not created by KSWizard.
KSWizard::~KSWizard() 
{
	delete welcome;
	delete location;
	delete devices;
	delete download;
}

void KSWizard::slotNextPage() {
	wizardStack->setCurrentIndex( wizardStack->currentIndex() + 1 );
	if ( wizardStack->currentIndex() == wizardStack->count() - 1 ) 
		user1Button()->setEnabled( false );

	user2Button()->setEnabled( true );
}

void KSWizard::slotPrevPage() {
	wizardStack->setCurrentIndex( wizardStack->currentIndex() - 1 );
	if ( wizardStack->currentIndex() == 0 ) 
		user2Button()->setEnabled( false );

	user1Button()->setEnabled( true );
}

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
		QString sp;
		if ( !loc->province().isEmpty() )
			sp = loc->translatedProvince();

		if ( sc.toLower().startsWith( location->CityFilter->text().toLower() ) &&
				sp.toLower().startsWith( location->ProvinceFilter->text().toLower() ) &&
				ss.toLower().startsWith( location->CountryFilter->text().toLower() ) ) {
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
