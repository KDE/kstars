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

#include <QTimer>

#include <kmessagebox.h>

#include "finddialog.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skyobject.h"

FindDialogUI::FindDialogUI( QWidget *parent ) : QFrame( parent ) {
	setupUi( this );

	FilterType->insertItem( i18n ("Any") );
	FilterType->insertItem( i18n ("Stars") );
	FilterType->insertItem( i18n ("Solar System") );
	FilterType->insertItem( i18n ("Open Clusters") );
	FilterType->insertItem( i18n ("Glob. Clusters") );
	FilterType->insertItem( i18n ("Gas. Nebulae") );
	FilterType->insertItem( i18n ("Plan. Nebulae") );
	FilterType->insertItem( i18n ("Galaxies") );
	FilterType->insertItem( i18n ("Comets") );
	FilterType->insertItem( i18n ("Asteroids") );
	FilterType->insertItem( i18n ("Constellations") );

	SearchList->setMinimumWidth( 256 );
	SearchList->setMinimumHeight( 320 );
}

FindDialog::FindDialog( QWidget* parent ) :
		KDialog( parent, i18n( "Find Object" ), Ok|Cancel ),
		currentitem(0), Filter(0)
{
	ui = new FindDialogUI( this );
	setMainWidget( ui );

//Connect signals to slots
//	connect( this, SIGNAL( okClicked() ), this, SLOT( accept() ) ) ;
	connect( this, SIGNAL( cancelClicked() ), this, SLOT( reject() ) );
	connect( ui->SearchBox, SIGNAL( textChanged( const QString & ) ), SLOT( filter() ) );
	connect( ui->SearchBox, SIGNAL( returnPressed() ), SLOT( slotOk() ) );
	connect( ui->FilterType, SIGNAL( activated( int ) ), this, SLOT( setFilter( int ) ) );
	connect( ui->SearchList, SIGNAL (itemSelectionChanged()), SLOT (updateSelection()));
	connect( ui->SearchList, SIGNAL( itemDoubleClicked ( QListWidgetItem *  ) ), SLOT( slotOk() ) );

	// first create and paint dialog and then load list
	QTimer::singleShot(0, this, SLOT( init() ));
}

FindDialog::~FindDialog() {
}

void FindDialog::init() {
	ui->SearchBox->clear();
	ui->FilterType->setCurrentItem(0);  // show all types of objects
	filter();
}

void FindDialog::filter() {  //Filter the list of names with the string in the SearchBox
	KStars *p = (KStars *)parent();

	ui->SearchList->clear();
	QStringList ObjNames;

	QString searchString = ui->SearchBox->text();
	if ( searchString.isEmpty() ) {
		ObjNames = p->data()->skyComposite()->objectNames();
	} else {
		QRegExp rx("^"+searchString);
		rx.setCaseSensitivity( Qt::CaseInsensitive );
		ObjNames = p->data()->skyComposite()->objectNames().filter(rx);
	}

	if ( ObjNames.size() ) {
		if ( ObjNames.size() > 5000) {
			int index=0;
			while ( index+1000 < ObjNames.size() ) {
				ui->SearchList->addItems( ObjNames.mid( index, 1000 ) );
				index += 1000;
				kapp->processEvents();
			}
		} else 
			ui->SearchList->addItems( ObjNames );

		selectFirstItem(); // Automatically highlight first item
	}

	ui->SearchBox->setFocus();  // set cursor to QLineEdit
}

void FindDialog::filterByType() {
	KStars *p = (KStars *)parent();

	ui->SearchList->clear();	// QListBox
	QString searchFor = ui->SearchBox->text().lower();  // search string

	QStringList ObjNames;

	foreach ( QString name, p->data()->skyComposite()->objectNames() ) {
		//FIXME: We need pointers to the objects to filter by type
	}

	selectFirstItem();    // Automatically highlight first item
	ui->SearchBox->setFocus();  // set cursor to QLineEdit
}

void FindDialog::selectFirstItem() {
	ui->SearchList->setItemSelected( ui->SearchList->item(0), true );
}

void FindDialog::updateSelection() {
	KStars *p = (KStars *)parent();
	if ( ui->SearchList->selectedItems().size() ) {
		QString objName = ui->SearchList->selectedItems()[0]->text();
		currentitem = p->data()->skyComposite()->findByName( objName );
		ui->SearchBox->setFocus();  // set cursor to QLineEdit
	}
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
			disconnect( ui->SearchBox, SIGNAL( textChanged( const QString & ) ), this, SLOT( filterByType() ) );
			connect( ui->SearchBox, SIGNAL( textChanged( const QString & ) ), SLOT( filter() ) );
			filter();
		}
		else {
		// delete old connections and create new connections
			disconnect( ui->SearchBox, SIGNAL( textChanged( const QString & ) ), this, SLOT( filter() ) );
			connect( ui->SearchBox, SIGNAL( textChanged( const QString & ) ), SLOT( filterByType() ) );
			filterByType();
		}
	}
}

void FindDialog::slotOk() {
	//If no valid object selected, show a sorry-box.  Otherwise, emit accept()
	if ( currentItem() == 0 ) {
		QString message = i18n( "No object named %1 found." ).arg( ui->SearchBox->text() );
		KMessageBox::sorry( 0, message, i18n( "Bad object name" ) );
	} else {
		accept();
	}
}

void FindDialog::keyPressEvent( QKeyEvent *e ) {
	switch( e->key() ) {
		case Qt::Key_Down :
			if ( ui->SearchList->currentRow() < ((int) ui->SearchList->count()) - 1 )
				ui->SearchList->setCurrentRow( ui->SearchList->currentRow() + 1 );
			break;
			
		case Qt::Key_Up :
			if ( ui->SearchList->currentRow() )
				ui->SearchList->setCurrentRow( ui->SearchList->currentRow() - 1 );
			break;
			
		case Qt::Key_Escape :
			reject();
			break;
			
	}
}

#include "finddialog.moc"
