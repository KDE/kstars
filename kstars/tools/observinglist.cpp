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

#include "observinglist.h"

#include <stdio.h>

#include <QFile>
#include <QDir>
#include <QFrame>
#include <QTextStream>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QHeaderView>

#include <kpushbutton.h>
#include <kstatusbar.h>
#include <ktextedit.h>
#include <kinputdialog.h>
#include <kicon.h>
#include <kio/netaccess.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <ktemporaryfile.h>
#include <klineedit.h>

#include "obslistwizard.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skyobject.h"
#include "starobject.h"
#include "skymap.h"
#include "detaildialog.h"
#include "tools/altvstime.h"
#include "Options.h"

#include <config-kstars.h>

#ifdef HAVE_INDI_H
#include "indimenu.h"
#include "indielement.h"
#include "indiproperty.h"
#include "indidevice.h"
#include "devicemanager.h"
#include "indistd.h"
#endif

//
// ObservingListUI
// ---------------------------------
ObservingListUI::ObservingListUI( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}


//
// ObservingList
// ---------------------------------
ObservingList::ObservingList( KStars *_ks )
        : KDialog( (QWidget*)_ks ),
        ks( _ks ), LogObject(0), m_CurrentObject(0),
        noNameStars(0), isModified(false), bIsLarge(true)
{
    ui = new ObservingListUI( this );
    setMainWidget( ui );
    setCaption( i18n( "Observing List" ) );
    setButtons( KDialog::Close );

    //Set up the Table Views
    m_Model = new QStandardItemModel( 0, 5, this );
    m_Model->setHorizontalHeaderLabels( QStringList() << i18n( "Name" ) 
        << i18nc( "Right Ascension", "RA" ) << i18nc( "Declination", "Dec" )
        << i18nc( "Magnitude", "Mag" ) << i18n( "Type" ) );
    m_SortModel = new QSortFilterProxyModel( this );
    m_SortModel->setSourceModel( m_Model );
    m_SortModel->setDynamicSortFilter( true );
    ui->TableView->setModel( m_SortModel );
    ui->TableView->horizontalHeader()->setStretchLastSection( true );
    ui->TableView->horizontalHeader()->setResizeMode( QHeaderView::ResizeToContents );

    //Connections
    connect( this, SIGNAL( closeClicked() ), this, SLOT( slotClose() ) );
    connect( ui->TableView, SIGNAL( doubleClicked( const QModelIndex& ) ),
             this, SLOT( slotCenterObject() ) );
    connect( ui->TableView->selectionModel(), 
            SIGNAL( selectionChanged(const QItemSelection&, const QItemSelection&) ),
            this, SLOT( slotNewSelection() ) );
    connect( ui->RemoveButton, SIGNAL( clicked() ),
             this, SLOT( slotRemoveSelectedObjects() ) );
    connect( ui->CenterButton, SIGNAL( clicked() ),
             this, SLOT( slotCenterObject() ) );

    #ifdef HAVE_INDI_H
    connect( ui->ScopeButton, SIGNAL( clicked() ),
             this, SLOT( slotSlewToObject() ) );
    #endif

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
    connect( ui->MiniButton, SIGNAL( clicked() ),
             this, SLOT( slotToggleSize() ) );

    //Add icons to Push Buttons
    ui->OpenButton->setIcon( KIcon("document-open") );
    ui->SaveButton->setIcon( KIcon("document-save") );
    ui->SaveAsButton->setIcon( KIcon("document-save-as") );
    ui->WizardButton->setIcon( KIcon("games-solve") ); //is there a better icon for this button?
    ui->MiniButton->setIcon( KIcon("view-restore") );

    ui->CenterButton->setEnabled( false );
    ui->ScopeButton->setEnabled( false );
    ui->DetailsButton->setEnabled( false );
    ui->AVTButton->setEnabled( false );
    ui->RemoveButton->setEnabled( false );
    ui->NotesLabel->setEnabled( false );
    ui->NotesEdit->setEnabled( false );

    //Hide the MiniButton until I can figure out how to resize the Dialog!
    ui->MiniButton->hide();
}

bool ObservingList::contains( const SkyObject *q ) {
    foreach ( SkyObject* o, obsList() ) {
        if ( o == q ) return true;
    }

    return false;
}


//SLOTS

