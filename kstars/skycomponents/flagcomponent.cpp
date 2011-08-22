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
#include <klocale.h>

#include "Options.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyobjects/skypoint.h"
#include "ksfilereader.h"
#include "skypainter.h"
#include "kstars/projections/projector.h"

FlagComponent::FlagComponent( SkyComposite *parent )
    : PointListComponent(parent)
{
    // List user's directory
    m_Job = KIO::listDir( KUrl( KStandardDirs::locateLocal("appdata", ".") ), KIO::HideProgressInfo, false );
    connect( m_Job,SIGNAL( entries(KIO::Job*, const KIO::UDSEntryList& ) ),
            SLOT( slotLoadImages( KIO::Job*, const KIO::UDSEntryList& ) ) );
    connect( m_Job, SIGNAL( result( KJob * ) ), this, SLOT( slotInit( KJob * ) ) );
}


FlagComponent::~FlagComponent()
{}

void FlagComponent::draw( SkyPainter *skyp ) {
    // Return if flags must not be draw
    if( ! selected() )
        return;

    // Return if no images are available
    if( m_Names.size() < 1 )
        return;

    // Draw all flags
    skyp->drawFlags();
}

bool FlagComponent::selected() {
    return Options::showFlags();
}

double FlagComponent::getEpoch (const QString &eName) {
    //If eName is empty (or not a number) assume 2000.0
    bool ok;
    double epoch = eName.toDouble( &ok );
    if( eName.isEmpty() || !ok )
        return 2000.0;
    return epoch;
}

