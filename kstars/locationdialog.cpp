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

#include <stdlib.h>
#include <stdio.h>

#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <klineedit.h>

#include <qlayout.h>
#include <qpushbutton.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlistbox.h>
#include <qcombobox.h>
#include <qfile.h>

#include "locationdialog.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "mapcanvas.h"
#include "dmsbox.h"

LocationDialog::LocationDialog( QWidget* parent )
    : KDialogBase( KDialogBase::Plain, i18n( "Set Geographic Location" ), Ok|Cancel, Ok, parent ) {

	KStars *p = (KStars *)parent;

	QFrame *page = plainPage();
	CityBox = new QGroupBox( page, "CityBox" );
	CoordBox = new QGroupBox( page, "CoordBox" );
	CityBox->setTitle( i18n( "Choose City" ) );
	CoordBox->setTitle( i18n( "Choose/Modify Coordinates" ) );

//Create Layout managers
	RootLay = new QVBoxLayout( page, 4, 4 ); //root mgr for dialog
	CityLay = new QVBoxLayout( CityBox, 6, 4 ); //root mgr for CityBox
	CityLay->setSpacing( 6 );
	CityLay->setMargin( 6 );
	hlay = new QHBoxLayout( 2 ); //this layout will be added to CityLay
	vlay = new QVBoxLayout( 2 ); //this layout will be added to hlay
	glay = new QGridLayout( 3, 2, 6 ); //this layout will be added to vlay

	CoordLay = new QVBoxLayout( CoordBox, 6, 4 ); //root mgr for coordbox
	CoordLay->setSpacing( 6 );
	CoordLay->setMargin( 6 );
	glay2 = new QGridLayout( 3, 4, 4 ); //this layout will be added to CoordLay
	hlayCoord   = new QHBoxLayout( 2 ); //this layout will be added to glay2
	hlayTZ      = new QHBoxLayout( 2 ); //this layout will be added to glay2
	hlayButtons = new QHBoxLayout( 2 ); //this layout will be added to glay2
	hlay3       = new QHBoxLayout( 2 ); //this layout will be added to CoordLay

//Create widgets
  CityFiltLabel = new QLabel( CityBox );
  CityFiltLabel->setText( i18n( "City filter:"  ) );
  ProvinceFiltLabel = new QLabel( CityBox );
  ProvinceFiltLabel->setText( i18n( "Province filter:"  ) );
  CountryFiltLabel = new QLabel( CityBox );
  CountryFiltLabel->setText( i18n( "Country filter:"  ) );

  CountLabel = new QLabel( CityBox );

  CityFilter = new KLineEdit( CityBox );
  CityFilter->setFocus();  // set focus to city inputline
  ProvinceFilter = new KLineEdit( CityBox );
  CountryFilter = new KLineEdit( CityBox );

  GeoBox = new QListBox( CityBox );
  GeoBox->setVScrollBarMode( QListBox::AlwaysOn );
  GeoBox->setHScrollBarMode( QListBox::AlwaysOff );

	MapView = new MapCanvas( CityBox );
	MapView->setFixedSize( 360, 180 ); //each pixel 1 deg x 2 deg

	NewCityLabel = new QLabel( i18n( "City:" ), CoordBox );
	NewProvinceLabel = new QLabel( i18n( "State/Province:" ), CoordBox );
	NewCountryLabel = new QLabel( i18n( "Country:" ), CoordBox );
	LongLabel = new QLabel( i18n( "Longitude:" ), CoordBox );
	LatLabel = new QLabel( i18n( "Latitude:" ), CoordBox );
	TZLabel = new QLabel( i18n( "timezone offset from universal time", "UT offset:" ), CoordBox );
	TZRuleLabel = new QLabel( i18n( "daylight savings time rule", "DST rule:" ), CoordBox );

	NewCityName = new KLineEdit( CoordBox );
	NewProvinceName = new KLineEdit( CoordBox );
	NewCountryName = new KLineEdit( CoordBox );
	NewLong = new dmsBox( CoordBox );
	NewLat = new dmsBox( CoordBox );

	TZBox = new QComboBox( CoordBox );
	TZRuleBox = new QComboBox( CoordBox );
	TZBox->setMinimumWidth( 16 );
	TZRuleBox->setMinimumWidth( 16 );
	TZBox->setEditable( true );
	TZBox->setDuplicatesEnabled( false );

	for ( int i=0; i<25; ++i )
		TZBox->insertItem( KGlobal::locale()->formatNumber( (double)(i-12) ) );

	QMap<QString, TimeZoneRule>::Iterator it = p->data()->Rulebook.begin();
	QMap<QString, TimeZoneRule>::Iterator itEnd = p->data()->Rulebook.end();
	for ( ; it != itEnd; ++it )
		if ( it.key().length() )
			TZRuleBox->insertItem( it.key() );

	ClearFields = new QPushButton( i18n( "Clear Fields" ), CoordBox, "ClearFields" );
	ShowTZRules = new QPushButton( i18n( "Explain DST Rules" ), CoordBox, "ShowDSTRules" );

	AddCityButton = new QPushButton( i18n ( "Add to List" ), CoordBox, "AddCityButton" );

//Pack the widgets into the layouts
	RootLay->addWidget( CityBox, 0, 0 );
	RootLay->addWidget( CoordBox, 0, 0 );

	CityLay->addSpacing( 14 );
	CityLay->addLayout( hlay, 0 );

	hlay->addLayout( vlay, 0 );
	hlay->addSpacing( 12 );
	hlay->addWidget( GeoBox, 0, 0 );

	vlay->addWidget( MapView, 0, 0 );
	vlay->addLayout( glay, 0 );
	vlay->addWidget( CountLabel, 0, 0 );

	glay->addWidget( CityFiltLabel, 0, 0 );
	glay->addWidget( ProvinceFiltLabel, 1, 0 );
	glay->addWidget( CountryFiltLabel, 2, 0 );
	glay->addWidget( CityFilter, 0, 1 );
	glay->addWidget( ProvinceFilter, 1, 1 );
	glay->addWidget( CountryFilter, 2, 1 );

	hlay->activate();

	CoordLay->addSpacing( 14 );
	CoordLay->addLayout( glay2, 0 );
	CoordLay->addLayout( hlay3, 0 );

	glay2->addWidget( NewCityLabel, 0, 0 );
	glay2->addWidget( NewProvinceLabel, 1, 0 );
	glay2->addWidget( NewCountryLabel, 2, 0 );
	glay2->addWidget( NewCityName, 0, 1 );
	glay2->addWidget( NewProvinceName, 1, 1 );
	glay2->addWidget( NewCountryName, 2, 1 );
	glay2->addLayout( hlayCoord, 0, 3 );
	glay2->addLayout( hlayTZ, 1, 3 );
	glay2->addLayout( hlayButtons, 2, 3 );

	hlayCoord->addWidget( LongLabel );
	hlayCoord->addWidget( NewLong );
	hlayCoord->addWidget( LatLabel );
	hlayCoord->addWidget( NewLat );

	hlayTZ->addWidget( TZLabel );
	hlayTZ->addWidget( TZBox );
	hlayTZ->addWidget( TZRuleLabel );
	hlayTZ->addWidget( TZRuleBox );

	hlayButtons->addStretch();
	hlayButtons->addWidget( ClearFields );
	hlayButtons->addWidget( ShowTZRules );

	hlay3->addStretch();
	hlay3->addWidget( AddCityButton, 0 );

	CoordLay->activate();
	RootLay->activate();

	connect( this, SIGNAL( cancelClicked() ), this, SLOT( reject() ) );
	connect( CityFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( filterCity() ) );
	connect( ProvinceFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( filterCity() ) );
	connect( CountryFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( filterCity() ) );
	connect( NewCityName, SIGNAL( textChanged( const QString & ) ), this, SLOT( nameChanged() ) );
	connect( NewProvinceName, SIGNAL( textChanged( const QString & ) ), this, SLOT( nameChanged() ) );
	connect( NewCountryName, SIGNAL( textChanged( const QString & ) ), this, SLOT( nameChanged() ) );
	connect( NewLong, SIGNAL( textChanged( const QString & ) ), this, SLOT( dataChanged() ) );
	connect( NewLat, SIGNAL( textChanged( const QString & ) ), this, SLOT( dataChanged() ) );
	connect( TZBox, SIGNAL( activated(int) ), this, SLOT( dataChanged() ) );
	connect( TZRuleBox, SIGNAL( activated(int) ), this, SLOT( dataChanged() ) );
	connect( GeoBox, SIGNAL( selectionChanged() ), this, SLOT( changeCity() ) );
	connect( AddCityButton, SIGNAL( clicked() ), this, SLOT( addCity() ) );
	connect( ClearFields, SIGNAL( clicked() ), this, SLOT( clearFields() ) );
	connect( ShowTZRules, SIGNAL( clicked() ), this, SLOT( showTZRules() ) );

	dataModified = false;
	nameModified = false;
	AddCityButton->setEnabled( false );

	NewCityName->setTrapReturnKey(true);
	NewProvinceName->setTrapReturnKey(true);
	NewCountryName->setTrapReturnKey(true);
	CityFilter->setTrapReturnKey(true);
	ProvinceFilter->setTrapReturnKey(true);
	CountryFilter->setTrapReturnKey(true);

	filteredCityList.setAutoDelete( false );
	
	initCityList();
	resize (640, 480);
}

