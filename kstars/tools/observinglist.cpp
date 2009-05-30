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
#include <kplotobject.h>
#include <kplotaxis.h>
#include <kplotwidget.h>
#include <ksnumbers.h>

#include "ksalmanac.h"
#include <kstandarddirs.h>
#include "obslistwizard.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "dialogs/locationdialog.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/starobject.h"
#include "skymap.h"
#include "dialogs/detaildialog.h"
#include "dialogs/finddialog.h"
#include "tools/altvstime.h"
#include "tools/wutdialog.h"
#include "Options.h"

#include <config-kstars.h>

#ifdef HAVE_INDI_H
#include "indi/indimenu.h"
#include "indi/indielement.h"
#include "indi/indiproperty.h"
#include "indi/indidevice.h"
#include "indi/devicemanager.h"
#include "indi/indistd.h"
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
        ks( _ks ), LogObject(0), m_CurrentObject(0), PlotObject(0),
        noNameStars(0), isModified(false), bIsLarge(true)
{
    ui = new ObservingListUI( this );
    setMainWidget( ui );
    setCaption( i18n( "Observing List" ) );
    setButtons( KDialog::Close );
    dt = KStarsDateTime::currentDateTime();
    geo = ks->geo();

    //Set up the Table Views
    m_Model = new QStandardItemModel( 0, 5, this );
    m_Session = new QStandardItemModel( 0, 5 );
    m_Model->setHorizontalHeaderLabels( QStringList() << i18n( "Name" ) 
        << i18nc( "Right Ascension", "RA" ) << i18nc( "Declination", "Dec" )
        << i18nc( "Magnitude", "Mag" ) << i18n( "Type" ) );
    m_Session->setHorizontalHeaderLabels( QStringList() << i18n( "Name" ) 
        << i18nc( "Right Ascension", "RA" ) << i18nc( "Declination", "Dec" )
        << i18nc( "Magnitude", "Mag" ) << i18n( "Type" ) 
        << i18n( "Time" ) << i18nc( "Altitude", "Alt" ) << i18nc( "Azimuth", "Az" ));
    m_SortModel = new QSortFilterProxyModel( this );
    m_SortModel->setSourceModel( m_Model );
    m_SortModel->setDynamicSortFilter( true );
    ui->TableView->setModel( m_SortModel );
    ui->TableView->horizontalHeader()->setStretchLastSection( true );
    ui->TableView->horizontalHeader()->setResizeMode( QHeaderView::ResizeToContents );
    m_SortModelSession = new QSortFilterProxyModel;
    m_SortModelSession->setSourceModel( m_Session );
    m_SortModelSession->setDynamicSortFilter( true );
    ui->SessionView->setModel( m_SortModelSession );
    ui->SessionView->horizontalHeader()->setStretchLastSection( true );
    ui->SessionView->horizontalHeader()->setResizeMode( QHeaderView::ResizeToContents );
    ksal = KSAlmanac::Instance();
    ksal->setLocation(geo);
    ui->View->setSunRiseSetTimes(ksal->getSunRise(),ksal->getSunSet());
    ui->View->setLimits( -12.0, 12.0, -90.0, 90.0 );
    ui->View->axis(KPlotWidget::BottomAxis)->setTickLabelFormat( 't' );
    ui->View->axis(KPlotWidget::BottomAxis)->setLabel( i18n( "Local Time" ) );
    ui->View->axis(KPlotWidget::TopAxis)->setTickLabelFormat( 't' );
    ui->View->axis(KPlotWidget::TopAxis)->setTickLabelsShown( true );
    ui->DateEdit->setDate(dt.date());
    ui->SetLocation->setText( geo -> fullName() );
    //Connections
    connect( this, SIGNAL( closeClicked() ), this, SLOT( slotClose() ) );
    connect( ui->TableView, SIGNAL( doubleClicked( const QModelIndex& ) ),
             this, SLOT( slotCenterObject() ) );
    connect( ui->TableView->selectionModel(), 
            SIGNAL( selectionChanged(const QItemSelection&, const QItemSelection&) ),
            this, SLOT( slotNewSelection() ) );
    connect( ui->SessionView->selectionModel(), 
            SIGNAL( selectionChanged(const QItemSelection&, const QItemSelection&) ),
            this, SLOT( slotNewSelection() ) );
    connect( ui->RemoveButton, SIGNAL( clicked() ),
             this, SLOT( slotRemoveSelectedObjects() ) );
    connect( ui->CenterButton, SIGNAL( clicked() ),
             this, SLOT( slotCenterObject() ) );
    connect( ui->AddToSession, SIGNAL( clicked() ),
             this, SLOT( slotAddToSession() ) );
    #ifdef HAVE_INDI_H
    connect( ui->ScopeButton, SIGNAL( clicked() ),
             this, SLOT( slotSlewToObject() ) );
    #endif

    connect( ui->DetailsButton, SIGNAL( clicked() ),
             this, SLOT( slotDetails() ) );
    connect( ui->AVTButton, SIGNAL( clicked() ),
             this, SLOT( slotAVT() ) );
    connect( ui->WUTButton, SIGNAL( clicked() ),
             this, SLOT( slotWUT() ) );
    connect( ui->FindButton, SIGNAL( clicked() ),
             this, SLOT( slotFind() ) );
    connect( ui->OpenButton, SIGNAL( clicked() ),
             this, SLOT( slotOpenList() ) );
    connect( ui->SaveButton, SIGNAL( clicked() ),
             this, SLOT( slotSaveSession() ) );
    connect( ui->SaveAsButton, SIGNAL( clicked() ),
             this, SLOT( slotSaveSessionAs() ) );
    connect( ui->WizardButton, SIGNAL( clicked() ),
             this, SLOT( slotWizard() ) );
    connect( ui->MiniButton, SIGNAL( clicked() ),
             this, SLOT( slotToggleSize() ) );
    connect( ui->SetLocation, SIGNAL( clicked() ),
             this, SLOT( slotLocation() ) );
    connect( ui->Update, SIGNAL( clicked() ),
             this, SLOT( slotUpdate() ) );
    connect( ui->SetTime, SIGNAL( clicked() ),
             this, SLOT( slotSetTime() ) );
    connect( ui->tabWidget, SIGNAL( currentChanged(int) ),
             this, SLOT( slotChangeTab(int) ) );
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
    ui->AddToSession->setEnabled( false );
    ui->SetTime->setEnabled( false );
    ui->TimeEdit->setEnabled( false );

    slotLoadWishList();
    //Hide the MiniButton until I can figure out how to resize the Dialog!
//    ui->MiniButton->hide();
}

