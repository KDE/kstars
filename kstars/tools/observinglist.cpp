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
#include <qwidgetstack.h>
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
#include "obslistwizard.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skyobject.h"
#include "starobject.h"
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
				Close, Close, parent, "observinglist", false ), ks( _ks ), LogObject(0), oCurrent(0), 
				noNameStars(0), isModified(false), bIsLarge(true)
{
	QFrame *page = plainPage();
	QVBoxLayout *vlay = new QVBoxLayout( page, 0, 0 );
	ui = new ObservingListUI( page );
	vlay->addWidget( ui );

	//FIXME: enable MiniButton when I figure out how to resize the window correctly
	ui->MiniButton->hide();

	//Connections
	connect( this, SIGNAL( closeClicked() ), this, SLOT( slotClose() ) );
	connect( ui->TableStack, SIGNAL( aboutToShow( QWidget* ) ), 
			this, SLOT( slotPrepTable( QWidget* ) ) );
	connect( ui->FullTable, SIGNAL( selectionChanged() ), 
			this, SLOT( slotNewSelection() ) );
	connect( ui->TinyTable, SIGNAL( selectionChanged() ), 
			this, SLOT( slotNewSelection() ) );
	connect( ui->FullTable, SIGNAL( doubleClicked( QListViewItem*, const QPoint&, int) ), 
			this, SLOT( slotCenterObject() ) );
	connect( ui->TinyTable, SIGNAL( doubleClicked( QListBoxItem* ) ), 
			this, SLOT( slotCenterObject() ) );
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
	connect( ui->WizardButton, SIGNAL( clicked() ),
			this, SLOT( slotWizard() ) );

	//FIXME: enable MiniButton
// 	connect( ui->MiniButton, SIGNAL( clicked() ),
// 			this, SLOT( slotToggleSize() ) );

	obsList.setAutoDelete( false ); //do NOT delete removed pointers!
	
	//Add icons to Push Buttons
	KIconLoader *icons = KGlobal::iconLoader();
	ui->OpenButton->setPixmap( icons->loadIcon( "fileopen", KIcon::Toolbar ) );
	ui->SaveButton->setPixmap( icons->loadIcon( "filesave", KIcon::Toolbar ) );
	ui->SaveAsButton->setPixmap( icons->loadIcon( "filesaveas", KIcon::Toolbar ) );
	ui->WizardButton->setPixmap( icons->loadIcon( "wizard", KIcon::Toolbar ) );
	ui->MiniButton->setPixmap( icons->loadIcon( "window_nofullscreen", KIcon::Toolbar ) );

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
void ObservingList::slotPrepTable( QWidget *tab ) {
	if ( tab == ui->FullTable ) {
	} else {
	}
}

void ObservingList::slotAddObject( SkyObject *obj ) {
	if ( ! obj ) obj = ks->map()->clickedObject();

	//First, make sure object is not already in the list
	for ( SkyObject *o = obsList.first(); o; o = obsList.next() ) {
		if ( obj == o ) {
			//FIXME STRINGS FREEZE
			//ks->statusBar()->changeItem( i18n( "%1 is already in the observing list." ).arg( obj->name() ), 0 );
			return;
		}
	}

	//Insert object in obsList
	obsList.append( obj );
	if ( ! isModified ) isModified = true; 

	//Insert object entry in FullTable and TinyTable
	QString smag("--");
	if ( obj->mag() < 90.0 ) smag = QString::number( obj->mag(), 'g', 2 );
	new KListViewItem( ui->FullTable, obj->translatedName(), 
			obj->ra()->toHMSString(),
			obj->dec()->toDMSString(),
			smag,
			obj->typeName() );
	ui->TinyTable->insertItem( obj->translatedName() );

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
		
	//Remove entry from FullTable
	bool objectFound = false;
	QListViewItemIterator it( ui->FullTable );
	while ( it.current() ) {
		QListViewItem *item = it.current();

		//If the object is named "star" then match coordinates instead of name
		if ( o->translatedName() == i18n( "star" ) ) {
			if ( item->text(1) == o->ra()->toHMSString() 
							&& item->text(2) == o->dec()->toDMSString() ) {
				delete item;
				objectFound = true;
				break;
							}
		} else if ( item->text( 0 ) == o->translatedName() ) {
			delete item;
			objectFound = true;
			break;
		}
		++it;
	}

	if ( ! objectFound ) {
		kdDebug() << i18n( "Cannot remove Object %1; not found in table." ).arg(o->translatedName()) << endl;
	} else {
		//Remove object from TinyTable
		for ( uint i=0; i < ui->TinyTable->count(); i++ ) {
			if ( o->translatedName() == ui->TinyTable->text(i) ) {
				ui->TinyTable->removeItem(i);
				break;
			}
		}
	}
}

void ObservingList::slotRemoveObjects() {
	if ( SelectedObjects.count() == 0) return;
	
	for ( SkyObject *o = SelectedObjects.first(); o; o = SelectedObjects.next() ) 
		slotRemoveObject( o );

	slotNewSelection();
}

void ObservingList::slotNewSelection() {
	//If the TinyTable is visible, we need to sync the selection in the FullTable
	if ( sender() == ui->TinyTable ) syncTableSelection();

	//Construct list of selected objects
	SelectedObjects.clear();
	QListViewItemIterator it( ui->FullTable, QListViewItemIterator::Selected ); //loop over selected items
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

		//This shouldn't be necessary.  For some reason, obsList.current() 
		//is valid here, but in subsequent functions (such as slotCenterObject) 
		//called *right after* this one, obsList.current()==NULL.  No idea why.
		oCurrent = obsList.current();

	} else if ( SelectedObjects.count() == 0 ) {
		//Disable buttons
		ui->CenterButton->setEnabled( false );
		ui->ScopeButton->setEnabled( false );
		ui->DetailsButton->setEnabled( false );
		ui->AVTButton->setEnabled( false );
		ui->RemoveButton->setEnabled( false );
		ui->NotesLabel->setEnabled( false );
		ui->NotesEdit->setEnabled( false );
		oCurrent = 0;
		
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
		oCurrent = 0;

		//Clear the user log text box.
		saveCurrentUserLog();
	}

}

