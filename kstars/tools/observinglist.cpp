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
#include <qfile.h>
#include <qdir.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qstringlist.h>
#include <klistview.h>
#include <kpushbutton.h>
#include <kstatusbar.h>
#include <ktextedit.h>
#include <kinputdialog.h>
#include <kicontheme.h>
#include <kiconloader.h>
#include <kio/netaccess.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <ktempfile.h>
#include <klineedit.h>

#include "observinglist.h"
#include "observinglistui.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skyobject.h"
#include "skymap.h"
#include "detaildialog.h"
#include "tools/altvstime.h"

#include "indimenu.h"
#include "indielement.h"
#include "indiproperty.h"
#include "indidevice.h"
#include "devicemanager.h"
#include "indistd.h"

ObservingList::ObservingList( KStars *_ks, QWidget* parent )
		: KDialogBase( KDialogBase::Plain, i18n( "Observing List" ), 
				Close, Close, parent, "observinglist", false ), ks( _ks ), LogObject(0), noNameStars(0), isModified(false)
{
	QFrame *page = plainPage();
	setMainWidget( page );
	QVBoxLayout *vlay = new QVBoxLayout( page, 0, spacingHint() );
	ui = new ObservingListUI( page );
	vlay->addWidget( ui );
  vlay->addSpacing( 12 );

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

	connect( ui->OpenButton, SIGNAL( clicked() ), 
			this, SLOT( slotOpenList() ) );
	connect( ui->SaveButton, SIGNAL( clicked() ),
			this, SLOT( slotSaveList() ) );
	connect( ui->SaveAsButton, SIGNAL( clicked() ),
			this, SLOT( slotSaveListAs() ) );

	obsList.setAutoDelete( false ); //do NOT delete removed pointers!
	
	//Add icons to Push Buttons
	KIconLoader *icons = KGlobal::iconLoader();
	ui->OpenButton->setPixmap( icons->loadIcon( "fileopen", KIcon::Toolbar ) );
	ui->SaveButton->setPixmap( icons->loadIcon( "filesave", KIcon::Toolbar ) );
	ui->SaveAsButton->setPixmap( icons->loadIcon( "filesaveas", KIcon::Toolbar ) );

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
	if ( ! isModified ) isModified = true; 

	//Insert object entry in table
	QString smag("--");
	if ( obj->mag() < 90.0 ) smag = QString::number( obj->mag(), 'g', 2 );
	new KListViewItem( ui->table, obj->translatedName(), 
			obj->ra()->toHMSString(),
			obj->dec()->toDMSString(),
			smag,
			obj->typeName() );

	//Note addition in statusbar
	ks->statusBar()->changeItem( i18n( "Added %1 to observing list." ).arg( obj->name() ), 0 );
}

void ObservingList::slotRemoveObject( SkyObject *o ) {
	if ( !o ) 
		o = ks->map()->clickedObject();
	
	obsList.remove(o);
	if ( ! isModified ) isModified = true;

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
		kdDebug() << i18n( "Cannot remove Object %1; not found in table." ).arg(o->translatedName()) << endl;
	}
}

void ObservingList::slotRemoveObjects() {
	if ( SelectedObjects.count() == 0) return;
	
	for ( SkyObject *o = SelectedObjects.first(); o; o = SelectedObjects.next() ) 
		slotRemoveObject( o );

	slotNewSelection();
}

