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
		   Close, Close, parent ), ks( _ks )
{
  QFrame *page = plainPage();
//  setMainWidget( page );
  QHBoxLayout *hlay = new QHBoxLayout( page, 0, spacingHint() );
  ui = new ObservingListUI( page );
  hlay->addWidget( ui );
  
  //Connections
  connect( this, SIGNAL( closeClicked() ), this, SLOT( slotClose() ) );
  connect( ui->table, SIGNAL( selectionChanged( QListViewItem* ) ), 
	   this, SLOT( slotNewSelection( QListViewItem* ) ) );
  connect( ui->RemoveButton, SIGNAL( clicked() ), 
	   this, SLOT( slotRemoveObject() ) );
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
    obj->dec()->toHMSString(),
    QString::number( obj->mag() ),
    obj->typeName() );
}

void ObservingList::slotRemoveObject( const SkyObject *obj ) {
  if ( ui->table->childCount() == 0) return;
  
  QString name;
  if ( obj ) name = obj->translatedName();
  else if ( ui->table->selectedItem() ) name = ui->table->selectedItem()->text( 0 );
  else return;  //no remove possible
  
  slotRemoveObject( name );
}

void ObservingList::slotRemoveObject( const QString &name ) {
  //Find this object in obsList and remove it
  bool objectFound = false;
  for ( SkyObject* o = obsList.first(); o; o = obsList.next() ) {
    //DEBUG
    kdDebug() << o->translatedName() << "::" << name << endl;
    
    if ( o->translatedName() == name ) {
      objectFound = true;
      obsList.remove(o);
      break;
    }
  }
  
  if ( ! objectFound ) {
    kdDebug() << i18n( "Can't remove object %1; not found in obsList." ).arg(name) << endl;
  }
  
  //Remove entry from table
  objectFound = false;
  QListViewItemIterator it( ui->table );
  while ( it.current() ) {
    QListViewItem *item = it.current();
    if ( item->text( 0 ) == name ) {
      //Change current item
      if ( item->itemBelow() ) {
        //DEBUG
        kdDebug() << "item Below" << endl;
        ui->table->setSelected( item->itemBelow(), true );
      } else if ( item->itemAbove() ) {
        //DEBUG
        kdDebug() << "item Above" << endl;
        ui->table->setSelected( item->itemAbove(), true );
      } else {
        //DEBUG
        kdDebug() << "setting NULL item in table" << endl;
        ui->table->clearSelection();
      }
      
      //DEBUG
      if ( ui->table->selectedItem() ) {
        kdDebug() << "selectedItem: " << ui->table->selectedItem()->text(0) << endl;
      }
      
      delete item;
      objectFound = true;
      break;
    }
    ++it;
  }

  if ( ! objectFound ) {
    kdDebug() << i18n( "Can't remove Object %1; not found in table." ).arg(name) << endl;
  }
}

void ObservingList::slotNewSelection( QListViewItem *item ) {
  //DEBUG
  kdDebug() << "selected item changed" << endl;
  
  if ( item ) {
    QString name( item->text(0) );
    
    //DEBUG
    kdDebug() << "slotNewSelection(): " << name << endl;
    
    //First, save the last object's user log to disk, if necessary
    if ( ! ui->NotesEdit->text().isEmpty() && ui->NotesEdit->text() !=
	 i18n("Record here observation logs and/or data on %1.").arg( name ) ) {
      obsList.current()->saveUserLog( ui->NotesEdit->text() );
    }
    
    //Find the selected object in the obsList,
    //then break the loop.  Now obsList.current() 
    //points to the new selected object
    bool found( false );
    for ( SkyObject* o = obsList.first(); o; o = obsList.next() ) {
      if ( o->translatedName() == name ) {
	found = true;
	break;
      }
    }
    
    if ( ! found ) { 
      kdDebug() << i18n( "Object %1 not found in obsList." ).arg( name ) << endl;
    } else {
      //Display the object's current user notes in the NotesEdit
      ui->NotesLabel->setText( i18n( "observing notes for %1:" ).arg( name ) );
      if ( obsList.current()->userLog.isEmpty() ) {
        ui->NotesEdit->setText( i18n("Record here observation logs and/or data on %1.").arg( name ) );
      } else {
        ui->NotesEdit->setText( obsList.current()->userLog );
      }
    }
  
    //Enable buttons
    ui->CenterButton->setEnabled( true );
    ui->ScopeButton->setEnabled( true );
    ui->DetailsButton->setEnabled( true );
    ui->AVTButton->setEnabled( true );
    ui->RemoveButton->setEnabled( true );
  } else {
    //Disable buttons
    ui->CenterButton->setEnabled( false );
    ui->ScopeButton->setEnabled( false );
    ui->DetailsButton->setEnabled( false );
    ui->AVTButton->setEnabled( false );
    ui->RemoveButton->setEnabled( false );
  }
}

void ObservingList::slotCenterObject() {
  if ( obsList.current() ) {
    ks->map()->setClickedObject( obsList.current() );
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

//FIXME: This will spawn a new AVT window for each object; 
//should be able to add multiple objects to one AVT.
void ObservingList::slotAVT() {
  if ( obsList.current() ) {
    AltVsTime avt( ks );
    avt.processObject( obsList.current() );
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

#include "observinglist.moc"