void ObservingList::slotAddObject( SkyObject *obj ) {
    if ( ! obj ) obj = ks->map()->clickedObject();

    //First, make sure object is not already in the list
    foreach ( SkyObject *o, obsList() ) {
        if ( obj == o ) {
            ks->statusBar()->changeItem( i18n( "%1 is already in the observing list.", obj->name() ), 0 );
            return;
        }
    }

    //Insert object in obsList
    m_ObservingList.append( obj );
    m_CurrentObject = obj;
    QList<QStandardItem*> itemList;
    QString smag = "--";
    if ( obj->mag() < 90.0 ) smag = QString::number( obj->mag(), 'g', 2 );
    itemList << new QStandardItem( obj->translatedName() ) 
            << new QStandardItem( obj->ra()->toHMSString() ) 
            << new QStandardItem( obj->dec()->toDMSString() ) 
            << new QStandardItem( smag )
            << new QStandardItem( obj->typeName() );
    m_Model->appendRow( itemList );
    if ( ! isModified ) isModified = true;

    //Note addition in statusbar
    ks->statusBar()->changeItem( i18n( "Added %1 to observing list.", obj->name() ), 0 );

    ui->TableView->resizeColumnsToContents();
}

void ObservingList::slotRemoveObject( SkyObject *o ) {
    if ( !o )
        o = ks->map()->clickedObject();

    int k = obsList().indexOf( o );
    if ( k < 0 ) return; //object not in observing list

    //DEBUG
    kDebug() << "Removing " << o->name();

    if ( o == LogObject )
        saveCurrentUserLog();

    //Remove row from the TableView model
    bool found(false);
    if ( o->name() == "star" ) {
        //Find object in table by RA and Dec
        for ( int irow = 0; irow < m_Model->rowCount(); ++irow ) {
            QString ra = m_Model->item(irow, 1)->text();
            QString dc = m_Model->item(irow, 2)->text();
            if ( o->ra()->toHMSString() == ra && o->dec()->toDMSString() == dc ) {
                m_Model->removeRow(irow);
                found = true;
                break;
            }
        }
    } else { // name is not "star"
        //Find object in table by name
        for ( int irow = 0; irow < m_Model->rowCount(); ++irow ) {
            QString name = m_Model->item(irow, 0)->text();
            if ( o->translatedName() == name ) {
                m_Model->removeRow(irow);
                found = true;
                break;
            }
        }
    }

    if ( !found ) kDebug() << "Did not find object named " << o->translatedName() << " in the Table!";

    obsList().removeAt(k);
    if ( ! isModified ) isModified = true;

    ui->TableView->resizeColumnsToContents();
}

void ObservingList::slotRemoveSelectedObjects() {
    if ( ! ui->TableView->selectionModel()->hasSelection() ) return;

    //Find each object by name in the observing list, and remove it
    //Go backwards so item alignment doesn't get screwed up as rows are removed.
    for ( int irow = m_Model->rowCount()-1; irow >= 0; --irow ) {
        if ( ui->TableView->selectionModel()->isRowSelected( irow, QModelIndex() ) ) {
            QModelIndex mSortIndex = m_SortModel->index( irow, 0 );
            QModelIndex mIndex = m_SortModel->mapToSource( mSortIndex );
            int irow = mIndex.row();
            QString ra = m_Model->item(irow, 1)->text();
            QString dc = m_Model->item(irow, 2)->text();

            foreach ( SkyObject *o, obsList() ) {
                //Stars named "star" must be matched by coordinates
                if ( o->name() == "star" ) {
                    if ( o->ra()->toHMSString() == ra && o->dec()->toDMSString() == dc ) {
                        slotRemoveObject( o );
                        break;
                    }

                } else if ( o->translatedName() == mIndex.data().toString() ) {
                    slotRemoveObject( o );
                    break;
                }
            }
        }
    }

    //we've removed all selected objects, so clear the selection
    ui->TableView->selectionModel()->clear();
}

