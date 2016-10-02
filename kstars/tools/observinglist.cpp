/***************************************************************************
                          observinglist.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 29 Nov 2004
    copyright            : (C) 2004-2014 by Jeff Woods, Jason Harris,
                           Prakash Mohan, Akarsh Simha
    email                : jcwoods@bellsouth.net, jharris@30doradus.org,
                           prakash.mohan@kdemail.net, akarsh@kde.org
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
#include "sessionsortfilterproxymodel.h"

#include "ksalmanac.h"
#include "obslistwizard.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "ksdssimage.h"
#include "ksdssdownloader.h"
#include "dialogs/locationdialog.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/starobject.h"
#include "skycomponents/skymapcomposite.h"
#include "skymap.h"
#include "dialogs/detaildialog.h"
#include "dialogs/finddialog.h"
#include "tools/altvstime.h"
#include "tools/wutdialog.h"
#include "Options.h"
#include "imageviewer.h"
#include "thumbnailpicker.h"
#include "obslistpopupmenu.h"
#include "oal/log.h"
#include "oal/oal.h"
#include "oal/execute.h"
#include "tools/eyepiecefield.h"
#include "fov.h"
#include "constellationboundarylines.h"

#include <config-kstars.h>

#ifdef HAVE_INDI
#include <basedevice.h>
#include "indi/indilistener.h"
#include "indi/drivermanager.h"
#include "indi/driverinfo.h"
#include "ekos/ekosmanager.h"
#endif
#include "kspaths.h"
#include <KPlotting/KPlotAxis>
#include <KPlotting/KPlotObject>
#include <KMessageBox>

#include <QFile>
#include <QDir>
#include <QFrame>
#include <QTextStream>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QDirIterator>
#include <QPushButton>
#include <QStatusBar>
#include <QFileDialog>
#include <QStandardPaths>
#include <QTextEdit>
#include <QLineEdit>
#include <QInputDialog>

#include <cstdio>

//
// ObservingListUI
// ---------------------------------
ObservingListUI::ObservingListUI( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

//
// ObservingList
// ---------------------------------
ObservingList::ObservingList()
        : QDialog( (QWidget*) KStars::Instance() ),
        LogObject(0), m_CurrentObject(0),
          isModified(false), bIsLarge(true), m_dl( 0 )
{
    ui = new ObservingListUI( this );
    QVBoxLayout *mainLayout= new QVBoxLayout;
    mainLayout->addWidget(ui);
    setWindowTitle( i18n( "Observation Planner" ) );

    // Close button seems redundant since one can close the window -- occupies space
    /*
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    */

    setLayout( mainLayout );

    dt = KStarsDateTime::currentDateTime();
    setFocusPolicy(Qt::StrongFocus);
    geo = KStarsData::Instance()->geo();
    sessionView = false;
    m_listFileName = QString();
    pmenu = new ObsListPopupMenu();
    //Set up the Table Views
    m_WishListModel = new QStandardItemModel( 0, 5, this );
    m_SessionModel = new QStandardItemModel( 0, 5 );

    m_WishListModel->setHorizontalHeaderLabels(
        QStringList() << i18n( "Name" )
        << i18n( "Alternate Name" )
        << i18nc( "Right Ascension", "RA (J2000)" )
        << i18nc( "Declination", "Dec (J2000)" )
        << i18nc( "Magnitude", "Mag" )
        << i18n( "Type" )
        << i18n( "Current Altitude" )
        );
    m_SessionModel->setHorizontalHeaderLabels(
        QStringList() << i18n( "Name" )
        << i18n( "Alternate Name" )
        << i18nc( "Right Ascension", "RA (J2000)" )
        << i18nc( "Declination", "Dec (J2000)" )
        << i18nc( "Magnitude", "Mag" )
        << i18n( "Type" )
        << i18nc( "Constellation", "Constell." )
        << i18n( "Time" )
        << i18nc( "Altitude", "Alt" )
        << i18nc( "Azimuth", "Az" )
        );

    m_WishListSortModel = new QSortFilterProxyModel( this );
    m_WishListSortModel->setSourceModel( m_WishListModel );
    m_WishListSortModel->setDynamicSortFilter( true );
    m_WishListSortModel->setSortRole( Qt::UserRole );
    ui->WishListView->setModel( m_WishListSortModel );
    ui->WishListView->horizontalHeader()->setStretchLastSection( true );

    ui->WishListView->horizontalHeader()->setSectionResizeMode( QHeaderView::Interactive );
    m_SessionSortModel = new SessionSortFilterProxyModel();
    m_SessionSortModel->setSourceModel( m_SessionModel );
    m_SessionSortModel->setDynamicSortFilter( true );
    ui->SessionView->setModel( m_SessionSortModel );
    ui->SessionView->horizontalHeader()->setStretchLastSection( true );
    ui->SessionView->horizontalHeader()->setSectionResizeMode( QHeaderView::Interactive );
    ksal = new KSAlmanac;
    ksal->setLocation(geo);
    ui->avt->setGeoLocation( geo );
    ui->avt->setSunRiseSetTimes(ksal->getSunRise(),ksal->getSunSet());
    ui->avt->setLimits( -12.0, 12.0, -90.0, 90.0 );
    ui->avt->axis(KPlotWidget::BottomAxis)->setTickLabelFormat( 't' );
    ui->avt->axis(KPlotWidget::BottomAxis)->setLabel( i18n( "Local Time" ) );
    ui->avt->axis(KPlotWidget::TopAxis)->setTickLabelFormat( 't' );
    ui->avt->axis(KPlotWidget::TopAxis)->setTickLabelsShown( true );
    ui->DateEdit->setDate(dt.date());
    ui->SetLocation->setText( geo -> fullName() );
    ui->ImagePreview->installEventFilter( this );
    ui->WishListView->viewport()->installEventFilter( this );
    ui->WishListView->installEventFilter( this );
    ui->SessionView->viewport()->installEventFilter( this );
    ui->SessionView->installEventFilter( this );
    // setDefaultImage();
    //Connections
    connect( ui->WishListView, SIGNAL( doubleClicked( const QModelIndex& ) ),
             this, SLOT( slotCenterObject() ) );
    connect( ui->WishListView->selectionModel(),
            SIGNAL( selectionChanged(const QItemSelection&, const QItemSelection&) ),
            this, SLOT( slotNewSelection() ) );
    connect( ui->SessionView->selectionModel(),
            SIGNAL( selectionChanged(const QItemSelection&, const QItemSelection&) ),
            this, SLOT( slotNewSelection() ) );
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
    connect( ui->DeleteImage, SIGNAL( clicked() ),
             this, SLOT( slotDeleteCurrentImage() ) );
    connect( ui->SearchImage, SIGNAL( clicked() ),
             this, SLOT( slotSearchImage() ) );
    connect( ui->SetTime, SIGNAL( clicked() ),
             this, SLOT( slotSetTime() ) );
    connect( ui->tabWidget, SIGNAL( currentChanged(int) ),
             this, SLOT( slotChangeTab(int) ) );
    connect( ui->saveImages, SIGNAL( clicked() ),
             this, SLOT( slotSaveAllImages() ) );
    connect( ui->DeleteAllImages, SIGNAL( clicked() ),
             this, SLOT( slotDeleteAllImages() ) );
    connect( ui->OALExport, SIGNAL( clicked() ),
             this, SLOT( slotOALExport() ) );
    //Add icons to Push Buttons
    ui->OpenButton->setIcon( QIcon::fromTheme("document-open", QIcon(":/icons/breeze/default/document-open.svg")) );
    ui->SaveButton->setIcon( QIcon::fromTheme("document-save", QIcon(":/icons/breeze/default/document-save.svg")) );
    ui->SaveAsButton->setIcon( QIcon::fromTheme("document-save-as", QIcon(":/icons/breeze/default/document-save-as.svg")) );
    ui->WizardButton->setIcon( QIcon::fromTheme("tools-wizard", QIcon(":/icons/breeze/default/tools-wizard.svg")) );
    ui->MiniButton->setIcon( QIcon::fromTheme("view-restore", QIcon(":/icons/breeze/default/view-restore.svg")) );
    noSelection = true;
    showScope = false;
    ui->NotesLabel->setEnabled( false );
    ui->NotesEdit->setEnabled( false );
    ui->SetTime->setEnabled( false );
    ui->TimeEdit->setEnabled( false );
    ui->SearchImage->setEnabled( false );
    ui->saveImages->setEnabled( false );
    ui->DeleteImage->setEnabled( false );
    ui->OALExport->setEnabled( false );

    m_NoImagePixmap = QPixmap(":/images/noimage.png").scaledToHeight(ui->ImagePreview->width());

    m_altCostHelper = [ this ]( const SkyPoint &p ) -> QStandardItem * {
        const double inf = std::numeric_limits<double>::infinity();
        double altCost = 0.;
        QString itemText;
        double maxAlt = p.maxAlt( *( geo->lat() ) );
        if ( Options::obsListDemoteHole() && maxAlt > 90. - Options::obsListHoleSize() )
            maxAlt = 90. - Options::obsListHoleSize();
        if (  maxAlt <= 0. ) {
            altCost = -inf;
            itemText = i18n( "Never rises" );
        }
        else {
            altCost = ( p.alt().Degrees() / maxAlt ) * 100.;
            if ( altCost < 0 )
                itemText = i18nc( "Short text to describe that object has not risen yet", "Not risen" );
            else {
                if ( altCost > 100. ) {
                    altCost = -inf;
                    itemText = i18nc( "Object is in the Dobsonian hole", "In hole" );
                }
                else
                    itemText = QString::number( altCost, 'f', 0 ) + '%';
            }
        }

        QStandardItem *altItem = new QStandardItem( itemText );
        altItem->setData( altCost, Qt::UserRole );
        qDebug() << "Updating altitude for " << p.ra().toHMSString() << " " << p.dec().toDMSString() << " alt = " << p.alt().toDMSString() << " info to " << itemText;
        return altItem;
    };

    slotLoadWishList(); //Load the wishlist from disk if present
    m_CurrentObject = 0;
    setSaveImagesButton();
    //Hide the MiniButton until I can figure out how to resize the Dialog!
    //    ui->MiniButton->hide();

    // Set up for the large-size view
    bIsLarge = false;
    slotToggleSize();

    slotUpdateAltitudes();
    m_altitudeUpdater = new QTimer( this );
    connect( m_altitudeUpdater, SIGNAL( timeout() ), this, SLOT( slotUpdateAltitudes() ) );
    m_altitudeUpdater->start( 120000 ); // update altitudes every 2 minutes

}

