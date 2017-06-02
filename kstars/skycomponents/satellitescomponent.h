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

#ifndef SATELLITESCOMPONENT_H
#define SATELLITESCOMPONENT_H

#include <QList>

//#include <kio/job.h>

#include "skycomponent.h"
#include "satellitegroup.h"

class Satellite;
class FileDownloader;

/**
	*@class SatellitesComponent
	*Represents artificial satellites on the sky map.
	*@author Jérôme SONRIER
	*@version 1.0
	*/
class SatellitesComponent : public SkyComponent
{
    public:
        /**
         *@short Constructor
         *@param parent pointer to the parent SkyComposite
         */
        explicit SatellitesComponent( SkyComposite * parent );

        /**
         *@short Destructor
         */
        ~SatellitesComponent();

        /**
         *@return true if satellites must be draw.
         */
        bool selected() Q_DECL_OVERRIDE;

        /**
         *Draw all satellites.
         *@param skyp SkyPainter to use
         */
        void draw( SkyPainter * skyp ) Q_DECL_OVERRIDE;

        /**
         *Update satellites position.
         *@param num
         */
        void update( KSNumbers * num ) Q_DECL_OVERRIDE;

        /**
         *Download new TLE files
         */
        void updateTLEs();

        /**
         *@return The list of all groups
         */
        QList<SatelliteGroup *> groups();

        /**
         *Search a satellite by name.
         *@param name The name of the satellite
         *@return Satellite that was find or 0
         */
        Satellite * findSatellite( QString name );

        /**
         *Draw label of a satellite.
         *@param sat The satellite
         *@param pos The position of the satellite
         */
        void drawLabel( Satellite * sat, QPointF pos );

        /**
         *Search the nearest satellite from point p
         *@param p
         *@param maxrad
         */
        SkyObject * objectNearest( SkyPoint * p, double &maxrad ) Q_DECL_OVERRIDE;

        /**
         * Return object given name
         * @param name object name
         * @return object if found, otherwise nullptr
         */
        SkyObject * findByName( const QString &name ) Q_DECL_OVERRIDE;

        void loadData();

    protected:
        void drawTrails( SkyPainter * skyp ) Q_DECL_OVERRIDE;


    private:
        QList<SatelliteGroup *> m_groups;   // List of all groups
        QHash<QString, Satellite *> nameHash;
};

#endif
