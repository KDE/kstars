/***************************************************************************
                          satellitescomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : Tue 02 Mar 2011
    copyright            : (C) 2009 by Jerome SONRIER
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

#include "satellitegroup.h"
#include "skycomponent.h"

#include <QList>

class QPointF;
class Satellite;
class FileDownloader;

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
    void drawLabel(Satellite *sat, const QPointF& pos);

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
