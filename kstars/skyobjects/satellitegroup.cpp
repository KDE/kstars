/*
    SPDX-FileCopyrightText: 2011 Jerome SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "satellitegroup.h"

#include "ksutils.h"
#include "kspaths.h"
#include "skyobjects/satellite.h"

#include <KLocalizedString>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

SatelliteGroup::SatelliteGroup(const QString &name, const QString &tle_filename, const QUrl &update_url)
{
    m_name     = name;
    m_tle_file = tle_filename;
    m_tle_url  = update_url;

    // Read orbital elements file and create satellites
    readElements();
}

void SatelliteGroup::readElements()
{
    // Delete all satellites
    qDeleteAll(*this);
    clear();

    QFile file;
    if (KSUtils::openDataFile(file, m_tle_file))
    {
        append(parseElements(file.readAll()));
        file.close();
    }
}

QList<Satellite *> SatelliteGroup::parseElements(const QByteArray &data)
{
    QList<Satellite *> satellites;

    const QByteArray trimmed = data.trimmed();
    if (trimmed.startsWith('[') || trimmed.startsWith('{'))
    {
        // OMM records in JSON: an array of objects, or a single object
        const QJsonDocument document = QJsonDocument::fromJson(trimmed);
        QJsonArray records = document.array();
        if (document.isObject())
            records.append(document.object());

        for (const QJsonValue &record : std::as_const(records))
        {
            if (Satellite *sat = Satellite::fromOMM(record.toObject()))
                satellites.append(sat);
        }
        return satellites;
    }

    // Three-line TLE: name line followed by line 1 and line 2
    QTextStream stream(data);
    while (!stream.atEnd())
    {
        const QString sat_name = stream.readLine().trimmed();
        const QString line1    = stream.readLine();
        const QString line2    = stream.readLine();
        if (line1.startsWith('1') && line2.startsWith('2'))
            satellites.append(new Satellite(sat_name, line1, line2));
    }
    return satellites;
}

bool SatelliteGroup::containsElements(const QByteArray &data)
{
    const QList<Satellite *> satellites = parseElements(data);
    const bool found = !satellites.isEmpty();
    qDeleteAll(satellites);
    return found;
}

bool SatelliteGroup::saveElements(const QByteArray &data, QString &error)
{
    if (!containsElements(data))
    {
        error = i18n("The server did not return any orbital elements.");
        return false;
    }

    QFile file(tleFilename().toLocalFile());
    if (!file.open(QFile::WriteOnly))
    {
        error = file.errorString();
        return false;
    }

    if (file.write(data) != data.size())
    {
        error = file.errorString();
        return false;
    }
    file.close();

    readElements();
    return true;
}

void SatelliteGroup::updateSatellitesPos()
{
    QMutableListIterator<Satellite *> sats(*this);

    while (sats.hasNext())
    {
        Satellite *sat = sats.next();

        if (sat->selected())
        {
            int rc = sat->updatePos();
            // If position cannot be calculated, remove it from list
            if (rc != 0)
                sats.remove();
        }
    }
}

QUrl SatelliteGroup::tleFilename()
{
    // Return absolute path with "file:" before the path
    //return QUrl( "file:" + (QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(m_tle_file));
    return QUrl::fromLocalFile(QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(m_tle_file));
}

QUrl SatelliteGroup::tleUrl()
{
    return m_tle_url;
}

QString SatelliteGroup::name()
{
    return m_name;
}
