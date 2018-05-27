/***************************************************************************
                          satellitegroup.h  -  K Desktop Planetarium
                             -------------------
    begin                : Tue 22 Mar 2011
    copyright            : (C) 2011 by Jerome SONRIER
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
