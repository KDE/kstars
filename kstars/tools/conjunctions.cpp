/***************************************************************************
                          conjunctions.cpp  -  Conjunctions Tool
                             -------------------
    begin                : Sun 20th Apr 2008
    copyright            : (C) 2008 Akarsh Simha
    email                : akarshsimha@gmail.com
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 * Much of the code here is taken from Pablo de Vicente's                  *
 * modcalcplanets.cpp                                                      *
 *                                                                         *
 ***************************************************************************/

#include "conjunctions.h"

#include <QProgressDialog>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QPointer>
#include <QFileDialog>

#include <KLocalizedString>
#include <kmessagebox.h>

#include "ksconjunct.h"
#include "geolocation.h"
#include "dialogs/locationdialog.h"
#include "dms.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "widgets/dmsbox.h"
#include "dialogs/finddialog.h"
#include "skyobjects/kssun.h"
#include "skyobjects/ksplanet.h"
#include "skyobjects/ksplanetbase.h"
#include "skyobjects/ksmoon.h"
#include "skyobjects/kspluto.h"
#include "skyobjects/kscomet.h"
#include "skyobjects/ksasteroid.h"
#include "skycomponents/skymapcomposite.h"
#include "skymap.h"

ConjunctionsTool::ConjunctionsTool(QWidget *parentSplit)
    : QFrame(parentSplit), Object1( 0 ), Object2( 0 ) {

    setupUi(this);

    KStarsData *kd = KStarsData::Instance();
    KStarsDateTime dtStart ( KStarsDateTime::currentDateTime() );
    KStarsDateTime dtStop ( dtStart.djd() + 365.24 ); // TODO: Refine

    startDate -> setDateTime( dtStart.dateTime() );
    stopDate -> setDateTime( dtStop.dateTime() );

    geoPlace = kd -> geo();
    LocationButton -> setText( geoPlace -> fullName() );

    // Init filter type combobox
    FilterTypeComboBox->addItem( xi18n ("Single Object...") );
    FilterTypeComboBox->addItem( xi18n ("Any") );
    FilterTypeComboBox->addItem( xi18n ("Stars") );
    FilterTypeComboBox->addItem( xi18n ("Solar System") );
    FilterTypeComboBox->addItem( xi18n ("Planets") );
    FilterTypeComboBox->addItem( xi18n ("Comets") );
    FilterTypeComboBox->addItem( xi18n ("Asteroids") );
    FilterTypeComboBox->addItem( xi18n ("Open Clusters") );
    FilterTypeComboBox->addItem( xi18n ("Globular Clusters") );
    FilterTypeComboBox->addItem( xi18n ("Gaseous Nebulae") );
    FilterTypeComboBox->addItem( xi18n ("Planetary Nebulae") );
    FilterTypeComboBox->addItem( xi18n ("Galaxies") );

    pNames[KSPlanetBase::MERCURY] = xi18n("Mercury");
    pNames[KSPlanetBase::VENUS] = xi18n("Venus");
    pNames[KSPlanetBase::MARS] = xi18n("Mars");
    pNames[KSPlanetBase::JUPITER] = xi18n("Jupiter");
    pNames[KSPlanetBase::SATURN] = xi18n("Saturn");
    pNames[KSPlanetBase::URANUS] = xi18n("Uranus");
    pNames[KSPlanetBase::NEPTUNE] = xi18n("Neptune");
    pNames[KSPlanetBase::PLUTO] = xi18n("Pluto");
    pNames[KSPlanetBase::SUN] = xi18n("Sun");
    pNames[KSPlanetBase::MOON] = xi18n("Moon");

    for ( int i=0; i<KSPlanetBase::UNKNOWN_PLANET; ++i ) {
        //      Obj1ComboBox->insertItem( i, pNames[i] );
        Obj2ComboBox->insertItem( i, pNames[i] );
    }

    // Initialize the Maximum Separation box to 1 degree
    maxSeparationBox->setDegType( true );
    maxSeparationBox->setDMS( "01 00 00.0" );

    //Set up the Table Views
    m_Model = new QStandardItemModel( 0, 5, this );
    m_Model->setHorizontalHeaderLabels( QStringList() << xi18n( "Conjunction/Opposition" ) 
            << xi18n( "Date & Time (UT)" ) << xi18n( "Object 1" ) << xi18n( "Object 2" ) << xi18n( "Separation" ) );
    m_SortModel = new QSortFilterProxyModel( this );
    m_SortModel->setSourceModel( m_Model );
    OutputList->setModel( m_SortModel );
    OutputList->setSortingEnabled(true);
    OutputList->horizontalHeader()->setStretchLastSection( true );
    //TODO Check this
    //OutputList->horizontalHeader()->setResizeMode(  QHeaderView::Interactive );
    OutputList->horizontalHeader()->resizeSection(2, 100);
    OutputList->horizontalHeader()->resizeSection(3, 100);
    OutputList->horizontalHeader()->resizeSection(4, 120); //is it bad way to fix default size of columns ?

    //FilterEdit->showClearButton = true;
    ClearFilterButton->setIcon( QIcon::fromTheme( "edit-clear" ) );

    m_index = 0;

    // signals and slots connections
    connect(LocationButton, SIGNAL(clicked()), this, SLOT(slotLocation()));
    connect(Obj1FindButton, SIGNAL(clicked()), this, SLOT(slotFindObject()));
    connect(ComputeButton, SIGNAL(clicked()), this, SLOT(slotCompute()));
    connect( FilterTypeComboBox, SIGNAL( currentIndexChanged(int) ), SLOT( slotFilterType(int) ) );
    connect( ClearButton, SIGNAL( clicked() ), this, SLOT( slotClear() ) );
    connect( ExportButton, SIGNAL( clicked() ), this, SLOT( slotExport() ) );
    connect( OutputList, SIGNAL( doubleClicked( const QModelIndex& ) ), this, SLOT( slotGoto() ) );
    connect( ClearFilterButton, SIGNAL( clicked() ), FilterEdit, SLOT( clear() ) );
    connect( FilterEdit, SIGNAL( textChanged( const QString & ) ), this, SLOT( slotFilterReg( const QString & ) ) );

    show();
}

