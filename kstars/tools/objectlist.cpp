/***************************************************************************
                          objectlist.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 
    copyright            : (C) 2010 by Victor Carbune
    email                : victor.carbune@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "objectlist.h"

#include <stdio.h>

#include <QFile>
#include <QDir>
#include <QFrame>
#include <QTextStream>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QDirIterator>
#include <QSqlField>

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
#include <kio/copyjob.h>
#include <kstandarddirs.h>

#include "ksalmanac.h"
#include "obslistwizard.h"
#include "kstars.h"
#include "kstarsdb.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "dialogs/locationdialog.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/deepskyobject.h"
#include "skyobjects/starobject.h"
#include "skycomponents/skymapcomposite.h"
#include "skycomponents/skymesh.h"
#include "skymap.h"
#include "dialogs/detaildialog.h"
#include "dialogs/finddialog.h"
#include "tools/altvstime.h"
#include "tools/wutdialog.h"
#include "Options.h"
#include "imageviewer.h"
#include "thumbnailpicker.h"
#include "oal/log.h"
#include "oal/oal.h"
#include "oal/execute.h"
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
// ObjectListUI
// ---------------------------------
ObjectListUI::ObjectListUI( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

//
// ObjectList
// ---------------------------------
ObjectList::ObjectList( KStars *_ks )
        : KDialog( (QWidget*)_ks ),
        ks( _ks )
{
    // Setup the dialog components
    ui = new ObjectListUI( this );
    setMainWidget( ui );
    setCaption( i18n( "Object List" ) );
    setButtons( KDialog::Close );

    /*
    The database contains the following types (the same, from the old ngcic.dat):
                { STAR=0, CATALOG_STAR=1, PLANET=2, OPEN_CLUSTER=3, GLOBULAR_CLUSTER=4,
                GASEOUS_NEBULA=5, PLANETARY_NEBULA=6, SUPERNOVA_REMNANT=7, GALAXY=8,
                COMET=9, ASTEROID=10, CONSTELLATION=11, MOON=12, ASTERISM=13, 
                GALAXY_CLUSTER=14, DARK_NEBULA=15, QUASAR=16, MULT_STAR=17, TYPE_UNKNOWN };
    */

    ui->FilterType->addItem( i18n ("Any"), QVariant(-1) );
    ui->FilterType->addItem( i18n ("Stars"), QVariant(0) );
    ui->FilterType->addItem( i18n ("Solar System"), QVariant(2) );
    ui->FilterType->addItem( i18n ("Open Clusters"), QVariant(3) );
    ui->FilterType->addItem( i18n ("Globular Clusters"), QVariant(4) );
    ui->FilterType->addItem( i18n ("Gaseous Nebulae"), QVariant(5) );
    ui->FilterType->addItem( i18n ("Planetary Nebulae"), QVariant(6) );
    ui->FilterType->addItem( i18n ("Supernova Remnant"), QVariant(7) );
    ui->FilterType->addItem( i18n ("Galaxies"), QVariant(8) );
    ui->FilterType->addItem( i18n ("Comets"), QVariant(9) );
    ui->FilterType->addItem( i18n ("Asteroids"), QVariant(10) );
    ui->FilterType->addItem( i18n ("Constellations"), QVariant(11) );
    ui->FilterType->addItem( i18n ("Galaxy Cluster"), QVariant(14) );
    ui->FilterType->addItem( i18n ("Dark Nebula"), QVariant(15) );
    ui->FilterType->addItem( i18n ("Quasar"), QVariant(16) );
    ui->FilterType->addItem( i18n ("Multiple Star"), QVariant(17) );

    ui->FilterType->setCurrentIndex(0);

    // Default maximum magnitude (18)
    ui->MaxMagnitude->setValue(18.00);

    // Display the catalogs from the database
    QSqlQuery query;
    if (!query.exec("SELECT name, rowid FROM ctg ORDER BY rowid")) {
        kDebug() << query.lastError();
    }

    ui->CatalogList->addItem( i18n ("Any"), QVariant(-1) );
    while (query.next()) {
        ui->CatalogList->addItem( query.value(0).toString(), query.value(1) );
    }

    // By default, just display all the objects, mixing all the catalogs
    QString subQuery = "(SELECT group_concat(ctg.name || \" \" || od.designation) FROM od " + 
       QString("INNER JOIN ctg ON od.idCTG = ctg.rowid WHERE od.idDSO = dso.rowid) AS Designations");

    QString searchQuery = "SELECT " + subQuery + ", dso.longname AS Name, dso.rowid FROM dso WHERE dso.bmag <= " + QString::number(ui->MaxMagnitude->value());

    m_TableModel = new QSqlQueryModel();

    m_TableModel->setQuery(searchQuery);
    
    kDebug() << m_TableModel->lastError();
    kDebug() << m_TableModel->query().lastQuery();

    ui->TableView->setModel( m_TableModel );

    // Query change connections
    connect( ui->FilterType, SIGNAL( activated( int ) ), this, SLOT( enqueueSearch() ) );
    connect( ui->CatalogList, SIGNAL( activated( int ) ), this, SLOT( enqueueSearch() ) );
    connect( ui->SearchBox, SIGNAL( textChanged( const QString & ) ), SLOT( enqueueSearch() ) );
    connect( ui->SemimajorAxis, SIGNAL( valueChanged( double ) ), this, SLOT( enqueueSearch() ) );
    connect( ui->SemiminorAxis, SIGNAL( valueChanged( double ) ), this, SLOT( enqueueSearch() ) );
    connect( ui->MinMagnitude, SIGNAL( valueChanged( double ) ), this, SLOT( enqueueSearch() ) );
    connect( ui->MaxMagnitude, SIGNAL( valueChanged( double ) ), this, SLOT( enqueueSearch() ) );

    // Object specific connections
    connect( ui->TableView, SIGNAL( doubleClicked( const QModelIndex &) ), SLOT ( slotSelectObject (const QModelIndex &) ));
    connect( ui->TableView->selectionModel() , SIGNAL (selectionChanged(const QItemSelection &, const QItemSelection &)), SLOT (slotNewSelection()) );


    // Some display options
    ui->TableView->verticalHeader()->hide();

    ui->TableView->horizontalHeader()->setStretchLastSection( true );