void ObservingList::slotNewSelection() {
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

void ObservingList::slotSlewToObject()
{

  INDI_D *indidev(NULL);
  INDI_P *eqCoord, *onSet;
  INDI_E *RAEle, *DecEle, *ConnectEle, *nameEle;
  bool useJ2000( false);
  SkyObject *selectedObj;
  SkyPoint sp;
  
  selectedObj = SelectedObjects.first();
  if (selectedObj == NULL)
    return;

  indidev->stdDev->currentObject = NULL;
  // Find the first device with EQUATORIAL_EOD_COORD or EQUATORIAL_COORD and with SLEW element
  // i.e. the first telescope we find!
  
  INDIMenu *imenu = ks->getINDIMenu();
  
  for (unsigned int i=0; i < imenu->mgr.count() ; i++)
  {
    for (unsigned int j=0; j < imenu->mgr.at(i)->indi_dev.count(); j++)
    {
       indidev = imenu->mgr.at(i)->indi_dev.at(j);
       eqCoord = indidev->findProp("EQUATORIAL_EOD_COORD");
       if (eqCoord == NULL)
       {
	 eqCoord = indidev->findProp("EQUATORIAL_COORD");
	 useJ2000 = true;
       }
     
       if (eqCoord == NULL) continue;
       
       ConnectEle = indidev->findElem("CONNECT");
       if (!ConnectEle) continue;
       
       if (ConnectEle->state == PS_OFF)
       {
	 KMessageBox::error(0, i18n("Telescope %1 is offline. Please connect and retry again.").arg(indidev->label));
	 return;
       }

       RAEle = eqCoord->findElement("RA");
       if (!RAEle) continue;
       DecEle = eqCoord->findElement("DEC");
       if (!DecEle) continue;
       
       onSet = indidev->findProp("ON_COORD_SET");
       if (!onSet) continue;
       
       onSet->activateSwitch("SLEW");
       
       sp.set (selectedObj->ra(), selectedObj->dec());
       
       if (useJ2000)
	 sp.apparentCoord(ks->data()->ut().djd(), (long double) J2000);

       // Use JNow coordinate as required by INDI
       RAEle->write_w->setText(QString("%1:%2:%3").arg(sp.ra()->hour()).arg(sp.ra()->minute()).arg(sp.ra()->second()));
       DecEle->write_w->setText(QString("%1:%2:%3").arg(sp.dec()->degree()).arg(sp.dec()->arcmin()).arg(sp.dec()->arcsec()));
       
       eqCoord->newText();

       indidev->stdDev->currentObject = selectedObj;
        if (indidev->stdDev->currentObject)
          {
             nameEle = indidev->findElem("OBJECT_NAME");
             if (nameEle && nameEle->pp->perm != PP_RO)
             {
                 nameEle->write_w->setText(indidev->stdDev->currentObject->name());
                 nameEle->pp->newText();
             }
        }
       
       return;
    }
  }
       
  // We didn't find any telescopes
  KMessageBox::sorry(0, i18n("KStars did not find any active telescopes."));
  
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
	if ( obsList.current() && ! ui->NotesEdit->text().isEmpty() && ui->NotesEdit->text() 
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
		ui->NotesLabel->setText( i18n( "Observing notes for object:" ) );
		LogObject = NULL;
	}
}

void ObservingList::slotOpenList() {
	KURL fileURL = KFileDialog::getOpenURL( QDir::homeDirPath(), "*.obslist|KStars Observing List (*.obslist)" );
	QFile f;

	if ( fileURL.isValid() ) {
		if ( ! fileURL.isLocalFile() ) {
			//Save remote list to a temporary local file
			KTempFile tmpfile;
			tmpfile.setAutoDelete(true);
			FileName = tmpfile.name();
			if( KIO::NetAccess::download( fileURL, FileName, this ) ) 
				f.setName( FileName );

		} else {
			FileName = fileURL.path();
			f.setName( FileName );
		}

		if ( !f.open( IO_ReadOnly) ) {
			QString message = i18n( "Could not open file %1" ).arg( f.name() );
			KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
			return;
		}

		//Before loading a new list, do we need to save the current one?
		//Assume that if the list is empty, then there's no need to save
		if ( obsList.count() ) {
			if ( isModified ) {
				QString message = i18n( "Do you want to save the current list before opening a new list?" );
				if ( KMessageBox::questionYesNo( this, message, 
						i18n( "Save current list?" ) ) == KMessageBox::Yes )
					slotSaveList();
			}

			//If we ever allow merging the loaded list with 
			//the existing one, that code would go here
			obsList.clear();
			ui->table->clear();
		}

		//First line is the name of the list.  The rest of the file should 
		//be object names, one per line.
		QTextStream istream(&f);
		QString line;
		ListName = istream.readLine();

		while ( ! istream.eof() ) {
			line = istream.readLine();
			SkyObject *o = ks->data()->objectNamed( line );
			if ( o ) slotAddObject( o );
		}

		//Newly-opened list should not trigger isModified flag
		isModified = false;

		f.close();

	} else if ( fileURL.path() != "" ) {
		QString message = i18n( "The specified file is invalid.  Try another file?" );
		if ( KMessageBox::warningYesNo( this, message, i18n("Invalid file") ) == KMessageBox::Yes ) {
			slotOpenList();
		}
	}
}

void ObservingList::slotSaveListAs() {
	bool ok(false);
	ListName = KInputDialog::getText( i18n( "Enter list name" ), 
			i18n( "List Name:" ), "", &ok );

	if ( ok ) {
		KURL fileURL = KFileDialog::getSaveURL( QDir::homeDirPath(), "*.obslist|KStars Observing List (*.obslist)" );

		if ( fileURL.isValid() ) 
			FileName = fileURL.path();

		slotSaveList();
	}
}

void ObservingList::slotSaveList() {
	if ( ListName.isEmpty() || FileName.isEmpty() ) {
		slotSaveListAs();
		return;
	}

	QFile f( FileName );
	if ( !f.open( IO_WriteOnly) ) {
		QString message = i18n( "Could not open file %1.  Try a different filename?" ).arg( f.name() );
		
		if ( KMessageBox::warningYesNo( 0, message, i18n( "Could Not Open File" ) ) == KMessageBox::Yes ) {
			FileName == "";
			slotSaveList();
		}
		return;
	}
	
	QTextStream ostream(&f);
	ostream << ListName << endl;
	for ( SkyObject* o = obsList.first(); o; o = obsList.next() ) {
		ostream << o->name() << endl;
	}

	f.close();
	isModified = false;
}

#include "observinglist.moc"
