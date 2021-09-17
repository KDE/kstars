/*
    SPDX-FileCopyrightText: 2009 Jerome SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "satellitegroup.h"
#include "skycomponent.h"

#include <QList>

class QPointF;
class Satellite;

/**
 * @class SatellitesComponent
 * Represents artificial satellites on the sky map.
 * @author Jérôme SONRIER
 * @version 1.0
 */
class SatellitesComponent : public SkyComponent
{
    public:
        /**
         * @short Constructor
         * @param parent pointer to the parent SkyComposite
         */
        explicit SatellitesComponent(SkyComposite *parent = nullptr);

        /**
         * @short Destructor
         */
        ~SatellitesComponent() override;

        /**
         * @return true if satellites must be draw.
         */
        bool selected() override;

        /**
         * Draw all satellites.
         * @param skyp SkyPainter to use
         */
        void draw(SkyPainter *skyp) override;

        /**
         * Update satellites position.
         * @param num
         */
        void update(KSNumbers *num) override;

        /**
         * Download new TLE files
         */
        void updateTLEs();

        /**
         * @return The list of all groups
         */
        QList<SatelliteGroup *> groups();

        /**
         * Search a satellite by name.
         * @param name The name of the satellite
         * @return Satellite that was find or 0
         */
        Satellite *findSatellite(QString name);

        /**
         * Draw label of a satellite.
         * @param sat The satellite
         * @param pos The position of the satellite
         */
        void drawLabel(Satellite *sat, const QPointF &pos);

        /**
         * Search the nearest satellite from point p
         * @param p
         * @param maxrad
         */
        SkyObject *objectNearest(SkyPoint *p, double &maxrad) override;

        /**
         * Return object given name
         * @param name object name
         * @return object if found, otherwise nullptr
         */
        SkyObject *findByName(const QString &name) override;

        void loadData();

    protected:
        void drawTrails(SkyPainter *skyp) override;

    private:
        QList<SatelliteGroup *> m_groups; // List of all groups
        QHash<QString, Satellite *> nameHash;
};