//  ui->TableView->horizontalHeader()->setResizeMode( QHeaderView::ResizeToContents );

    ui->TableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->TableView->setColumnHidden(2, true);
}

ObjectList::~ObjectList()
{
}

void ObjectList::enqueueSearch()
{
    QString searchedName = processSearchText();
    qlonglong catalogID = ui->CatalogList->itemData(ui->CatalogList->currentIndex()).toInt(),
              itemType = ui->FilterType->itemData(ui->FilterType->currentIndex()).toInt();

    QString subQuery, searchQuery;

    // Check if magnitude constrains are consistent
    if (ui->MinMagnitude->value() > ui->MaxMagnitude->value()) {
        ui->MinMagnitude->setValue(ui->MaxMagnitude->value());
    }
    
    // Particular catalog requested
    if (catalogID != -1) {
        searchQuery = "SELECT ctg.name || \" \" || od.designation AS Designations, dso.longname AS Name, dso.rowid " +
           QString("FROM od INNER JOIN ctg ON od.idCTG = ctg.rowid INNER JOIN dso ON od.idDSO = dso.rowid WHERE od.idCTG = ") + QString::number(catalogID) + " ";

        searchQuery += "AND (Designations LIKE \'%" + searchedName + "%\' OR Name LIKE \'%" + searchedName + "%\') ";
    } else {
        subQuery = "(SELECT group_concat(ctg.name || \" \" || od.designation) FROM od INNER JOIN ctg ON od.idCTG = ctg.rowid WHERE od.idDSO = dso.rowid) AS Designations";
        searchQuery = "SELECT " + subQuery + ", dso.longname AS Name, dso.rowid FROM dso ";

        searchQuery += "WHERE (Designations LIKE \'%" + searchedName + "%\' OR Name LIKE \'%" + searchedName + "%\') ";
    }

    // Specific object types
    if (itemType != -1) {
        searchQuery += "AND dso.type = " + QString::number(itemType) + " ";
    }

    // Magnitude constraints
    searchQuery += "AND dso.bmag <= \'" + QString::number(ui->MaxMagnitude->value()) + "\' ";
    searchQuery += "AND dso.bmag >= " + QString::number(ui->MinMagnitude->value()) + " ";
    
    // Semiaxis constraints
    searchQuery += "AND dso.minor >= \'" + QString::number(ui->SemiminorAxis->value()) + "\' ";
    searchQuery += "AND dso.major >= \'" + QString::number(ui->SemimajorAxis->value()) + "\' ";

//  searchQuery += "ORDER BY dso.rowid";

    m_TableModel->setQuery(searchQuery);

    // for debugging purposes
    kDebug() << m_TableModel->lastError();
    kDebug() << searchQuery;
//  kDebug() << m_TableModel->lastQuery();
}

