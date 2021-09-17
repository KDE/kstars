/*
    SPDX-FileCopyrightText: 2014 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/* Project Includes */
#include "nameresolver.h"
#include "catalogobject.h"

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
#include <QEventLoop>

#include <kstars_debug.h>

std::pair<bool, CatalogObject> NameResolver::resolveName(const QString &name)
{
    const auto &found_sesame{ NameResolverInternals::sesameResolver(name) };
    if (!found_sesame.first)
    {
        QString msg =
            xi18n("Error: sesameResolver failed. Could not resolve name on CDS Sesame.");
        qCDebug(KSTARS) << msg;

#ifdef KSTARS_LITE
        KStarsLite::Instance()->notificationMessage(msg);
#endif
    }
    // More to be done here if the resolved name is SIMBAD
    return found_sesame;
}

std::pair<bool, CatalogObject>
NameResolver::NameResolverInternals::sesameResolver(const QString &name)
{
    QUrl resolverUrl = QUrl(
        QString("http://cdsweb.u-strasbg.fr/cgi-bin/nph-sesame/-oxpFI/SNV?%1").arg(name));

    QString msg = xi18n("Attempting to resolve object %1 using CDS Sesame.", name);
    qCDebug(KSTARS) << msg;

#ifdef KSTARS_LITE
    KStarsLite::Instance()->notificationMessage(msg);
#endif

    QNetworkAccessManager manager;
    QNetworkReply *response = manager.get(QNetworkRequest(resolverUrl));
    Q_ASSERT(response);

    // Wait synchronously
    QEventLoop event;
    QObject::connect(response, SIGNAL(finished()), &event, SLOT(quit()));
    event.exec();

    if (response->error() != QNetworkReply::NoError)
    {
        msg = xi18n("Error trying to get XML response from CDS Sesame server: %1",
                    response->errorString());
        qWarning() << msg;

#ifdef KSTARS_LITE
        KStarsLite::Instance()->notificationMessage(msg);
#endif
        return { false, {} };
    }

    QXmlStreamReader xml(response->readAll());
    response->deleteLater();
    if (xml.atEnd())
    {
        // file is empty
        msg = xi18n("Empty result instead of expected XML from CDS Sesame. Maybe bad "
                    "Internet connection?");
        qCDebug(KSTARS) << msg;

#ifdef KSTARS_LITE
        KStarsLite::Instance()->notificationMessage(msg);
#endif
        return { false, {} };
    }

    CatalogObject data{
        CatalogObject::oid{}, SkyObject::STAR, dms{ 0 }, dms{ 0 }, 0, name, name
    };

    bool found{ false };
    while (!xml.atEnd() && !xml.hasError())
    {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (xml.isStartDocument())
            continue;

        if (token == QXmlStreamReader::StartElement)
        {
            qCDebug(KSTARS) << "Parsing token with name " << xml.name();

            if (xml.name() == "Resolver")
            {
                found = true;
                // This is the section we want
                char resolver                   = 0;
                QXmlStreamAttributes attributes = xml.attributes();
                if (attributes.hasAttribute("name"))
                    resolver =
                        attributes.value("name")
                            .at(0)
                            .toLatin1(); // Expected to be S (Simbad), V (VizieR), or N (NED)
                else
                {
                    resolver = 0; // NUL character for unknown resolver
                    qWarning() << "Warning: Unknown resolver "
                               << attributes.value("name ")
                               << " while reading output from CDS Sesame";
                }

                qCDebug(KSTARS)
                    << "Resolved by " << resolver << attributes.value("name") << "!";

                // Start reading the data to pick out the relevant ones
                while (xml.readNextStartElement())
                {
                    if (xml.name() == "otype")
                    {
                        const QString typeString = xml.readElementText();
                        data.setType(interpretObjectType(typeString));
                    }
                    else if (xml.name() == "jradeg")
                    {
                        data.setRA0(dms{ xml.readElementText().toDouble() });
                    }
                    else if (xml.name() == "jdedeg")
                    {
                        data.setDec0(dms{ xml.readElementText().toDouble() });
                    }
                    else if (xml.name() == "mag")
                    {
                        attributes = xml.attributes();
                        char band;
                        if (attributes.hasAttribute("band"))
                        {
                            band = attributes.value("band").at(0).toLatin1();
                        }
                        else
                        {
                            qWarning() << "Warning: Magnitude of unknown band found "
                                          "while reading output from CDS Sesame";
                            band = 0;
                        }

                        float mag = NaN::f;
                        xml.readNext();
                        if (xml.isCharacters())
                        {
                            qCDebug(KSTARS) << "characters: " << xml.tokenString();
                            mag = xml.tokenString().toFloat();
                        }
                        else if (xml.isStartElement())
                        {
                            while (xml.name() != "v")
                            {
                                qCDebug(KSTARS) << "element: " << xml.name();
                                xml.readNextStartElement();
                            }
                            mag = xml.readElementText().toFloat();
                            qCDebug(KSTARS)
                                << "Got " << xml.tokenString() << " mag = " << mag;
                            while (!xml.atEnd() && xml.readNext() && xml.name() != "mag")
                                ; // finish reading the <mag> tag all the way to </mag>
                        }
                        else
                            qWarning()
                                << "Failed to parse Xml token in magnitude element: "
                                << xml.tokenString();

                        if (band == 'V')
                        {
                            data.setMag(mag);
                        }
                        else if (band == 'B')
                        {
                            data.setFlux(mag); // FIXME: This is bad
                            if (std::isnan(data.mag()))
                                data.setMag(mag); // FIXME: This is bad too
                        }
                        // Don't know what to do with other magnitudes, until we have a magnitude hash
                    }
                    else if (xml.name() == "oname") // Primary identifier
                    {
                        QString contents = xml.readElementText();
                        data.setCatalogIdentifier(contents);
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
    if (xml.hasError())
    {
        msg = xi18n("Error parsing XML from CDS Sesame: %1 on line %2 @ col = %3",
                    xml.errorString(), xml.lineNumber(), xml.columnNumber());
        qCDebug(KSTARS) << msg;

#ifdef KSTARS_LITE
        KStarsLite::Instance()->notificationMessage(msg);
#endif
        return { false, {} };
    }

    if (!found)
        return { false, {} };

    msg = xi18n("Resolved %1 successfully.", name);
    qCDebug(KSTARS) << msg;

#ifdef KSTARS_LITE
    KStarsLite::Instance()->notificationMessage(msg);
#endif
    qCDebug(KSTARS) << "Object type: " << SkyObject::typeName(data.type())
                    << "; Coordinates: " << data.ra0().Degrees() << ";"
                    << data.dec().Degrees();
    return { true, data };
}

// bool NameResolver::NameResolverInternals::getDataFromSimbad( class CatalogObject &data ) {
//     // TODO: Implement
//     // QUrl( QString( "http://simbad.u-strasbg.fr/simbad/sim-script?script=output%20console=off%20script=off%0Aformat%20object%20%22%25DIM%22%0A" ) + data.name );
// }

SkyObject::TYPE NameResolver::NameResolverInternals::interpretObjectType(const QString &typeString, bool caseSensitive)
{
    // FIXME: Due to the quirks of Sesame (SIMBAD vs NED etc), it
    // might be very difficult to discern the type in all cases.  The
    // best way TODO things might be to first run the query with NED,
    // and if it is extragalactic, then trust NED and
    // accept. Otherwise, or if NED did not return a result, re-run
    // the query on SIMBAD and VizieR and use that result, if any.

    // See https://simbad.u-strasbg.fr/simbad/sim-display?data=otypes for Object Classification in SIMBAD

    // Highest likelihood is a galaxy of some form, so we process that first
    const QString &s       = typeString; // To make the code compact
    Qt::CaseSensitivity cs = (caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);

    QStringList galaxyTypes;
    QStringList galaxyGroupTypes;
    QStringList openClusterTypes;
    QStringList radioSourceTypes;
    galaxyTypes << "G"
                << "LIN"
                << "AGN"
                << "GiG"
                << "GiC"
                << "H2G"
                << "BiC"
                << "GiP"
                << "HzG"
                << "rG"
                << "AG?"
                << "EmG"
                << "LSB"
                << "SBG"
                << "bCG"
                << "SyG"
                << "Sy1"
                << "Sy2"
                << "Gxy";

    galaxyGroupTypes << "GClstr"
                     << "GGroup"
                     << "GPair"
                     << "ClG"
                     << "CGG"
                     << "PaG"
                     << "IG"
                     << "GrG"
                     << "SCG"; // NOTE (FIXME?): Compact groups and pairs of galaxies ar treated like galaxy clusters

    openClusterTypes << "*Cl"
                     << "Cl*"
                     << "OpC"
                     << "As*"
                     << "St*"
                     << "OCl";

    radioSourceTypes << "Rad"
                     << "mR"
                     << "cm"
                     << "mm"
                     << "smm"
                     << "HI"
                     << "rB"
                     << "Mas";

    if (galaxyTypes.contains(s, cs))
        return SkyObject::GALAXY;

    if (galaxyGroupTypes.contains(s, cs))
        return SkyObject::GALAXY_CLUSTER;

    if (openClusterTypes.contains(s, cs))
        return SkyObject::OPEN_CLUSTER; // FIXME: NED doesn't distinguish between globular clusters and open clusters!!

    auto check = [typeString, cs](const QString &type) { return (!QString::compare(typeString, type, cs)); };

    if (check("GlC") || check("GlCl"))
        return SkyObject::GLOBULAR_CLUSTER;
    if (check("Neb") || check("HII") || check("HH")) // FIXME: The last one is Herbig-Haro object
        return SkyObject::GASEOUS_NEBULA;
    if (check("SNR")) // FIXME: Simbad returns "ISM" for Veil Nebula (Interstellar Medium??)
        return SkyObject::SUPERNOVA_REMNANT;
    if (check("PN") || check("PNeb") || check("PNe") || check("pA*")) // FIXME: The latter is actually Proto PN
        return SkyObject::PLANETARY_NEBULA;
    if (typeString == "*")
        return SkyObject::CATALOG_STAR;
    if (check("QSO"))
        return SkyObject::QUASAR;
    if (check("DN") || check("DNe") || check("DNeb") || check("glb")) // The latter is Bok globule
        return SkyObject::DARK_NEBULA;
    if (radioSourceTypes.contains(s, cs))
        return SkyObject::RADIO_SOURCE;
    if (typeString == "**")
        return SkyObject::MULT_STAR;
    if (typeString.contains('*') && !check("C?*"))
        return SkyObject::CATALOG_STAR;

    return SkyObject::TYPE_UNKNOWN;
    // FIXME: complete this method
}