void ObservingList::slotNewSelection() {
    QModelIndexList selectedItems = m_SortModel->mapSelectionToSource( ui->TableView->selectionModel()->selection() ).indexes();

    //Enable widgets when one object selected
    if ( selectedItems.size() == m_Model->columnCount() ) {
        QString newName( selectedItems[0].data().toString() );

        //Enable buttons
        ui->CenterButton->setEnabled( true );
	#ifdef HAVE_INDI_H
        ui->ScopeButton->setEnabled( true );
	#endif
        ui->DetailsButton->setEnabled( true );
        ui->AVTButton->setEnabled( true );
        ui->RemoveButton->setEnabled( true );

        //Find the selected object in the obsList,
        //then break the loop.  Now obsList.current()
        //points to the new selected object (until now it was the previous object)
        bool found(false);
        SkyObject *o;
        foreach ( o, obsList() ) {
            if ( o->translatedName() == newName ) {
                found = true;
                break;
            }
        }
        if ( found ) {
            m_CurrentObject = o;

            if ( newName != i18n( "star" ) ) {
                //Display the current object's user notes in the NotesEdit
                //First, save the last object's user log to disk, if necessary
                saveCurrentUserLog(); //uses LogObject, which is still the previous obj.

                //set LogObject to the new selected object
                LogObject = currentObject();

                ui->NotesLabel->setEnabled( true );
                ui->NotesEdit->setEnabled( true );

                ui->NotesLabel->setText( i18n( "observing notes for %1:", LogObject->translatedName() ) );
                if ( LogObject->userLog().isEmpty() ) {
                    ui->NotesEdit->setPlainText( i18n("Record here observation logs and/or data on %1.", LogObject->translatedName() ) );
                } else {
                    ui->NotesEdit->setPlainText( LogObject->userLog() );
                }
            } else { //selected object is named "star"
                //clear the log text box
                saveCurrentUserLog();
                ui->NotesLabel->setText( i18n( "observing notes (disabled for unnamed star)" ) );
                ui->NotesLabel->setEnabled( false );
                ui->NotesEdit->clear();
                ui->NotesEdit->setEnabled( false );
            }

        } else {
            kDebug() << i18n( "Object %1 not found in observing ist.", newName );
        } 

    } else if ( selectedItems.size() == 0 ) {
        //Disable buttons
        ui->CenterButton->setEnabled( false );
        ui->ScopeButton->setEnabled( false );
        ui->DetailsButton->setEnabled( false );
        ui->AVTButton->setEnabled( false );
        ui->RemoveButton->setEnabled( false );
        //FIXME: after 4.0, set this label to "Select one object to record notes on it here"
        ui->NotesLabel->setText( i18n( "observing notes for object:" ) );
        ui->NotesLabel->setEnabled( false );
        ui->NotesEdit->setEnabled( false );
        m_CurrentObject = 0;

        //Clear the user log text box.
        saveCurrentUserLog();
        ui->NotesEdit->setPlainText("");

    } else { //more than one object selected.
        ui->CenterButton->setEnabled( false );
        ui->ScopeButton->setEnabled( false );
        ui->DetailsButton->setEnabled( false );
        ui->AVTButton->setEnabled( true );
        ui->RemoveButton->setEnabled( true );
        //FIXME: after 4.0, set this label to "Select one object to record notes on it here"
        ui->NotesLabel->setText( i18n( "observing notes for object:" ) );
        ui->NotesLabel->setEnabled( false );
        ui->NotesEdit->setEnabled( false );
        m_CurrentObject = 0;

        //Clear the user log text box.
        saveCurrentUserLog();
        ui->NotesEdit->setPlainText("");
    }

}

void ObservingList::slotCenterObject() {
    if ( currentObject() ) {
        ks->map()->setClickedObject( currentObject() );
        ks->map()->setClickedPoint( currentObject() );
        ks->map()->slotCenter();
    }
}