bool ObservingList::contains( const SkyObject *q ) {
    foreach ( SkyObject* o, obsList() ) {
        if ( o == q ) return true;
    }

    return false;
}


//SLOTS

void ObservingList::slotAddObject( SkyObject *obj, bool session, bool init ) {
    bool flag=true;
    if ( ! obj ) obj = ks->map()->clickedObject();

    //First, make sure object is not already in the list
    foreach ( SkyObject *o, obsList() ) {
        if ( obj == o ) {
            flag = false;
            if( !session ) {
                ks->statusBar()->changeItem( i18n( "%1 is already in the observing list.", obj->name() ), 0 );
                return;
            }
        }
    }
    if( session ) 
        foreach ( SkyObject *o, SessionList() ) {
            if ( obj == o ) {
                ks->statusBar()->changeItem( i18n( "%1 is already in the session list.", obj->name() ), 0 );
                return;
            }
        }
    
    QString smag = "--";
    if ( obj->mag() < 90.0 ) smag = QString::number( obj->mag(), 'g', 2 );
       
    //Insert object in obsList
    if(flag) {
        m_ObservingList.append( obj );
        m_CurrentObject = obj;
        QList<QStandardItem*> itemList;
        if(obj->name() == "star" )
            itemList << new QStandardItem( obj->translatedName() ) 
                    << new QStandardItem( obj->ra0()->toHMSString() ) 
                    << new QStandardItem( obj->dec0()->toDMSString() ) 
                    << new QStandardItem( smag )
                    << new QStandardItem( obj->typeName() );
        else
            itemList << new QStandardItem( obj->translatedName() ) 
                    << new QStandardItem( obj->recomputeCoords(dt,geo).ra()->toHMSString() ) 
                    << new QStandardItem( obj->recomputeCoords(dt,geo).dec()->toDMSString() ) 
                    << new QStandardItem( smag )
                    << new QStandardItem( obj->typeName() );
        m_Model->appendRow( itemList );
        //Note addition in statusbar
        ks->statusBar()->changeItem( i18n( "Added %1 to observing list.", obj->name() ), 0 );
        ui->TableView->resizeColumnsToContents(); 
        if( !init) slotSaveList();
    }
     
    if( session )
    {
        m_SessionList.append(obj);
        QList<QStandardItem*> itemList;
        if(obj->name() == "star" )
            itemList << new QStandardItem( obj->translatedName() ) 
                    << new QStandardItem( obj->ra0()->toHMSString() ) 
                    << new QStandardItem( obj->dec0()->toDMSString() ) 
                    << new QStandardItem( smag )
                    << new QStandardItem( obj->typeName() )
                    << new QStandardItem( "--"  );
        else
            itemList << new QStandardItem( obj->translatedName() ) 
                    << new QStandardItem( obj->recomputeCoords(dt,geo).ra()->toHMSString() ) 
                    << new QStandardItem( obj->recomputeCoords(dt,geo).dec()->toDMSString() ) 
                    << new QStandardItem( smag )
                    << new QStandardItem( obj->typeName() )
                    << new QStandardItem( TimeHash.value( obj->name(), obj->transitTime( dt, geo ) ).toString( "h:mm A" ) );
        m_Session->appendRow( itemList );
        if ( ! isModified ) isModified = true;
        ui->SessionView->resizeColumnsToContents();
        ks->statusBar()->changeItem( i18n( "Added %1 to session list.", obj->name() ), 0 );
    }
}

