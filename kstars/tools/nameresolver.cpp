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
#include <kio/filecopyjob.h>

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
        qDebug() << "Error: sesameResolver failed. Could not resolve name on CDS Sesame!";
        return data; // default data structure with no information
    }
    // More to be done here if the resolved name is SIMBAD
    return data;

}

bool NameResolver::NameResolverInternals::sesameResolver( class CatalogEntryData &data, const QString &name ) {

    QUrl resolverUrl = QUrl( QString( "http://cdsweb.u-strasbg.fr/cgi-bin/nph-sesame/-oxpFI/SNV?%1" ).arg( name ) );

    qDebug() << "Attempting to resolve object " << name << "using CDS Sesame.";
    QNetworkAccessManager manager;
    QNetworkReply *response = manager.get( QNetworkRequest( resolverUrl ) );

    // Wait synchronously
    QEventLoop event;
    QObject::connect( response, SIGNAL( finished() ), &event, SLOT( quit() ) );
    event.exec();

    if (response->error() != QNetworkReply::NoError) {
        qWarning() << "Error trying to get XML response from CDS Sesame server: " << response->errorString();
        return false;
    }

    QXmlStreamReader xml( response->readAll() );
    if( xml.atEnd() ) {
        // file is empty
        qDebug() << "Empty result instead of expected XML from CDS Sesame! Maybe bad internet connection?";
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
                char resolver;
                QXmlStreamAttributes attributes = xml.attributes();
                if( attributes.hasAttribute( "name" ) )
                    resolver = attributes.value("name").at( 0 ).toLatin1(); // Expected to be S (Simbad), V (VizieR), or N (NED)
                else {
                    resolver = 0; // NUL character for unknown resolver
                    qWarning() << "Warning: Unknown resolver " << attributes.value( "name " ) << " while reading output from CDS Sesame";
                }
                qDebug() << "Resolved by " << attributes.value( "name" ) << "!";

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
                            attributes.value("band").at(0).toLatin1();
                        }
                        else {
                            qWarning() << "Warning: Magnitude of unknown band found while reading output from CDS Sesame";
                            band = 0;
                        }

                        if( band == 'V' ) {
                            data.magnitude = xml.readElementText().toFloat();
                        }
                        else if( band == 'B' ) {
                            data.flux = xml.readElementText().toFloat(); // FIXME: This is bad
                        }
                        // Don't know what to do with other magnitudes, until we have a magnitude hash
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
        qDebug() << "Error parsing XML from CDS Sesame: " << xml.errorString() << " on line " << xml.lineNumber() << " @ col = " << xml.columnNumber();
        return false;
    }
    qDebug() << "Resolved " << name << " successfully!";
    qDebug() << "Object type: " << SkyObject::typeName( data.type ) << "; Coordinates: " << data.ra << ";" << data.dec;
    return true;
}

// bool NameResolver::NameResolverInternals::getDataFromSimbad( class CatalogEntryData &data ) {
//     // TODO: Implement
//     // QUrl( QString( "http://simbad.u-strasbg.fr/simbad/sim-script?script=output%20console=off%20script=off%0Aformat%20object%20%22%25DIM%22%0A" ) + data.name );
// }

SkyObject::TYPE NameResolver::NameResolverInternals::interpretObjectType( const QString &typeString ) {

    // FIXME: Due to the quirks of Sesame (SIMBAD vs NED etc), it
    // might be very difficult to discern the type in all cases.  The
    // best way TODO things might be to first run the query with NED,
    // and if it is extragalactic, then trust NED and
    // accept. Otherwise, or if NED did not return a result, re-run
    // the query on SIMBAD and VizieR and use that result, if any.

    // Highest likelihood is a galaxy of some form
    if( typeString == "G" || typeString == "LIN" || typeString == "AGN" || typeString == "GiG" || typeString == "H2G" ) {
        // this is a galaxy
        return SkyObject::GALAXY;
    }
    // Next possibility is galaxy cluster
    if( typeString == "GClstr" || typeString == "GGroup" || typeString == "GPair" || typeString == "ClG" || typeString == "CGG" ) { // NOTE (FIXME?): Compact groups and pairs of galaxies are treated like galaxy clusters
        return SkyObject::GALAXY_CLUSTER;
    }
    if( typeString == "*Cl" || typeString == "Cl*" || typeString == "OpC" )
        return SkyObject::OPEN_CLUSTER; // FIXME: NED doesn't distinguish between globular clusters and open clusters!!
    if( typeString == "GlC" )
        return SkyObject::GLOBULAR_CLUSTER;
    if( typeString == "Neb" || typeString == "HII" )
        return SkyObject::GASEOUS_NEBULA;
    if( typeString == "SNR" ) // FIXME: Simbad returns "ISM" for Veil Nebula (Interstellar Medium??)
        return SkyObject::SUPERNOVA_REMNANT;
    if( typeString == "PN" )
        return SkyObject::PLANETARY_NEBULA;
    if( typeString == "*" )
        return SkyObject::STAR;

    return SkyObject::TYPE_UNKNOWN;
    // FIXME: complete this method

}
