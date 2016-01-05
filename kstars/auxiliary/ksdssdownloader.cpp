/***************************************************************************
                 ksdssdownloader.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Tue 05 Jan 2016 03:39:18 CST
    copyright            : (c) 2016 by Akarsh Simha
    email                : akarsh.simha@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


/* Project Includes */
#include "ksdssdownloader.h"
#include "ksdssimage.h"
#include "skypoint.h"
#include "skyobject.h"
#include "deepskyobject.h"
#include "dms.h"
#include "Options.h"

/* KDE Includes */
#include <KIO/Job>
#include <KIO/CopyJob>

/* Qt Includes */
#include <QString>
#include <QImage>
#include <QImageWriter>
#include <QUrl>
#include <QMimeDatabase>
#include <QMimeType>
#include <QTemporaryFile>


KSDssDownloader::KSDssDownloader(const SkyPoint * const p, const QString &destFileName, QObject *parent ) : QObject( parent ) {
    // Initialize version preferences. FIXME: This must be made a
    // user-changeable option just in case someone likes red
    m_VersionPreference << "poss2ukstu_blue" << "poss2ukstu_red" << "poss1_blue" << "poss1_red" << "quickv" << "poss2ukstu_ir";
    m_TempFile.open();
    startDownload( p, destFileName );
}

QString KSDssDownloader::getDSSURL( const SkyPoint * const p, const QString &version, struct KSDssImage::Metadata *md ) {
        const DeepSkyObject *dso = 0;
        double height, width;

        double dss_default_size = Options::defaultDSSImageSize();
        double dss_padding = Options::dSSPadding();

        Q_ASSERT( p );
        Q_ASSERT( dss_default_size > 0.0 && dss_padding >= 0.0 );

        dso = dynamic_cast<const DeepSkyObject *>( p );

        // Decide what to do about the height and width
        if( dso ) {
            // For deep-sky objects, use their height and width information
            double a, b, pa;
            a = dso->a();
            b = dso->a() * dso->e(); // Use a * e instead of b, since e() returns 1 whenever one of the dimensions is zero. This is important for circular objects
            pa = dso->pa() * dms::DegToRad;

            // We now want to convert a, b, and pa into an image
            // height and width -- i.e. a dRA and dDec.
            // DSS uses dDec for height and dRA for width. (i.e. "top" is north in the DSS images, AFAICT)
            // From some trigonometry, assuming we have a rectangular object (worst case), we need:
            width = a * sin( pa ) + b * cos( pa );
            height = a * cos( pa ) + b * sin( pa );
            // 'a' and 'b' are in arcminutes, so height and width are in arcminutes

            // Pad the RA and Dec, so that we show more of the sky than just the object.
            height += dss_padding;
            width += dss_padding;
        }
        else {
            // For a generic sky object, we don't know what to do. So
            // we just assume the default size.
            height = width = dss_default_size;
        }
        // There's no point in tiny DSS images that are smaller than dss_default_size
        if( height < dss_default_size )
            height = dss_default_size;
        if( width < dss_default_size )
            width = dss_default_size;

        QString URL = getDSSURL( p->ra0(), p->dec0(), width, height, "gif", version, md );
        if( md ) {
            const SkyObject *obj = 0;
            obj = dynamic_cast<const SkyObject *>( p );
            if( obj && obj->hasName() )
                md->object = obj->name();
        }
        return URL;
}

QString KSDssDownloader::getDSSURL( const dms &ra, const dms &dec, float width, float height, const QString & type_, const QString & version_, struct KSDssImage::Metadata *md ) {

    const double dss_default_size = Options::defaultDSSImageSize();

    QString version = version_.toLower();
    QString type = type_.toLower();
    QString URLprefix = QString( "http://archive.stsci.edu/cgi-bin/dss_search?v=%1&" ).arg(version);
    QString URLsuffix = QString( "&e=J2000&f=%1&c=none&fov=NONE" ).arg(type);

    char decsgn = ( dec.Degrees() < 0.0 ) ? '-' : '+';
    int dd = abs( dec.degree() );
    int dm = abs( dec.arcmin() );
    int ds = abs( dec.arcsec() );

    // Infinite and NaN sizes are replaced by the default size and tiny DSS images are resized to default size
    if( !qIsFinite( height ) || height <= 0.0 )
        height = dss_default_size;
    if( !qIsFinite( width ) || width <= 0.0)
        width = dss_default_size;

    // DSS accepts images that are no larger than 75 arcminutes
    if( height > 75.0 )
        height = 75.0;
    if( width > 75.0 )
        width = 75.0;

    QString RAString, DecString, SizeString;
    DecString = DecString.sprintf( "&d=%c%02d+%02d+%02d", decsgn, dd, dm, ds );
    RAString  = RAString.sprintf( "r=%02d+%02d+%02d", ra.hour(), ra.minute(), ra.second() );
    SizeString = SizeString.sprintf( "&h=%02.1f&w=%02.1f", height, width );

    if( md ) {
        md->src = KSDssImage::Metadata::DSS;
        md->width = width;
        md->height = height;
        md->ra0 = ra;
        md->dec0 = dec;
        md->version = version;
        md->format = ( type.contains( "fit" ) ? KSDssImage::Metadata::FITS : KSDssImage::Metadata::GIF );
        md->height = height;
        md->width = width;

        if( version.contains( "poss2" ) )
            md->gen = 2;
        else if( version.contains( "poss1" ) )
            md->gen = 1;
        else if( version.contains( "quickv" ) )
            md->gen = 4;
        else
            md->gen = -1;

        if( version.contains( "red" ) )
            md->band='R';
        else if( version.contains( "blue" ) )
            md->band='B';
        else if( version.contains( "ir" ) )
            md->band='I';
        else if( version.contains( "quickv" ) )
            md->band='V';
        else
            md->band='?';

        md->object = QString();
    }

    return ( URLprefix + RAString + DecString + SizeString + URLsuffix );
}