ObservingList::~ObservingList()
{
    delete ksal;
    delete m_SessionModel;
    delete m_WishListModel;
    delete m_SessionSortModel;
}

//SLOTS

void ObservingList::slotAddObject( SkyObject *obj, bool session, bool update ) {
    bool addToWishList=true;
    if( ! obj )
        obj = SkyMap::Instance()->clickedObject(); // Eh? Why? Weird default behavior.

    if ( !obj ) {
        qWarning() << "Trying to add null object to observing list! Ignoring.";
        return;
    }

    QString finalObjectName = getObjectName(obj);

    if (finalObjectName.isEmpty())
    {
            KMessageBox::sorry(0, i18n( "Unnamed stars are not supported in the observing lists"));
            return;
    }

    //First, make sure object is not already in the list
    if ( obsList().contains( obj ) ) {
        addToWishList = false;
        if( ! session ) {
            KStars::Instance()->statusBar()->showMessage( i18n( "%1 is already in your wishlist.", finalObjectName ), 0 );
            return;
        }
    }

    if ( session && sessionList().contains( obj ) ) {
        KStars::Instance()->statusBar()->showMessage( i18n( "%1 is already in the session plan.", finalObjectName ), 0 );
        return;
    }

    // JM: If we are loading observing list from disk, solar system objects magnitudes are not calculated until later
    // Therefore, we manual invoke updateCoords to force computation of magnitude.
    if ( (obj->type() == SkyObject::COMET || obj->type() == SkyObject::ASTEROID || obj->type() == SkyObject::MOON ||
          obj->type() == SkyObject::PLANET) && obj->mag() == 0)
    {
        KSNumbers num( dt.djd() );
        CachingDms LST = geo->GSTtoLST( dt.gst() );
        obj->updateCoords(&num, true, geo->lat(), &LST, true);
    }

    QString smag = "--";
    if (  - 30.0 < obj->mag() && obj->mag() < 90.0 )
        smag = QString::number( obj->mag(), 'f', 2 ); // The lower limit to avoid display of unrealistic comet magnitudes

    SkyPoint p = obj->recomputeHorizontalCoords( dt, geo );

    QList<QStandardItem*> itemList;

    auto getItemWithUserRole = [] ( const QString &itemText ) -> QStandardItem * {
        QStandardItem *ret = new QStandardItem( itemText );
        ret->setData( itemText, Qt::UserRole );
        return ret;
    };

    // Fill itemlist with items that are common to both wishlist additions and session plan additions
    auto populateItemList = [ &getItemWithUserRole, &itemList, &finalObjectName, &obj, &p, &smag ]() {
        itemList.clear();
        QStandardItem *keyItem = getItemWithUserRole( finalObjectName );
        keyItem->setData( QVariant::fromValue<void *>( static_cast<void *>( obj ) ), Qt::UserRole + 1 );
        itemList << keyItem // NOTE: The rest of the methods assume that the SkyObject pointer is available in the first column!
        << getItemWithUserRole( obj->translatedLongName() )
        << getItemWithUserRole( p.ra0().toHMSString() )
        << getItemWithUserRole( p.dec0().toDMSString() )
        << getItemWithUserRole( smag )
        << getItemWithUserRole( obj->typeName() );
    };

    //Insert object in the Wish List
    if( addToWishList ) {

        m_WishList.append( obj );
        m_CurrentObject = obj;

        //QString ra, dec;
        //ra = "";//p.ra().toHMSString();
        //dec = p.dec().toDMSString();

        populateItemList();
        // FIXME: Instead sort by a "clever" observability score, calculated as follows:
        //     - First sort by (max altitude) - (current altitude) rounded off to the nearest
        //     - Weight by declination - latitude (in the northern hemisphere, southern objects get higher precedence)
        //     - Demote objects in the hole
        SkyPoint p = obj->recomputeHorizontalCoords( KStarsDateTime::currentDateTimeUtc(), geo ); // Current => now
        itemList << m_altCostHelper( p );
        m_WishListModel->appendRow( itemList );

        //Note addition in statusbar
        KStars::Instance()->statusBar()->showMessage( i18n( "Added %1 to observing list.", finalObjectName ), 0 );
        ui->WishListView->resizeColumnsToContents();
        if( ! update ) slotSaveList();
    }
    //Insert object in the Session List
    if( session ){
        m_SessionList.append(obj);
        dt.setTime( TimeHash.value( finalObjectName, obj->transitTime( dt, geo ) ) );
        dms lst(geo->GSTtoLST( dt.gst() ));
        p.EquatorialToHorizontal( &lst, geo->lat() );

        QString ra, dec, time = "--", alt = "--", az = "--";

        QStandardItem *BestTime = new QStandardItem();
        /*if(obj->name() == "star" ) {
            ra = obj->ra0().toHMSString();
            dec = obj->dec0().toDMSString();
            BestTime->setData( QString( "--" ), Qt::DisplayRole );
        }
        else {*/
        BestTime->setData( TimeHash.value( finalObjectName, obj->transitTime( dt, geo ) ), Qt::DisplayRole );
        alt = p.alt().toDMSString();
        az = p.az().toDMSString();
        //}
        // TODO: Change the rest of the parameters to their appropriate datatypes.
        populateItemList();
        itemList << getItemWithUserRole( KSUtils::constNameToAbbrev( KStarsData::Instance()->skyComposite()->constellationBoundary()->constellationName( obj ) ) )
                 << BestTime
                 << getItemWithUserRole( alt )
                 << getItemWithUserRole( az );

        m_SessionModel->appendRow( itemList );
        //Adding an object should trigger the modified flag
        isModified = true;
        ui->SessionView->resizeColumnsToContents();
        //Note addition in statusbar
        KStars::Instance()->statusBar()->showMessage( i18n( "Added %1 to session list.", finalObjectName ), 0 );
    }
    setSaveImagesButton();
}