ConjunctionsTool::~ConjunctionsTool(){
    delete Object1;
    delete Object2;
}

void ConjunctionsTool::slotGoto() {
    int index = m_SortModel->mapToSource( OutputList->currentIndex() ).row(); // Get the number of the line
    long double jd = outputJDList.value( index );
    KStarsDateTime dt;
    KStars *ks = KStars::Instance();
    KStarsData *data = KStarsData::Instance();
    SkyMap *map = ks->map();

    // Show conjunction
    data->setLocation( *geoPlace );
    dt.setDJD( jd );
    data->changeDateTime( dt );
    map->setClickedObject( data->skyComposite()->findByName( m_Model->data( m_Model->index( index, 2 ) ).toString() ) );
    map->setClickedPoint( map->clickedObject() );
    map->slotCenter();
}

void ConjunctionsTool::slotFindObject() {
    QPointer<FindDialog> fd = new FindDialog( KStars::Instance() );
    if ( fd->exec() == QDialog::Accepted ) {
        delete Object1;
        if( !fd->selectedObject() )
            return;
        Object1 = fd->selectedObject()->clone();
        if( Object1 )
            Obj1FindButton->setText( Object1->name() );
    }
    delete fd;
}

void ConjunctionsTool::slotLocation()
{
    QPointer<LocationDialog> ld( new LocationDialog( this ) );
    if ( ld->exec() == QDialog::Accepted && ld ) {
        geoPlace = ld->selectedCity();
        LocationButton -> setText( geoPlace -> fullName() );
    }
    delete ld;
}

void ConjunctionsTool::slotFilterType( int )
{
    // Disable find button if the user select an object type
    if ( FilterTypeComboBox->currentIndex() == 0 )
        Obj1FindButton->setEnabled( true );
    else
        Obj1FindButton->setEnabled( false );
}

void ConjunctionsTool::slotClear()
{
    m_Model->setRowCount ( 0 );
    outputJDList.clear();
    m_index = 0;
}