LocationDialog::~LocationDialog(){
}

void LocationDialog::initCityList( void ) {
	KStars *p = (KStars *)parent();
	for (GeoLocation *loc = p->data()->geoList.first(); loc; loc = p->data()->geoList.next())
	{
		GeoBox->insertItem( loc->fullName() );
		filteredCityList.append( loc );

		//If TZ is not even integer value, add it to listbox
		if ( loc->TZ0() - int( loc->TZ0() ) && ! TZBox->listBox()->findItem( KGlobal::locale()->formatNumber( loc->TZ0() ) ) ) {
			for ( unsigned int i=0; i<((unsigned int) TZBox->count()); ++i ) {
				if ( TZBox->text( i ).toDouble() > loc->TZ0() ) {
					TZBox->insertItem( KGlobal::locale()->formatNumber( loc->TZ0() ), i-1 );
					break;
				}
			}
		}
	}

	//Sort the list of Cities alphabetically...note that filteredCityList may now have a different ordering!
	GeoBox->sort();
	
	CountLabel->setText( i18n("One city matches search criteria","%n cities match search criteria",GeoBox->count()) );

	// attempt to highlight the current kstars location in the GeoBox
	GeoBox->setCurrentItem( 0 );
	if ( GeoBox->count() ) {
		for ( uint i=0; i<GeoBox->count(); i++ ) {
			if ( GeoBox->item(i)->text() == p->geo()->fullName() ) {
				GeoBox->setCurrentItem( i );
				break;
			}
		}
	}
}