void ObservingList::slotRemoveObject( SkyObject *o, bool session, bool update ) {
    if( ! update ) { // EH?!
        if ( ! o )
            o = SkyMap::Instance()->clickedObject();
        else if( sessionView ) //else if is needed as clickedObject should not be removed from the session list.
            session = true;
    }

    // Remove from hash
    ImagePreviewHash.remove(o);

    QStandardItemModel *currentModel;

    int k;
    if (! session) {
        currentModel = m_WishListModel;
        k = obsList().indexOf( o );
    }
    else {
        currentModel = m_SessionModel;
        k = sessionList().indexOf( o );
    }

    if ( o == LogObject ) saveCurrentUserLog();
    //Remove row from the TableView model
    for ( int irow = 0; irow < currentModel->rowCount(); ++irow ) {
        QString name = currentModel->item(irow, 0)->text();
        if ( getObjectName(o) == name ) {
            currentModel->removeRow(irow);
            break;
        }
    }

    if( ! session ) {
        obsList().removeAt(k);
        ui->avt->removeAllPlotObjects();
        ui->WishListView->resizeColumnsToContents();
        if( ! update )
            slotSaveList();
    } else {
        if( ! update )
            TimeHash.remove( o->name() );
        sessionList().removeAt(k); //Remove from the session list
        isModified = true;         //Removing an object should trigger the modified flag
        ui->avt->removeAllPlotObjects();
        ui->SessionView->resizeColumnsToContents();
    }
}

void ObservingList::slotRemoveSelectedObjects() {

    //Find each object by name in the session list, and remove it
    //Go backwards so item alignment doesn't get screwed up as rows are removed.
    for ( int irow = getActiveModel()->rowCount()-1; irow >= 0; --irow ) {
        bool rowSelected;
        if ( sessionView )
            rowSelected = ui->SessionView->selectionModel()->isRowSelected( irow, QModelIndex() );
        else
            rowSelected = ui->WishListView->selectionModel()->isRowSelected( irow, QModelIndex() );

        if ( rowSelected ) {
            QModelIndex sortIndex, index;
            sortIndex = getActiveSortModel()->index( irow, 0 );
            index = getActiveSortModel()->mapToSource( sortIndex );
            SkyObject *o = static_cast<SkyObject *>( index.data( Qt::UserRole + 1 ).value<void *>() );
            Q_ASSERT( o );
            slotRemoveObject(o, sessionView);
        }
    }

    if (sessionView) {
        //we've removed all selected objects, so clear the selection
        ui->SessionView->selectionModel()->clear();
        //Update the lists in the Execute window as well
        KStarsData::Instance()->executeSession()->init();
    }

    setSaveImagesButton();
    ui->ImagePreview->setCursor( Qt::ArrowCursor );
}