void FlagComponent::loadFromFile() {
    KSFileReader fileReader;
    bool imageFound = false;

    // Return if flags.dat does not exist
    if( !QFile::exists ( KStandardDirs::locateLocal( "appdata", "flags.dat" ) ) )
        return;

    // Return if flags.dat can not be read
    if( !fileReader.open( "flags.dat" ) )
        return;

    // Read flags.dat
    while ( fileReader.hasMoreLines() ) {
        // Split line and ignore it if it's too short or if it's a comment
        QStringList line = fileReader.readLine().split( ' ' );
        if ( line.size() < 4 )
            continue;
        if ( line.at( 0 ) == "#" )
            continue;

        // Read coordinates
        dms r( line.at( 0 ) );
        dms d( line.at( 1 ) );
        SkyPoint* flagPoint = new SkyPoint( r, d );
        pointList().append( flagPoint );

        // Read epoch
        m_Epoch.append( line.at( 2 ) );

        // Read image name
        QString str = line.at( 3 );
        str = str.replace( '_', ' ');
        for(int i = 0; i < m_Names.size(); ++i ) {
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

        // If there is no label, use an empty string, red color and continue.
        if ( ! ( line.size() > 4 ) ) {
            m_Labels.append( "" );
            m_LabelColors.append( QColor( "red" ) );
            continue;
        }

        // Read label and color label
        // If the last word is a color value, use it to set label color
        // Else use red
        QRegExp rxLabelColor( "^#[a-fA-F0-9]{6}$" );
        if ( rxLabelColor.exactMatch( line.last() ) ) {
            m_LabelColors.append( QColor( line.last() ) );
            line.removeLast();
        } else {
            m_LabelColors.append( QColor( "red" ) );
        }

        str.clear();
        for ( int i=4; i < line.size() - 1; ++i )
            str += line.at( i ) + ' ';
        str += line.at( line.size() - 1 );
        m_Labels.append( str );
    }
}

void FlagComponent::saveToFile() {
    QFile file( KStandardDirs::locateLocal( "appdata", "flags.dat.tmp" ) );
    file.open( QIODevice::WriteOnly );

    QTextStream ostream(&file);
    for ( int i=0; i < size(); ++i ) {
        ostream << QString::number( pointList().at( i )->ra0().Degrees() ) << " "
                << QString::number( pointList().at( i )->dec0().Degrees() ) << " "
                << epoch ( i ) << " "
                << imageName( i ).replace( ' ', '_' ) << " "
                << label( i ) << " "
                << labelColor( i ).name() << "\n";
    }

    QFile::remove( KStandardDirs::locateLocal( "appdata", "flags.dat" ) );
    file.rename( KStandardDirs::locateLocal( "appdata", "flags.dat" ) );
    file.close();
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

void FlagComponent::add( SkyPoint* flagPoint, QString epoch, QString image, QString label, QColor labelColor ) {
    pointList().append( flagPoint );
    m_Epoch.append( epoch );

    for(int i = 0; i<m_Names.size(); i++ ) {
        if( image == m_Names.at( i ) )
            m_FlagImages.append( i );
    }

    m_Labels.append( label );
    m_LabelColors.append( labelColor );
}

void FlagComponent::remove( int index ) {
    // check if flag of required index exists
    if ( index > pointList().size() - 1 ) {
        return;
    }

    pointList().removeAt( index );
    m_Epoch.removeAt( index );
    m_FlagImages.removeAt( index );
    m_Labels.removeAt( index );
    m_LabelColors.removeAt( index );

    // request SkyMap update
    KStars::Instance()->map()->forceUpdate();
}

void FlagComponent::updateFlag( int index, SkyPoint *flagPoint, QString epoch, QString image, QString label, QColor labelColor ) {
    if ( index > pointList().size() -1 ) {
        return;
    }
    delete pointList().at( index );
    pointList().replace( index, flagPoint);

    m_Epoch.replace( index, epoch );

    for(int i = 0; i<m_Names.size(); i++ ) {
        if( image == m_Names.at( i ) )
            m_FlagImages.replace( index, i );
    }

    m_Labels.replace( index, label );
    m_LabelColors.replace( index, labelColor );
}

void FlagComponent::slotLoadImages( KIO::Job*, const KIO::UDSEntryList& list ) {
    // Add the default flag images to available images list
    m_Names.append( i18n( "No icon" ) );
    m_Images.append( QImage() );
    m_Names.append( i18n( "Default" ) );
    m_Images.append( QImage( KStandardDirs::locate( "appdata", "defaultflag.gif" ) ));

    // Add all other images found in user appdata directory
    foreach( KIO::UDSEntry entry, list) {
        KFileItem item(entry, m_Job->url(), false, true);
        if( item.name().startsWith( "_flag" ) ) {
            QString fileName = item.name()
                .replace(QRegExp("\\.[^.]*$"), QString())
                .replace(QRegExp("^_flag"),   QString())
                .replace('_',' ');
            m_Names.append( fileName );
            m_Images.append( QImage( item.localPath() ));
        }
    }
}

void FlagComponent::slotInit( KJob *job ) {
    Q_UNUSED (job)

    loadFromFile();

    // Redraw Skymap
    KStars::Instance()->map()->forceUpdate(false);
}


QStringList FlagComponent::getNames() {
    return m_Names;
}

int FlagComponent::size() {
    return pointList().size();
}

QString FlagComponent::epoch( int index ) {
    if ( index > m_Epoch.size() - 1 ) {
        return QString();
    }

    return m_Epoch.at( index );
}

QString FlagComponent::label( int index ) {
    if ( index > m_Labels.size() - 1 ) {
        return QString();
    }

    return m_Labels.at( index );
}

QColor FlagComponent::labelColor( int index ) {
    if ( index > m_LabelColors.size() -1 ) {
        return QColor();
    }

    return m_LabelColors.at( index );
}

QImage FlagComponent::image( int index ) {
    if ( index > m_FlagImages.size() - 1 ) {
        return QImage();
    }

    if ( m_FlagImages.at( index ) > m_Images.size() - 1 ) {
        return QImage();
    }

    return m_Images.at( m_FlagImages.at( index ) );
}

QString FlagComponent::imageName( int index ) {
    if ( index > m_FlagImages.size() - 1 ) {
        return QString();
    }

    if ( m_FlagImages.at( index ) > m_Names.size() - 1 ) {
        return QString();
    }

    return m_Names.at( m_FlagImages.at( index ) );
}

QList<QImage> FlagComponent::imageList() {
    return m_Images;
}

QList<int> FlagComponent::getFlagsNear( SkyPoint *point, float radius )
{
    QList<int> retVal;

    int idx = 0;
    foreach (SkyPoint *cp, pointList()) {
        if ( cp->angularDistanceTo(point).Degrees() <= radius ) {
            retVal.append( idx );
        }

        idx++;
    }

    return retVal;
}

QList<int> FlagComponent::getFlagsNearPix ( SkyPoint *point, int pixelRadius )
{
    const Projector *proj = KStars::Instance()->map()->projector();
    QPointF pos = proj->toScreen(point);
    QList<int> retVal;

    int ptr = 0;
    foreach ( SkyPoint *cp, pointList() ) {
        QPointF pos2 = proj->toScreen(cp);
        int dx = (pos2 - pos).x();
        int dy = (pos2 - pos).y();

        if ( sqrt( dx * dx + dy * dy ) <= pixelRadius ) {
            //point is inside pixelRadius circle
            retVal.append(ptr);
        }

        ptr++;
    }

    return retVal;
}

QImage FlagComponent::imageList( int index ) {
    if ( index > m_Images.size() - 1 ) {
        return QImage();
    }

    return m_Images.at( index );
}
