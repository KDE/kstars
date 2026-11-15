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
        SatelliteGroup(const QString &name, const QString &tle_filename, const QUrl &update_url);

        virtual ~SatelliteGroup() = default;

        /**
         * Read the orbital elements file of the group and create all satellites found in the file.
         */
        void readElements();

        /**
         * @brief Replace the local orbital elements file of the group with downloaded data and reload the satellites.
         *
         * The local file is left untouched when @p data holds no usable orbital elements, so an error page or an
         * empty reply from the server never wipes the satellites of the group.
         * @param data Downloaded orbital elements, in TLE or OMM JSON format.
         * @param error Set to a description of the problem on failure.
         * @return true if the data was saved and reloaded.
         */
        bool saveElements(const QByteArray &data, QString &error);

        /**
         * @brief Create the satellites described by @p data.
         *
         * The format is detected from the content: OMM records in JSON (an array of objects or a single object),
         * otherwise three-line TLE (name line, line 1 and line 2). Invalid entries are skipped.
         * @return The satellites, owned by the caller.
         */
        static QList<Satellite *> parseElements(const QByteArray &data);

        /**
         * @return true if @p data holds at least one satellite that parseElements() can load.
         */
        static bool containsElements(const QByteArray &data);

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
