/***************************************************************************
                          flagcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Fri 16 Jan 2009
    copyright            : (C) 2009 by Jerome SONRIER
    email                : jsid@emor3j.fr.eu.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include "flagcomponent.h"

#include <kio/job.h>
#include <kstandarddirs.h>
#include <kfileitem.h>

#include "Options.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "../skyobjects/skypoint.h"
#include "ksfilereader.h"


FlagComponent::FlagComponent( SkyComponent *parent )
    : PointListComponent(parent)
{
    // List user's directory
    m_Job = KIO::listDir( KUrl( KStandardDirs::locateLocal("appdata", ".") ), KIO::HideProgressInfo, false );
    connect(m_Job, SIGNAL(entries(KIO::Job*, const KIO::UDSEntryList&)), 
            SLOT(slotLoadImages(KIO::Job*, const KIO::UDSEntryList&)));
}


FlagComponent::~FlagComponent()
{
}

void FlagComponent::init( KStarsData *data ) {
    KSFileReader fileReader;
    QStringList line;
    QString str;
    int i;
    bool imageFound = false;

    // Return if flags.dat does not exist
    if ( ! QFile::exists ( KStandardDirs::locateLocal( "appdata", "flags.dat" ) ) ) return;

    // Return if flags.dat cna not be read
    if ( ! fileReader.open( "flags.dat" ) ) return;

    // Read flags.dat
    while ( fileReader.hasMoreLines() ) {
        // Split line and ignore it if it's too short or if it's a comment
        line = fileReader.readLine().split( ' ' );
        if ( line.size() < 4 ) continue;
        if ( line.at( 0 ) == "#" ) continue;

        // Read coordinates
        dms r( line.at( 0 ) );
        dms d( line.at( 1 ) );
        SkyPoint* flagPoint = new SkyPoint( r, d );
        pointList().append( flagPoint );

        // Read epoch
        m_Epoch.append( line.at( 2 ) );

        // Read image name
        str = line.at( 3 );
        str = str.replace( '_', ' ');
        for ( i=0; i<m_Names.size(); ++i ) {
            if ( str == m_Names.at( i ) ) {
                m_FlagImages.append( i );
                imageFound = true;
            }
        }

        // If the image sprecified in flags.dat does not exist,
        // use the default one
        if ( ! imageFound )
            m_FlagImages.append( 0 );

        imageFound = false;

        // Continue if there is no label
        if ( ! line.size() > 4 )
            continue;

        // Read label
        str.clear();
        for ( i=4; i<line.size(); ++i ) {
            str += line.at( i ) + ' ';
        }

        m_Labels.append( str );
    }
}

void FlagComponent::draw( QPainter& psky )
{
    // Return if flags must not be draw
    if ( ! selected() ) return;

    SkyMap *map = SkyMap::Instance();
    KStarsData *data = KStarsData::Instance();
    SkyPoint *p;
    QPointF o;
    QImage flagImage;
    double epoch;
    long double jd;
    int i;

    for ( i=0; i<pointList().size(); i++ ) {
        // Get Screen coordinates
        p = pointList().at( i );
        epoch = getEpoch( m_Epoch.at( i ) );
        jd = epochToJd ( epoch );
        p->apparentCoord(jd, data->ut().djd() );
        p->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        o = map->toScreen( p, false );

        // Draw flag image
        flagImage = m_Images.at( m_FlagImages.at( i ) );
        o.setX( o.x() - flagImage.width()*0.5 );
        o.setY( o.y() - flagImage.height()*0.5 );
        psky.drawImage( o, flagImage );

        // Draw flag label
        o.setX( o.x() + 25.0 );
        o.setY( o.y() + 12.0 );
        psky.save();
        psky.setPen( QColor( 255, 0, 0 ) );
        psky.setFont( QFont( "Courier New", 10, QFont::Bold ) );
        psky.drawText( o, m_Labels.at( i ) );
        psky.restore();
    }
}


bool FlagComponent::selected() {
    return Options::showFlags();
}

double FlagComponent::getEpoch (const QString &eName) {
    //If eName is empty (or not a number) assume 2000.0
    bool ok(false);
    double epoch = eName.toDouble( &ok );
    if ( eName.isEmpty() || ! ok )
        return 2000.0;

    return epoch;
}

long double FlagComponent::epochToJd (double epoch) {

    double yearsTo2000 = 2000.0 - epoch;

    if (epoch == 1950.0) {
        return 2433282.4235;
    } else if ( epoch == 2000.0 ) {
        return J2000;
    } else {
        return ( J2000 - yearsTo2000 * 365.2425 );
    }

}

void FlagComponent::add( SkyPoint* flagPoint, QString epoch, QString image, QString label ) {
    int i;

    pointList().append( flagPoint );
    m_Epoch.append( epoch );

    for ( i=0; i<m_Names.size(); i++ ) {
        if ( image == m_Names.at( i ) ) {
            m_FlagImages.append( i );
        }
    }

    m_Labels.append( label );
}

void FlagComponent::remove( int index ) {
    pointList().removeAt( index );
    m_Epoch.removeAt( index );
    m_FlagImages.removeAt( index );
    m_Labels.removeAt( index );
}

void FlagComponent::slotLoadImages( KIO::Job* job, const KIO::UDSEntryList& list ) {
    int index = 0;
    QString fileName;
    QStringList fileNameLst;
    QImage flagImage;

    m_Names.append( "Default" );
    flagImage.load( KStandardDirs::locate( "appdata", "defaultflag.gif" ) );
    m_Images.append( flagImage );

    for ( KIO::UDSEntryList::ConstIterator it = list.begin(); it != list.end(); ++it ) {
        KFileItem* item = new KFileItem(*it, m_Job->url(), false, true);
        if ( item->name().startsWith( QLatin1String( "_flag" ) ) ) {
            fileNameLst = item->name().split( '.' );
            fileNameLst.removeLast();
            fileName = fileNameLst.join( "." );
            fileName = fileName.right( fileName.size() - 5 );
            fileName = fileName.replace( '_', ' ' );
            m_Names.append( fileName );

            flagImage.load( item->localPath() );
            m_Images.append( flagImage );

            index++;
        }
    }
}

QStringList FlagComponent::getNames() {
    return m_Names;
}

int FlagComponent::size() {
    return pointList().size();
}

QString FlagComponent::epoch( int index ) {
    return m_Epoch.at( index );
}

QString FlagComponent::label( int index ) {
    return m_Labels.at( index );
}

QImage FlagComponent::image( int index ) {
    return m_Images.at( m_FlagImages.at( index ) );
}

QString FlagComponent::imageName( int index ) {
    return m_Names.at( m_FlagImages.at( index ) );
}

QList<QImage> FlagComponent::imageList() {
    return m_Images;
}

QImage FlagComponent::imageList( int index ) {
    return m_Images.at( index );
}