void LocationDialog::filterCity( void ) {
	KStars *p = (KStars *)parent();
	GeoBox->clear();
	filteredCityList.clear();
	
	nameModified = false;
	dataModified = false;
	AddCityButton->setEnabled( false );

	for (GeoLocation *loc = p->data()->geoList.first(); loc; loc = p->data()->geoList.next()) {
		QString sc( loc->translatedName() );
		QString ss( loc->translatedCountry() );
		QString sp = "";
		if ( !loc->province().isEmpty() )
			sp = loc->translatedProvince();

		if ( sc.lower().startsWith( CityFilter->text().lower() ) &&
				sp.lower().startsWith( ProvinceFilter->text().lower() ) &&
				ss.lower().startsWith( CountryFilter->text().lower() ) ) {

			GeoBox->insertItem( loc->fullName() );
			filteredCityList.append( loc );
		}
	}

	GeoBox->sort();
	
	CountLabel->setText( i18n("One city matches search criteria","%n cities match search criteria", GeoBox->count()) );

	if ( GeoBox->firstItem() )		// set first item in list as selected
		GeoBox->setCurrentItem( GeoBox->firstItem() );

	MapView->repaint();
}

void LocationDialog::changeCity( void ) {
	//when the selected city changes, set newCity, and redraw map
	SelectedCity = 0L;
	if ( GeoBox->currentItem() >= 0 ) {
		for (GeoLocation *loc = filteredCityList.first(); loc; loc = filteredCityList.next()) {
			if ( loc->fullName() == GeoBox->currentText() ) {
				SelectedCity = loc;
				break;
			}
		}
	}
	
	MapView->repaint();

	//Fill the fields at the bottom of the window with the selected city's data.
	if ( SelectedCity ) {
		KStars *p = (KStars *)parent();
		NewCityName->setText( SelectedCity->translatedName() );
		NewProvinceName->setText( SelectedCity->translatedProvince() );
		NewCountryName->setText( SelectedCity->translatedCountry() );
		NewLong->showInDegrees( SelectedCity->lng() );
		NewLat->showInDegrees( SelectedCity->lat() );
		TZBox->setCurrentText( KGlobal::locale()->formatNumber( SelectedCity->TZ0() ) );
		
		//Pick the City's rule from the rulebook
		for ( int i=0; i<TZRuleBox->count(); ++i ) {
			if ( p->data()->Rulebook[ TZRuleBox->text(i) ].equals( SelectedCity->tzrule() ) ) {
				TZRuleBox->setCurrentItem( i );
		//DEBUG
		kdDebug() << "tzrule: " << TZRuleBox->text(i) <<":"<<i << endl;

				break;
			}
		}
	}
	
	nameModified = false;
	dataModified = false;
	AddCityButton->setEnabled( false );
}

