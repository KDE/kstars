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

#include <KMessageBox>

#include <KLocalizedString>
#include <QStandardPaths>

#include "kstarsdata.h"
#include "kspaths.h"

LocationDialogUI::LocationDialogUI( QWidget *parent ) : QFrame( parent )
{
    setupUi(this);
}

LocationDialog::LocationDialog( QWidget* parent ) :
    QDialog( parent ), timer( 0 )
{
    KStarsData* data = KStarsData::Instance();

    SelectedCity = NULL;
    ld = new LocationDialogUI(this);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ld);
    setLayout(mainLayout);

    ld->MapView->setLocationDialog( this );

    setWindowTitle( i18n( "Set Geographic Location" ) );

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

    ld->AddCityButton->setIcon(QIcon::fromTheme("list-add"));
    ld->RemoveButton->setIcon(QIcon::fromTheme("list-remove"));
    ld->UpdateButton->setIcon(QIcon::fromTheme("svn-update"));

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
    connect( ld->RemoveButton, SIGNAL(clicked()), this, SLOT(removeCity()));
    connect( ld->UpdateButton, SIGNAL(clicked()), this, SLOT(updateCity()));

    ld->DSTLabel->setText( "<a href=\"showrules\">" + i18n("DST Rule:") + "</a>" );
    connect( ld->DSTLabel, SIGNAL( linkActivated(const QString &) ), this, SLOT( showTZRules() ) );

    dataModified = false;
    nameModified = false;
    ld->AddCityButton->setEnabled( false );    

    ld->errorLabel->setText( QString() );

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

    ld->CountLabel->setText( i18np("One city matches search criteria","%1 cities match search criteria", ld->GeoBox->count()) );

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
    ld->UpdateButton->setEnabled( false );

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

    ld->CountLabel->setText( i18np("One city matches search criteria","%1 cities match search criteria", ld->GeoBox->count()) );

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

        ld->RemoveButton->setEnabled(SelectedCity->isReadOnly() == false);
    }

    nameModified = false;
    dataModified = false;
    ld->AddCityButton->setEnabled( false );
    ld->UpdateButton->setEnabled( false );
}

bool LocationDialog::addCity()
{
    return updateCity(CITY_ADD);
}

bool LocationDialog::updateCity()
{
    if (SelectedCity == NULL)
        return false;

    return updateCity(CITY_UPDATE);

}

bool LocationDialog::removeCity()
{
    if (SelectedCity == NULL)
        return false;

    return updateCity(CITY_REMOVE);
}


