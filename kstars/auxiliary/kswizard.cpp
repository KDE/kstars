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
#include <QLineEdit>
#include <QPushButton>
#include <kns3/downloaddialog.h>
#include <QStandardPaths>

#include "kspaths.h"
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
    QDialog( parent )
{
    wizardStack = new QStackedWidget( this );

    setWindowTitle( i18n("Setup Wizard") );

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(wizardStack);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, Qt::Horizontal);
    nextB = new QPushButton(i18n("&Next >"));
    nextB->setDefault(true);
    backB = new QPushButton(i18n("< &Back"));
    backB->setEnabled(false);

    buttonBox->addButton(backB, QDialogButtonBox::ActionRole);
    buttonBox->addButton(nextB, QDialogButtonBox::ActionRole);

    mainLayout->addWidget(buttonBox);

    WizWelcomeUI* welcome = new WizWelcomeUI( wizardStack );
    location = new WizLocationUI( wizardStack );
    WizDownloadUI* download = new WizDownloadUI( wizardStack );

    wizardStack->addWidget( welcome );
    wizardStack->addWidget( location );
    wizardStack->addWidget( download );
    wizardStack->setCurrentWidget( welcome );

    //Load images into banner frames.
    QPixmap im;
    if( im.load(KSPaths::locate(QStandardPaths::GenericDataLocation, "wzstars.png")) )
        welcome->Banner->setPixmap( im );
    if( im.load(KSPaths::locate(QStandardPaths::GenericDataLocation, "wzgeo.png")) )
        location->Banner->setPixmap( im );
    if( im.load(KSPaths::locate(QStandardPaths::GenericDataLocation, "wzdownload.png")) )
        download->Banner->setPixmap( im );

    //connect signals/slots
    connect(nextB, SIGNAL(clicked()), this, SLOT(slotNextPage()));
    connect(backB, SIGNAL(clicked()), this, SLOT(slotPrevPage()));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    connect( location->CityListBox, SIGNAL( itemSelectionChanged () ), this, SLOT( slotChangeCity() ) );
    connect( location->CityFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
    connect( location->ProvinceFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
    connect( location->CountryFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterCities() ) );
    connect( download->DownloadButton, SIGNAL( clicked() ), this, SLOT( slotDownload() ) );

    //Initialize Geographic Location page
    initGeoPage();
}

//Do NOT delete members of filteredCityList!  They are not created by KSWizard.
KSWizard::~KSWizard()
{}

void KSWizard::setButtonsEnabled() {
    nextB->setEnabled(wizardStack->currentIndex() < wizardStack->count()-1 );
    backB->setEnabled(wizardStack->currentIndex() > 0 );
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