void LocationDialog::addCity( void ) {
	KStars *p = (KStars *)parent();
	bCityAdded = false;

	if ( !nameModified && !dataModified ) {
		QString message = i18n( "This City already exists in the database." );
		KMessageBox::sorry( 0, message, i18n( "Error: Duplicate Entry" ) );
		return;
	}

	bool latOk(false), lngOk(false), tzOk(false);
	dms lat = NewLat->createDms( true, &latOk );
	dms lng = NewLong->createDms( true, &lngOk );
	double TZ = TZBox->lineEdit()->text().toDouble( &tzOk );

	if ( NewCityName->text().isEmpty() || NewCountryName->text().isEmpty() ) {
		QString message = i18n( "All fields (except province) must be filled to add this location." );
		KMessageBox::sorry( 0, message, i18n( "Fields are Empty" ) );
		return;

	//FIXME after strings freeze lifts, separate TZ check from lat/long check
	} else if ( ! latOk || ! lngOk || ! tzOk ) {
		QString message = i18n( "Could not parse coordinates." );
		KMessageBox::sorry( 0, message, i18n( "Bad Coordinates" ) );
		return;
	} else {
		if ( !nameModified ) {
			QString message = i18n( "Really override original data for this city?" );
			if ( KMessageBox::questionYesNo( 0, message, i18n( "Override Existing Data?" ), i18n("Override Data"), i18n("Do Not Override")) == KMessageBox::No )
				return; //user answered No.
		}

		QString entry;
		QFile file;

		//Strip off white space
		QString name = NewCityName->text().stripWhiteSpace();
		QString province = NewProvinceName->text().stripWhiteSpace();
		QString country = NewCountryName->text().stripWhiteSpace();

		//check for user's city database.  If it doesn't exist, create it.
		file.setName( locateLocal( "appdata", "mycities.dat" ) ); //determine filename in local user KDE directory tree.

		if ( !file.open( IO_ReadWrite | IO_Append ) ) {
			QString message = i18n( "Local cities database could not be opened.\nLocation will not be recorded." );
			KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
			return;
		} else {
			char ltsgn = 'N'; if ( lat.degree()<0 ) ltsgn = 'S';
			char lgsgn = 'E'; if ( lng.degree()<0 ) lgsgn = 'W';
			QString TZrule = TZRuleBox->currentText();

			entry = entry.sprintf( "%-32s : %-21s : %-21s : %2d : %2d : %2d : %c : %3d : %2d : %2d : %c : %5.1f : %2s\n",
						name.local8Bit().data(), province.local8Bit().data(), country.local8Bit().data(),
						abs(lat.degree()), lat.arcmin(), lat.arcsec(), ltsgn,
						abs(lng.degree()), lng.arcmin(), lat.arcsec(), lgsgn,
						TZ, TZrule.local8Bit().data() );

			QTextStream stream( &file );
			stream << entry;
			file.close();

			//Add city to geoList...don't need to insert it alphabetically, since we always sort GeoList
			GeoLocation *g = new GeoLocation( lng.Degrees(), lat.Degrees(),
														NewCityName->text(), NewProvinceName->text(), NewCountryName->text(),
														TZ, &p->data()->Rulebook[ TZrule ] );
			p->data()->geoList.append( g );
			
			//(possibly) insert new city into GeoBox by running filterCity()
			filterCity();
			
			//Attempt to highlight new city in list
			GeoBox->setCurrentItem( 0 );
			if ( GeoBox->count() ) {
				for ( uint i=0; i<GeoBox->count(); i++ ) {
					if ( GeoBox->item(i)->text() == g->fullName() ) {
						GeoBox->setCurrentItem( i );
						break;
					}
				}
			}
		
		}
	}

	bCityAdded = true;
	return;
}

