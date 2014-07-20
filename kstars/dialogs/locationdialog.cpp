/***************************************************************************
                          locationdialog.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 11 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "locationdialog.h"
#include "ui_locationdialog.h"

#include <cstdlib>
#include <cstdio>

#include <QFile>
#include <QTextStream>
#include <QListWidget>

#include <kmessagebox.h>

#include <KLocalizedString>
#include <QStandardPaths>

#include "kstarsdata.h"

LocationDialogUI::LocationDialogUI( QWidget *parent ) : QFrame( parent )
{
    setupUi(this);
}

LocationDialog::LocationDialog( QWidget* parent ) :
    QDialog( parent ), timer( 0 )
{
    KStarsData* data = KStarsData::Instance();

    ld = new LocationDialogUI(this);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ld);
    setLayout(mainLayout);

    ld->MapView->setLocationDialog( this );

    setWindowTitle( xi18n( "Set Geographic Location" ) );

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(slotOk()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    for ( int i=0; i<25; ++i )
        ld->TZBox->addItem( QLocale().toString( (double)(i-12) ) );

    //Populate DSTRuleBox
    foreach( const QString& key, data->getRulebook().keys() ) {
        if( !key.isEmpty() )
            ld->DSTRuleBox->addItem( key );
    }

    connect( ld->CityFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( enqueueFilterCity() ) );
    connect( ld->ProvinceFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( enqueueFilterCity() ) );
    connect( ld->CountryFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( enqueueFilterCity() ) );
    connect( ld->NewCityName, SIGNAL( textChanged( const QString & ) ), this, SLOT( nameChanged() ) );
    connect( ld->NewProvinceName, SIGNAL( textChanged( const QString & ) ), this, SLOT( nameChanged() ) );
    connect( ld->NewCountryName, SIGNAL( textChanged( const QString & ) ), this, SLOT( nameChanged() ) );
    connect( ld->NewLong, SIGNAL( textChanged( const QString & ) ), this, SLOT( dataChanged() ) );
    connect( ld->NewLat, SIGNAL( textChanged( const QString & ) ), this, SLOT( dataChanged() ) );
    connect( ld->TZBox, SIGNAL( activated(int) ), this, SLOT( dataChanged() ) );
    connect( ld->DSTRuleBox, SIGNAL( activated(int) ), this, SLOT( dataChanged() ) );
    connect( ld->GeoBox, SIGNAL( itemSelectionChanged () ), this, SLOT( changeCity() ) );
    connect( ld->AddCityButton, SIGNAL( clicked() ), this, SLOT( addCity() ) );
    connect( ld->ClearFieldsButton, SIGNAL( clicked() ), this, SLOT( clearFields() ) );

    ld->DSTLabel->setText( "<a href=\"showrules\">" + xi18n("DST Rule:") + "</a>" );
    connect( ld->DSTLabel, SIGNAL( linkActivated(const QString &) ), this, SLOT( showTZRules() ) );

    dataModified = false;
    nameModified = false;
    ld->AddCityButton->setEnabled( false );

    initCityList();
    resize (640, 480);    
}

void LocationDialog::initCityList() {
    KStarsData* data = KStarsData::Instance();
    foreach ( GeoLocation *loc, data->getGeoList() )
    {
        ld->GeoBox->addItem( loc->fullName() );
        filteredCityList.append( loc );

        //If TZ is not an even integer value, add it to listbox
        if ( loc->TZ0() - int( loc->TZ0() ) && ld->TZBox->findText( QLocale().toString( loc->TZ0() ) ) != -1 ) {
            for ( int i=0; i < ld->TZBox->count(); ++i ) {
                if ( ld->TZBox->itemText( i ).toDouble() > loc->TZ0() ) {
                    ld->TZBox->addItem( QLocale().toString( loc->TZ0() ), i-1 );
                    break;
                }
            }
        }
    }

    //Sort the list of Cities alphabetically...note that filteredCityList may now have a different ordering!
    ld->GeoBox->sortItems();

    ld->CountLabel->setText( xi18np("One city matches search criteria","%1 cities match search criteria", ld->GeoBox->count()) );

    // attempt to highlight the current kstars location in the GeoBox
    ld->GeoBox->setCurrentItem( 0 );
    for( int i=0; i < ld->GeoBox->count(); i++ ) {
        if ( ld->GeoBox->item(i)->text() == data->geo()->fullName() ) {
            ld->GeoBox->setCurrentRow( i );
            break;
        }
    }
}

void LocationDialog::enqueueFilterCity() {
    if( timer )
        timer->stop();
    else {
        timer = new QTimer( this );
        timer->setSingleShot( true );
        connect( timer, SIGNAL( timeout() ), this, SLOT( filterCity() ) );
    }
    timer->start( 500 );
}

void LocationDialog::filterCity() {
    KStarsData* data = KStarsData::Instance();
    ld->GeoBox->clear();
    //Do NOT delete members of filteredCityList!
    while( !filteredCityList.isEmpty() )
        filteredCityList.takeFirst();

    nameModified = false;
    dataModified = false;
    ld->AddCityButton->setEnabled( false );

    foreach ( GeoLocation *loc, data->getGeoList() ) {
        QString sc( loc->translatedName() );
        QString ss( loc->translatedCountry() );
        QString sp = "";
        if ( !loc->province().isEmpty() )
            sp = loc->translatedProvince();

        if ( sc.toLower().startsWith( ld->CityFilter->text().toLower() ) &&
                sp.toLower().startsWith( ld->ProvinceFilter->text().toLower() ) &&
                ss.toLower().startsWith( ld->CountryFilter->text().toLower() ) ) {

            ld->GeoBox->addItem( loc->fullName() );
            filteredCityList.append( loc );
        }
    }

    ld->GeoBox->sortItems();

    ld->CountLabel->setText( xi18np("One city matches search criteria","%1 cities match search criteria", ld->GeoBox->count()) );

    if ( ld->GeoBox->count() > 0 )		// set first item in list as selected
        ld->GeoBox->setCurrentItem( ld->GeoBox->item(0) );

    ld->MapView->repaint();
}

void LocationDialog::changeCity() {
    KStarsData* data = KStarsData::Instance();
    //when the selected city changes, set newCity, and redraw map
    SelectedCity = 0L;
    if ( ld->GeoBox->currentItem() ) {
        for ( int i=0; i < filteredCityList.size(); ++i ) {
            GeoLocation *loc = filteredCityList.at(i);
            if ( loc->fullName() == ld->GeoBox->currentItem()->text()) {
                SelectedCity = loc;
                break;
            }
        }
    }

    ld->MapView->repaint();

    //Fill the fields at the bottom of the window with the selected city's data.
    if ( SelectedCity ) {
        ld->NewCityName->setText( SelectedCity->translatedName() );
        if ( SelectedCity->province().isEmpty() )
            ld->NewProvinceName->setText( QString() );
        else
            ld->NewProvinceName->setText( SelectedCity->translatedProvince() );

        ld->NewCountryName->setText( SelectedCity->translatedCountry() );
        ld->NewLong->showInDegrees( SelectedCity->lng() );
        ld->NewLat->showInDegrees( SelectedCity->lat() );
        ld->TZBox->setEditText( QLocale().toString( SelectedCity->TZ0() ) );

        //Pick the City's rule from the rulebook
        for ( int i=0; i < ld->DSTRuleBox->count(); ++i ) {
            TimeZoneRule tzr = data->getRulebook().value( ld->DSTRuleBox->itemText(i) );
            if ( tzr.equals( SelectedCity->tzrule() ) ) {
                ld->DSTRuleBox->setCurrentIndex( i );
                break;
            }
        }
    }

    nameModified = false;
    dataModified = false;
    ld->AddCityButton->setEnabled( false );
}

bool LocationDialog::addCity( ) {
    KStarsData* data = KStarsData::Instance();
    if ( !nameModified && !dataModified ) {
        QString message = xi18n( "This City already exists in the database." );
        KMessageBox::sorry( 0, message, xi18n( "Error: Duplicate Entry" ) );
        return false;
    }

    bool latOk(false), lngOk(false), tzOk(false);
    dms lat = ld->NewLat->createDms( true, &latOk );
    dms lng = ld->NewLong->createDms( true, &lngOk );
    QString TimeZoneString = ld->TZBox->lineEdit()->text();
    TimeZoneString.replace( QLocale().decimalPoint(), "." );
    double TZ = TimeZoneString.toDouble( &tzOk );

    if ( ld->NewCityName->text().isEmpty() || ld->NewCountryName->text().isEmpty() ) {
        QString message = xi18n( "All fields (except province) must be filled to add this location." );
        KMessageBox::sorry( 0, message, xi18n( "Fields are Empty" ) );
        return false;
    } else if ( ! latOk || ! lngOk ) {
        QString message = xi18n( "Could not parse the Latitude/Longitude." );
        KMessageBox::sorry( 0, message, xi18n( "Bad Coordinates" ) );
        return false;
    } else if( ! tzOk) {
    	QString message = xi18n( "Could not parse coordinates." );
        KMessageBox::sorry( 0, message, xi18n( "Bad Coordinates" ) );
        return false;
    } else {
        if ( !nameModified ) {
            QString message = xi18n( "Really override original data for this city?" );
            if ( KMessageBox::questionYesNo( 0, message, xi18n( "Override Existing Data?" ), KGuiItem(xi18n("Override Data")), KGuiItem(xi18n("Do Not Override"))) == KMessageBox::No )
                return false; //user answered No.
        }

        QString entry;
        QFile file;

        //Strip off white space
        QString name = ld->NewCityName->text().trimmed();
        QString province = ld->NewProvinceName->text().trimmed();
        QString country = ld->NewCountryName->text().trimmed();

        //check for user's city database.  If it doesn't exist, create it.
        file.setFileName( QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1Char('/') + "mycities.dat" ) ; //determine filename in local user KDE directory tree.

        if ( !file.open( QIODevice::ReadWrite | QIODevice::Append ) ) {
            QString message = xi18n( "Local cities database could not be opened.\nLocation will not be recorded." );
            KMessageBox::sorry( 0, message, xi18n( "Could Not Open File" ) );
            return false;
        } else {
            char ltsgn = 'N'; if ( lat.degree()<0 ) ltsgn = 'S';
            char lgsgn = 'E'; if ( lng.degree()<0 ) lgsgn = 'W';
            QString TZrule = ld->DSTRuleBox->currentText();

            entry = entry.sprintf( "%-32s : %-21s : %-21s : %2d : %2d : %2d : %c : %3d : %2d : %2d : %c : %5.1f : %2s\n",
                                   name.toLocal8Bit().data(), province.toLocal8Bit().data(), country.toLocal8Bit().data(),
                                   abs(lat.degree()), lat.arcmin(), lat.arcsec(), ltsgn,
                                   abs(lng.degree()), lng.arcmin(), lat.arcsec(), lgsgn,
                                   TZ, TZrule.toLocal8Bit().data() );

            QTextStream stream( &file );
            stream << entry;
            file.close();

            //Add city to geoList...don't need to insert it alphabetically, since we always sort GeoList
            GeoLocation *g = new GeoLocation( lng, lat,
                                              ld->NewCityName->text(), ld->NewProvinceName->text(), ld->NewCountryName->text(),
                                              TZ, &data->Rulebook[ TZrule ] );
            // FIXME: Uses friendship
            data->geoList.append( g );

            //(possibly) insert new city into GeoBox by running filterCity()
            filterCity();

            //Attempt to highlight new city in list
            ld->GeoBox->setCurrentItem( 0 );
            if ( ld->GeoBox->count() ) {
                for ( int i=0; i<ld->GeoBox->count(); i++ ) {
                    if ( ld->GeoBox->item(i)->text() == g->fullName() ) {
                        ld->GeoBox->setCurrentRow( i );
                        break;
                    }
                }
            }

        }
    }
    return true;
}

void LocationDialog::findCitiesNear( int lng, int lat ) {
    KStarsData* data = KStarsData::Instance();
    //find all cities within 3 degrees of (lng, lat); list them in GeoBox
    ld->GeoBox->clear();
    //Remember, do NOT delete members of filteredCityList
    while ( ! filteredCityList.isEmpty() ) filteredCityList.takeFirst();

    foreach ( GeoLocation *loc, data->getGeoList() ) {
        if ( ( abs(	lng - int( loc->lng()->Degrees() ) ) < 3 ) &&
                ( abs( lat - int( loc->lat()->Degrees() ) ) < 3 ) ) {

            ld->GeoBox->addItem( loc->fullName() );
            filteredCityList.append( loc );
        }
    }

    ld->GeoBox->sortItems();
    ld->CountLabel->setText( xi18np("One city matches search criteria","%1 cities match search criteria", ld->GeoBox->count()) );

    if ( ld->GeoBox->count() > 0 )		// set first item in list as selected
        ld->GeoBox->setCurrentItem( ld->GeoBox->item(0) );

    repaint();
}

bool LocationDialog::checkLongLat() {
    if ( ld->NewLong->text().isEmpty() || ld->NewLat->text().isEmpty() )
        return false;

    bool ok;
    double lng = ld->NewLong->createDms(true, &ok).Degrees();
    if( !ok )
        return false;
    double lat = ld->NewLat->createDms(true, &ok).Degrees();
    if( !ok )
        return false;

    if( fabs(lng) > 180 || fabs(lat) > 90 )
        return false;

    return true;
}

void LocationDialog::clearFields() {
    ld->CityFilter->clear();
    ld->ProvinceFilter->clear();
    ld->CountryFilter->clear();
    ld->NewCityName->clear();
    ld->NewProvinceName->clear();
    ld->NewCountryName->clear();
    ld->NewLong->clearFields();
    ld->NewLat->clearFields();
    ld->TZBox->lineEdit()->setText( QLocale().toString( 0.0 ) );
    ld->DSTRuleBox->setCurrentIndex( 0 );
    nameModified = true;
    dataModified = false;
    ld->AddCityButton->setEnabled( false );
    ld->NewCityName->setFocus();
}

void LocationDialog::showTZRules() {
    QStringList lines;
    lines.append( xi18n( " Start Date (Start Time)  /  Revert Date (Revert Time)" ) );
    lines.append( " " );
    lines.append( xi18n( "--: No DST correction" ) );
    lines.append( xi18n( "AU: last Sun in Oct. (02:00) / last Sun in Mar. (02:00)" ) );
    lines.append( xi18n( "BZ:  2nd Sun in Oct. (00:00) /  3rd Sun in Feb. (00:00)" ) );
    lines.append( xi18n( "CH:  2nd Sun in Apr. (00:00) /  2nd Sun in Sep. (00:00)" ) );
    lines.append( xi18n( "CL:  2nd Sun in Oct. (04:00) /  2nd Sun in Mar. (04:00)" ) );
    lines.append( xi18n( "CZ:  1st Sun in Oct. (02:45) /  3rd Sun in Mar. (02:45)" ) );
    lines.append( xi18n( "EE: Last Sun in Mar. (00:00) / Last Sun in Oct. (02:00)" ) );
    lines.append( xi18n( "EG: Last Fri in Apr. (00:00) / Last Thu in Sep. (00:00)" ) );
    lines.append( xi18n( "EU: Last Sun in Mar. (01:00) / Last Sun in Oct. (01:00)" ) );
    lines.append( xi18n( "FK:  1st Sun in Sep. (02:00) /  3rd Sun in Apr. (02:00)" ) );
    lines.append( xi18n( "HK:  2nd Sun in May  (03:30) /  3rd Sun in Oct. (03:30)" ) );
    lines.append( xi18n( "IQ: Apr 1 (03:00) / Oct. 1 (00:00)" ) );
    lines.append( xi18n( "IR: Mar 21 (00:00) / Sep. 22 (00:00)" ) );
    lines.append( xi18n( "JD: Last Thu in Mar. (00:00) / Last Thu in Sep. (00:00)" ) );
    lines.append( xi18n( "LB: Last Sun in Mar. (00:00) / Last Sun in Oct. (00:00)" ) );
    lines.append( xi18n( "MX:  1st Sun in May  (02:00) / Last Sun in Sep. (02:00)" ) );
    lines.append( xi18n( "NB:  1st Sun in Sep. (02:00) /  1st Sun in Apr. (02:00)" ) );
    lines.append( xi18n( "NZ:  1st Sun in Oct. (02:00) /  3rd Sun in Mar. (02:00)" ) );
    lines.append( xi18n( "PY:  1st Sun in Oct. (00:00) /  1st Sun in Mar. (00:00)" ) );
    lines.append( xi18n( "RU: Last Sun in Mar. (02:00) / Last Sun in Oct. (02:00)" ) );
    lines.append( xi18n( "SK:  2nd Sun in May  (00:00) /  2nd Sun in Oct. (00:00)" ) );
    lines.append( xi18n( "SY: Apr. 1 (00:00) / Oct. 1 (00:00)" ) );
    lines.append( xi18n( "TG:  1st Sun in Nov. (02:00) / Last Sun in Jan. (02:00)" ) );
    lines.append( xi18n( "TS:  1st Sun in Oct. (02:00) / Last Sun in Mar. (02:00)" ) );
    lines.append( xi18n( "US:  1st Sun in Apr. (02:00) / Last Sun in Oct. (02:00)" ) );
    lines.append( xi18n( "ZN: Apr. 1 (01:00) / Oct. 1 (00:00)" ) );

    QString message = xi18n( "Daylight Saving Time Rules" );

    QPointer<QDialog> tzd = new QDialog( this );
    tzd->setWindowTitle( message );

    QListWidget *lw = new QListWidget( tzd );
    lw->addItems( lines );
    //This is pretty lame...I have to measure the width of the first item in the
    //list widget, in order to set its width properly.  Why doesn't it just resize
    //the widget to fit the contents automatically?  I tried setting the sizePolicy,
    //no joy...
    int w = int( 1.1*lw->visualItemRect( lw->item(0) ).width() );
    lw->setMinimumWidth( w );   

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(lw);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), tzd, SLOT(reject()));

    tzd->setLayout(mainLayout);

    tzd->exec();

    delete tzd;
}

void LocationDialog::nameChanged() {
    nameModified = true;
    dataChanged();
}

//do not enable Add button until all data are present and valid.
void LocationDialog::dataChanged() {
    dataModified = true;
    ld->AddCityButton->setEnabled( !ld->NewCityName->text().isEmpty() &&
                                   !ld->NewCountryName->text().isEmpty() &&
                                   checkLongLat() );
}

void LocationDialog::slotOk() {
    if( addCityEnabled() && addCity() )
        accept();
}

bool LocationDialog::addCityEnabled() { return ld->AddCityButton->isEnabled(); }