void ObservingList::slotRemoveObject( SkyObject *o, bool session, bool update ) {
    if(!update) { 
        if ( !o )
            o = ks->map()->clickedObject();
        else
            if( ui->tabWidget->currentIndex() )
                session = true;
    }
    if( !session ) {
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
       
        ui->View->removeAllPlotObjects();
        ui->TableView->resizeColumnsToContents();
        if(!update) slotSaveList();
    } else {
        int k = SessionList().indexOf( o );
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
                QString ra = m_Session->item(irow, 1)->text();
                QString dc = m_Session->item(irow, 2)->text();
                if ( o->ra()->toHMSString() == ra && o->dec()->toDMSString() == dc ) {
                    m_Session->removeRow(irow);
                    found = true;
                    break;
                }
            }
        } else { // name is not "star"
            //Find object in table by name
            for ( int irow = 0; irow < m_Session->rowCount(); ++irow ) {
                QString name = m_Session->item(irow, 0)->text();
                if ( o->translatedName() == name ) {
                    m_Session->removeRow(irow);
                    found = true;
                    break;
                }
            }
        }
 
        if ( !found ) kDebug() << "Did not find object named " << o->translatedName() << " in the Table!";
 
        SessionList().removeAt(k);
        if ( ! isModified ) isModified = true;
       
        ui->View->removeAllPlotObjects();
        ui->SessionView->resizeColumnsToContents();
    }
}

