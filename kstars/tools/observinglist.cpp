/***************************************************************************
                          observinglist.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 29 Nov 2004
    copyright            : (C) 2004 by Jeff Woods, Jason Harris
    email                : jcwoods@bellsouth.net, jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <stdio.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qstringlist.h>
#include <klistview.h>
#include <kpushbutton.h>
#include <ktextedit.h>

#include "observinglist.h"
#include "observinglistui.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skyobject.h"
#include "skymap.h"
#include "detaildialog.h"
#include "tools/altvstime.h"

ObservingList::ObservingList( KStars *_ks, QWidget* parent )
		: KDialogBase( KDialogBase::Plain, i18n( "Observing List" ), 
				Close, Close, parent ), ks( _ks ), LogObject(0), noNameStars(0)
{
	QFrame *page = plainPage();
//	setMainWidget( page );
	QHBoxLayout *hlay = new QHBoxLayout( page, 0, spacingHint() );
	ui = new ObservingListUI( page );
	hlay->addWidget( ui );
  
	//Connections
	connect( this, SIGNAL( closeClicked() ), this, SLOT( slotClose() ) );
	connect( ui->table, SIGNAL( selectionChanged() ), 
			this, SLOT( slotNewSelection() ) );
	connect( ui->RemoveButton, SIGNAL( clicked() ), 
			this, SLOT( slotRemoveObjects() ) );
	connect( ui->CenterButton, SIGNAL( clicked() ), 
			this, SLOT( slotCenterObject() ) );
	connect( ui->ScopeButton, SIGNAL( clicked() ), 
			this, SLOT( slotSlewToObject() ) );
	connect( ui->DetailsButton, SIGNAL( clicked() ), 
			this, SLOT( slotDetails() ) );
	connect( ui->AVTButton, SIGNAL( clicked() ), 
			this, SLOT( slotAVT() ) );

	obsList.setAutoDelete( false ); //do NOT delete removed pointers!
	
	ui->CenterButton->setEnabled( false );
	ui->ScopeButton->setEnabled( false );
	ui->DetailsButton->setEnabled( false );
	ui->AVTButton->setEnabled( false );
	ui->RemoveButton->setEnabled( false );
	ui->NotesLabel->setEnabled( false );
	ui->NotesEdit->setEnabled( false );
}

bool ObservingList::contains( const SkyObject *q ) {
	for ( SkyObject* o = obsList.first(); o; o = obsList.next() ) {
		if ( o == q ) return true;
	}

	return false;
}


//SLOTS
void ObservingList::slotAddObject( SkyObject *obj ) {
	if ( ! obj ) obj = ks->map()->clickedObject();

	//Insert object in obsList
	obsList.append( obj );

	//Insert object entry in table
	new KListViewItem( ui->table, obj->translatedName(), 
			obj->ra()->toHMSString(),
			obj->dec()->toDMSString(),
			QString::number( obj->mag() ),
			obj->typeName() );
}

void ObservingList::slotRemoveObject( SkyObject *o ) {
	if ( !o ) 
		o = ks->map()->clickedObject();
	
	obsList.remove(o);
	if ( o == LogObject ) 
		saveCurrentUserLog();
		
	//Remove entry from table
	bool objectFound = false;
	QListViewItemIterator it( ui->table );
	while ( it.current() ) {
		QListViewItem *item = it.current();
			
			//If the object is named "star" then match coordinates instead of name
		if ( o->translatedName() == i18n( "star" ) ) {
			if ( item->text(1) == o->ra()->toHMSString() 
							&& item->text(2) == o->dec()->toDMSString() ) {
					//DEBUG
				kdDebug() << "removing anonymous star from list." << endl;
				kdDebug() << item->text(1) << "::" << o->ra()->toHMSString() << endl;
					
				delete item;
				objectFound = true;
				break;
							}
		} else if ( item->text( 0 ) == o->translatedName() ) {
				//DEBUG
			kdDebug() << "removing " << o->translatedName() << " from list." << endl;
				
			delete item;
			objectFound = true;
			break;
		}
		++it;
	}

	if ( ! objectFound ) {
		kdDebug() << i18n( "Can't remove Object %1; not found in table." ).arg(o->translatedName()) << endl;
	}
}

void ObservingList::slotRemoveObjects() {
	if ( SelectedObjects.count() == 0) return;
	
	for ( SkyObject *o = SelectedObjects.first(); o; o = SelectedObjects.next() ) {
		//DEBUG
		kdDebug() << o->translatedName() << endl;
		
		slotRemoveObject( o );
	} //end for-loop over SelectedObjects

	slotNewSelection();
}

void ObservingList::slotNewSelection() {
	//DEBUG
	kdDebug() << "selected item changed" << endl;

	//Construct list of selected objects
	SelectedObjects.clear();
	QListViewItemIterator it( ui->table, QListViewItemIterator::Selected ); //loop over selected items
	while ( it.current() ) {
		for ( SkyObject *o = obsList.first(); o; o = obsList.next() ) {
			if ( it.current()->text(0) == i18n("star") ) {
				if ( it.current()->text(1) == o->ra()->toHMSString() 
						&& it.current()->text(2) == o->dec()->toDMSString() ) {
					SelectedObjects.append(o);
					break;
				}
			} else if ( o->translatedName() == it.current()->text(0) ) {
				SelectedObjects.append( o );
				break;
			}
		}
		it++;
	}
	
	//Enable widgets when one object selected
	if ( SelectedObjects.count() == 1 ) {
		QString newName( SelectedObjects.first()->translatedName() );
		QString oldName( obsList.current()->translatedName() );
		
		//Enable buttons
		ui->CenterButton->setEnabled( true );
		ui->ScopeButton->setEnabled( true );
		ui->DetailsButton->setEnabled( true );
		ui->AVTButton->setEnabled( true );
		ui->RemoveButton->setEnabled( true );
		
		//Find the selected object in the obsList,
		//then break the loop.  Now obsList.current()
		//points to the new selected object (until now it was the previous object)
		bool found( false );
		for ( SkyObject* o = obsList.first(); o; o = obsList.next() ) {
			if ( o->translatedName() == newName ) {
				found = true;
				break;
			}
		}

		if ( ! found ) { 
			kdDebug() << i18n( "Object %1 not found in obsList." ).arg( newName ) << endl;
		} else if ( newName != i18n( "star" ) ) {
			//Display the object's current user notes in the NotesEdit
			//First, save the last object's user log to disk, if necessary
			saveCurrentUserLog();
			
			//set LogObject to the new selected object
			LogObject = obsList.current();
			
			ui->NotesLabel->setEnabled( true );
			ui->NotesEdit->setEnabled( true );
			
			ui->NotesLabel->setText( i18n( "observing notes for %1:" ).arg( LogObject->translatedName() ) );
			if ( LogObject->userLog.isEmpty() ) {
				ui->NotesEdit->setText( i18n("Record here observation logs and/or data on %1.").arg( LogObject->translatedName() ) );
			} else {
				ui->NotesEdit->setText( LogObject->userLog );
			}
		} else { //selected object is named "star"
			//clear the log text box
			saveCurrentUserLog();
			ui->NotesLabel->setEnabled( false );
			ui->NotesEdit->setEnabled( false );
		}

	} else if ( SelectedObjects.count() == 0 ) {
		//Disable buttons
		ui->CenterButton->setEnabled( false );
		ui->ScopeButton->setEnabled( false );
		ui->DetailsButton->setEnabled( false );
		ui->AVTButton->setEnabled( false );
		ui->RemoveButton->setEnabled( false );
		ui->NotesLabel->setEnabled( false );
		ui->NotesEdit->setEnabled( false );
		
		//Clear the user log text box.
		saveCurrentUserLog();
	} else { //more than one object selected.
		ui->CenterButton->setEnabled( false );
		ui->ScopeButton->setEnabled( false );
		ui->DetailsButton->setEnabled( false );
		ui->AVTButton->setEnabled( true );
		ui->RemoveButton->setEnabled( true );
		ui->NotesLabel->setEnabled( false );
		ui->NotesEdit->setEnabled( false );
		
		//Clear the user log text box.
		saveCurrentUserLog();
	}
}

void ObservingList::slotCenterObject() {
	if ( obsList.current() ) {
		ks->map()->setClickedObject( obsList.current() );
		ks->map()->setClickedPoint( obsList.current() );
		ks->map()->slotCenter();
	}
}

void ObservingList::slotSlewToObject() {
	//Needs Jasem  :)
}

//FIXME: This will open multiple Detail windows for each object;
//Should have one window whose target object changes with selection
void ObservingList::slotDetails() {
	if ( obsList.current() ) {
		DetailDialog dd( obsList.current(), ks->data()->lt(), ks->geo(), ks );
		dd.exec();
	}
}

void ObservingList::slotAVT() {
	if ( SelectedObjects.count() ) {
		AltVsTime avt( ks );
		for ( SkyObject *o = SelectedObjects.first(); o; o = SelectedObjects.next() ) {
			avt.processObject( o );
		}
		
		avt.exec();
	}
}

//FIXME: On close, we will need to close any open Details/AVT windows
void ObservingList::slotClose() {
	//Save the current User log text
	if ( ! ui->NotesEdit->text().isEmpty() && ui->NotesEdit->text() 
					!= i18n("Record here observation logs and/or data on %1.").arg( obsList.current()->name()) ) {
		obsList.current()->saveUserLog( ui->NotesEdit->text() );
	}
	
	hide();
}

void ObservingList::saveCurrentUserLog() {
	if ( ! ui->NotesEdit->text().isEmpty() && 
				ui->NotesEdit->text() != 
					i18n("Record here observation logs and/or data on %1.").arg( LogObject->translatedName() ) ) {
		LogObject->saveUserLog( ui->NotesEdit->text() );
		
		ui->NotesEdit->clear();
		ui->NotesLabel->setText( i18n( "observing notes for object:" ) );
		LogObject = NULL;
	}
}

#include "observinglist.moc"