void ObservingList::slotCenterObject() {
	if ( oCurrent ) {
		ks->map()->setClickedObject( oCurrent );
		ks->map()->setClickedPoint( oCurrent );
		ks->map()->slotCenter();
	}
}

void ObservingList::slotSlewToObject()
{

  INDI_D *indidev(NULL);
  INDI_P *prop(NULL), *onset(NULL);
  INDI_E *RAEle(NULL), *DecEle(NULL), *AzEle(NULL), *AltEle(NULL), *ConnectEle(NULL), *nameEle(NULL);
  bool useJ2000( false);
  int selectedCoord(0);
  SkyPoint sp;
  
  // Find the first device with EQUATORIAL_EOD_COORD or EQUATORIAL_COORD and with SLEW element
  // i.e. the first telescope we find!
  
  INDIMenu *imenu = ks->getINDIMenu();

  
  for (unsigned int i=0; i < imenu->mgr.count() ; i++)
  {
    for (unsigned int j=0; j < imenu->mgr.at(i)->indi_dev.count(); j++)
    {
       indidev = imenu->mgr.at(i)->indi_dev.at(j);
       indidev->stdDev->currentObject = NULL;
       prop = indidev->findProp("EQUATORIAL_EOD_COORD");
       	if (prop == NULL)
	{
		  prop = indidev->findProp("EQUATORIAL_COORD");
		  if (prop == NULL)
                  {
                    prop = indidev->findProp("HORIZONTAL_COORD");
                    if (prop == NULL)
        		continue;
                    else
                        selectedCoord = 1;		/* Select horizontal */
                  }
		  else
		        useJ2000 = true;
	}

       ConnectEle = indidev->findElem("CONNECT");
       if (!ConnectEle) continue;
       
       if (ConnectEle->state == PS_OFF)
       {
	 KMessageBox::error(0, i18n("Telescope %1 is offline. Please connect and retry again.").arg(indidev->label));
	 return;
       }

        switch (selectedCoord)
        {
          // Equatorial
          case 0:
           if (prop->perm == PP_RO) continue;
           RAEle  = prop->findElement("RA");
       	   if (!RAEle) continue;
   	   DecEle = prop->findElement("DEC");
   	   if (!DecEle) continue;
           break;

         // Horizontal
         case 1:
          if (prop->perm == PP_RO) continue;
          AzEle = prop->findElement("AZ");
          if (!AzEle) continue;
          AltEle = prop->findElement("ALT");
          if (!AltEle) continue;
          break;
        }
   
        onset = indidev->findProp("ON_COORD_SET");
        if (!onset) continue;
       
        onset->activateSwitch("SLEW");

        indidev->stdDev->currentObject = oCurrent;

      /* Send object name if available */
      if (indidev->stdDev->currentObject)
         {
             nameEle = indidev->findElem("OBJECT_NAME");
             if (nameEle && nameEle->pp->perm != PP_RO)
             {
                 nameEle->write_w->setText(indidev->stdDev->currentObject->name());
                 nameEle->pp->newText();
             }
          }

       switch (selectedCoord)
       {
         case 0:
            if (indidev->stdDev->currentObject)
		sp.set (indidev->stdDev->currentObject->ra(), indidev->stdDev->currentObject->dec());
            else
                sp.set (ks->map()->clickedPoint()->ra(), ks->map()->clickedPoint()->dec());

      	if (useJ2000)
	    sp.apparentCoord(ks->data()->ut().djd(), (long double) J2000);

    	   RAEle->write_w->setText(QString("%1:%2:%3").arg(sp.ra()->hour()).arg(sp.ra()->minute()).arg(sp.ra()->second()));
	   DecEle->write_w->setText(QString("%1:%2:%3").arg(sp.dec()->degree()).arg(sp.dec()->arcmin()).arg(sp.dec()->arcsec()));

          break;

       case 1:
         if (indidev->stdDev->currentObject)
         {
           sp.setAz(*indidev->stdDev->currentObject->az());
           sp.setAlt(*indidev->stdDev->currentObject->alt());
         }
         else
         {
           sp.setAz(*ks->map()->clickedPoint()->az());
           sp.setAlt(*ks->map()->clickedPoint()->alt());
         }

          AzEle->write_w->setText(QString("%1:%2:%3").arg(sp.az()->degree()).arg(sp.az()->arcmin()).arg(sp.az()->arcsec()));
          AltEle->write_w->setText(QString("%1:%2:%3").arg(sp.alt()->degree()).arg(sp.alt()->arcmin()).arg(sp.alt()->arcsec()));

         break;
       }

       prop->newText();
       
       return;
    }
  }
       
  // We didn't find any telescopes
  KMessageBox::sorry(0, i18n("KStars did not find any active telescopes."));
  
}

