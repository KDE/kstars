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
#include <qnumeric.h>


#include <QUrl>

#include <cmath>
#include <QStandardPaths>

bool KSUtils::openDataFile( QFile &file, const QString &s ) {
    QString FileName = QStandardPaths::locate(QStandardPaths::DataLocation, s );
    if ( !FileName.isNull() ) {
        file.setFileName( FileName );
        return file.open( QIODevice::ReadOnly );
    }
    return false;
}

QString KSUtils::getDSSURL( const SkyPoint * const p ) {
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

        return getDSSURL( p->ra0(), p->dec0(), width, height );
}

QString KSUtils::getDSSURL( const dms &ra, const dms &dec, float width, float height, const QString & type) {
    const QString URLprefix( "http://archive.stsci.edu/cgi-bin/dss_search?v=poss2ukstu_blue" );
    QString URLsuffix = QString( "&e=J2000&f=%1&c=none&fov=NONE" ).arg(type);
    const double dss_default_size = Options::defaultDSSImageSize();

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
    RAString  = RAString.sprintf( "&r=%02d+%02d+%02d", ra.hour(), ra.minute(), ra.second() );
    SizeString = SizeString.sprintf( "&h=%02.1f&w=%02.1f", height, width );

    return ( URLprefix + RAString + DecString + SizeString + URLsuffix );
}

QString KSUtils::toDirectionString( dms angle ) {
    // TODO: Instead of doing it this way, it would be nicer to
    // compute the string to arbitrary precision. Although that will
    // not be easy to localize.  (Consider, for instance, Indian
    // languages that have special names for the intercardinal points)
    // -- asimha

    static const char *directions[] = {
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "N"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "NNE"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "NE"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "ENE"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "E"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "ESE"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "SE"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "SSE"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "S"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "SSW"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "SW"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "WSW"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "W"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "WNW"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "NW"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "NNW"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "N"),
    };

    int index = (int)( (angle.Degrees() + 11.25) / 22.5); // A number between 0 and 16 (inclusive), 16 meaning the same thing as zero.

    return i18nc( "Abbreviated cardinal / intercardinal etc. direction", directions[ index ] );
}
