/*
    SPDX-FileCopyrightText: 2011 Jerome SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QUrl>

class Satellite;

/**
 * @class SatelliteGroup
 * Represents a group of artificial satellites.
 * @author Jérôme SONRIER
 * @version 1.0
 */
class SatelliteGroup : public QList<Satellite *>
{
  public:
    /**
     * @short Constructor
     */
    SatelliteGroup(const QString& name, const QString& tle_filename, const QUrl& update_url);

    virtual ~SatelliteGroup() = default;

    /**
     * Read TLE file of the group and create all satellites found in the file.
     */
    void readTLE();

    /**
     * Compute current position of the each satellites in the group.
     */
    void updateSatellitesPos();

    /**
     * @return TLE filename
     */
    QUrl tleFilename();

    /**
     * @return URL from which new TLE file must be download
     */
    QUrl tleUrl();

    /**
     * @return Name of the group
     */
    QString name();

  private:
    /// Group name
    QString m_name;
    /// TLE filename
    QString m_tle_file;
    /// URL used to update TLE file
    QUrl m_tle_url;
};