QString ObjectList::processSearchText()
{
    QRegExp re;
    QString searchtext = ui->SearchBox->text();

    re.setCaseSensitivity( Qt::CaseInsensitive ); 

    // If it is an NGC/IC/M catalog number, as in "M 76" or "NGC 5139", check for absence of the space
    re.setPattern("^(pgc|ugc|m|ngc|ic)\\s*\\d*$");
    if(ui->SearchBox->text().contains(re)) {
        re.setPattern("\\s*(\\d+)");
        searchtext.replace( re, " \\1" );
        re.setPattern("\\s*$");
        searchtext.remove( re );
        re.setPattern("^\\s*");
        searchtext.remove( re );
    }

    // TODO after KDE 4.1 release:
    // If it is a IAU standard three letter abbreviation for a constellation, then go to that constellation
    // Check for genetive names of stars. Example: alp CMa must go to alpha Canis Majoris

    return searchtext;
}

void ObjectList::slotSelectObject(const QModelIndex &index)
{
    drawObject(m_TableModel->record(index.row()).field(2).value().toLongLong());
    // some additional operations might be done here
}

void ObjectList::slotNewSelection() {
    QModelIndexList selectedItems;
    QModelIndex item;
    QList<SkyObject *> skyobjects;

    KStarsDB *ksdb = KStarsDB::Instance();

    qlonglong id;
    SkyObject *o;

    selectedItems = ui->TableView->selectionModel()->selectedRows(2);

    if( selectedItems.size() == 1 ) {
        id = selectedItems.first().data().toLongLong();
        o = ksdb->getObject(id);
        skyobjects.push_back(o);

        ui->TableView->setSkyObjectList(skyobjects);
    } else {
       foreach (item, selectedItems) {
           o = ksdb->getObject (item.data().toLongLong());
           skyobjects.push_back(o);
       }

       ui->TableView->setSkyObjectList(skyobjects);
    }
}