void ConjunctionsTool::slotExport()
{
    int i, j;
    QByteArray line;

    //QFile file( KFileDialog::getSaveFileName( QDir::homePath(), "*|All files", this, "Save Conjunctions" ) );
   QFile file( QFileDialog::getSaveFileName(0, xi18n("Save Conjunctions"), QDir::homePath(), "*|All files") );

    file.open( QIODevice::WriteOnly | QIODevice::Text );

    for ( i=0; i<m_Model->rowCount(); ++i ) {
        for ( j=0; j<m_Model->columnCount(); ++j ) {
            line.append( m_Model->data( m_Model->index( i, j ) ).toByteArray() );
            if ( j < m_Model->columnCount() - 1 )
                line.append( ";" );
            else
                line.append( "\n" );
        }
        file.write( line );
        line.clear();
    }

    file.close();
}

void ConjunctionsTool::slotFilterReg( const QString & filter)
{
    m_SortModel->setFilterRegExp( QRegExp( filter, Qt::CaseInsensitive, QRegExp::RegExp ) );
    m_SortModel->setFilterKeyColumn( -1 );
}

void ConjunctionsTool::slotCompute (void)
{
    KStarsDateTime dtStart = startDate -> dateTime();   // Start date
    KStarsDateTime dtStop = stopDate -> dateTime();     // Stop date
    long double startJD = dtStart.djd();                // Start julian day
    long double stopJD = dtStop.djd();                  // Stop julian day
    bool opposition = false;                            // true=opposition, false=conjunction
    if( Opposition->currentIndex() ) opposition = true;
    QStringList objects;                                // List of sky object used as Object1
    KStarsData *data = KStarsData::Instance();
    int progress = 0;

    // Check if we have a valid angle in maxSeparationBox
    dms maxSeparation( 0.0 );
    bool ok;
    maxSeparation = maxSeparationBox->createDms( true, &ok );

    if( !ok ) {
        KMessageBox::sorry( 0, xi18n("Maximum separation entered is not a valid angle. Use the What's this help feature for information on how to enter a valid angle") );
        return;
    }

    // Check if Object1 and Object2 are set
    if( FilterTypeComboBox->currentIndex() == 0 && !Object1 ) {
        KMessageBox::sorry( 0, xi18n("Please select an object to check conjunctions with, by clicking on the \'Find Object\' button.") );
        return;
    }
    Object2 = KSPlanetBase::createPlanet( Obj2ComboBox->currentIndex() );
    if( FilterTypeComboBox->currentIndex() == 0 && Object1->name() == Object2->name() ) {
        // FIXME: Must free the created Objects
    	KMessageBox::sorry( 0 , xi18n("Please select two different objects to check conjunctions with.") );
    	return;
    }

    // Init KSConjunct object
    KSConjunct ksc;
    connect( &ksc, SIGNAL(madeProgress(int)), this, SLOT(showProgress(int)) );
    ksc.setGeoLocation( geoPlace );

    switch ( FilterTypeComboBox->currentIndex() ) {
        case 1: // All object types
            foreach( int type, data->skyComposite()->objectNames().keys() )
                objects += data->skyComposite()->objectNames( type );
            break;
        case 2: // Stars
            objects += data->skyComposite()->objectNames( SkyObject::STAR );
            objects += data->skyComposite()->objectNames( SkyObject::CATALOG_STAR );
            break;
        case 3: // Solar system
            objects += data->skyComposite()->objectNames( SkyObject::PLANET );
            objects += data->skyComposite()->objectNames( SkyObject::COMET );
            objects += data->skyComposite()->objectNames( SkyObject::ASTEROID );
            objects += data->skyComposite()->objectNames( SkyObject::MOON );
            objects += xi18n("Sun");
            // Remove Object2  planet
            objects.removeAll( Object2->name() );
            break;
        case 4: // Planet
            objects += data->skyComposite()->objectNames( SkyObject::PLANET );
            // Remove Object2  planet
            objects.removeAll( Object2->name() );
            break;
        case 5: // Comet
            objects += data->skyComposite()->objectNames( SkyObject::COMET );
            break;
        case 6: // Ateroid
            objects += data->skyComposite()->objectNames( SkyObject::ASTEROID );
            break;
        case 7: // Open Clusters
            objects = data->skyComposite()->objectNames( SkyObject::OPEN_CLUSTER );
            break;
        case 8: // Open Clusters
            objects = data->skyComposite()->objectNames( SkyObject::GLOBULAR_CLUSTER );
            break;
        case 9: // Gaseous nebulae
            objects = data->skyComposite()->objectNames( SkyObject::GASEOUS_NEBULA );
            break;
        case 10: // Planetary nebula
            objects = data->skyComposite()->objectNames( SkyObject::PLANETARY_NEBULA );
            break;
        case 11: // Galaxies
            objects = data->skyComposite()->objectNames( SkyObject::GALAXY );
            break;
    }

    // Remove all Jupiter and Saturn moons
    // KStars crash if we compute a conjunction between a planet and one of this moon
    if ( FilterTypeComboBox->currentIndex() == 1 || 
         FilterTypeComboBox->currentIndex() == 3 ||
         FilterTypeComboBox->currentIndex() == 6 ) {
        objects.removeAll( "Io" );
        objects.removeAll( "Europa" );
        objects.removeAll( "Ganymede" );
        objects.removeAll( "Callisto" );
        objects.removeAll( "Mimas" );
        objects.removeAll( "Enceladus" );
        objects.removeAll( "Tethys" );
        objects.removeAll( "Dione" );
        objects.removeAll( "Rhea" );
        objects.removeAll( "Titan" );
        objects.removeAll( "Hyperion" );
        objects.removeAll( "Iapetus" );
    }

    if ( FilterTypeComboBox->currentIndex() != 0 ) {
        // Show a progress dialog while processing
        QProgressDialog progressDlg( xi18n( "Compute conjunction..." ), xi18n( "Abort" ), 0, objects.count(), this);
        progressDlg.setWindowModality( Qt::WindowModal );
        progressDlg.setValue( 0 );

        foreach( QString object, objects ) {
            // If the user click on the 'cancel' button
            if ( progressDlg.wasCanceled() )
                break;

            // Update progress dialog
            ++progress;
            progressDlg.setValue( progress );
            progressDlg.setLabelText( xi18n( "Compute conjunction between %1 and %2", Object2->name(), object ) );

            // Compute conjuction
            Object1 = data->skyComposite()->findByName( object );
            showConjunctions( ksc.findClosestApproach(*Object1, *Object2, startJD, stopJD, maxSeparation, opposition), object, Object2->name() );
        }

        progressDlg.setValue( objects.count() );
    } else {
        // Change cursor while we search for conjunction
        QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );

        ComputeStack->setCurrentIndex( 1 );
        showConjunctions( ksc.findClosestApproach(*Object1, *Object2, startJD, stopJD, maxSeparation, opposition), Object1->name(), Object2->name() );
        ComputeStack->setCurrentIndex( 0 );

        // Restore cursor
        QApplication::restoreOverrideCursor();
    }

    delete Object2;
    Object2 = NULL;
}

void ConjunctionsTool::showProgress(int n) {
    progress->setValue( n );
}

void ConjunctionsTool::showConjunctions(const QMap<long double, dms> &conjunctionlist,const QString &object1,const QString &object2)
{
    KStarsDateTime dt;
    QMap<long double, dms>::ConstIterator it;
    QList<QStandardItem*> itemList;

    for(it = conjunctionlist.constBegin(); it != conjunctionlist.constEnd(); ++it) {
        dt.setDJD( it.key() );
        QStandardItem* typeItem;

        if ( ! Opposition->currentIndex() )
            typeItem = new QStandardItem( xi18n( "Conjunction" ) );
        else
            typeItem = new QStandardItem( xi18n( "Opposition" ) );

        itemList << typeItem
                //FIXME TODO is this ISO date? is there a ready format to use?
                << new QStandardItem( QLocale().toString( dt.dateTime(), "YYYY-MM-DDTHH:mm:SS" ) )
                << new QStandardItem( object1 )
                << new QStandardItem( object2 )
                << new QStandardItem( it.value().toDMSString() );
        m_Model->appendRow( itemList );
        itemList.clear();

        outputJDList.insert( m_index, it.key() );
        ++m_index;
    }
}