void KSDssDownloader::initiateSingleDownloadAttempt( QUrl srcUrl ) {
    qDebug() << "Temp file is at " << m_TempFile.fileName();
    QUrl fileUrl = QUrl::fromLocalFile( m_TempFile.fileName() );
    qDebug() << "Attempt #" << m_attempt << "downloading DSS Image. URL: " << srcUrl << " to " << fileUrl;
    m_DownloadJob = KIO::copy( srcUrl, fileUrl, KIO::Overwrite ) ; // FIXME: Can be done with pure Qt
    connect ( m_DownloadJob, SIGNAL ( result (KJob *) ), SLOT ( downloadAttemptFinished() ) );
}

void KSDssDownloader::startDownload( const SkyPoint * const p, const QString &destFileName ) {
    QUrl srcUrl;
    m_FileName = destFileName;
    m_attempt = 0;
    srcUrl.setUrl( getDSSURL( p, m_VersionPreference[m_attempt], &m_AttemptData ) );
    initiateSingleDownloadAttempt( srcUrl );
}

void KSDssDownloader::downloadAttemptFinished() {
    // Check if there was an error downloading itself
    if( m_DownloadJob->error() != 0 ) {
        qDebug() << "Error " << m_DownloadJob->error() << " downloading DSS images!";
        emit downloadComplete( false );
        deleteLater();
        return;
    }

    if( m_AttemptData.src == KSDssImage::Metadata::SDSS ) {
        // FIXME: do SDSS-y things
        emit downloadComplete( false );
        deleteLater();
        return;
    }
    else {
        // Check if we have a proper DSS image or the DSS server failed
        QMimeDatabase mdb;
        QMimeType mt = mdb.mimeTypeForFile( m_TempFile.fileName(), QMimeDatabase::MatchContent );
        if( mt.name().contains( "image", Qt::CaseInsensitive ) ) {
            qDebug() << "DSS download was successful";
            emit downloadComplete( writeImageFile() );
            deleteLater();
            return;
        }

        // We must have failed, try the next attempt
        QUrl srcUrl;
        m_attempt++;
        if( m_attempt == m_VersionPreference.count() ) {
            // Nothing downloaded... very strange. Fail.
            qDebug() << "Error downloading DSS images: All alternatives failed!";
            emit downloadComplete( false );
            deleteLater();
            return;
        }
        srcUrl.setUrl( getDSSURL( m_AttemptData.ra0, m_AttemptData.dec0, m_AttemptData.width, m_AttemptData.height,
                                  ( ( m_AttemptData.format == KSDssImage::Metadata::FITS ) ? "fits" : "gif" ),
                                  m_VersionPreference[ m_attempt ], &m_AttemptData ) );
        initiateSingleDownloadAttempt( srcUrl );
    }
}

bool KSDssDownloader::writeImageFile() {
    // Write the temporary file into an image file with metadata
    QImage img( m_TempFile.fileName() );
    QImageWriter writer( m_FileName, "png" );
    writer.setText( "Source", QString::number( KSDssImage::Metadata::DSS ) );
    writer.setText( "Format", QString::number( KSDssImage::Metadata::PNG ) );
    writer.setText( "Version", m_AttemptData.version );
    writer.setText( "Object", m_AttemptData.object );
    writer.setText( "RA", m_AttemptData.ra0.toHMSString() );
    writer.setText( "Dec", m_AttemptData.dec0.toDMSString() );
    writer.setText( "Width", QString::number( m_AttemptData.width ) );
    writer.setText( "Height", QString::number( m_AttemptData.height ) );
    writer.setText( "Band", QString() + m_AttemptData.band );
    writer.setText( "Generation", QString::number( m_AttemptData.gen ) );
    writer.setText( "Author", "KStars KSDssDownloader" );
    return writer.write( img );
}