void ObservingList::slotSlewToObject()
{
#ifdef HAVE_INDI_H

    INDI_D *indidev(NULL);
    INDI_P *prop(NULL), *onset(NULL);
    INDI_E *RAEle(NULL), *DecEle(NULL), *AzEle(NULL), *AltEle(NULL), *ConnectEle(NULL), *nameEle(NULL);
    bool useJ2000( false);
    int selectedCoord(0);
    SkyPoint sp;

    // Find the first device with EQUATORIAL_EOD_COORD or EQUATORIAL_COORD and with SLEW element
    // i.e. the first telescope we find!
    INDIMenu *imenu = ks->indiMenu();

    for (int i=0; i < imenu->managers.size() ; i++)
    {
        for (int j=0; j < imenu->managers.at(i)->indi_dev.size(); j++)
        {
            indidev = imenu->managers.at(i)->indi_dev.at(j);
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
                KMessageBox::error(0, i18n("Telescope %1 is offline. Please connect and retry again.", indidev->label));
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

            indidev->stdDev->currentObject = currentObject();

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

#endif
}

//FIXME: This will open multiple Detail windows for each object;
//Should have one window whose target object changes with selection
void ObservingList::slotDetails() {
    if ( currentObject() ) {
        DetailDialog dd( currentObject(), ks->data()->lt(), ks->geo(), ks );
        dd.exec();
    }
}

void ObservingList::slotAVT() {
    QModelIndexList selectedItems = m_SortModel->mapSelectionToSource( ui->TableView->selectionModel()->selection() ).indexes();

    if ( selectedItems.size() ) {
        AltVsTime avt( ks );
        foreach ( const QModelIndex &i, selectedItems ) {
            foreach ( SkyObject *o, obsList() ) {
                if ( o->translatedName() == i.data().toString() )
                    avt.processObject( o );
            }
        }

        avt.exec();
    }
}

//FIXME: On close, we will need to close any open Details/AVT windows
void ObservingList::slotClose() {
    //Save the current User log text
    if ( currentObject() && ! ui->NotesEdit->toPlainText().isEmpty() && ui->NotesEdit->toPlainText()
            != i18n("Record here observation logs and/or data on %1.", currentObject()->name()) ) {
        currentObject()->saveUserLog( ui->NotesEdit->toPlainText() );
    }

    hide();
}

void ObservingList::saveCurrentUserLog() {
    if ( ! ui->NotesEdit->toPlainText().isEmpty() &&
            ui->NotesEdit->toPlainText() !=
            i18n("Record here observation logs and/or data on %1.", LogObject->translatedName() ) ) {
        LogObject->saveUserLog( ui->NotesEdit->toPlainText() );

        ui->NotesEdit->clear();
        ui->NotesLabel->setText( i18n( "Observing notes for object:" ) );
        LogObject = NULL;
    }
}

void ObservingList::slotOpenList() {
    KUrl fileURL = KFileDialog::getOpenUrl( QDir::homePath(), "*.obslist|KStars Observing List (*.obslist)" );
    QFile f;

    if ( fileURL.isValid() ) {
        if ( ! fileURL.isLocalFile() ) {
            //Save remote list to a temporary local file
            KTemporaryFile tmpfile;
            tmpfile.setAutoRemove(false);
            tmpfile.open();
            FileName = tmpfile.fileName();
            if( KIO::NetAccess::download( fileURL, FileName, this ) )
                f.setFileName( FileName );

        } else {
            FileName = fileURL.path();
            f.setFileName( FileName );
        }

        if ( !f.open( QIODevice::ReadOnly) ) {
            QString message = i18n( "Could not open file %1", f.fileName() );
            KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
            return;
        }

        saveCurrentList();
        //First line is the name of the list.  The rest of the file should
        //be object names, one per line.
        QTextStream istream(&f);
        QString line;
        ListName = istream.readLine();

        while ( ! istream.atEnd() ) {
            line = istream.readLine();

            //If the object is named "star", add it by coordinates
            SkyObject *o;
            if ( line.startsWith( QLatin1String( "star" ) ) ) {
                QStringList fields = line.split( ' ', QString::SkipEmptyParts );
                dms ra = dms::fromString( fields[1], false ); //false = hours
                dms dc = dms::fromString( fields[2], true );  //true  = degrees
                SkyPoint p( ra, dc );
                double maxrad = 1000.0/Options::zoomFactor();
                o = ks->data()->skyComposite()->starNearest( &p, maxrad );
            } else {
                o = ks->data()->objectNamed( line );
            }

            //If we haven't identified the object, try interpreting the 
            //name as a star's genetive name (with ascii letters)
            if ( !o ) o = ks->data()->skyComposite()->findStarByGenetiveName( line );

            if ( o ) slotAddObject( o );
        }

        //Newly-opened list should not trigger isModified flag
        isModified = false;

        f.close();

    } else if ( !fileURL.path().isEmpty() ) {
        QString message = i18n( "The specified file is invalid.  Try another file?" );
        if ( KMessageBox::warningYesNo( this, message, i18n("Invalid File"), KGuiItem(i18n("Try Another")), KGuiItem(i18n("Do Not Try")) ) == KMessageBox::Yes ) {
            slotOpenList();
        }
    }
}

void ObservingList::saveCurrentList() {
    //Before loading a new list, do we need to save the current one?
    //Assume that if the list is empty, then there's no need to save
    if ( obsList().size() ) {
        if ( isModified ) {
            QString message = i18n( "Do you want to save the current list before opening a new list?" );
            if ( KMessageBox::questionYesNo( this, message,
                                             i18n( "Save Current List?" ), KStandardGuiItem::save(), KStandardGuiItem::discard() ) == KMessageBox::Yes )
                slotSaveList();
        }

        //If we ever allow merging the loaded list with
        //the existing one, that code would go here

        m_Model->clear();
        obsList().clear();
        m_CurrentObject = 0;
    }
}

void ObservingList::slotSaveListAs() {
    bool ok(false);
    ListName = KInputDialog::getText( i18n( "Enter List Name" ),
                                      i18n( "List name:" ), QString(), &ok );

    if ( ok ) {
        KUrl fileURL = KFileDialog::getSaveUrl( QDir::homePath(), "*.obslist|KStars Observing List (*.obslist)" );

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
    if ( !f.open( QIODevice::WriteOnly) ) {
        QString message = i18n( "Could not open file %1.  Try a different filename?", f.fileName() );

        if ( KMessageBox::warningYesNo( 0, message, i18n( "Could Not Open File" ), KGuiItem(i18n("Try Different")), KGuiItem(i18n("Do Not Try")) ) == KMessageBox::Yes ) {
            FileName.clear();
            slotSaveList();
        }
        return;
    }

    QTextStream ostream(&f);
    ostream << ListName << endl;
    foreach ( SkyObject* o, obsList() ) {
        if ( o->name() == "star" ) {
            ostream << o->name() << "  " << o->ra()->Hours() << "  " << o->dec()->Degrees() << endl;
        } else if ( o->type() == SkyObject::STAR ) {
            StarObject *s = (StarObject*)o;

            if ( s->name() == s->gname() ) {
                ostream << s->name2() << endl;
            } else { 
                ostream << s->name() << endl;
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

        foreach ( SkyObject *o, wizard.obsList() ) {
            slotAddObject( o );
        }
    }
}

void ObservingList::slotToggleSize() {
    if ( isLarge() ) {
        ui->MiniButton->setIcon( KIcon("view-fullscreen") );
        //Abbreviate text on each button

        ui->CenterButton->setText( i18nc( "First letter in 'Center'", "C" ) );
        ui->ScopeButton->setText( i18nc( "First letter in 'Scope'", "S" ) );
        ui->DetailsButton->setText( i18nc( "First letter in 'Details'", "D" ) );
        ui->AVTButton->setText( i18nc( "First letter in 'Alt vs Time'", "A" ) );
        ui->RemoveButton->setText( i18nc( "First letter in 'Remove'", "R" ) );

        //Hide columns 1-5
        ui->TableView->hideColumn(1);
        ui->TableView->hideColumn(2);
        ui->TableView->hideColumn(3);
        ui->TableView->hideColumn(4);
        ui->TableView->hideColumn(5);

        //Hide the headers
        ui->TableView->horizontalHeader()->hide();
        ui->TableView->verticalHeader()->hide();

        //Hide Observing notes
        ui->NotesLabel->hide();
        ui->NotesEdit->hide();

        //Set the width of the Table to be the width of 5 toolbar buttons, 
        //or the width of column 1, whichever is larger
        int w = 5*ui->MiniButton->width();
        if ( ui->TableView->columnWidth(0) > w ) {
            w = ui->TableView->columnWidth(0);
        } else {
            ui->TableView->setColumnWidth(0, w);
        }

        int left, right, top, bottom;
        ui->layout()->getContentsMargins( &left, &top, &right, &bottom );

        resize( w + left + right, height() );

        bIsLarge = false;

    } else {
        ui->MiniButton->setIcon( KIcon("view-restore") );

        //Show columns 1-5
        ui->TableView->showColumn(1);
        ui->TableView->showColumn(2);
        ui->TableView->showColumn(3);
        ui->TableView->showColumn(4);
        ui->TableView->showColumn(5);

        //Show the horizontal header
        ui->TableView->horizontalHeader()->show();

        //Expand text on each button
        ui->CenterButton->setText( i18n( "Center" ) );
        ui->ScopeButton->setText( i18n( "Scope" ) );
        ui->DetailsButton->setText( i18n( "Details" ) );
        ui->AVTButton->setText( i18n( "Alt vs Time" ) );
        ui->RemoveButton->setText( i18n( "Remove" ) );

        //Show Observing notes
        ui->NotesLabel->show();
        ui->NotesEdit->show();

        adjustSize();
        bIsLarge = true;
    }
}

#include "observinglist.moc"
