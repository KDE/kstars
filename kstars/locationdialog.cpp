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

#include <qlabel.h>
#include <qlineedit.h>
#include <qlistbox.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qgroupbox.h>
#include <qvalidator.h>
#include <qvariant.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qfile.h>
#include <qtextstream.h>

#include <klocale.h>
#include <kmessagebox.h>

#include "kstars.h"
#include "locationdialog.h"

#if (KDE_VERSION <= 222)
#include <kstddirs.h>
#else
#include <kstandarddirs.h>
#endif

LocationDialog::LocationDialog( QWidget* parent )
    : KDialogBase( KDialogBase::Plain, i18n( "Set Location" ), Ok|Cancel, Ok, parent ) {

	QFrame *page = plainPage();
	CityBox = new QGroupBox( page, "CityBox" );
	CoordBox = new QGroupBox( page, "CoordBox" );
	CityBox->setTitle( i18n( "Choose City" ) );
	CoordBox->setTitle( i18n( "Choose Coordinates" ) );
	
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
	glay2 = new QGridLayout( 3, 4, 4 ); //this layout will be added to vlay2
	hlay2 = new QHBoxLayout( 2 ); //this layout will be added to vlay2

//Create widgets
  CityFiltLabel = new QLabel( CityBox );
  CityFiltLabel->setText( i18n( "City Filter"  ) );
  ProvinceFiltLabel = new QLabel( CityBox );
  ProvinceFiltLabel->setText( i18n( "Province Filter"  ) );
  CountryFiltLabel = new QLabel( CityBox );
  CountryFiltLabel->setText( i18n( "Country Filter"  ) );

  CountLabel = new QLabel( CityBox );

  CityFilter = new QLineEdit( CityBox );
  CityFilter->setFocus();		// set focus to city inputline
  ProvinceFilter = new QLineEdit( CityBox );
  CountryFilter = new QLineEdit( CityBox );

  GeoBox = new QListBox( CityBox );
  GeoBox->setVScrollBarMode( QListBox::AlwaysOn );
  GeoBox->setHScrollBarMode( QListBox::AlwaysOff );

	MapView = new MapCanvas( CityBox );
	MapView->setFixedSize( 360, 180 ); //each pixel 1 deg x 2 deg

	NewCityLabel = new QLabel( CoordBox );
	NewProvinceLabel = new QLabel( CoordBox );
	NewCountryLabel = new QLabel( CoordBox );
	LongLabel = new QLabel( CoordBox );
	LatLabel = new QLabel( CoordBox );

	NewCityName = new QLineEdit( CoordBox );
	NewProvinceName = new QLineEdit( CoordBox );
	NewCountryName = new QLineEdit( CoordBox );
	NewLong = new QLineEdit( CoordBox );
	NewLat = new QLineEdit( CoordBox );
	AddCityButton = new QPushButton( i18n ( "Add to List" ), CoordBox, "AddCityButton" );
	AddCityButton->setEnabled( true );
	
	//blank widget needed for empty place in grid layout
	Empty = new QWidget( CoordBox );
		
	NewCityLabel->setText( i18n( "City:" ) );
	NewProvinceLabel->setText( i18n( "State/Province:" ) );
	NewCountryLabel->setText( i18n( "Country:" ) );
	LongLabel->setText( i18n( "Longitude:" ) );
	LatLabel->setText( i18n( "Latitude:" ) );
	
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
//	CoordLay->addLayout( hlay2, 0 );
	
	glay2->addWidget( NewCityLabel, 0, 0 );
	glay2->addWidget( NewProvinceLabel, 1, 0 );
	glay2->addWidget( NewCountryLabel, 2, 0 );
	glay2->addWidget( NewCityName, 0, 1 );
	glay2->addWidget( NewProvinceName, 1, 1 );
	glay2->addWidget( NewCountryName, 2, 1 );
	glay2->addWidget( LongLabel, 0, 2 );
	glay2->addWidget( LatLabel, 1, 2 );
	glay2->addWidget( Empty, 2, 2 );
	glay2->addWidget( NewLong, 0, 3 );
	glay2->addWidget( NewLat, 1, 3 );
	glay2->addLayout( hlay2, 2, 3 );

	hlay2->addStretch();
	hlay2->addWidget( AddCityButton, 0 );
	
	CoordLay->activate();
	RootLay->activate();
	
	NewLong->setValidator( new QDoubleValidator( -180.0, 180.0, 4, NewLong ) );
	NewLat->setValidator( new QDoubleValidator( -90.0, 90.0, 4, NewLat ) );

  GeoID.resize(10000);

  connect( this, SIGNAL( okClicked() ), this, SLOT( accept() ) ) ;
  connect( this, SIGNAL( cancelClicked() ), this, SLOT( reject() ) );
  connect( CityFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( filterCity() ) );
  connect( ProvinceFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( filterCity() ) );
  connect( CountryFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( filterCity() ) );
  connect( NewLong, SIGNAL( textChanged( const QString & ) ), this, SLOT( checkLong() ) );
  connect( NewLat, SIGNAL( textChanged( const QString & ) ), this, SLOT( checkLat() ) );
	connect( GeoBox, SIGNAL( selectionChanged() ), this, SLOT( changeCity() ) );
	connect( AddCityButton, SIGNAL( clicked() ), this, SLOT( addCity() ) );
	
	initCityList();
	resize (640, 480);
}

