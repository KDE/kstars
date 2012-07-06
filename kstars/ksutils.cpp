/***************************************************************************
                          ksutils.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Jan  7 10:48:09 EST 2002
    copyright            : (C) 2002 by Mark Hollomon
    email                : mhh@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "ksutils.h"

#include "deepskyobject.h"
#include "skyobject.h"
#include "Options.h"

#include <QFile>

#include <kstandarddirs.h>
#include <KUrl>

#include <math.h>

bool KSUtils::openDataFile( QFile &file, const QString &s ) {
    QString FileName = KStandardDirs::locate( "appdata", s );
    if ( !FileName.isNull() ) {
        file.setFileName( FileName );
        return file.open( QIODevice::ReadOnly );
    }
    return false;
}

QString KSUtils::getDSSURL( const SkyPoint * const p ) {
        const DeepSkyObject *dso = 0;
        dms ra(0.0), dec(0.0);
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
            // TODO: Deal with round objects, which may have undefined 'b' and 'pa', but a sensible 'a'.

            // We now want to convert a, b, and pa into an image
            // height and width -- i.e. a dRA and dDec.
            // DSS uses dDec for height and dRA for width. (i.e. "top" is north in the DSS images, AFAICT)
            // From some trigonometry, assuming we have a rectangular object (worst case), we need:
            width = a * sin( pa ) + b * cos( pa );
            height = a * cos( pa ) + b * sin( pa );
            // 'a' and 'b' are in arcminutes, so height and width are in arcminutes

            // Pad the RA and Dec, so that we show more of the sky than just the object.
            // TODO: Make padding user-configurable
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
        // DSS accepts images that are no larger than 75 arcminutes
        if( height > 75.0 )
            height = 75.0;
        if( width > 75.0 )
            width = 75.0;

        return getDSSURL( p->ra0(), p->dec(), width, height );
}

QString KSUtils::getDSSURL( const dms &ra, const dms &dec, float width, float height ) {
    const QString URLprefix( "http://archive.stsci.edu/cgi-bin/dss_search?v=1" );
    const QString URLsuffix( "&e=J2000&f=gif&c=none&fov=NONE" );
    const double dss_default_size = Options::defaultDSSImageSize();

    char decsgn = ( dec.Degrees() < 0.0 ) ? '-' : '+';
    int dd = abs( dec.degree() );
    int dm = abs( dec.arcmin() );
    int ds = abs( dec.arcsec() );

    // Infinite and NaN sizes are replaced by the default size
    if( !isinf( height ) )
        height = dss_default_size;
    if( !isinf( width ) )
        width = dss_default_size;

    // Negative / zero sizes are replaced by the default size 
    if( height <= 0.0 )
        height = dss_default_size;
    if( width <= 0.0 )
        width = dss_default_size;

    // DSS accepts images that are no larger than 75 arcminutes
    if( height > 75.0 )
        height = 75.0;
    if( width > 75.0 )
        width = 75.0;

    QString RAString, DecString, SizeString;
    DecString = DecString.sprintf( "&d=%c%02d+%02d+%02d", decsgn, dd, dm, ds );
    RAString  = RAString.sprintf( "&r=%02d+%02d+%02d", ra.hour(), ra.minute(), ra.second() );
    SizeString = SizeString.sprintf( "&h=%02.1f&w=%02.1f", height, width );

    return ( URLprefix + RAString + DecString + SizeString + URLsuffix );

}
