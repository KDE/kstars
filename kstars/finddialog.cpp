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

#include <qlayout.h>
#include <qlineedit.h>
#include <qlabel.h>
#include <qcombobox.h>
#include <qlistbox.h>
#include <qtimer.h>

#include <kmessagebox.h>

#include "finddialog.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skyobject.h"
#include "skyobjectname.h"
#include "objectnamelist.h"


FindDialog::FindDialog( QWidget* parent ) :
		KDialogBase( KDialogBase::Plain, i18n( "Find Object" ), Ok|Cancel, Ok, parent ),
		vlay(0), hlay(0), SearchList(0), SearchBox(0), filterTypeLabel(0), filterType(0),
		currentitem(0)
{
	QFrame *page = plainPage();

//Create Layout managers
	vlay = new QVBoxLayout( page, 2, 2 );
	hlay = new QHBoxLayout( 2 ); //this mgr will be added to vlay

//Create Widgets
	SearchBox = new QLineEdit( page, "SearchBox" );

	filterTypeLabel = new QLabel( page, "filterTypeLabel" );
	filterTypeLabel->setAlignment( AlignRight );
	filterTypeLabel->setText( i18n( "Filter by type: " ) );

	filterType = new QComboBox( page, "filterType" );
	filterType->setEditable( false );
	filterType->insertItem( i18n ("Any") );
	filterType->insertItem( i18n ("Stars") );
	//	filterType->insertItem( i18n ("Double Stars") );
	filterType->insertItem( i18n ("Solar System") );
	filterType->insertItem( i18n ("Open Clusters") );
	filterType->insertItem( i18n ("Glob. Clusters") );
	filterType->insertItem( i18n ("Gas. Nebulae") );
	filterType->insertItem( i18n ("Plan. Nebulae") );
	//	filterType->insertItem( i18n ("SN Remnants") );
	filterType->insertItem( i18n ("Galaxies") );
	filterType->insertItem( i18n ("Comets") );
	filterType->insertItem( i18n ("Asteroids") );
	filterType->insertItem( i18n ("Constellations") );

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

// no item currently set
	currentitem = 0;

// no filters set
	Filter = 0;

//Connect signals to slots
//	connect( this, SIGNAL( okClicked() ), this, SLOT( accept() ) ) ;
	connect( this, SIGNAL( cancelClicked() ), this, SLOT( reject() ) );
	connect( SearchBox, SIGNAL( textChanged( const QString & ) ), SLOT( filter() ) );
	connect( SearchBox, SIGNAL( returnPressed() ), SLOT( slotOk() ) );
	connect( filterType, SIGNAL( activated( int ) ), this, SLOT( setFilter( int ) ) );
	connect( SearchList, SIGNAL (selectionChanged  (QListBoxItem *)), SLOT (updateSelection (QListBoxItem *)));
	connect( SearchList, SIGNAL( doubleClicked ( QListBoxItem *  ) ), SLOT( slotOk() ) );

	// first create and paint dialog and then load list
	QTimer::singleShot(0, this, SLOT( init() ));
}

FindDialog::~FindDialog() {
	delete SearchList;
}

void FindDialog::init() {
	SearchBox->clear();  // QLineEdit
	filterType->setCurrentItem(0);  // show all types of objects
	filter();
}

void FindDialog::filter() {  //Filter the list of names with the string in the SearchBox
	KStars *p = (KStars *)parent();

	SearchList->clear();
	ObjectNameList &ObjNames = p->data()->ObjNames;
	// check if latin names are used
	ObjNames.setLanguage( Options::useLatinConstellNames() );

	QString searchFor = SearchBox->text().lower();
		for ( SkyObjectName *name = ObjNames.first( searchFor ); name; name = ObjNames.next() ) {
			if ( name->text().lower().startsWith( searchFor ) ) {
				new SkyObjectNameListItem ( SearchList, name );
/*				if ( i++ >= 5000 ) {              //Every 5000 name insertions,
					kapp->processEvents ( 50 );		//spend 50 msec processing KApplication events
					i = 0;
				}*/
			}
		}
	setListItemEnabled(); // Automatically highlight first item
	SearchBox->setFocus();  // set cursor to QLineEdit
}

void FindDialog::filterByType() {
	KStars *p = (KStars *)parent();

	SearchList->clear();	// QListBox
	QString searchFor = SearchBox->text().lower();  // search string

	ObjectNameList &ObjNames = p->data()->ObjNames;
	// check if latin names are used
	ObjNames.setLanguage( Options::useLatinConstellNames() );

	for ( SkyObjectName *name = ObjNames.first( searchFor ); name; name = ObjNames.next() ) {
	        //Special case: match SkyObject Type 0 with Filter==1 (stars)
		if ( name->skyObject()->type() == Filter || (name->skyObject()->type() == 0 && Filter == 1 ) ) {
			if ( name->text().lower().startsWith( searchFor ) ) {
				// for stars, don't show the ones below the faint limit
				if (Filter!=1 || name->skyObject()->mag() <= Options::magLimitDrawStar() ) {
					new SkyObjectNameListItem ( SearchList, name );
				}
			}
		}
	}

	setListItemEnabled();    // Automatically highlight first item
	SearchBox->setFocus();  // set cursor to QLineEdit
}

void FindDialog::setListItemEnabled() {
	SearchList->setSelected (0, true);
	if (!SearchList->isSelected (0))
		updateSelection (0);
}

void FindDialog::updateSelection (QListBoxItem *it) {
	currentitem = (SkyObjectNameListItem *) it;
	SearchBox->setFocus();  // set cursor to QLineEdit
}

void FindDialog::setFilter( int f ) {
        // Translate the Listbox index to the correct SkyObject Type ID 
        int f2( f ); // in most cases, they are the same number
	if ( f >= 7 ) f2 = f + 1; //need to skip unused "Supernova Remnant" Type at position 7
	
        // check if filter was changed or if filter is still the same
	if ( Filter != f2 ) {
		Filter = f2;
		if ( Filter == 0 ) {  // any type will shown
		// delete old connections and create new connections
			disconnect( SearchBox, SIGNAL( textChanged( const QString & ) ), this, SLOT( filterByType() ) );
			connect( SearchBox, SIGNAL( textChanged( const QString & ) ), SLOT( filter() ) );
			filter();
		}
		else {
		// delete old connections and create new connections
			disconnect( SearchBox, SIGNAL( textChanged( const QString & ) ), this, SLOT( filter() ) );
			connect( SearchBox, SIGNAL( textChanged( const QString & ) ), SLOT( filterByType() ) );
			filterByType();
		}
	}
}

void FindDialog::slotOk() {
	//If no valid object selected, show a sorry-box.  Otherwise, emit accept()
	if ( currentItem() == 0 ) {
		QString message = i18n( "No object named %1 found." ).arg( SearchBox->text() );
		KMessageBox::sorry( 0, message, i18n( "Bad object name" ) );
	} else {
		accept();
	}
}

void FindDialog::keyPressEvent( QKeyEvent *e ) {
	switch( e->key() ) {
		case Key_Down :
			if ( SearchList->currentItem() < ((int) SearchList->count()) - 1 )
				SearchList->setCurrentItem( SearchList->currentItem() + 1 );
			break;
			
		case Key_Up :
			if ( SearchList->currentItem() )
				SearchList->setCurrentItem( SearchList->currentItem() - 1 );
			break;
			
		case Key_Escape :
			reject();
			break;
			
	}
}

#include "finddialog.moc"