//FIXME: This will open multiple Detail windows for each object;
//Should have one window whose target object changes with selection
void ObservingList::slotDetails() {
	if ( oCurrent ) {
		DetailDialog dd( oCurrent, ks->data()->lt(), ks->geo(), ks );
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
	if ( oCurrent && ! ui->NotesEdit->text().isEmpty() && ui->NotesEdit->text() 
					!= i18n("Record here observation logs and/or data on %1.").arg( oCurrent->name()) ) {
		oCurrent->saveUserLog( ui->NotesEdit->text() );
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

		saveCurrentList();
		//First line is the name of the list.  The rest of the file should 
		//be object names, one per line.
		QTextStream istream(&f);
		QString line;
		ListName = istream.readLine();

		while ( ! istream.eof() ) {
			line = istream.readLine();
			//DEBUG
			kdDebug() << line << endl;

			//If the object is named "star", add it by coordinates
			SkyObject *o = 0;
			if ( line.startsWith( "star" ) ) {
				QStringList fields = QStringList::split( " ", line );
				//DEBUG
				kdDebug() << fields << endl;

				double ra = dms::fromString( fields[1], false ).Degrees(); //false = hours
				double dc = dms::fromString( fields[2], true ).Degrees();  //true  = degrees

				//Identify the star with these coordinates
				double rmax = 1.; 
				for ( uint i=0; i < ks->data()->starList.count(); ++i ) {
					SkyObject *s = (SkyObject*)(ks->data()->starList.at(i));
					double dra = fabs( ra - s->ra()->Degrees() );
					double ddc = fabs( dc - s->dec()->Degrees() );
					if ( dra < rmax && ddc < rmax ) {
						o = s;
						rmax = sqrt( dra*dra + ddc*ddc );
					}
				}
			} else {
				o = ks->data()->objectNamed( line );
			}

			if ( o ) slotAddObject( o );
		}

		//Newly-opened list should not trigger isModified flag
		isModified = false;

		f.close();

	} else if ( fileURL.path() != "" ) {
		QString message = i18n( "The specified file is invalid.  Try another file?" );
		if ( KMessageBox::warningYesNo( this, message, i18n("Invalid File"), i18n("Try Another"), i18n("Do Not Try") ) == KMessageBox::Yes ) {
			slotOpenList();
		}
	}
}

void ObservingList::saveCurrentList() {
	//Before loading a new list, do we need to save the current one?
	//Assume that if the list is empty, then there's no need to save
	if ( obsList.count() ) {
		if ( isModified ) {
			QString message = i18n( "Do you want to save the current list before opening a new list?" );
			if ( KMessageBox::questionYesNo( this, message, 
					i18n( "Save Current List?" ), KStdGuiItem::save(), KStdGuiItem::discard() ) == KMessageBox::Yes )
				slotSaveList();
		}

		//If we ever allow merging the loaded list with 
		//the existing one, that code would go here
		obsList.clear();
		ui->FullTable->clear();
	}
}

void ObservingList::slotSaveListAs() {
	bool ok(false);
	ListName = KInputDialog::getText( i18n( "Enter List Name" ), 
			i18n( "List name:" ), "", &ok );

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
		
		if ( KMessageBox::warningYesNo( 0, message, i18n( "Could Not Open File" ), i18n("Try Different"), i18n("Do Not Try") ) == KMessageBox::Yes ) {
			FileName == "";
			slotSaveList();
		}
		return;
	}
	
	QTextStream ostream(&f);
	ostream << ListName << endl;

	//Save objects to the list using their name.  If it's a star with a genetive name 
	//(i.e., "sigma orionis"), save the name with ascii characters, not greek letters.
	//If it's an unnamed star, save "star" and the star's coordinates.
	for ( SkyObject* o = obsList.first(); o; o = obsList.next() ) {
		if ( o->name() == "star" ) {
			ostream << o->name() << "  " << o->ra()->Hours() << "  " << o->dec()->Degrees() << endl;
		} else if ( o->type() == SkyObject::STAR ) {
			StarObject *s = (StarObject*)o;

			if ( s->name() == s->gname() ) {
				ostream << s->gname( false ) << endl;
			} else {
				ostream << o->name() << endl;
			}		    
		} else {
			ostream << o->name() << endl;
		}
	}

	f.close();
	isModified = false;
}