LocationDialog::~LocationDialog(){
}

void LocationDialog::initCityList( void ) {
	KStars *p = (KStars *)parent();
	for (GeoLocation *loc = p->data()->geoList.first(); loc; loc = p->data()->geoList.next())
	{
		QString s;
		if ( loc->province().isEmpty() ) {
			s = loc->translatedName() + ", " + loc->translatedCountry();
		} else {
			s = loc->translatedName() + ", " + loc->translatedProvince() + ", " + loc->translatedCountry();
		}
		GeoBox->insertItem( s );
		GeoID[GeoBox->count() - 1] = p->data()->geoList.at();
	}
	QString scount = i18n( "%1 cities match search criteria" ).arg( (int)GeoBox->count());
	CountLabel->setText( scount );
	
	if ( GeoBox->firstItem() )		// set first item in list as selected
		GeoBox->setCurrentItem( GeoBox->firstItem() );
}

void LocationDialog::filterCity( void ) {
	KStars *p = (KStars *)parent();
	GeoBox->clear();
	
	for (GeoLocation *loc = p->data()->geoList.first(); loc; loc = p->data()->geoList.next()) {
		QString sc( loc->translatedName() );
		QString ss( loc->translatedCountry() );
		QString sp = "";
		if ( !loc->province().isEmpty() )
			sp = loc->translatedProvince();

		if ( sc.lower().startsWith( CityFilter->text().lower() ) &&
				sp.lower().startsWith( ProvinceFilter->text().lower() ) &&
				ss.lower().startsWith( CountryFilter->text().lower() ) ) {
			sc.append( ", " );
			if ( !sp.isEmpty() ) {
				sc.append( sp );
				sc.append( ", " );
			}
			sc.append( ss );

			GeoBox->insertItem( sc );
			GeoID[GeoBox->count() - 1] = p->data()->geoList.at();
		}
	}

	QString scount = i18n( "%1 cities match search criteria" ).arg(GeoBox->count());
	CountLabel->setText( scount );

	if ( GeoBox->firstItem() )		// set first item in list as selected
		GeoBox->setCurrentItem( GeoBox->firstItem() );

	MapView->repaint();
}

void LocationDialog::changeCity( void ) {
	//when the selected city changes, set newCity, and redraw map
	newCity = GeoID[GeoBox->currentItem()];
	MapView->repaint();

//	KStars *p = (KStars *)parent();
//	kdWarning() << "TimeZoneRule: " << p->data()->geoList.at(newCity)->tzrule() << endl;
}

int LocationDialog::getCityIndex( void ) {
	int i = GeoBox->currentItem();
	if (i >= 0) { return GeoID[i]; }
	else { return i; }
}

