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


#include <klocale.h>
#include <kmessagebox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qlistbox.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qgroupbox.h>
#include <qvariant.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include "kstars.h"
#include <stdio.h>

#include "locationdialog.h"

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
	glay = new QGridLayout( 2, 2, 6 ); //this layout will be added to vlay

	CoordLay = new QVBoxLayout( CoordBox, 6, 4 ); //root mgr for coordbox
	CoordLay->setSpacing( 6 );
	CoordLay->setMargin( 6 );
	glay2 = new QGridLayout( 2, 4, 4 ); //this layout will be added to vlay2
	hlay2 = new QHBoxLayout( 2 ); //this layout will be added to vlay2

//Create widgets
  TextLabel1_2 = new QLabel( CityBox, "TextLabel1_2" );
  TextLabel1_2->setText( i18n( "State filter"  ) );

  QFont std_font(  TextLabel1_2->font() );
//  std_font.setFamily( "helvetica" );
//  std_font.setPointSize( 12 );
//  std_font.setBold( TRUE );
  TextLabel1_2->setFont( std_font );

  TextLabel1 = new QLabel( CityBox, "TextLabel1" );
  TextLabel1->setText( i18n( "City filter"  ) );
  TextLabel1->setFont( std_font );

  TextLabel2 = new QLabel( CityBox, "TextLabel2" );
//  TextLabel2->setText( i18n( "30000 cities match search criteria"  ) );		// TK: I think it's not needed
  TextLabel2->setFont( std_font );

  CityFilter = new QLineEdit( CityBox, "CityFilter" );
  CityFilter->setFocus();		// set focus to city inputline
  StateFilter = new QLineEdit( CityBox, "StateFilter" );

  GeoBox = new QListBox( CityBox, "GeoBox" );
  GeoBox->setVScrollBarMode( QListBox::AlwaysOn );
  GeoBox->setHScrollBarMode( QListBox::AlwaysOff );

	MapView = new MapCanvas( CityBox, "MapView" );
	MapView->setFixedSize( 360, 180 ); //each pixel 1 deg x 2 deg

	NewCityLabel = new QLabel( CoordBox, "NewCityLabel" );
	NewStateLabel = new QLabel( CoordBox, "NewStateLabel" );
	LongLabel = new QLabel( CoordBox, "LongLabel" );
	LatLabel = new QLabel( CoordBox, "LatLabel" );
	NewCityName = new QLineEdit( CoordBox, "NewCityName" );
	NewStateName = new QLineEdit( CoordBox, "NewCityName" );
	NewLong = new QLineEdit( CoordBox, "NewLong" );
	NewLat = new QLineEdit( CoordBox, "NewLat" );
	AddCityButton = new QPushButton( i18n ( "Add to List" ), CoordBox, "AddCityButton" );
	AddCityButton->setEnabled( false );
		
	NewCityLabel->setText( i18n( "City name:" ) );
	NewStateLabel->setText( i18n( "State name:" ) );
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
	vlay->addWidget( TextLabel2, 0, 0 );

	glay->addWidget( TextLabel1, 0, 0 );
	glay->addWidget( TextLabel1_2, 1, 0 );
	glay->addWidget( CityFilter, 0, 1 );
	glay->addWidget( StateFilter, 1, 1 );

	hlay->activate();
	
	CoordLay->addSpacing( 14 );
	CoordLay->addLayout( glay2, 0 );
	CoordLay->addLayout( hlay2, 0 );
	
	glay2->addWidget( NewCityLabel, 0, 0 );
	glay2->addWidget( LongLabel, 1, 0 );
	glay2->addWidget( NewCityName, 0, 1 );
	glay2->addWidget( NewLong, 1, 1 );
	glay2->addWidget( NewStateLabel, 0, 2 );
	glay2->addWidget( LatLabel, 1, 2 );
	glay2->addWidget( NewStateName, 0, 3 );
	glay2->addWidget( NewLat, 1, 3 );

	hlay2->addStretch();
	hlay2->addWidget( AddCityButton, 0 );
	
	CoordLay->activate();
	RootLay->activate();
	
  GeoID.resize(10000);

  connect( this, SIGNAL( okClicked() ), this, SLOT( accept() ) ) ;
  connect( this, SIGNAL( cancelClicked() ), this, SLOT( reject() ) );
  connect( CityFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( filterCity() ) );
  connect( StateFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( filterCity() ) );
	connect( GeoBox, SIGNAL( selectionChanged() ), this, SLOT( changeCity() ) );
	connect( AddCityButton, SIGNAL( clicked() ), this, SLOT( addCity() ) );
	
	initCityList();
	resize (640, 480);
}