bool LocationDialog::updateCity(CityOperation operation)
{
    if (operation == CITY_REMOVE)
    {
        QString message = i18n( "Are you sure you want to remove %1?", selectedCityName() );
        if ( KMessageBox::questionYesNo( 0, message, i18n( "Remove City?" )) == KMessageBox::No )
            return false; //user answered No.
    }
    else if (!nameModified && !dataModified )
    {
        QString message = i18n( "This city already exists in the database." );
        KMessageBox::sorry( 0, message, i18n( "Error: Duplicate Entry" ) );
        return false;
    }

    bool latOk(false), lngOk(false), tzOk(false);
    dms lat = ld->NewLat->createDms( true, &latOk );
    dms lng = ld->NewLong->createDms( true, &lngOk );
    QString TimeZoneString = ld->TZBox->lineEdit()->text();
    TimeZoneString.replace( QLocale().decimalPoint(), "." );
    double TZ = TimeZoneString.toDouble( &tzOk );

    if ( ld->NewCityName->text().isEmpty() || ld->NewCountryName->text().isEmpty() )
    {
        QString message = i18n( "All fields (except province) must be filled to add this location." );
        KMessageBox::sorry( 0, message, i18n( "Fields are Empty" ) );
        return false;
    } else if ( ! latOk || ! lngOk )
    {
        QString message = i18n( "Could not parse the Latitude/Longitude." );
        KMessageBox::sorry( 0, message, i18n( "Bad Coordinates" ) );
        return false;
    } else if( ! tzOk)
    {
    	QString message = i18n( "Could not parse coordinates." );
        KMessageBox::sorry( 0, message, i18n( "Bad Coordinates" ) );
        return false;
    }

    // If name is still the same then it's an update operation
    if ( operation == CITY_ADD && !nameModified )
        operation = CITY_UPDATE;

        /*if ( !nameModified )
        {
            QString message = i18n( "Really override original data for this city?" );
            if ( KMessageBox::questionYesNo( 0, message, i18n( "Override Existing Data?" ), KGuiItem(i18n("Override Data")), KGuiItem(i18n("Do Not Override"))) == KMessageBox::No )
                return false; //user answered No.
        }*/

        QSqlDatabase mycitydb = QSqlDatabase::database("mycitydb");
        QString dbfile = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "mycitydb.sqlite";

        // If it doesn't exist, create it
        if (QFile::exists(dbfile) == false)
        {
            mycitydb.setDatabaseName(dbfile);
            mycitydb.open();
            QSqlQuery create_query(mycitydb);
            QString query("CREATE TABLE city ( "
                          "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                          "Name TEXT DEFAULT NULL, "
                          "Province TEXT DEFAULT NULL, "
                          "Country TEXT DEFAULT NULL, "
                          "Latitude TEXT DEFAULT NULL, "
                          "Longitude TEXT DEFAULT NULL, "
                          "TZ REAL DEFAULT NULL, "
                          "TZRule TEXT DEFAULT NULL)");
            if (create_query.exec(query) == false)
            {
                qWarning() << create_query.lastError() << endl;
                return false;
            }
        }
        else if (mycitydb.open() == false)
        {
            qWarning() << mycitydb.lastError() << endl;
            return false;
        }

        //Strip off white space
        QString name = ld->NewCityName->text().trimmed();
        QString province = ld->NewProvinceName->text().trimmed();
        QString country = ld->NewCountryName->text().trimmed();
        QString TZrule = ld->DSTRuleBox->currentText();
        GeoLocation *g = NULL;

        switch (operation)
        {
            case CITY_ADD:
            {
                QSqlQuery add_query(mycitydb);
                add_query.prepare("INSERT INTO city(Name, Province, Country, Latitude, Longitude, TZ, TZRule) VALUES(:Name, :Province, :Country, :Latitude, :Longitude, :TZ, :TZRule)");
                add_query.bindValue(":Name", name);
                add_query.bindValue(":Province", province);
                add_query.bindValue(":Country", country);
                add_query.bindValue(":Latitude", lat.toDMSString());
                add_query.bindValue(":Longitude", lng.toDMSString());
                add_query.bindValue(":TZ", TZ);
                add_query.bindValue(":TZRule", TZrule);
                if (add_query.exec() == false)
                {
                    qWarning() << add_query.lastError() << endl;
                    return false;
                }

                //Add city to geoList...don't need to insert it alphabetically, since we always sort GeoList
                g = new GeoLocation( lng, lat, name, province, country, TZ, & KStarsData::Instance()->Rulebook[ TZrule ] );
                KStarsData::Instance()->getGeoList().append(g);
            }
            break;

            case CITY_UPDATE:
            {

                g = SelectedCity;

                QSqlQuery update_query(mycitydb);
                update_query.prepare("UPDATE city SET Name = :newName, Province = :newProvince, Country = :newCountry, Latitude = :Latitude, Longitude = :Longitude, TZ = :TZ, TZRule = :TZRule WHERE "
                                  "Name = :Name AND Province = :Province AND Country = :Country");
                update_query.bindValue(":newName", name);
                update_query.bindValue(":newProvince", province);
                update_query.bindValue(":newCountry", country);
                update_query.bindValue(":Name", SelectedCity->name());
                update_query.bindValue(":Province", SelectedCity->province());
                update_query.bindValue(":Country", SelectedCity->country());
                update_query.bindValue(":Latitude", lat.toDMSString());
                update_query.bindValue(":Longitude", lng.toDMSString());
                update_query.bindValue(":TZ", TZ);
                update_query.bindValue(":TZRule", TZrule);
                if (update_query.exec() == false)
                {
                    qWarning() << update_query.lastError() << endl;
                    return false;
                }

                g->setName(name);
                g->setProvince(province);
                g->setCountry(country);
                g->setLat(lat);
                g->setLong(lng);
                g->setTZ(TZ);
                g->setTZRule(& KStarsData::Instance()->Rulebook[ TZrule ]);

            }
            break;

            case CITY_REMOVE:
            {
                g = SelectedCity;
                QSqlQuery delete_query(mycitydb);
                delete_query.prepare("DELETE FROM city WHERE Name = :Name AND Province = :Province AND Country = :Country");
                delete_query.bindValue(":Name", name);
                delete_query.bindValue(":Province", province);
                delete_query.bindValue(":Country", country);
                if (delete_query.exec() == false)
                {
                    qWarning() << delete_query.lastError() << endl;
                    return false;
                }

                filteredCityList.removeOne(g);
                KStarsData::Instance()->getGeoList().removeOne(g);
                delete(g);
                g=NULL;
            }
            break;
        }

        //(possibly) insert new city into GeoBox by running filterCity()
        filterCity();

        //Attempt to highlight new city in list
        ld->GeoBox->setCurrentItem( 0 );
        if (g && ld->GeoBox->count() )
        {
            for ( int i=0; i<ld->GeoBox->count(); i++ )
            {
                if ( ld->GeoBox->item(i)->text() == g->fullName() )
                {
                    ld->GeoBox->setCurrentRow( i );
                    break;
                }
            }
        }

        mycitydb.commit();
        mycitydb.close();

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
    ld->CountLabel->setText( i18np("One city matches search criteria","%1 cities match search criteria", ld->GeoBox->count()) );

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

void LocationDialog::clearFields()
{
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
    ld->UpdateButton->setEnabled( false );
    ld->NewCityName->setFocus();

}

void LocationDialog::showTZRules() {
    QStringList lines;
    lines.append( i18n( " Start Date (Start Time)  /  Revert Date (Revert Time)" ) );
    lines.append( " " );
    lines.append( i18n( "--: No DST correction" ) );
    lines.append( i18n( "AU: last Sun in Oct. (02:00) / last Sun in Mar. (02:00)" ) );
    lines.append( i18n( "BZ:  2nd Sun in Oct. (00:00) /  3rd Sun in Feb. (00:00)" ) );
    lines.append( i18n( "CH:  2nd Sun in Apr. (00:00) /  2nd Sun in Sep. (00:00)" ) );
    lines.append( i18n( "CL:  2nd Sun in Oct. (04:00) /  2nd Sun in Mar. (04:00)" ) );
    lines.append( i18n( "CZ:  1st Sun in Oct. (02:45) /  3rd Sun in Mar. (02:45)" ) );
    lines.append( i18n( "EE: Last Sun in Mar. (00:00) / Last Sun in Oct. (02:00)" ) );
    lines.append( i18n( "EG: Last Fri in Apr. (00:00) / Last Thu in Sep. (00:00)" ) );
    lines.append( i18n( "EU: Last Sun in Mar. (01:00) / Last Sun in Oct. (01:00)" ) );
    lines.append( i18n( "FK:  1st Sun in Sep. (02:00) /  3rd Sun in Apr. (02:00)" ) );
    lines.append( i18n( "HK:  2nd Sun in May  (03:30) /  3rd Sun in Oct. (03:30)" ) );
    lines.append( i18n( "IQ: Apr 1 (03:00) / Oct. 1 (00:00)" ) );
    lines.append( i18n( "IR: Mar 21 (00:00) / Sep. 22 (00:00)" ) );
    lines.append( i18n( "JD: Last Thu in Mar. (00:00) / Last Thu in Sep. (00:00)" ) );
    lines.append( i18n( "LB: Last Sun in Mar. (00:00) / Last Sun in Oct. (00:00)" ) );
    lines.append( i18n( "MX:  1st Sun in May  (02:00) / Last Sun in Sep. (02:00)" ) );
    lines.append( i18n( "NB:  1st Sun in Sep. (02:00) /  1st Sun in Apr. (02:00)" ) );
    lines.append( i18n( "NZ:  1st Sun in Oct. (02:00) /  3rd Sun in Mar. (02:00)" ) );
    lines.append( i18n( "PY:  1st Sun in Oct. (00:00) /  1st Sun in Mar. (00:00)" ) );
    lines.append( i18n( "RU: Last Sun in Mar. (02:00) / Last Sun in Oct. (02:00)" ) );
    lines.append( i18n( "SK:  2nd Sun in May  (00:00) /  2nd Sun in Oct. (00:00)" ) );
    lines.append( i18n( "SY: Apr. 1 (00:00) / Oct. 1 (00:00)" ) );
    lines.append( i18n( "TG:  1st Sun in Nov. (02:00) / Last Sun in Jan. (02:00)" ) );
    lines.append( i18n( "TS:  1st Sun in Oct. (02:00) / Last Sun in Mar. (02:00)" ) );
    lines.append( i18n( "US:  1st Sun in Apr. (02:00) / Last Sun in Oct. (02:00)" ) );
    lines.append( i18n( "ZN: Apr. 1 (01:00) / Oct. 1 (00:00)" ) );

    QString message = i18n( "Daylight Saving Time Rules" );

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
    ld->AddCityButton->setEnabled( nameModified && !ld->NewCityName->text().isEmpty() &&
                                   !ld->NewCountryName->text().isEmpty() &&
                                   checkLongLat() );
    if (SelectedCity)
        ld->UpdateButton->setEnabled(SelectedCity->isReadOnly() == false && !ld->NewCityName->text().isEmpty()
                                 && !ld->NewCountryName->text().isEmpty() &&
                                 checkLongLat());

    if( !addCityEnabled() ) {
        if( ld->NewCityName->text().isEmpty() ) {
            ld->errorLabel->setText( i18n( "Cannot add new location -- city name blank" ) );
        }
        else if( ld->NewCountryName->text().isEmpty() ) {
            ld->errorLabel->setText( i18n( "Cannot add new location -- country name blank" ) );
        }
        else if( !checkLongLat() ) {
            ld->errorLabel->setText( i18n( "Cannot add new location -- invalid latitude / longitude" ) );
        }
        else {
            ld->errorLabel->setText( i18n( "Cannot add new location -- please check all fields" ) );
        }
    }
    else {
        ld->errorLabel->setText( QString() );
    }

}

void LocationDialog::slotOk() {
    if( addCityEnabled())
    {
        if (addCity() )
         accept();
    }
    else
        accept();
}

bool LocationDialog::addCityEnabled() { return ld->AddCityButton->isEnabled(); }