void ObservingList::slotWizard() {
	ObsListWizard wizard( ks );
	if ( wizard.exec() == QDialog::Accepted ) {
		//Make sure current list is saved
		saveCurrentList();

		for ( SkyObject *o = wizard.obsList().first(); o; o = wizard.obsList().next() ) {
			slotAddObject( o );
		}
	}
}

void ObservingList::slotToggleSize() {
	if ( isLarge() ) {
		ui->MiniButton->setPixmap( KGlobal::iconLoader()->loadIcon( "window_fullscreen", KIcon::Toolbar ) );

		//switch widget stack to show TinyTable
		ui->TableStack->raiseWidget( ui->TinyTable );

		//Abbreviate text on each button
		ui->CenterButton->setText( i18n( "First letter in 'Center'", "C" ) );
		ui->ScopeButton->setText( i18n( "First letter in 'Scope'", "S" ) );
		ui->DetailsButton->setText( i18n( "First letter in 'Details'", "D" ) );
		ui->AVTButton->setText( i18n( "First letter in 'Alt vs Time'", "A" ) );
		ui->RemoveButton->setText( i18n( "First letter in 'Remove'", "R" ) );

		//Hide Observing notes
		ui->NotesLabel->hide();
		ui->NotesEdit->hide();

		syncTableSelection( false ); //sync TinyTable with FullTable
		adjustSize();
		bIsLarge = false;

	} else {
		ui->MiniButton->setPixmap( KGlobal::iconLoader()->loadIcon( "window_nofullscreen", KIcon::Toolbar ) );

		//switch widget stack to show FullTable
		ui->TableStack->raiseWidget( ui->FullTable );

		//Expand text on each button
		ui->CenterButton->setText( i18n( "Center" ) );
		ui->ScopeButton->setText( i18n( "Scope" ) );
		ui->DetailsButton->setText( i18n( "Details" ) );
		ui->AVTButton->setText( i18n( "Alt vs Time" ) );
		ui->RemoveButton->setText( i18n( "Remove" ) );

		//Show Observing notes
		ui->NotesLabel->show();
		ui->NotesEdit->show();

		syncTableSelection( true ); //sync FullTable with TinyTable
		adjustSize();
		bIsLarge = true;
	}
}

void ObservingList::syncTableSelection( bool syncFullTable ) {
	if ( syncFullTable ) {
		int i=0;
		QListViewItem *it =  ui->FullTable->firstChild();
		while ( it ) {
			it->setSelected( ui->TinyTable->isSelected( i++ ) );
			it->nextSibling();
		}
	} else {
		int i=0;
		QListViewItem *it =  ui->FullTable->firstChild();
		while ( it ) {
			ui->TinyTable->setSelected( i++, it->isSelected() );
			it->nextSibling();
		}
	}	
}

#include "observinglist.moc"
