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
#include <QTextStream>

SatelliteGroup::SatelliteGroup(const QString &name, const QString &tle_filename, const QUrl &update_url)
{
    m_name     = name;
    m_tle_file = tle_filename;
    m_tle_url  = update_url;

    // Read TLE file and create satellites
    readTLE();
}

void SatelliteGroup::readTLE()
{
    QFile file;
    QString line1, line2;

    // Delete all satellites
    qDeleteAll(*this);
    clear();

    // Read TLE file
    if (KSUtils::openDataFile(file, m_tle_file))
    {
        QTextStream stream(&file);
        while (!stream.atEnd())
        {
            // Read satellite name
            QString sat_name = stream.readLine().trimmed();
            line1            = stream.readLine();
            line2            = stream.readLine();
            // Create a new satellite and add it to the list of satellites
            if (line1.startsWith('1') && line2.startsWith('2'))
                append(new Satellite(sat_name, line1, line2));
        }
        file.close();
    }
}

bool SatelliteGroup::containsTLE(const QByteArray &data)
{
    QTextStream stream(data);
    while (!stream.atEnd())
    {
        // Same layout as readTLE(): name line followed by line 1 and line 2
        stream.readLine();
        const QString line1 = stream.readLine();
        const QString line2 = stream.readLine();
        if (line1.startsWith('1') && line2.startsWith('2'))
            return true;
    }
    return false;
}

bool SatelliteGroup::saveTLE(const QByteArray &data, QString &error)
{
    if (!containsTLE(data))
    {
        error = i18n("The server did not return any orbital elements in TLE format.");
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

    readTLE();
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