void LocationDialog::addCity( void ) {
	KStars *p = (KStars *)parent();

	if ( NewCityName->text().isEmpty() || NewCountryName->text().isEmpty() ||
		NewLat->text().isEmpty() || NewLong->text().isEmpty() ) {

		QString message = i18n( "All fields (except Province) must be filled to add this location." );
		KMessageBox::sorry( 0, message, i18n( "Fields are Empty" ) );
	} else {
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
			KMessageBox::sorry( 0, message, i18n( "Could not Open File" ) );
			return;
		} else {
			dms lat = dms( NewLat->text().toDouble() );
			dms lng = dms( NewLong->text().toDouble() );
			char ltsgn = 'N'; if ( lat.degree()<0 ) ltsgn = 'S';
			char lgsgn = 'E'; if ( lng.degree()<0 ) lgsgn = 'W';

			entry = entry.sprintf( "%-32s : %-21s : %-21s : %2d : %2d : %2d : %c : %3d : %2d : %2d : %c : x\n",
						name.local8Bit().data(), province.local8Bit().data(), country.local8Bit().data(),
						abs(lat.degree()), lat.getArcMin(), lat.getArcSec(), ltsgn,
						abs(lng.degree()), lng.getArcMin(), lat.getArcSec(), lgsgn );

			QTextStream stream( &file );
			stream << entry;
			file.close();

			//Add city to geoList and GeoBox
			//insert the new city into geoList alphabetically by city name
			unsigned int i;
			for ( i=0; i < p->data()->geoList.count(); ++i ) {
				if ( p->data()->geoList.at(i)->name().lower() > NewCityName->text().lower() ) {
					double TZ = double(int(lng.Degrees()/15.0)); //estimate time zone
					p->data()->geoList.insert( i, new GeoLocation( lng.Degrees(), lat.Degrees(), NewCityName->text(), NewProvinceName->text(), NewCountryName->text(), TZ ) );
					break;
				}
      }

			GeoBox->clear();
			initCityList();
			GeoBox->setCurrentItem( i );
		}
	}

	NewLong->clear();
	NewLat->clear();
	NewCityName->clear();
	NewProvinceName->clear();
	NewCountryName->clear();
}

void LocationDialog::findCitiesNear( int lng, int lat ) {
	KStars *ks = (KStars *)parent();

	//find all cities within 3 degrees of (lng, lat); list them in GeoBox
	GeoBox->clear();
	for ( unsigned int i=0; i<ks->data()->geoList.count(); ++i ) {
		if ( ( abs(	lng - int( ks->data()->geoList.at(i)->lng().Degrees() ) ) < 3 ) &&
				 ( abs( lat - int( ks->data()->geoList.at(i)->lat().Degrees() ) ) < 3 ) ) {
	    QString sc( ks->data()->geoList.at(i)->translatedName() );
			sc.append( ", " );
			if ( !ks->data()->geoList.at(i)->province().isEmpty() ) {
	      sc.append( ks->data()->geoList.at(i)->translatedProvince() );
	      sc.append( ", " );
			}
      sc.append( ks->data()->geoList.at(i)->translatedCountry() );

      GeoBox->insertItem( sc );
      GeoID[GeoBox->count() - 1] = i;
		}
	}

	QString scount = i18n( "%1 cities match search criteria" ).arg(GeoBox->count());
	CountLabel->setText( scount );

	if ( GeoBox->firstItem() )		// set first item in list as selected
		GeoBox->setCurrentItem( GeoBox->firstItem() );

	repaint();
}

void LocationDialog::checkLong( void ) {
	double lng = NewLong->text().toDouble();
	if ( lng < -180.0 || lng > 180.0 ) {
		QString message = i18n( "The longitude must be expressed as \na floating-point number between -180.0 and 180.0" );
		KMessageBox::sorry( 0, message, i18n( "Invalid Longitude" ) );
		NewLong->clear();
	}
}

void LocationDialog::checkLat( void ) {
	double lat = NewLat->text().toDouble();
	if ( lat < -90.0 || lat > 90.0 ) {
		QString message = i18n( "The latitude must be expressed as \na floating-point number between -90.0 and 90.0" );
		KMessageBox::sorry( 0, message, i18n( "Invalid Latitude" ) );
		NewLat->clear();
	}
}
#include "locationdialog.moc"