void LocationDialog::findCitiesNear( int lng, int lat ) {
	KStars *ks = (KStars *)parent();

	//find all cities within 3 degrees of (lng, lat); list them in GeoBox
	GeoBox->clear();
	filteredCityList.clear();
	for (GeoLocation *loc = ks->data()->geoList.first(); loc; loc = ks->data()->geoList.next()) {
		if ( ( abs(	lng - int( loc->lng()->Degrees() ) ) < 3 ) &&
				 ( abs( lat - int( loc->lat()->Degrees() ) ) < 3 ) ) {
			
			GeoBox->insertItem( loc->fullName() );
			filteredCityList.append( loc );
		}
	}

	GeoBox->sort();
	CountLabel->setText( i18n("One city matches search criteria","%n cities match search criteria",GeoBox->count()) );

	if ( GeoBox->firstItem() )		// set first item in list as selected
		GeoBox->setCurrentItem( GeoBox->firstItem() );

	repaint();
}

bool LocationDialog::checkLongLat( void ) {
	if ( NewLong->text().isEmpty() || NewLat->text().isEmpty() ) return false;

	bool ok(false);
	double lng = NewLong->createDms(true, &ok).Degrees();
	if ( ! ok ) return false;
	double lat = NewLat->createDms(true, &ok).Degrees();
	if ( ! ok ) return false;

	if ( lng < -180.0 || lng > 180.0 ) return false;
	if ( lat <  -90.0 || lat >  90.0 ) return false;

	return true;
}

void LocationDialog::clearFields( void ) {
	CityFilter->clear();
	ProvinceFilter->clear();
	CountryFilter->clear();
	NewCityName->clear();
	NewProvinceName->clear();
	NewCountryName->clear();
	NewLong->clearFields();
	NewLat->clearFields();
	TZBox->lineEdit()->setText( KGlobal::locale()->formatNumber( 0.0 ) );
	TZRuleBox->setCurrentItem( 0 );
	nameModified = true;
	dataModified = false;
	AddCityButton->setEnabled( false );
	NewCityName->setFocus();
}

void LocationDialog::showTZRules( void ) {
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
	KMessageBox::informationList( 0, message, lines, message );
}

void LocationDialog::nameChanged( void ) {
	nameModified = true;
	dataChanged();
}

//do not enable Add button until all data are present and valid.
void LocationDialog::dataChanged( void ) {
	dataModified = true;
	if ( ! NewCityName->text().isEmpty() && ! NewCountryName->text().isEmpty() && checkLongLat() )
		AddCityButton->setEnabled( true );
	else
		AddCityButton->setEnabled( false );
}

void LocationDialog::slotOk( void ) {
	bool bOkToClose = false;
	if ( addCityEnabled() ) { //user closed the location dialog without adding their new city;
		addCity();                   //call addCity() for them!
		bOkToClose = bCityAdded;
	} else {
		bOkToClose = true;
	}

	if ( bOkToClose ) accept();
}

bool LocationDialog::addCityEnabled() { return AddCityButton->isEnabled(); }

#include "locationdialog.moc"
