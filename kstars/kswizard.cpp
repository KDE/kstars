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

#include "kswizard.h"

#include <QFile>
#include <QStackedWidget>
#include <QPixmap>

#include <klineedit.h>
#include <QPushButton>
#include <kns3/downloaddialog.h>
#include <QStandardPaths>


#include "kstarsdata.h"
#include "geolocation.h"
#include "widgets/dmsbox.h"

namespace {
    bool hasPrefix(QString str, QString prefix) {
        if( prefix.isEmpty() )
            return true;
        return str.startsWith( prefix, Qt::CaseInsensitive );
    }
}

WizWelcomeUI::WizWelcomeUI( QWidget *parent ) : QFrame( parent ) {
    setupUi( this );
}

WizLocationUI::WizLocationUI( QWidget *parent ) : QFrame( parent ) {
    setupUi( this );
}

WizDownloadUI::WizDownloadUI( QWidget *parent ) : QFrame( parent ) {
    setupUi( this );
}

KSWizard::KSWizard( QWidget *parent ) :
    KDialog( parent )
{
    wizardStack = new QStackedWidget( this );
    setMainWidget( wizardStack );
    setCaption( xi18n("Setup Wizard") );
    setButtons( KDialog::User1|KDialog::User2|KDialog::Ok|KDialog::Cancel );
    setButtonGuiItem( KDialog::User1, KGuiItem( xi18n("&Next >"), QString(), xi18n("Go to next Wizard page") ) );
    setButtonGuiItem( KDialog::User2, KGuiItem( xi18n("< &Back"), QString(), xi18n("Go to previous Wizard page") ) );

    WizWelcomeUI* welcome = new WizWelcomeUI( wizardStack );
    location = new WizLocationUI( wizardStack );
    WizDownloadUI* download = new WizDownloadUI( wizardStack );

    wizardStack->addWidget( welcome );
    wizardStack->addWidget( location );
    wizardStack->addWidget( download );
    wizardStack->setCurrentWidget( welcome );

    //Load images into banner frames.
    QPixmap im;
    if( im.load(QStandardPaths::locate(QStandardPaths::DataLocation, "wzstars.png")) )
        welcome->Banner->setPixmap( im );
    if( im.load(QStandardPaths::locate(QStandardPaths::DataLocation, "wzgeo.png")) )
        location->Banner->setPixmap( im );
    if( im.load(QStandardPaths::locate(QStandardPaths::DataLocation, "wzdownload.png")) )
        download->Banner->setPixmap( im );

    //connect signals/slots
    connect( this, SIGNAL( user1Clicked() ), this, SLOT( slotNextPage() ) );
    connect( this, SIGNAL( user2Clicked() ), this, SLOT( slotPrevPage() ) );
    connect( location->CityListBox, SIGNAL( itemSelectionChanged () ), this, SLOT( slotChangeCity() ) );
    connect( location->CityFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
    connect( location->ProvinceFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
    connect( location->CountryFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
    connect( download->DownloadButton, SIGNAL( clicked() ), this, SLOT( slotDownload() ) );

    //Disable Back button
    enableButton( KDialog::User2, false );

    //Initialize Geographic Location page
    initGeoPage();
}

//Do NOT delete members of filteredCityList!  They are not created by KSWizard.
KSWizard::~KSWizard()
{}

void KSWizard::setButtonsEnabled() {
    enableButton( KDialog::User1, wizardStack->currentIndex() < wizardStack->count()-1 );
    enableButton( KDialog::User2, wizardStack->currentIndex() > 0 );
}

void KSWizard::slotNextPage() {
    wizardStack->setCurrentIndex( wizardStack->currentIndex() + 1 );
    setButtonsEnabled();
}

void KSWizard::slotPrevPage() {
    wizardStack->setCurrentIndex( wizardStack->currentIndex() - 1 );
    setButtonsEnabled();
}

void KSWizard::initGeoPage() {
    KStarsData* data = KStarsData::Instance();
    location->LongBox->setReadOnly( true );
    location->LatBox->setReadOnly( true );

    //Populate the CityListBox
    //flag the ID of the current City
    foreach ( GeoLocation *loc, data->getGeoList() ) {
        location->CityListBox->addItem( loc->fullName() );
        filteredCityList.append( loc );
        if ( loc->fullName() == data->geo()->fullName() ) {
            Geo = loc;
        }
    }

    //Sort alphabetically
    location->CityListBox->sortItems();
    //preset to current city
    location->CityListBox->setCurrentItem(location->CityListBox->findItems(QString(data->geo()->fullName()),
									   Qt::MatchExactly).at(0));
}

void KSWizard::slotChangeCity() {
    if ( location->CityListBox->currentItem() ) {
        for ( int i=0; i < filteredCityList.size(); ++i ) {
            if ( filteredCityList[i]->fullName() == location->CityListBox->currentItem()->text() ) {
                Geo = filteredCityList[i];
                break;
            }
        }
        location->LongBox->showInDegrees( Geo->lng() );
        location->LatBox->showInDegrees( Geo->lat() );
    }
}

void KSWizard::slotFilterCities() {
    location->CityListBox->clear();
    //Do NOT delete members of filteredCityList!
    filteredCityList.clear();

    foreach ( GeoLocation *loc, KStarsData::Instance()->getGeoList() ) {
        if( hasPrefix( loc->translatedName(),     location->CityFilter->text()     ) &&
            hasPrefix( loc->translatedCountry(),  location->CountryFilter->text() ) &&
            hasPrefix( loc->translatedProvince(), location->ProvinceFilter->text()  )
            )
        {
            location->CityListBox->addItem( loc->fullName() );
            filteredCityList.append( loc );
        }
    }
    location->CityListBox->sortItems();

    if ( location->CityListBox->count() > 0 )  // set first item in list as selected
        location->CityListBox->setCurrentItem( location->CityListBox->item(0) );
}

void KSWizard::slotDownload() {
    KNS3::DownloadDialog dlg;
    dlg.exec();
}

#include "kswizard.moc"