/*
SkyObject * ObjectList::getObject(qlonglong id)
{
    if (objectHash.contains(id)) {
        kDebug() << "Found a hashed object!";
        return objectHash[id];
    }
    
    KStarsData *data = KStarsData::Instance();
    SkyObject *o = NULL;

    // Variables needed to load all the data
    QString line, con, ss, name[10], longname;
    QString cat[10];

    // Magnitude, Right Ascension (seconds), Semimajor and Semiminor axis
    float mag, ras, a, b;

    // RA Hours, Minutes, DSO Type, NGC Index, Messier Index
    int rah, ram, type;

    // Dec Degrees, Minutes, Seconds, Position Angle, Sign
    int dd, dm, ds, pa, sgn;

    // PGC Index, UGC Index, Catalogs number
    int pgc = 0, ugc = 0, k = 0;

    // Index flag, nameflag, counter
    QChar iflag; bool hasName;

    QSqlQuery query, dsoquery;

    QString queryStatement =  QString("SELECT o.ra, o.dec, ") +
                                QString("o.sgn, ") +
                                QString("o.bmag, o.type, o.pa, o.minor, o.major, ") +
                                QString("o.longname, o.rowid FROM dso AS o ") +
                                QString("WHERE o.rowid = ") + QString::number(id);

    if (!query.exec(queryStatement)) {
        kDebug() << "Deep Sky select statement error: " << query.lastError();
    }

    while ( query.next() ) {
        // Right Ascension
        ras = query.value(0).toString().mid(0, 2).toInt();
        ram = query.value(0).toString().mid(2, 2).toInt();
        rah = query.value(0).toString().mid(4, 4).toFloat();

        // Declination
        dd = query.value(1).toString().mid(0, 2).toInt();
        dm = query.value(1).toString().mid(2, 2).toInt();
        ds = query.value(1).toString().mid(4, 2).toInt();

        // Position Angle, Magnitude, Semimajor axis
        pa = query.value(5).toInt(); mag = query.value(3).toFloat(); a = query.value(7).toFloat(); b = query.value(6).toFloat();

        // Object type, SGN
        type = query.value(4).toInt(); sgn = query.value(2).toInt();



        // Inner Join to retrieve all the catalogs in which the object appears
        dsoquery.prepare("SELECT od.designation, ctg.name FROM od INNER JOIN ctg ON od.idCTG = ctg.rowid WHERE od.idDSO = :iddso");
        dsoquery.bindValue(":iddso", query.value(13).toInt());

        if (!dsoquery.exec()) {
            kDebug() << "Error on retrieving the catalog list for an object: " << dsoquery.lastError();
        }

        // Parsing catalog information
        k = 0;

        while (dsoquery.next()) {
            name[k] = dsoquery.value(1).toString() + ' ' + dsoquery.value(0).toString();
            cat[k] = dsoquery.value(1).toString();
            k++;
        }

        hasName = true;
        longname = query.value(12).toString();

        if (!longname.isEmpty()) {
            if (name[0] == "") {
                name[0] = longname;
            }
        } else if (name[0] == "") {
            hasName = false;
            name[0] = "Unnamed Object";
        }

        dms r;
        r.setH( rah, ram, int(ras) );
        dms d( dd, dm, ds );

        if ( sgn == -1 ) { d.setD( -1.0*d.Degrees() ); }

        // Create new Deep Sky Object Instance
        if ( type==0 ) type = 1; //Make sure we use CATALOG_STAR, not STAR

        o = new DeepSkyObject( type, r, d, mag, name[0], name[1], longname, cat[0], a, b, pa, pgc, ugc );
        o->EquatorialToHorizontal( data->lst(), data->geo()->lat() );

        // Load the images associated to the deep sky object (this was done in KStarsData::readUrlData)
        dsoquery.prepare("SELECT url, title, type FROM dso_url WHERE idDSO = :iddso");
        dsoquery.bindValue(":iddso", query.value(13).toInt());

        if (!dsoquery.exec()) {
            kDebug() << "URL query error: " << dsoquery.lastError();
        } else {
            while (dsoquery.next()) {
                switch (dsoquery.value(2).toInt()) {
                    case IMG_URL:
                        o->ImageList().append(dsoquery.value(0).toString());
                        o->ImageTitle().append(dsoquery.value(1).toString());
                        break;

                    case INFO_URL:
                        o->InfoList().append(dsoquery.value(0).toString());
                        o->InfoTitle().append(dsoquery.value(1).toString());
                        break;
                }
            }
        }
        
        objectHash[id] = o;
        return o;
    }

    return 0;
}
*/

void ObjectList::drawObject(qlonglong id)
{
    kDebug() << id;
    KStarsDB* ksdb = KStarsDB::Instance();
    SkyObject *o = ksdb->getObject(id);

    // Focus the object
    SkyMap *map = KStars::Instance()->map();

    map->setClickedObject( o );
    map->setClickedPoint( o );
    map->slotCenter();
}

#include "objectlist.moc"
