/***************************************************************************
                          finddialog.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Jul 4 2001
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
#include <qframe.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qlist.h>
#include <qlistbox.h>
#include <qcombobox.h>
#include <qpushbutton.h>

#include "finddialog.h"
#include "kstars.h"

FindDialog::FindDialog( QWidget* parent )
	: KDialogBase( KDialogBase::Plain, i18n( "Find Object" ), Ok|Cancel, Ok, parent ) {
	
	QFrame *page = plainPage();

//Create Layout managers
	vlay = new QVBoxLayout( page, 2, 2 );
	hlay = new QHBoxLayout( 2 ); //this mgr will be added to vlay

//Create Widgets
	SearchBox = new QLineEdit( page, "SearchBox" );
	SearchBox->setFocus();
	
	filterTypeLabel = new QLabel( page, "filterTypeLabel" );
	filterTypeLabel->setAlignment( AlignRight );
	filterTypeLabel->setText( i18n( "Filter by type: " ) );
	
	filterType = new QComboBox( page, "filterType" );
	filterType->setEditable( false );
	filterType->insertItem( i18n ("Any") );
	filterType->insertItem( i18n ("Constellations") );
	filterType->insertItem( i18n ("Stars") );
	filterType->insertItem( i18n ("Double Stars") );
	filterType->insertItem( i18n ("Planets") );
	filterType->insertItem( i18n ("Open Clusters") );
	filterType->insertItem( i18n ("Glob. Clusters") );
	filterType->insertItem( i18n ("Gas. Nebulae") );
	filterType->insertItem( i18n ("Plan. Nebulae") );
	filterType->insertItem( i18n ("SN Remnants") );
	filterType->insertItem( i18n ("Galaxies") );
	
	SearchList = new QListBox( page, "SearchList" );
	SearchList->setMinimumWidth( 256 );
	SearchList->setMinimumHeight( 320 );
	SearchList->setVScrollBarMode( QListBox::AlwaysOn );
	SearchList->setHScrollBarMode( QListBox::AlwaysOff );

//Pack Widgets into layout manager
	hlay->addWidget( filterTypeLabel, 0, 0 );
	hlay->addWidget( filterType, 0, 0 );
	
	vlay->addWidget( SearchBox, 0, 0 );
	vlay->addSpacing( 12 );
	vlay->addWidget( SearchList, 0, 0 );
	vlay->addLayout( hlay, 0 );
	
	vlay->activate();

	currentitem = 0;
	
	connect( this, SIGNAL( okClicked() ), this, SLOT( accept() ) ) ;
	connect( this, SIGNAL( cancelClicked() ), this, SLOT( reject() ) );
	connect( SearchBox, SIGNAL( textChanged( const QString & ) ), SLOT( filter() ) );
	connect( filterType, SIGNAL( activated( int ) ), this, SLOT( filterByType( int ) ) );
	connect (SearchList, SIGNAL (selectionChanged  (QListBoxItem *)), SLOT (updateSelection (QListBoxItem *)));
	
	
	startTimer (0);		// reads the list after showing of Dialog-Window
}

FindDialog::~FindDialog() {
	delete SearchList;
}

void FindDialog::timerEvent (QTimerEvent *)
{
	killTimers();
	initObjectList();

}

void FindDialog::initObjectList( void ) {
  KStars *p = (KStars *)parent();

	//Initialize filteredList to be the full objList
	filteredList = new QList < SkyObject >;

	//Fill the SearchBox with the full list of strings
	SearchBox->clear();
	
	SearchList->sort (false);		// the Items will be faster inserted
	int i = 0;
	
	for (SkyObjectName *data = p->GetData()->ObjNames->first(); data; data = p->GetData()->ObjNames->next()) {
		new SkyObjectNameListItem ( SearchList, data, p->GetOptions()->useLatinConstellNames );
		if (i++ >= 5000) {
			kapp->processEvents (50);		// check Events of KApplication (on fast Machines not needed)
			i = 0;
		}
	}
	SearchList->sort (true);	// sort alphabeticly
	setListItemEnabled();
}

void FindDialog::filter() {
  KStars *p = (KStars *)parent();
	
	int i=0;
	SearchList->clear();
	filterType->setCurrentItem( 0 );
		
	//Loop through the full objList.  Add those objects which start
	//with the text in the SearchBox to the filteredList.
	//Similarly, add objNames items to SearchList.
	QString searchFor = SearchBox->text().lower();
	if ( p->GetOptions()->useLatinConstellNames ) {	// check if latin names are used
		for ( SkyObjectName *data = p->GetData()->ObjNames->first(); data; data = p->GetData()->ObjNames->next() ) {
			if ( data->text().lower().startsWith( searchFor ) ) {
				new SkyObjectNameListItem ( SearchList, data, true ); // p->GetOptions()->useLatinConstellNames );
				if (i++ >= 5000) {
					kapp->processEvents ( 50 );		// check Events of KApplication (on fast Machines not needed)
					i = 0;
				}
			}
			setListItemEnabled();
		}
	}
	else {	// if localized names are used
		for ( SkyObjectName *data = p->GetData()->ObjNames->first(); data; data = p->GetData()->ObjNames->next() ) {
			if ( data->translatedText().lower().startsWith( searchFor ) ) {
				new SkyObjectNameListItem ( SearchList, data, false ); // p->GetOptions()->useLatinConstellNames );
				if (i++ >= 5000) {
					kapp->processEvents ( 50 );		// check Events of KApplication (on fast Machines not needed)
					i = 0;
				}
			}
		setListItemEnabled();
		}		
	}
}

void FindDialog::filterByType( int iType ) {
	KStars *p = (KStars *)parent();

	int i = 0;	
	SearchList->clear();	// QListBox
	SearchBox->clear();	// QLineEdit

	//Construct filteredList; select by object type
	switch ( iType ) {
		case 0: //Any
			for (SkyObjectName *data = p->GetData()->ObjNames->first(); data; data = p->GetData()->ObjNames->next()) {
				new SkyObjectNameListItem ( SearchList, data, p->GetOptions()->useLatinConstellNames );
				if (i++ >= 5000) {
					kapp->processEvents (50);		// check Events of KApplication (on fast Machines not needed)
					i = 0;
				}
			}
			SearchList->sort (true);
			setListItemEnabled();
			break;
		
		default: //All others, filter according to objType
			for (SkyObjectName *data = p->GetData()->ObjNames->first(); data; data = p->GetData()->ObjNames->next()) {
				if (data->skyObject()->type + 2 == iType) {
					new SkyObjectNameListItem ( SearchList, data, p->GetOptions()->useLatinConstellNames );
				}
				if (i++ >= 5000) {
					kapp->processEvents (50);		// check Events of KApplication (on fast Machines not needed)
					i = 0;
				}
			}
			SearchList->sort (true);
			setListItemEnabled();
			break;
	}
}

void FindDialog::setListItemEnabled()
{
	SearchList->setSelected (0, true);
	if (!SearchList->isSelected (0))
		updateSelection (0);
}

void FindDialog::updateSelection (QListBoxItem *it)
{
	currentitem = (SkyObjectNameListItem *) it;
}
#include "finddialog.moc"