void ObservingList::slotNewSelection() {
    bool found = false;
    singleSelection = false;
    noSelection = false;
    showScope = false;
    //ui->ImagePreview->clearPreview();
    //ui->ImagePreview->setPixmap(QPixmap());
    ui->ImagePreview->setCursor( Qt::ArrowCursor );
    QModelIndexList selectedItems;
    QString newName;
    SkyObject *o;
    QString labelText;
    ui->DeleteImage->setEnabled( false );

    selectedItems = getActiveSortModel()->mapSelectionToSource( getActiveView()->selectionModel()->selection() ).indexes();

    if ( selectedItems.size() == getActiveModel()->columnCount() ) {
        newName = selectedItems[0].data().toString();
        singleSelection = true;
        //Find the selected object in the SessionList,
        //then break the loop.  Now SessionList.current()
        //points to the new selected object (until now it was the previous object)
        foreach ( o,  getActiveList() ) {
            if ( getObjectName(o) == newName ) {
                found = true;
                break;
            }
        }
    }

    if( singleSelection ) {
        //Enable buttons
        ui->ImagePreview->setCursor( Qt::PointingHandCursor );
        #ifdef HAVE_INDI
            showScope = true;
        #endif
        if ( found )
        {
            m_CurrentObject = o;
            //QPoint pos(0,0);
            plot( o );
            //Change the m_currentImageFileName, DSS/SDSS Url to correspond to the new object
            setCurrentImage( o );
            ui->SearchImage->setEnabled( true );
            if ( newName != i18n( "star" ) ) {
                //Display the current object's user notes in the NotesEdit
                //First, save the last object's user log to disk, if necessary
                saveCurrentUserLog(); //uses LogObject, which is still the previous obj.
                //set LogObject to the new selected object
                LogObject = currentObject();
                ui->NotesLabel->setEnabled( true );
                ui->NotesEdit->setEnabled( true );
                ui->NotesLabel->setText( i18n( "observing notes for %1:", getObjectName(LogObject) ) );
                if ( LogObject->userLog().isEmpty() ) {
                    ui->NotesEdit->setPlainText( i18n( "Record here observation logs and/or data on %1.", getObjectName(LogObject)) );
                } else {
                    ui->NotesEdit->setPlainText( LogObject->userLog() );
                }
                if( sessionView ) {
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
                ui->SearchImage->setEnabled( false );
            }
            QString ImagePath = getCurrentImagePath();
            if( QFile::exists( getCurrentImagePath() ) )  {
                ;
            }
            else
                ImagePath = QString();
            if( !ImagePath.isEmpty() )
            {
                //If the image is present, show it!
                KSDssImage ksdi( ImagePath );
                KSDssImage::Metadata md = ksdi.getMetadata();
                //ui->ImagePreview->showPreview( QUrl::fromLocalFile( ksdi.getFileName() ) );
                if (ImagePreviewHash.contains(o) == false)
                    ImagePreviewHash[o] = QPixmap(ksdi.getFileName()).scaledToHeight(ui->ImagePreview->width());

                //ui->ImagePreview->setPixmap(QPixmap(ksdi.getFileName()).scaledToHeight(ui->ImagePreview->width()));
                ui->ImagePreview->setPixmap(ImagePreviewHash[o]);
                if( md.width != 0 ) {// FIXME: Need better test for meta data presence
                    ui->dssMetadataLabel->setText( i18n( "DSS Image metadata: \n Size: %1\' x %2\' \n Photometric band: %3 \n Version: %4",
                                                         QString::number( md.width ), QString::number( md.height ), QString() + md.band, md.version ) );
                }
                else
                    ui->dssMetadataLabel->setText( i18n( "No image info available." ) );
                ui->ImagePreview->show();
                ui->DeleteImage->setEnabled( true );
            }
            else
            {
                setDefaultImage();
                ui->dssMetadataLabel->setText( i18n( "No image available. Click on the placeholder image to download one." ) );
            }
            QString cname = KStarsData::Instance()->skyComposite()->constellationBoundary()->constellationName( o );
            if ( o->type() != SkyObject::CONSTELLATION ) {
                labelText = "<b>";
                if ( o->type() == SkyObject::PLANET )
                    labelText += o->translatedName();
                else
                    labelText += o->name();
                if ( std::isfinite( o->mag() ) && o->mag() <= 30. )
                    labelText += ":</b> " + i18nc("%1 magnitude of object, %2 type of sky object (planet, asteroid etc), %3 name of a constellation", "%1 mag %2 in %3", o->mag(), o->typeName().toLower(), cname );
                else
                    labelText += ":</b> " + i18nc("%1 type of sky object (planet, asteroid etc), %2 name of a constellation", "%1 in %2", o->typeName(), cname );
            }
        }
        else
        {
            setDefaultImage();
            qDebug() << "Object " << newName << " not found in list.";
        }
        ui->quickInfoLabel->setText( labelText );
    } else {
        if ( selectedItems.size() == 0 ) {//Nothing selected
            //Disable buttons
            noSelection = true;
            ui->NotesLabel->setText( i18n( "Select an object to record notes on it here:" ) );
            ui->NotesLabel->setEnabled( false );
            ui->NotesEdit->setEnabled( false );
            m_CurrentObject = 0;
            ui->TimeEdit->setEnabled( false );
            ui->SetTime->setEnabled( false );
            ui->SearchImage->setEnabled( false );
            //Clear the user log text box.
            saveCurrentUserLog();
            ui->NotesEdit->setPlainText("");
            //Clear the plot in the AVTPlotwidget
            ui->avt->removeAllPlotObjects();
        } else { //more than one object selected.
            ui->NotesLabel->setText( i18n( "Select a single object to record notes on it here:" ) );
            ui->NotesLabel->setEnabled( false );
            ui->NotesEdit->setEnabled( false );
            ui->TimeEdit->setEnabled( false );
            ui->SetTime->setEnabled( false );
            ui->SearchImage->setEnabled( false );
            m_CurrentObject = 0;
            //Clear the plot in the AVTPlotwidget
            ui->avt->removeAllPlotObjects();
            //Clear the user log text box.
            saveCurrentUserLog();
            ui->NotesEdit->setPlainText("");
            ui->quickInfoLabel->setText( QString() );
        }
    }
}

void ObservingList::slotCenterObject() {
    if ( getSelectedItems().size() == 1 ) {
        SkyMap::Instance()->setClickedObject( currentObject() );
        SkyMap::Instance()->setClickedPoint( currentObject() );
        SkyMap::Instance()->slotCenter();
    }
}

void ObservingList::slotSlewToObject()
{
#ifdef HAVE_INDI

    if (INDIListener::Instance()->size() == 0)
    {
        KMessageBox::sorry(0, i18n("KStars did not find any active telescopes."));
        return;
    }

    foreach(ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *bd = gd->getBaseDevice();

        if (gd->getType() != KSTARS_TELESCOPE)
            continue;

        if (bd == NULL)
            continue;

        if (bd->isConnected() == false)
        {
            KMessageBox::error(0, i18n("Telescope %1 is offline. Please connect and retry again.", gd->getDeviceName()));
            return;
        }


        ISD::GDSetCommand SlewCMD(INDI_SWITCH, "ON_COORD_SET", "TRACK", ISS_ON, this);

        gd->setProperty(&SlewCMD);
        gd->runCommand(INDI_SEND_COORDS, currentObject());

        return;

    }

    KMessageBox::sorry(0, i18n("KStars did not find any active telescopes."));

#endif
}

void ObservingList::slotAddToEkosScheduler()
{
    #ifdef HAVE_INDI
    KStars::Instance()->ekosManager()->addObjectToScheduler(currentObject());
    #endif

}

//FIXME: This will open multiple Detail windows for each object;
//Should have one window whose target object changes with selection
void ObservingList::slotDetails() {
    if ( currentObject() ) {
        QPointer<DetailDialog> dd = new DetailDialog( currentObject(), KStarsData::Instance()->ut(), geo, KStars::Instance() );
        dd->exec();
        delete dd;
    }
}

void ObservingList::slotWUT() {
    KStarsDateTime lt = dt;
    lt.setTime( QTime(8,0,0) );
    QPointer<WUTDialog> w = new WUTDialog( KStars::Instance(), sessionView, geo, lt );
    w->exec();
    delete w;
}

void ObservingList::slotAddToSession() {
    Q_ASSERT( ! sessionView );
    if ( getSelectedItems().size() ) {
        foreach ( const QModelIndex &i, getSelectedItems() ) {
            foreach ( SkyObject *o, obsList() )
                if ( getObjectName(o) == i.data().toString() )
                    slotAddObject( o, true );

        }
    }
}

void ObservingList::slotFind() {
   QPointer<FindDialog> fd = new FindDialog( KStars::Instance() );
   if ( fd->exec() == QDialog::Accepted ) {
       SkyObject *o = fd->targetObject();
       if( o != 0 ) {
           slotAddObject( o, sessionView );
       }
   }
   delete fd;
}

void ObservingList::slotEyepieceView() {
    KStars::Instance()->slotEyepieceView( currentObject(), getCurrentImagePath() );
}

void ObservingList::slotAVT() {
    QModelIndexList selectedItems;
    // TODO: Think and see if there's a more effecient way to do this. I can't seem to think of any, but this code looks like it could be improved. - Akarsh
    selectedItems = ( sessionView ? m_SessionSortModel->mapSelectionToSource( ui->SessionView->selectionModel()->selection() ).indexes() : m_WishListSortModel->mapSelectionToSource( ui->WishListView->selectionModel()->selection() ).indexes() );

    if ( selectedItems.size() ) {
        QPointer<AltVsTime> avt = new AltVsTime( KStars::Instance() );
        foreach ( const QModelIndex &i, selectedItems ) {
            if ( i.column() == 0 ) {
                SkyObject *o = static_cast<SkyObject *>( i.data( Qt::UserRole + 1 ).value<void *>() );
                Q_ASSERT( o );
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
    saveCurrentUserLog();
    ui->avt->removeAllPlotObjects();
    slotNewSelection();
    saveCurrentList();
    hide();
}

void ObservingList::saveCurrentUserLog() {
    if ( ! ui->NotesEdit->toPlainText().isEmpty() &&
            ui->NotesEdit->toPlainText() !=
            i18n( "Record here observation logs and/or data on %1.", getObjectName(LogObject) ) ) {
        LogObject->saveUserLog( ui->NotesEdit->toPlainText() );
        ui->NotesEdit->clear();
        ui->NotesLabel->setText( i18n( "Observing notes for object:" ) );
        LogObject = NULL;
    }
}

void ObservingList::slotOpenList()
{
    QUrl fileURL =QFileDialog::getOpenFileUrl(KStars::Instance(), i18n("Open Observing List"), QUrl(), "KStars Observing List (*.obslist)" );
    QFile f;

    if ( fileURL.isValid() )
    {

        f.setFileName( fileURL.toLocalFile() );
        //FIXME do we still need to do this?
        /*
        if ( ! fileURL.isLocalFile() ) {
            //Save remote list to a temporary local file
            QTemporaryFile tmpfile;
            tmpfile.setAutoRemove(false);
            tmpfile.open();
            m_listFileName = tmpfile.fileName();
            if( KIO::NetAccess::download( fileURL, m_listFileName, this ) )
                f.setFileName( m_listFileName );

        } else {
            m_listFileName = fileURL.toLocalFile();
            f.setFileName( m_listFileName );
        }
        */

        if ( ! f.open( QIODevice::ReadOnly) ) {
            QString message = i18n( "Could not open file %1", f.fileName() );
            KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
            return;
        }
        saveCurrentList();//See if the current list needs to be saved before opening the new one
        ui->tabWidget->setCurrentIndex(1);
        slotChangeTab(1);
        sessionList().clear();
        TimeHash.clear();
        m_CurrentObject = 0;
        m_SessionModel->removeRows( 0, m_SessionModel->rowCount() );
        //First line is the name of the list. The rest of the file is
        //object names, one per line. With the TimeHash value if present
        QTextStream istream( &f );
        QString input;
        input = istream.readAll();
        OAL::Log logObject;
        logObject.readBegin( input );
        //Set the New TimeHash
        TimeHash = logObject.timeHash();
        geo = logObject.geoLocation();
        dt = logObject.dateTime();
        foreach( SkyObject *o, *( logObject.targetList() ) )
        slotAddObject( o, true );
        //Update the location and user set times from file
        slotUpdate();
        //Newly-opened list should not trigger isModified flag
        isModified = false;
        f.close();
    } else if ( ! fileURL.toLocalFile().isEmpty() ) {
        KMessageBox::sorry( 0 , i18n( "The specified file is invalid" ) );
    }
}

void ObservingList::saveCurrentList() {
    //Before loading a new list, do we need to save the current one?
    //Assume that if the list is empty, then there's no need to save
    if ( sessionList().size() ) {
        if ( isModified ) {
            QString message = i18n( "Do you want to save the current session?" );
            if ( KMessageBox::questionYesNo( this, message,
                                             i18n( "Save Current session?" ), KStandardGuiItem::save(), KStandardGuiItem::discard() ) == KMessageBox::Yes )
                slotSaveSession();
        }
    }
}

void ObservingList::slotSaveSessionAs(bool nativeSave) {
    if (sessionList().isEmpty())
       return;

    QUrl fileURL = QFileDialog::getSaveFileUrl(KStars::Instance(), i18n("Save Observing List"), QUrl(), "KStars Observing List (*.obslist)" );
    if ( fileURL.isValid() ) {
        m_listFileName = fileURL.toLocalFile();
        slotSaveSession(nativeSave);
    }
}

void ObservingList::slotSaveList() {
    QFile f;
    // FIXME: Move wishlist into a database.
    // TODO: Support multiple wishlists.
    f.setFileName( KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "wishlist.obslist" ) ;
    if ( ! f.open( QIODevice::WriteOnly ) ) {
        qDebug() << "Cannot write list to  file"; // TODO: This should be presented as a message box to the user
        return;
    }
    QTextStream ostream( &f );
    foreach ( SkyObject* o, obsList() ) {
        if ( !o ) {
            qWarning() << "Null entry in observing wishlist! Skipping!";
            continue;
        }
        if ( o->name() == "star" ) {
            //ostream << o->name() << "  " << o->ra0().Hours() << "  " << o->dec0().Degrees() << endl;
            ostream << getObjectName(o, false) << endl;
        } else if ( o->type() == SkyObject::STAR ) {
            StarObject *s = (StarObject*)o;
            if ( s->name() == s->gname() )
                ostream << s->name2() << endl;
            else
                ostream << s->name() << endl;
        } else {
            ostream << o->name() << endl;
        }
    }
    f.close();
}

void ObservingList::slotLoadWishList() {
    QFile f;
    f.setFileName( KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "wishlist.obslist" ) ;
    if ( ! f.open( QIODevice::ReadOnly) ) {
       qDebug() << "No WishList Saved yet";
       return;
    }
    QTextStream istream( &f );
    QString line;
    while ( ! istream.atEnd() ) {
        line = istream.readLine();
        //If the object is named "star", add it by coordinates
        SkyObject *o;
        /*if ( line.startsWith( QLatin1String( "star" ) ) ) {
            QStringList fields = line.split( ' ', QString::SkipEmptyParts );
            dms ra = dms::fromString( fields[1], false ); //false = hours
            dms dc = dms::fromString( fields[2], true );  //true  = degrees
            SkyPoint p( ra, dc );
            double maxrad = 1000.0/Options::zoomFactor();
            o = ks->data()->skyComposite()->starNearest( &p, maxrad );
        }
        else {*/
        o = KStarsData::Instance()->objectNamed( line );
        //}
        //If we haven't identified the object, try interpreting the
        //name as a star's genetive name (with ascii letters)
        if ( ! o ) o = KStarsData::Instance()->skyComposite()->findStarByGenetiveName( line );
        if ( o ) slotAddObject( o, false, true );
    }
    f.close();
}

void ObservingList::slotSaveSession(bool nativeSave) {
    if (sessionList().isEmpty())
    {
        KMessageBox::error(0, i18n("Cannot save an empty session list!"));
        return;
    }

    if ( m_listFileName.isEmpty() ) {
        slotSaveSessionAs(nativeSave);
        return;
    }
    QFile f( m_listFileName );
    if( ! f.open( QIODevice::WriteOnly ) ) {
        QString message = i18n( "Could not open file %1.  Try a different filename?", f.fileName() );
        if ( KMessageBox::warningYesNo( 0, message, i18n( "Could Not Open File" ), KGuiItem(i18n("Try Different")), KGuiItem(i18n("Do Not Try")) ) == KMessageBox::Yes ) {
            m_listFileName.clear();
            slotSaveSessionAs(nativeSave);
        }
    return;
    }
    QTextStream ostream( &f );
    OAL::Log log;
    ostream<< log.writeLog( nativeSave );
    f.close();
    isModified = false;//We've saved the session, so reset the modified flag.
}

void ObservingList::slotWizard() {
    QPointer<ObsListWizard> wizard = new ObsListWizard( KStars::Instance() );
    if ( wizard->exec() == QDialog::Accepted ) {
        foreach ( SkyObject *o, wizard->obsList() ) {
            slotAddObject( o );
        }
    }
    delete wizard;
}

void ObservingList::plot( SkyObject *o ) {
    if( !o )
        return;
    float DayOffset = 0;
    if( TimeHash.value( o->name(), o->transitTime( dt, geo ) ).hour() > 12 )
        DayOffset = 1;

    QDateTime midnight = QDateTime(dt.date(), QTime());
    KStarsDateTime ut = geo->LTtoUT(midnight);
    double h1 = geo->GSTtoLST( ut.gst() ).Hours();
    if ( h1 > 12.0 )
        h1 -= 24.0;

    ui->avt->setSecondaryLimits( h1, h1 + 24.0, -90.0, 90.0 );
    ksal->setLocation(geo);
    ksal->setDate( &ut );
    ui->avt->setGeoLocation( geo );
    ui->avt->setSunRiseSetTimes( ksal->getSunRise(), ksal->getSunSet() );
    ui->avt->setDawnDuskTimes( ksal->getDawnAstronomicalTwilight(), ksal->getDuskAstronomicalTwilight() );
    ui->avt->setMinMaxSunAlt( ksal->getSunMinAlt(), ksal->getSunMaxAlt() );
    ui->avt->setMoonRiseSetTimes( ksal->getMoonRise(), ksal->getMoonSet() );
    ui->avt->setMoonIllum( ksal->getMoonIllum() );
    ui->avt->update();
    KPlotObject *po = new KPlotObject( Qt::white, KPlotObject::Lines, 2.0 );
    for ( double h = -12.0; h <= 12.0; h += 0.5 ) {
        po->addPoint( h, findAltitude( o, ( h + DayOffset * 24.0 ) ) );
    }
    ui->avt->removeAllPlotObjects();
    ui->avt->addPlotObject( po );
}

double ObservingList::findAltitude( SkyPoint *p, double hour ) {

    // Jasem 2015-09-05 Using correct procedure to find altitude
    SkyPoint sp = *p; // make a copy
    QDateTime midnight = QDateTime(dt.date(), QTime());
    KStarsDateTime ut = geo->LTtoUT(midnight);
    KStarsDateTime targetDateTime = ut.addSecs( hour*3600.0 );
    dms LST = geo->GSTtoLST( targetDateTime.gst() );
    sp.EquatorialToHorizontal( &LST, geo->lat() );
    return sp.alt().Degrees();

}

void ObservingList::slotToggleSize() {
    if ( isLarge() ) {
        ui->MiniButton->setIcon( QIcon::fromTheme("view-fullscreen", QIcon(":/icons/breeze/default/view-fullscreen.svg")) );
        //Abbreviate text on each button
        ui->FindButton->setText( "" );
        ui->FindButton->setIcon( QIcon::fromTheme("edit-find", QIcon(":/icons/breeze/default/edit-find.svg")) );
        ui->WUTButton->setText( i18nc( "Abbreviation of What's Up Tonight", "WUT" ) );
        ui->saveImages->setText( "" );
        ui->DeleteAllImages->setText( "" );
        ui->saveImages->setIcon( QIcon::fromTheme( "download", QIcon(":/icons/breeze/default/download.svg")) );
        ui->DeleteAllImages->setIcon( QIcon::fromTheme( "edit-delete", QIcon(":/icons/breeze/default/edit-delete.svg")) );
        ui->refLabel->setText( i18nc( "Abbreviation for Reference Images:", "RefImg:" ) );
        ui->addLabel->setText( i18nc( "Add objects to a list", "Add:" ) );
        //Hide columns 1-5
        ui->WishListView->hideColumn(1);
        ui->WishListView->hideColumn(2);
        ui->WishListView->hideColumn(3);
        ui->WishListView->hideColumn(4);
        ui->WishListView->hideColumn(5);
        //Hide the headers
        ui->WishListView->horizontalHeader()->hide();
        ui->WishListView->verticalHeader()->hide();
        //Hide Observing notes
        ui->NotesLabel->hide();
        ui->NotesEdit->hide();
        //ui->kseparator->hide();
        ui->avt->hide();
        ui->dssMetadataLabel->hide();
        ui->setMinimumSize(320, 600);
        //Set the width of the Table to be the width of 5 toolbar buttons,
        //or the width of column 1, whichever is larger
        /*
        int w = 5*ui->MiniButton->width();
        if ( ui->WishListView->columnWidth(0) > w ) {
            w = ui->WishListView->columnWidth(0);
        } else {
            ui->WishListView->setColumnWidth(0, w);
        }
        int left, right, top, bottom;
        ui->layout()->getContentsMargins( &left, &top, &right, &bottom );
        resize( w + left + right, height() );
        */
        bIsLarge = false;
        ui->resize( 400, ui->height() );
        adjustSize();
        this->resize( 400, this->height() );
        update();
    } else {
        ui->MiniButton->setIcon( QIcon::fromTheme( "view-restore", QIcon(":/icons/breeze/default/view-restore.svg")) );
        //Show columns 1-5
        ui->WishListView->showColumn(1);
        ui->WishListView->showColumn(2);
        ui->WishListView->showColumn(3);
        ui->WishListView->showColumn(4);
        ui->WishListView->showColumn(5);
        //Show the horizontal header
        ui->WishListView->horizontalHeader()->show();
        //Expand text on each button
        ui->FindButton->setText( i18n( "Find &Object") );
        ui->saveImages->setText( i18n( "Download all Images" ) );
        ui->DeleteAllImages->setText( i18n( "Delete all Images" ) );
        ui->FindButton->setIcon( QIcon() );
        ui->saveImages->setIcon( QIcon() );
        ui->DeleteAllImages->setIcon( QIcon() );
        ui->WUTButton->setText( i18n( "What's up Tonight tool" ) );
        ui->refLabel->setText( i18nc( "Abbreviation for Reference Images:", "Reference Images:" ) );
        ui->addLabel->setText( i18nc( "Add objects to a list", "Adding Objects:" ) );
        //Show Observing notes
        ui->NotesLabel->show();
        ui->NotesEdit->show();
        //ui->kseparator->show();
        ui->setMinimumSize(837, 650);
        ui->avt->show();
        ui->dssMetadataLabel->show();
        adjustSize();
        update();
        bIsLarge = true;
    }
}

void ObservingList::slotChangeTab(int index) {
    noSelection = true;
    saveCurrentUserLog();
    ui->NotesLabel->setText( i18n( "Select an object to record notes on it here:" ) );
    ui->NotesLabel->setEnabled( false );
    ui->NotesEdit->setEnabled( false );
    ui->TimeEdit->setEnabled( false );
    ui->SetTime->setEnabled( false );
    ui->SearchImage->setEnabled( false );
    ui->DeleteImage->setEnabled( false );
    m_CurrentObject = 0;
    sessionView     = index != 0;
    setSaveImagesButton();
    ui->WizardButton->setEnabled( ! sessionView );//wizard adds only to the Wish List
    ui->OALExport->setEnabled( sessionView );
    //Clear the selection in the Tables
    ui->WishListView->clearSelection();
    ui->SessionView->clearSelection();
    //Clear the user log text box.
    saveCurrentUserLog();
    ui->NotesEdit->setPlainText("");
    ui->avt->removeAllPlotObjects();
}

void ObservingList::slotLocation() {
    QPointer<LocationDialog> ld = new LocationDialog( this );
    if ( ld->exec() == QDialog::Accepted ) {
        geo = ld->selectedCity();
        ui->SetLocation -> setText( geo -> fullName() );
    }
    delete ld;
}

void ObservingList::slotUpdate() {
    dt.setDate( ui->DateEdit->date() );
    ui->avt->removeAllPlotObjects();
    //Creating a copy of the lists, we can't use the original lists as they'll keep getting modified as the loop iterates
    QList<SkyObject*> _obsList=m_WishList, _SessionList=m_SessionList;
    foreach ( SkyObject *o, _obsList ) {
        if( o->name() != "star" ) {
            slotRemoveObject( o, false, true );
            slotAddObject( o, false, true );
        }
    }
    foreach ( SkyObject *obj, _SessionList ) {
        if( obj->name() != "star" ) {
            slotRemoveObject( obj, true, true );
            slotAddObject( obj, true, true );
        }
    }
}

void ObservingList::slotSetTime() {
    SkyObject *o = currentObject();
    slotRemoveObject( o, true );
    TimeHash [o->name()] = ui->TimeEdit->time();
    slotAddObject( o, true, true );
}

void ObservingList::slotCustomDSS() {
    ui->SearchImage->setEnabled( false );
    //ui->ImagePreview->clearPreview();
    ui->ImagePreview->setPixmap(QPixmap());

    KSDssImage::Metadata md;
    bool ok = true;

    int width = QInputDialog::getInt(this, i18n("Customized DSS Download"), i18n("Specify image width (arcminutes): "), 15, 15, 75, 1, &ok );
    int height = QInputDialog::getInt(this, i18n("Customized DSS Download"), i18n("Specify image height (arcminutes): "), 15, 15, 75, 1, &ok );
    QStringList strList = ( QStringList() << "poss2ukstu_blue" << "poss2ukstu_red" << "poss2ukstu_ir" << "poss1_blue" << "poss1_red" << "quickv" << "all" );
    QString version = QInputDialog::getItem(this, i18n("Customized DSS Download"), i18n("Specify version: "), strList, 0, false, &ok );

    QUrl srcUrl( KSDssDownloader::getDSSURL( currentObject()->ra0(), currentObject()->dec0(), width, height, "gif", version, &md ) );

    delete m_dl;
    m_dl = new KSDssDownloader();
    connect( m_dl, SIGNAL ( downloadComplete( bool ) ), SLOT ( downloadReady( bool ) ) );
    m_dl->startSingleDownload( srcUrl, getCurrentImagePath(), md );

}

void ObservingList::slotGetImage( bool _dss, const SkyObject *o ) {
    dss = _dss;
    Q_ASSERT( !o || o == currentObject() ); // FIXME: Meaningless to operate on m_currentImageFileName unless o == currentObject()!
    if( !o )
        o = currentObject();
    ui->SearchImage->setEnabled( false );
    //ui->ImagePreview->clearPreview();
    ui->ImagePreview->setPixmap(QPixmap());
    setCurrentImage( o );
    QString currentImagePath = getCurrentImagePath();
    if ( QFile::exists( currentImagePath ) )
        QFile::remove( currentImagePath ) ;
    //QUrl url;
    dss = true;
    qWarning() << "FIXME: Removed support for SDSS. Until reintroduction, we will supply a DSS image";
    KSDssDownloader *dler = new KSDssDownloader( o, currentImagePath );
    connect( dler, SIGNAL( downloadComplete( bool ) ), SLOT( downloadReady( bool ) ) );
}

void ObservingList::downloadReady( bool success ) {
    // set downloadJob to 0, but don't delete it - the job will be deleted automatically
    //    downloadJob = 0;

    delete m_dl; m_dl = 0; // required if we came from slotCustomDSS; does nothing otherwise

    if( !success ) {
        KMessageBox::sorry(0, i18n( "Failed to download DSS/SDSS image!" ) );
    }
    else {

        /*
          if( QFile( KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QDir::separator() + m_currentImageFileName ).size() > 13000)
          //The default image is around 8689 bytes
        */
        //ui->ImagePreview->showPreview( QUrl::fromLocalFile( getCurrentImagePath() ) );
        ui->ImagePreview->setPixmap(QPixmap( getCurrentImagePath() ).scaledToHeight(ui->ImagePreview->width()));
        saveThumbImage();
        ui->ImagePreview->show();
        ui->ImagePreview->setCursor( Qt::PointingHandCursor );
        ui->DeleteImage->setEnabled( true );
    }
    /*
    // FIXME: Implement a priority order SDSS > DSS in the DSS downloader
    else if( ! dss )
        slotGetImage( true );
    */
}

void ObservingList::setCurrentImage( const SkyObject *o  ) {
    QString sanitizedName = o->name().remove(' ').remove( '\'' ).remove( '\"' );
    m_currentImageFileName = "Image_" +  sanitizedName;
    ThumbImage = "thumb-" + sanitizedName.toLower() + ".png";
    if( o->name() == "star" ) {
        QString RAString( o->ra0().toHMSString() );
        QString DecString( o->dec0().toDMSString() );
        m_currentImageFileName = "Image_J" + RAString.remove(' ').remove( ':' ) + DecString.remove(' ').remove( ':' ); // Note: Changed naming convention to standard 2016-08-25 asimha; old images shall have to be re-downloaded.
        // Unnecessary complication below:
        // QChar decsgn = ( (o->dec0().Degrees() < 0.0 ) ? '-' : '+' );
        // m_currentImageFileName = m_currentImageFileName.remove('+').remove('-') + decsgn;
    }
    QString imagePath;
    if ( QFile::exists( imagePath = getCurrentImagePath() ) ) { // New convention -- append filename extension so file is usable on Windows etc.
        QFile::rename( imagePath, imagePath + ".png" );
    }
    m_currentImageFileName += ".png";
    // DSSUrl = KSDssDownloader::getDSSURL( o );
    // QString UrlPrefix("http://casjobs.sdss.org/ImgCutoutDR6/getjpeg.aspx?"); // FIXME: Upgrade to use SDSS Data Release 9 / 10. DR6 is well outdated.
    // QString UrlSuffix("&scale=1.0&width=600&height=600&opt=GST&query=SR(10,20)");

    // QString RA, Dec;
    // RA = RA.sprintf( "ra=%f", o->ra0().Degrees() );
    // Dec = Dec.sprintf( "&dec=%f", o->dec0().Degrees() );

    // SDSSUrl = UrlPrefix + RA + Dec + UrlSuffix;
}

QString ObservingList::getCurrentImagePath() {
    QString currentImagePath = KSPaths::locate( QStandardPaths::GenericDataLocation , m_currentImageFileName );
    if ( QFile::exists( currentImagePath ) ) {
        return currentImagePath;
    }
    else
        return ( KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + m_currentImageFileName );

}

void ObservingList::slotSaveAllImages() {
    ui->SearchImage->setEnabled( false );
    ui->DeleteImage->setEnabled( false );
    m_CurrentObject = 0;
    //Clear the selection in the Tables
    ui->WishListView->clearSelection();
    ui->SessionView->clearSelection();

    foreach( SkyObject *o, getActiveList() ) {
        if( !o )
            continue; // FIXME: Why would we have null objects? But appears that we do.
        setCurrentImage( o );
        QString img( getCurrentImagePath()  );
        //        QUrl url( ( Options::obsListPreferDSS() ) ? DSSUrl : SDSSUrl ); // FIXME: We have removed SDSS support!
        QUrl url( KSDssDownloader::getDSSURL( o ) );
        if( ! o->isSolarSystem() )//TODO find a way for adding support for solar system images
            saveImage( url, img, o );
    }
}

void ObservingList::saveImage( QUrl /*url*/, QString /*filename*/, const SkyObject *o )
{

    if( !o )
        o = currentObject();
    Q_ASSERT( o );
    if( ! QFile::exists( getCurrentImagePath()  ) )
    {
        // Call the DSS downloader
        slotGetImage( true, o );
    }

}

void ObservingList::slotImageViewer()
{
    QPointer<ImageViewer> iv;
    QString currentImagePath = getCurrentImagePath();
    if( QFile::exists( currentImagePath ) )
    {
        QUrl url = QUrl::fromLocalFile( currentImagePath );
        iv = new ImageViewer( url );
    }

    if( iv )
        iv->show();
}

void ObservingList::slotDeleteAllImages() {
    if( KMessageBox::warningYesNo( 0, i18n( "This will delete all saved images. Are you sure you want to do this?" ), i18n( "Delete All Images" ) ) == KMessageBox::No )
        return;
    ui->ImagePreview->setCursor( Qt::ArrowCursor );
    ui->SearchImage->setEnabled( false );
    ui->DeleteImage->setEnabled( false );
    m_CurrentObject = 0;
    //Clear the selection in the Tables
    ui->WishListView->clearSelection();
    ui->SessionView->clearSelection();
    //ui->ImagePreview->clearPreview();
    ui->ImagePreview->setPixmap(QPixmap());
    QDirIterator iterator( KSPaths::writableLocation(QStandardPaths::GenericDataLocation)) ;
    while( iterator.hasNext() )
    {
        // TODO: Probably, there should be a different directory for cached images in the observing list.
        if( iterator.fileName().contains( "Image" ) && ( ! iterator.fileName().contains( "dat" ) ) && ( ! iterator.fileName().contains( "obslist" ) ) ) {
            QFile file( iterator.filePath() );
            file.remove();
        }
        iterator.next();
    }
}

void ObservingList::setSaveImagesButton() {
    ui->saveImages->setEnabled( ! getActiveList().isEmpty() );
}

// FIXME: Is there a reason to implement these as an event filter,
// instead of as a signal-slot connection? Shouldn't we just use slots
// to subscribe to various events from the Table / Session view?
//
// NOTE: ui->ImagePreview is a QLabel, which has no clicked() event or
// public mouseReleaseEvent(), so eventFilter makes sense.
bool ObservingList::eventFilter( QObject *obj, QEvent *event ) {
    if( obj == ui->ImagePreview ) {
        if( event->type() == QEvent::MouseButtonRelease ) {
            if( currentObject() ) {
                if( !QFile::exists( getCurrentImagePath() ) ) {
                    if( ! currentObject()->isSolarSystem() )
                        slotGetImage( Options::obsListPreferDSS() );
                    else
                        slotSearchImage();
                }
                else
                    slotImageViewer();
            }
            return true;
        }
    }
    if( obj == ui->WishListView->viewport() || obj == ui->SessionView->viewport() ) {
        bool sessionViewEvent = ( obj == ui->SessionView->viewport() );

        if( event->type() == QEvent::MouseButtonRelease  ) { // Mouse button release event
            QMouseEvent *mouseEvent = static_cast<QMouseEvent* >(event);
            QPoint pos( mouseEvent->globalX() , mouseEvent->globalY() );

            if( mouseEvent->button() == Qt::RightButton ) {
                if( !noSelection ) {
                    pmenu->initPopupMenu( sessionViewEvent, !singleSelection, showScope );
                    pmenu->popup( pos );
                }
                return true;
            }
        }
    }

    if( obj == ui->WishListView || obj == ui->SessionView )
    {
        if (event->type() == QEvent::KeyPress)
        {
                QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
                if (keyEvent->key() == Qt::Key_Delete) {
                    slotRemoveSelectedObjects();
                    return true;
                }
        }

    }

    return false;
}

void ObservingList::slotSearchImage() {
    QPixmap *pm = new QPixmap;
    QPointer<ThumbnailPicker> tp = new ThumbnailPicker( currentObject(), *pm, this, 600, 600, i18n( "Image Chooser" ) );
    if ( tp->exec() == QDialog::Accepted )
    {
        QString currentImagePath = getCurrentImagePath();
        QFile f( currentImagePath );

        //If a real image was set, save it.
        if ( tp->imageFound() ) {
            tp->image()->save( f.fileName(), "PNG" );
            //ui->ImagePreview->showPreview( QUrl::fromLocalFile( f.fileName() ) );
            ui->ImagePreview->setPixmap(QPixmap(f.fileName() ).scaledToHeight(ui->ImagePreview->width()));
            saveThumbImage();
            slotNewSelection();
        }
    }
    delete tp;
}

void ObservingList::slotDeleteCurrentImage() {
    QFile::remove( getCurrentImagePath() );
    ImagePreviewHash.remove(m_CurrentObject);
    slotNewSelection();
}

void ObservingList::saveThumbImage() {
    if( ! QFile::exists( KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + ThumbImage ) )  {
        QImage img( getCurrentImagePath() );
        img = img.scaled( 200, 200, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
        img.save( KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + ThumbImage ) ;
    }
}

QString ObservingList::getTime( const SkyObject *o ) const {
    return TimeHash.value( o->name(), QTime( 30,0,0 ) ).toString( "h:mm:ss AP" );
}

QTime ObservingList::scheduledTime( SkyObject *o ) const {
    return TimeHash.value( o->name(), o->transitTime( dt, geo ) );
}

void ObservingList::setTime( const SkyObject *o, QTime t ) {
    TimeHash.insert( o->name(), t);
}


void ObservingList::slotOALExport() {
    slotSaveSessionAs(false);
}

void ObservingList::slotAddVisibleObj() {
    KStarsDateTime lt = dt;
    lt.setTime( QTime(8,0,0) );
    QPointer<WUTDialog> w = new WUTDialog( KStars::Instance(), sessionView, geo, lt );
    w->init();
    QModelIndexList selectedItems;
    selectedItems = m_WishListSortModel->mapSelectionToSource( ui->WishListView->selectionModel()->selection() ).indexes();
    if ( selectedItems.size() )
        foreach ( const QModelIndex &i, selectedItems ) {
            foreach ( SkyObject *o, obsList() )
                if ( getObjectName(o) == i.data().toString() && w->checkVisibility( o ) )
                    slotAddObject( o, true );
        }
    delete w;
}

SkyObject* ObservingList::findObjectByName( QString name ) {
    foreach( SkyObject* o, sessionList() )
        if( getObjectName(o, false) == name )
            return o;
    return NULL;
}

void ObservingList::selectObject( const SkyObject *o ) {
    ui->tabWidget->setCurrentIndex( 1 );
    ui->SessionView->selectionModel()->clear();
    for ( int irow = m_SessionModel->rowCount()-1; irow >= 0; --irow ) {
        QModelIndex mSortIndex = m_SessionSortModel->index( irow, 0 );
        QModelIndex mIndex = m_SessionSortModel->mapToSource( mSortIndex );
        int idxrow = mIndex.row();
        if(  m_SessionModel->item(idxrow, 0)->text() == getObjectName(o) )
            ui->SessionView->selectRow( idxrow );
        slotNewSelection();
    }
}

void ObservingList::setDefaultImage()
{
    ui->ImagePreview->setPixmap(m_NoImagePixmap);
}

QString ObservingList::getObjectName(const SkyObject *o, bool translated)
{
    QString finalObjectName;
    if( o->name() == "star" )
    {
        StarObject *s = (StarObject *)o;

        // JM: Enable HD Index stars to be added to the observing list.
        if( s->getHDIndex() != 0 )
                finalObjectName = QString("HD %1" ).arg( QString::number( s->getHDIndex() ) );
    }
    else
         finalObjectName = translated ? o->translatedName() : o->name();

    return finalObjectName;

}


void ObservingList::slotUpdateAltitudes() {
    // FIXME: Update upon gaining visibility, do not update when not visible
    KStarsDateTime now = KStarsDateTime::currentDateTimeUtc();
    qDebug() << "Updating altitudes in observation planner @ JD - J2000 = " << double( now.djd() - J2000 );
    for ( int irow = m_WishListModel->rowCount() - 1; irow >= 0; --irow ) {
        QModelIndex idx = m_WishListSortModel->mapToSource( m_WishListSortModel->index( irow, 0 ) );
        SkyObject *o = static_cast<SkyObject *>( idx.data( Qt::UserRole + 1 ).value<void *>() );
        Q_ASSERT( o );
        SkyPoint p = o->recomputeHorizontalCoords( now, geo );
        idx = m_WishListSortModel->mapToSource( m_WishListSortModel->index( irow, m_WishListSortModel->columnCount() - 1 ) );
        QStandardItem *replacement = m_altCostHelper( p );
        m_WishListModel->setData( idx, replacement->data( Qt::DisplayRole ), Qt::DisplayRole  );
        m_WishListModel->setData( idx, replacement->data( Qt::UserRole ), Qt::UserRole );
        delete replacement;
    }
    emit m_WishListModel->dataChanged( m_WishListModel->index( 0, m_WishListModel->columnCount() - 1 ),
                                       m_WishListModel->index( m_WishListModel->rowCount() - 1, m_WishListModel->columnCount() - 1 ) );
}
