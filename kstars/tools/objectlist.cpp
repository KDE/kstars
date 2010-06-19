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
#include "obslistpopupmenu.h"
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
// ObservingList
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
    DATABASE TYPES
                { STAR=0, CATALOG_STAR=1, PLANET=2, OPEN_CLUSTER=3, GLOBULAR_CLUSTER=4,
                GASEOUS_NEBULA=5, PLANETARY_NEBULA=6, SUPERNOVA_REMNANT=7, GALAXY=8,
                COMET=9, ASTEROID=10, CONSTELLATION=11, MOON=12, ASTERISM=13, 
                GALAXY_CLUSTER=14, DARK_NEBULA=15, QUASAR=16, MULT_STAR=17, TYPE_UNKNOWN };
    */

    ui->FilterType->addItem( i18n ("Any") );
    ui->FilterType->addItem( i18n ("Stars") );
    ui->FilterType->addItem( i18n ("Solar System") );
    ui->FilterType->addItem( i18n ("Open Clusters") );
    ui->FilterType->addItem( i18n ("Globular Clusters") );
    ui->FilterType->addItem( i18n ("Gaseous Nebulae") );
    ui->FilterType->addItem( i18n ("Planetary Nebulae") );
    ui->FilterType->addItem( i18n ("Supernova Remnant") );
    ui->FilterType->addItem( i18n ("Galaxies") );
    ui->FilterType->addItem( i18n ("Comets") );
    ui->FilterType->addItem( i18n ("Asteroids") );
    ui->FilterType->addItem( i18n ("Constellations") );
    ui->FilterType->addItem( i18n ("Galaxy Cluster") );
    ui->FilterType->addItem( i18n ("Dark Nebula") );
    ui->FilterType->addItem( i18n ("Quasar") );
    ui->FilterType->addItem( i18n ("Multiple Star") );

    ui->FilterType->setCurrentIndex(0);

    connect( ui->FilterType, SIGNAL( activated( int ) ), this, SLOT( enqueueSearch() ) );
    connect( ui->FilterMagnitude, SIGNAL( valueChanged( double ) ), this, SLOT( enqueueSearch() ) );
    connect( ui->SearchBox, SIGNAL( textChanged( const QString & ) ), SLOT( enqueueSearch() ) );
    connect( ui->TableView, SIGNAL( doubleClicked( const QModelIndex &) ), SLOT ( selectObject (const QModelIndex &) ));

    // By default, just display all the objects
    m_TableModel = new QSqlQueryModel();
    QString searchQuery ="SELECT (ctg.name || \" \" || od.designation) AS design, dso.longname, dso.rowid FROM od INNER JOIN ctg ON (od.idCTG = ctg.rowid) INNER JOIN dso ON (od.idDSO = dso.rowid) ";
    searchQuery += "WHERE dso.bmag <= 13.00";
    
    m_TableModel->setQuery(searchQuery);
    
    kDebug() << m_TableModel->lastError();
    ui->TableView->setModel( m_TableModel );

    ui->TableView->verticalHeader()->hide();
    ui->TableView->horizontalHeader()->hide();

    ui->TableView->horizontalHeader()->setStretchLastSection( true );
    ui->TableView->horizontalHeader()->setResizeMode( QHeaderView::ResizeToContents );

    ui->TableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->TableView->setColumnHidden(2, true);
}

ObjectList::~ObjectList()
{
}

void ObjectList::enqueueSearch()
{
    QString searchedName = processSearchText();
    QString searchQuery ="SELECT (ctg.name || \" \" || od.designation) AS design, dso.longname AS lname, dso.rowid "
        + QString("FROM od INNER JOIN ctg ON (od.idCTG = ctg.rowid) INNER JOIN dso ON (od.idDSO = dso.rowid) ");
    searchQuery += "WHERE (design LIKE \'%" + searchedName + "%\' OR lname LIKE \'%" + searchedName + "%\') ";

    if (ui->FilterType->currentIndex() != 0) {
        if (ui->FilterType->currentIndex() > 11) // adjust the numbers to match the database types
            searchQuery += "AND type = " + QString::number(ui->FilterType->currentIndex() + 2) + " ";
        else
            searchQuery += "AND type = " + QString::number(ui->FilterType->currentIndex()) + " ";
    }

    searchQuery += "AND dso.bmag <= \'" + QString::number(ui->FilterMagnitude->value()) + "\' ";

    searchQuery += "ORDER BY ctg.name";

    m_TableModel->setQuery(searchQuery);

    // for debugging purposes
    kDebug() << m_TableModel->lastError();
    kDebug() << m_TableModel->query().lastQuery();
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

void ObjectList::selectObject(const QModelIndex &index)
{
    kDebug() << m_TableModel->record(index.row()).field(2).value();
    drawObject(m_TableModel->record(index.row()).field(2).value().toLongLong());

    // some additional operations might be done here
}

void ObjectList::drawObject(qlonglong id)
{
    SkyMesh *skyMesh = SkyMesh::Instance();
    KStarsData *data = KStarsData::Instance();

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
    QChar iflag; bool hasName; int i;

    QSqlQuery query, dsoquery;

    QString queryStatement =  QString("SELECT o.rah, o.ram, o.ras, ") +
                                QString("o.sgn, o.decd, o.decm, o.decs, ") +
                                QString("o.bmag, o.type, o.pa, o.minor, o.major, ") +
                                QString("o.longname, o.rowid FROM dso AS o ") +
                                QString("WHERE o.rowid = ") + QString::number(id);

    if (!query.exec(queryStatement)) {
        kDebug() << "Deep Sky select statement error: " << query.lastError();
    }

    while ( query.next() ) {
        // Right Ascension
        ras = query.value(2).toFloat(); ram = query.value(1).toInt(); rah = query.value(0).toInt();

        // Declination
        dd = query.value(4).toInt(); dm = query.value(5).toInt(), ds = query.value(6).toInt();

        // Position Angle, Magnitude, Semimajor axis
        pa = query.value(9).toInt(); mag = query.value(7).toFloat(); a = query.value(11).toFloat(); b = query.value(10).toFloat();

        // Object type, SGN
        type = query.value(8).toInt(); sgn = query.value(3).toInt();


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
        DeepSkyObject *o = 0;
        if ( type==0 ) type = 1; //Make sure we use CATALOG_STAR, not STAR

        o = new DeepSkyObject( type, r, d, mag, name[0], name[1], longname, cat[0], a, b, pa, pgc, ugc );
        o->EquatorialToHorizontal( data->lst(), data->geo()->lat() );

//      Trixel trixel = skyMesh->index( (SkyPoint*) o );

        SkyMap *map = KStars::Instance()->map();
        SkyMapComposite *skyComp = data->skyComposite();

        map->setClickedObject( o );
        map->setClickedPoint( o );
        map->slotCenter();

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
    }
}

#include "objectlist.moc"