void ObservingList::slotRemoveSelectedObjects() {
    if(ui->tabWidget->currentIndex())
    {
        if ( ! ui->SessionView->selectionModel()->hasSelection() ) return;

        //Find each object by name in the observing list, and remove it
        //Go backwards so item alignment doesn't get screwed up as rows are removed.
        for ( int irow = m_Session->rowCount()-1; irow >= 0; --irow ) {
            if ( ui->SessionView->selectionModel()->isRowSelected( irow, QModelIndex() ) ) {
                QModelIndex mSortIndex = m_SortModelSession->index( irow, 0 );
                QModelIndex mIndex = m_SortModelSession->mapToSource( mSortIndex );
                int irow = mIndex.row();
                QString ra = m_Session->item(irow, 1)->text();
                QString dc = m_Session->item(irow, 2)->text();

                foreach ( SkyObject *o, SessionList() ) {
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
        ui->SessionView->selectionModel()->clear();
    } else {
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

}

void ObservingList::slotNewSelection() {
    bool singleSelection = false, found = false;
    QModelIndexList selectedItems;
    QString newName;
    SkyObject *o;
    if( ui->tabWidget->currentIndex() ) {
        selectedItems = m_SortModelSession->mapSelectionToSource( ui->SessionView->selectionModel()->selection() ).indexes();
 
        //When one object is selected
        if ( selectedItems.size() == m_Session->columnCount() ) {
            newName = selectedItems[0].data().toString();
            singleSelection = true;
 
            //Find the selected object in the SessionList,
            //then break the loop.  Now SessionList.current()
            //points to the new selected object (until now it was the previous object)
            foreach ( o, SessionList() ) {
                if ( o->translatedName() == newName ) {
                    found = true;
                    break;
                }
            }
        }
    } else {
        selectedItems = m_SortModel->mapSelectionToSource( ui->TableView->selectionModel()->selection() ).indexes();
 
        //When one object is selected
        if ( selectedItems.size() == m_Model->columnCount() ) {
            newName = selectedItems[0].data().toString();
            singleSelection = true;
             
            //Find the selected object in the obsList,
            //then break the loop.  Now obsList.current()
            //points to the new selected object (until now it was the previous object)
            foreach ( o, obsList() ) {
                if ( o->translatedName() == newName ) {
                    found = true;
                break;
                }
            }
        }
    }
    if( singleSelection ) {
        //Enable buttons
            ui->CenterButton->setEnabled( true );
        #ifdef HAVE_INDI_H
            ui->ScopeButton->setEnabled( true );
        #endif
        ui->DetailsButton->setEnabled( true );
        ui->AVTButton->setEnabled( true );
        ui->RemoveButton->setEnabled( true );
        ui->AddToSession->setEnabled( true );
        if ( found ) {
            m_CurrentObject = o;
            PlotObject = currentObject();
            plot( PlotObject );

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
                if( ui->tabWidget->currentIndex() ) {
                    ui->TimeEdit->setEnabled( true );
                    ui->SetTime->setEnabled( true );
                    ui->TimeEdit->setTime( TimeHash.value( o->name(), o->transitTime( dt, geo ) ) );
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
            kDebug() << i18n( "Object %1 not found in list.", newName );
        } 
    } else if ( selectedItems.size() == 0 ) {
        //Disable buttons
        ui->CenterButton->setEnabled( false );
        ui->ScopeButton->setEnabled( false );
        ui->DetailsButton->setEnabled( false );
        ui->AVTButton->setEnabled( false );
        ui->RemoveButton->setEnabled( false );
        ui->NotesLabel->setText( i18n( "Select an object to record notes on it here:" ) );
        ui->NotesLabel->setEnabled( false );
        ui->NotesEdit->setEnabled( false );
        ui->AddToSession->setEnabled( false );
        m_CurrentObject = 0;
        ui->TimeEdit->setEnabled( false );
        ui->SetTime->setEnabled( false );

        //Clear the user log text box.
        saveCurrentUserLog();
        ui->NotesEdit->setPlainText("");
        ui->View->removeAllPlotObjects();

    } else { //more than one object selected.
        ui->CenterButton->setEnabled( false );
        ui->ScopeButton->setEnabled( false );
        ui->DetailsButton->setEnabled( false );
        ui->AVTButton->setEnabled( true );
        ui->RemoveButton->setEnabled( true );
        ui->AddToSession->setEnabled( true );
        ui->NotesLabel->setText( i18n( "Select an object to record notes on it here:" ) );
        ui->NotesLabel->setEnabled( false );
        ui->NotesEdit->setEnabled( false );
        ui->TimeEdit->setEnabled( false );
        ui->SetTime->setEnabled( false );
        m_CurrentObject = 0;
        ui->View->removeAllPlotObjects();

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
                        selectedCoord = 1;      /* Select horizontal */
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
        QPointer<DetailDialog> dd = new DetailDialog( currentObject(), ks->data()->lt(), geo, ks );
        dd->exec();
	delete dd;
    }
}

void ObservingList::slotWUT() {
    WUTDialog w( ks );    
    w.exec();
}

void ObservingList::slotAddToSession() {
    QModelIndexList selectedItems = m_SortModel->mapSelectionToSource( ui->TableView->selectionModel()->selection() ).indexes();
    if ( selectedItems.size() ) {
        foreach ( const QModelIndex &i, selectedItems ) {
            foreach ( SkyObject *o, obsList() ) {
                if ( o->translatedName() == i.data().toString() )
                    slotAddObject( o, true );
                }
        }
    }
}

void ObservingList::slotFind() {
    FindDialog fd( ks );    
    if ( fd.exec() == QDialog::Accepted ) {
       SkyObject *o = fd.selectedObject();
       if( o!= 0 )
       slotAddObject( o );
    }

}

void ObservingList::slotAVT() {
    QModelIndexList selectedItems = m_SortModel->mapSelectionToSource( ui->TableView->selectionModel()->selection() ).indexes();

    if ( selectedItems.size() ) {
        QPointer<AltVsTime> avt = new AltVsTime( ks );
        foreach ( const QModelIndex &i, selectedItems ) {
            foreach ( SkyObject *o, obsList() ) {
                if ( o->translatedName() == i.data().toString() )
                    avt->processObject( o );
            }
        }

        avt->exec();
	delete avt;
    }
}

//FIXME: On close, we will need to close any open Details/AVT windows
void ObservingList::slotClose() {
    //Save the current User log text
    if ( currentObject() && ! ui->NotesEdit->toPlainText().isEmpty() && ui->NotesEdit->toPlainText()
            != i18n("Record here observation logs and/or data on %1.", currentObject()->name()) ) {
        currentObject()->saveUserLog( ui->NotesEdit->toPlainText() );
    }
    ui->View->removeAllPlotObjects();
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
            FileName = fileURL.toLocalFile();
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
        SessionName = istream.readLine();
        line = istream.readLine();
        QStringList fields = line.split('~');
        geo = ks->data()->locationNamed(fields[0],fields[1],fields[2]);
        ui->SetLocation -> setText( geo -> fullName() );

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
            } else if ( line.startsWith( "Begin Hash" ) ) {
                break;
            } else {
                o = ks->data()->objectNamed( line );
            }

            //If we haven't identified the object, try interpreting the 
            //name as a star's genetive name (with ascii letters)
            if ( !o ) o = ks->data()->skyComposite()->findStarByGenetiveName( line );

            if ( o ) slotAddObject( o, true );
        }
        while ( ! istream.atEnd() ) {
            line = istream.readLine();
            QStringList hashdata = line.split(':');
            TimeHash.insert( hashdata[0], QTime::fromString( hashdata[1], " hms ap" ) );
        }
        //Update the location and user set times from file
        slotUpdate();
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
    if ( SessionList().size() ) {
        if ( isModified ) {
            QString message = i18n( "Do you want to save the current session before opening a new session?" );
            if ( KMessageBox::questionYesNo( this, message,
                                             i18n( "Save Current session?" ), KStandardGuiItem::save(), KStandardGuiItem::discard() ) == KMessageBox::Yes )
                slotSaveSession();
        }

        //If we ever allow merging the loaded list with
        //the existing one, that code would go here
    }
}

void ObservingList::slotSaveSessionAs() {
    bool ok(false);
    SessionName = KInputDialog::getText( i18n( "Enter Session Name" ),
                                      i18n( "Session name:" ), QString(), &ok );
    if ( ok ) {
        KUrl fileURL = KFileDialog::getSaveUrl( QDir::homePath(), "*.obslist|KStars Observing List (*.obslist)" );
        if ( fileURL.isValid() )
            FileName = fileURL.path();
        slotSaveSession();
    }
}

void ObservingList::slotSaveList() {
    QFile f;
    f.setFileName( KStandardDirs::locateLocal( "appdata", "wishlist.obslist" ) );   
    if ( !f.open( QIODevice::WriteOnly) ) {
        kDebug()<<"Cannot write list to  file";
        return;
    }

    QTextStream ostream(&f);
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
}

void ObservingList::slotLoadWishList() {
    QFile f;
    f.setFileName( KStandardDirs::locateLocal( "appdata", "wishlist.obslist" ) );   

    if ( !f.open( QIODevice::ReadOnly) ) {
     //  QString message = i18n( "Could not open file %1", f.fileName() );
      // KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
       kDebug()<<" No WishList Saved yet";
       return;
   }

   
    QTextStream istream(&f);
    QString line;

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

        if ( o ) slotAddObject( o, false, true );
    }
    f.close();
}


void ObservingList::slotSaveSession() {
    if ( FileName.isEmpty() || SessionName.isEmpty()  ) {
        slotSaveSessionAs();
        return;
    }

    QFile f( FileName );
    if(!f.open(QIODevice::WriteOnly)) {
        QString message = i18n( "Could not open file %1.  Try a different filename?", f.fileName() );
        if ( KMessageBox::warningYesNo( 0, message, i18n( "Could Not Open File" ), KGuiItem(i18n("Try Different")), KGuiItem(i18n("Do Not Try")) ) == KMessageBox::Yes ) {
            FileName.clear();
            slotSaveSessionAs();
        }
    return;
    }
    QTextStream ostream(&f);
    ostream << SessionName << endl;
    ostream << geo->name() << "~" <<geo->province() << "~" << geo->country() << endl;
    foreach ( SkyObject* o, SessionList() ) {
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
    ostream << "Begin Hash"<<endl;
    QHashIterator<QString, QTime> i(TimeHash);
    while (i.hasNext()) {
        i.next();
        ostream << i.key() << ": " << i.value().toString("hms ap") << endl;
    }
    f.close();
    isModified = false;
}

void ObservingList::slotWizard() {
    QPointer<ObsListWizard> wizard = new ObsListWizard( ks );
    if ( wizard->exec() == QDialog::Accepted ) {
        foreach ( SkyObject *o, wizard->obsList() ) {
            slotAddObject( o );
        }
    }
    delete wizard;
}

void ObservingList::plot( SkyObject *o ) {
    if( !o ) return;
    float DayOffset = 0;
    if( TimeHash.value( o->name(), o->transitTime( dt, geo ) ).hour() > 12)
        DayOffset = 1;
    KStarsDateTime ut = dt;
    ut.setTime(QTime());
    ut = geo->LTtoUT(ut);
    ut = ut.addSecs( (0.5+DayOffset)*86400.0 );
    double h1 = geo->GSTtoLST( ut.gst() ).Hours();
    if ( h1 > 12.0 ) h1 -= 24.0;
    double h2 = h1 + 24.0;
    ui->View->setSecondaryLimits( h1, h2, -90.0, 90.0 );
    ksal->setLocation(geo);
    ui->View->setSunRiseSetTimes(ksal->getSunRise(),ksal->getSunSet());
    ui->View->update();
    KPlotObject *po = new KPlotObject( Qt::white, KPlotObject::Lines, 2.0 );
        for ( double h=-12.0; h<=12.0; h+=0.5 ) {
            po->addPoint( h, findAltitude( o, (h + DayOffset*24.0) ) );
        }
    ui->View->removeAllPlotObjects();
    ui->View->addPlotObject( po );
}
double ObservingList::findAltitude( SkyPoint *p, double hour ) {
    KStarsDateTime ut = dt;
    ut.setTime(QTime());
    ut = geo->LTtoUT(ut);
    ut= ut.addSecs( hour*3600.0 );
    dms LST = geo->GSTtoLST( ut.gst() );
    p->EquatorialToHorizontal( &LST, geo->lat() );
    return p->alt()->Degrees();
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
        ui->WUTButton->setText( i18nc( "First letter in 'WUT'", "W" ) );
        ui->FindButton->setText( i18nc( "First letter in 'Find'", "F" ) );
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
        ui->View->hide();
//      ui->Spacer1->hide();
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
        ui->WUTButton->setText( i18n( "WUT") );
        ui->FindButton->setText( i18n( "Find &amp;Object") );

        //Show Observing notes
        ui->NotesLabel->show();
        ui->NotesEdit->show();
        ui->View->show();
        adjustSize();
        bIsLarge = true;
    }
}

void ObservingList::slotChangeTab(int index) {
    ui->CenterButton->setEnabled( false );
    ui->ScopeButton->setEnabled( false );
    ui->DetailsButton->setEnabled( false );
    ui->AVTButton->setEnabled( false );
    ui->RemoveButton->setEnabled( false );
    ui->NotesLabel->setText( i18n( "Select an object to record notes on it here:" ) );
    ui->NotesLabel->setEnabled( false );
    ui->NotesEdit->setEnabled( false );
    ui->AddToSession->setEnabled( false );
    ui->TimeEdit->setEnabled( false );
    ui->SetTime->setEnabled( false );
    m_CurrentObject = 0;
    if(index)
        ui->WizardButton->setEnabled( false );
    else 
        ui->WizardButton->setEnabled( true );
    //Clear the user log text box.
    saveCurrentUserLog();
    ui->NotesEdit->setPlainText("");
    ui->View->removeAllPlotObjects();
}
void ObservingList::slotLocation() {
    LocationDialog ld( (KStars*) topLevelWidget()->parent() );

    if ( ld.exec() == QDialog::Accepted ) {
        geo = ld.selectedCity();
        ui->SetLocation -> setText( geo -> fullName() );
    }
}

void ObservingList::slotUpdate() {
    dt.setDate( ui->DateEdit->date() );
    ui->View->removeAllPlotObjects();
    //Creating a copy of the lists, we can't use the original lists as they'll keep getting modified as the loop iterates
    QList<SkyObject*> _obsList=m_ObservingList, _SessionList=m_SessionList;
    foreach ( SkyObject *o, _obsList ) {
        if( o->name() != "star" ) {
            slotRemoveObject( o, false, true );
            slotAddObject( o, false, true );
        }
    }
    foreach ( SkyObject *obj, _SessionList ) {
        if( obj->name() != "star" ) {  
            slotRemoveObject( obj, true );
            slotAddObject( obj, true, true );
        }
    }
}
void ObservingList::slotSetTime() {
    SkyObject *o = currentObject();
    slotRemoveObject( o, true );
    TimeHash [o->name()] = ui->TimeEdit->time();
    slotAddObject(o, true, true );
}
#include "observinglist.moc"
