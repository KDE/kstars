/***************************************************************************
                 nameresolver.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun 24 Aug 2014 02:28:09 CDT
    copyright            : (c) 2014 by Akarsh Simha
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
#include "nameresolver.h"
#include "../../datahandlers/catalogentrydata.h"

/* KDE Includes */
#ifndef KSTARS_LITE
#include <kio/filecopyjob.h>
#else
#include "kstarslite.h"
#endif

/* Qt Includes */
#include <QUrl>
#include <QTemporaryFile>
#include <QString>
#include <QXmlStreamReader>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QDebug>
#include <QEventLoop>

class CatalogEntryData NameResolver::resolveName( const QString &name ) {
    class CatalogEntryData data;

    data.long_name = name;
    if( !NameResolverInternals::sesameResolver( data, name ) ) {
        QString msg = xi18n("Error: sesameResolver failed. Could not resolve name on CDS Sesame!");
        qDebug() << msg;

#ifdef KSTARS_LITE
        KStarsLite::Instance()->notificationMessage(msg);
#endif
        return data; // default data structure with no information
    }
    // More to be done here if the resolved name is SIMBAD
    return data;

}

bool NameResolver::NameResolverInternals::sesameResolver( class CatalogEntryData &data, const QString &name ) {

    QUrl resolverUrl = QUrl( QString( "http://cdsweb.u-strasbg.fr/cgi-bin/nph-sesame/-oxpFI/SNV?%1" ).arg( name ) );

    QString msg = xi18n("Attempting to resolve object %1 using CDS Sesame.", name);
    qDebug() << msg;

#ifdef KSTARS_LITE
    KStarsLite::Instance()->notificationMessage(msg);
#endif

    QNetworkAccessManager manager;
    QNetworkReply *response = manager.get( QNetworkRequest( resolverUrl ) );
    Q_ASSERT( response );

    // Wait synchronously
    QEventLoop event;
    QObject::connect( response, SIGNAL( finished() ), &event, SLOT( quit() ) );
    event.exec();

    if (response->error() != QNetworkReply::NoError) {
        msg = xi18n("Error trying to get XML response from CDS Sesame server: %1", response->errorString());
        qWarning() << msg;

    #ifdef KSTARS_LITE
        KStarsLite::Instance()->notificationMessage(msg);
    #endif
        return false;
    }

    QXmlStreamReader xml( response->readAll() );
    response->deleteLater();
    if( xml.atEnd() ) {
        // file is empty
        msg = xi18n("Empty result instead of expected XML from CDS Sesame! Maybe bad internet connection?");
        qDebug() << msg;

    #ifdef KSTARS_LITE
        KStarsLite::Instance()->notificationMessage(msg);
    #endif
        return false;
    }

    while( !xml.atEnd() && !xml.hasError() ) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if( xml.isStartDocument() )
            continue;

        if( token == QXmlStreamReader::StartElement ) {
            qDebug() << "Parsing token with name " << xml.name();

            if( xml.name() == "Resolver" ) {
                // This is the section we want
                char resolver=0;
                QXmlStreamAttributes attributes = xml.attributes();
                if( attributes.hasAttribute( "name" ) )
                    resolver = attributes.value("name").at( 0 ).toLatin1(); // Expected to be S (Simbad), V (VizieR), or N (NED)
                else {
                    resolver = 0; // NUL character for unknown resolver
                    qWarning() << "Warning: Unknown resolver " << attributes.value( "name " ) << " while reading output from CDS Sesame";
                }

                qDebug() << "Resolved by " << resolver << attributes.value( "name" ) << "!";

                // Start reading the data to pick out the relevant ones
                while( xml.readNextStartElement() ) {
                    if( xml.name() == "otype" ) {
                        const QString typeString = xml.readElementText();
                        data.type = interpretObjectType( typeString );
                    }
                    else if( xml.name() == "jradeg" ) {
                        data.ra = xml.readElementText().toDouble();
                    }
                    else if( xml.name() == "jdedeg" ) {
                        data.dec = xml.readElementText().toDouble();
                    }
                    else if( xml.name() == "mag" ) {
                        attributes = xml.attributes();
                        char band;
                        if( attributes.hasAttribute( "band" ) ) {
                            band = attributes.value("band").at(0).toLatin1();
                        }
                        else {
                            qWarning() << "Warning: Magnitude of unknown band found while reading output from CDS Sesame";
                            band = 0;
                        }

                        float mag = NaN::f;
                        xml.readNext();
                        if( xml.isCharacters() ) {
                            qDebug() << "characters: " << xml.tokenString();
                            mag = xml.tokenString().toFloat();
                        }
                        else if( xml.isStartElement() ) {
                            while( xml.name() != "v" ) {
                                qDebug() << "element: " << xml.name();
                                xml.readNextStartElement();
                            }
                            mag = xml.readElementText().toFloat();
                            qDebug() << "Got " << xml.tokenString() << " mag = " << mag;
                            while( !xml.atEnd() && xml.readNext() && xml.name() != "mag" ); // finish reading the <mag> tag all the way to </mag>
                        }
                        else
                            qWarning() << "Failed to parse Xml token in magnitude element: " << xml.tokenString();

                        if( band == 'V' ) {
                            data.magnitude = mag;
                        }
                        else if( band == 'B' ) {
                            data.flux = mag; // FIXME: This is bad
                            if( std::isnan( data.magnitude ) )
                                data.magnitude = mag; // FIXME: This is bad too
                        }
                        // Don't know what to do with other magnitudes, until we have a magnitude hash
                    }
                    else if( xml.name() == "oname" ) { // Primary identifier
                        QRegExp regex(" *([a-ZA-Z][a-zA-Z ]*) *([0-9]+) *");
                        QString contents = xml.readElementText();
                        if( contents.contains( regex ) ) { // Has a simple catalog + ID format (this excludes designations like Foo JHHMMMSS+DDMMSS as CatalogEntryData currently doesn't support them, but includes designations like VII Zw 466, where VII Zw will take the place of the catalog)
                            data.catalog_name = regex.cap(1);
                            data.ID = regex.cap(2).toInt();
                        }
                    }
                    else
                        xml.skipCurrentElement();
                    // TODO: Parse aliases for common names
                }
                break;

            }
            else
                continue;
        }
    }
    if( xml.hasError() ) {
        msg = xi18n("Error parsing XML from CDS Sesame: %1 on line %2 @ col = %3", xml.errorString(), xml.lineNumber(), xml.columnNumber());
        qDebug() << msg;

    #ifdef KSTARS_LITE
        KStarsLite::Instance()->notificationMessage(msg);
    #endif
        return false;
    }

    msg = xi18n("Resolved %1 successfully!", name);
    qDebug() << msg;