LocationDialog::~LocationDialog(){
}

bool LocationDialog::event( QEvent* ev )
{
  bool ret = QDialog::event( ev );
  if ( ev->type() == QEvent::ApplicationFontChange ) {
    QFont TextLabel1_2_font(  TextLabel1_2->font() );
    TextLabel1_2_font.setFamily( "helvetica" );
    TextLabel1_2_font.setPointSize( 12 );
    TextLabel1_2_font.setBold( TRUE );
    TextLabel1_2->setFont( TextLabel1_2_font );
    QFont TextLabel1_font(  TextLabel1->font() );
    TextLabel1_font.setFamily( "helvetica" );
    TextLabel1_font.setPointSize( 12 );
    TextLabel1_font.setBold( TRUE );
    TextLabel1->setFont( TextLabel1_font );
    QFont TextLabel2_font(  TextLabel2->font() );
    TextLabel2_font.setFamily( "helvetica" );
    TextLabel2_font.setPointSize( 14 );
    TextLabel2_font.setBold( TRUE );
    TextLabel2->setFont( TextLabel2_font );
  }
  return ret;
}

void LocationDialog::initCityList( void ) {
	KStars *p = (KStars *)parent();
	for (GeoLocation *data = p->GetData()->geoList.first(); data; data = p->GetData()->geoList.next())
	{
		QString s ( i18n( data->name() ) );
    	s.append( "  " );
   		s.append( i18n( data->state() ) );
   		GeoBox->insertItem( s );
    	GeoID[GeoBox->count() - 1] = p->GetData()->geoList.at();
	}

	QString countLabel;
	countLabel.sprintf( i18n( "%d cities match search criteria" ), GeoBox->count() );
	TextLabel2->setText( countLabel );
	
	if ( GeoBox->firstItem() )		// set first item in list as selected
		GeoBox->setCurrentItem( GeoBox->firstItem() );
}

void LocationDialog::filterCity( void ) {
	KStars *p = (KStars *)parent();
	GeoBox->clear();
	
	for ( unsigned int i=0; i< p->GetData()->geoList.count(); ++i ) {
		QString sc( i18n( p->GetData()->geoList.at(i)->name() ) );
		QString ss( i18n( p->GetData()->geoList.at(i)->state() ) );

		if ( sc.lower().startsWith( CityFilter->text().lower() ) && ss.lower().startsWith( StateFilter->text().lower() ) ) {
			sc.append( "  " );
			sc.append( ss );
			GeoBox->insertItem( sc );
			GeoID[GeoBox->count() - 1] = i;
		}
	}

	QString countLabel;
	countLabel.sprintf( i18n( "%d cities match search criteria" ), GeoBox->count() );
	TextLabel2->setText( countLabel );

	if ( GeoBox->firstItem() )		// set first item in list as selected
		GeoBox->setCurrentItem( GeoBox->firstItem() );
}

void LocationDialog::changeCity( void ) {
	//when the selected city changes, set newCity, and redraw map
	newCity = GeoID[GeoBox->currentItem()];		
	MapView->repaint();
}

int LocationDialog::getCityIndex( void ) {
	int i = GeoBox->currentItem();
	if (i >= 0) { return GeoID[i]; }
	else { return i; }
}

void LocationDialog::addCity( void ) {
	if ( NewCityName->text().isEmpty() || NewStateName->text().isEmpty() ||
		NewLat->text().isEmpty() || NewLong->text().isEmpty() ) {

		QString message = i18n( "All fields must be filled to add this location." );		
		KMessageBox::sorry( 0, message, i18n( "Fields are empty" ) );
	} else {
		//check for user's city database.  If it doesn't exist, create it.
	}
}