#ifdef KSTARS_LITE
    KStarsLite::Instance()->notificationMessage(msg);
#endif
    qDebug() << "Object type: " << SkyObject::typeName( data.type ) << "; Coordinates: " << data.ra << ";" << data.dec;
    return true;
}

// bool NameResolver::NameResolverInternals::getDataFromSimbad( class CatalogEntryData &data ) {
//     // TODO: Implement
//     // QUrl( QString( "http://simbad.u-strasbg.fr/simbad/sim-script?script=output%20console=off%20script=off%0Aformat%20object%20%22%25DIM%22%0A" ) + data.name );
// }

SkyObject::TYPE NameResolver::NameResolverInternals::interpretObjectType( const QString &typeString, bool caseSensitive ) {

    // FIXME: Due to the quirks of Sesame (SIMBAD vs NED etc), it
    // might be very difficult to discern the type in all cases.  The
    // best way TODO things might be to first run the query with NED,
    // and if it is extragalactic, then trust NED and
    // accept. Otherwise, or if NED did not return a result, re-run
    // the query on SIMBAD and VizieR and use that result, if any.

    // See http://cds.u-strasbg.fr/cgi-bin/Otype?IR for Object Classification in SIMBAD

    // Highest likelihood is a galaxy of some form, so we process that first
    const QString &s = typeString; // To make the code compact
    Qt::CaseSensitivity cs = ( caseSensitive  ? Qt::CaseSensitive : Qt::CaseInsensitive );

    QStringList galaxyTypes;
    QStringList galaxyGroupTypes;
    QStringList openClusterTypes;
    QStringList radioSourceTypes;
    galaxyTypes << "G" << "LIN" << "AGN" << "GiG" << "GiC" << "H2G" << "BiC"
                << "GiP" << "HzG" << "rG" << "AG?" << "EmG" << "LSB" << "SBG"
                << "bCG" << "SyG" << "Sy1" << "Sy2" << "Gxy";

    galaxyGroupTypes << "GClstr" << "GGroup" << "GPair" << "ClG" << "CGG"
                     << "PaG" << "IG" << "GrG" << "SCG"; // NOTE (FIXME?): Compact groups and pairs of galaxies ar treated like galaxy clusters

    openClusterTypes << "*Cl" << "Cl*" << "OpC" << "As*" << "St*" << "OCl";

    radioSourceTypes << "Rad" << "mR" << "cm" << "mm" << "smm" << "HI" << "rB" << "Mas";

    if ( galaxyTypes.contains( s, cs ) )
        return SkyObject::GALAXY;

    if ( galaxyGroupTypes.contains( s, cs ) )
        return SkyObject::GALAXY_CLUSTER;

    if ( openClusterTypes.contains( s, cs ) )
        return SkyObject::OPEN_CLUSTER; // FIXME: NED doesn't distinguish between globular clusters and open clusters!!

    auto check = [typeString, cs]( const QString &type ) {
        return ( !QString::compare( typeString, type, cs ) );
    };

    if( check( "GlC" ) || check( "GlCl" ) )
        return SkyObject::GLOBULAR_CLUSTER;
    if( check( "Neb" ) || check( "HII" ) || check( "HH" ) ) // FIXME: The last one is Herbig-Haro object
        return SkyObject::GASEOUS_NEBULA;
    if( check( "SNR" ) ) // FIXME: Simbad returns "ISM" for Veil Nebula (Interstellar Medium??)
        return SkyObject::SUPERNOVA_REMNANT;
    if( check( "PN" ) || check( "PNeb" )
        || check( "PNe" ) || check( "pA*" ) ) // FIXME: The latter is actually Proto PN
        return SkyObject::PLANETARY_NEBULA;
    if( typeString == "*" )
        return SkyObject::CATALOG_STAR;
    if( check( "QSO" ) )
        return SkyObject::QUASAR;
    if( check( "DN" ) || check( "DNe" )
        || check( "DNeb" ) || check( "glb" ) ) // The latter is Bok globule
        return SkyObject::DARK_NEBULA;
    if ( radioSourceTypes.contains( s, cs ) )
        return SkyObject::RADIO_SOURCE;
    if( typeString == "**" )
        return SkyObject::MULT_STAR;
    if( typeString.contains('*') && !check( "C?*" ) )
        return SkyObject::CATALOG_STAR;

    return SkyObject::TYPE_UNKNOWN;
    // FIXME: complete this method

}
