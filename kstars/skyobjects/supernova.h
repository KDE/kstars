/***************************************************************************
                          supernova.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sunday, 19th June, 2011
    copyright            : (C) 2011 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SUPERNOVA_H
#define SUPERNOVA_H

#include "skyobject.h"

#include <qvarlengtharray.h>

/**
 * @class Supernova
 * Represents the supernova object. It is a subclass of the SkyObject class.
 * This class has the information for different supernovae.
 * @author Samikshan Bairagya
 */

/**
 * @note The Data File Contains the following parameters
 * @li RA           Right Ascension
 * @li Dec          Declination
 * @li Magnitude    Magnitude at discovery
 * @li serialNumber Serial Number for the Supernova
 * @li type         Supernova Type
 * @li hostGalaxy   Host Galaxy for the supernova
 * @li offset       Offset from the nucleus of the host galaxy as reported at time of discovery
 * @li discoverer   Discoverer(s) of the supernova
 * @li date         Date of observation of the supernova
 */
class Supernova : public SkyObject
{
public:
    explicit Supernova( dms ra, dms dec, QString& date, float m = 0.0, const QString& serialNo=QString(),
                        const QString& type=QString(), const QString& hostGalaxy=QString(), const QString& offset=QString(),const QString& discoverer=QString() );
    //virtual Supernova* clone() const;

    /** Destructor(Empty) */
    virtual ~Supernova() {}

    //virtual bool loadData();

    //void setNames( QString name, QString name2 );

    /** @return true if the star has a serial number */
    inline bool hasName() const { return ( !serialNumber.isEmpty());  }

    /** @return the Serial Number of the Supernova */
    inline virtual QString name( void ) const { return serialNumber;}

    /**
     *@return the Right Ascension
     */
    inline dms getRA() { return RA ; }

    /**
     *@return the Declination
     */
    inline dms getDec() { return Dec ; }

    /**
     * @return Magnitude for the Supernova
     */
    inline float getMagnitude() { return Magnitude ; }

    /**
     * @return the type of the supernova
     */
    inline QString getType() { return type; }

    /**
     * @return the host galaxy of the supernova
     */
    inline QString getHostGalaxy() { return hostGalaxy; }

    /**
     * @return the date the supernova was observed
     */
    inline QString getDate() { return date; }

private:
    QString serialNumber, type, hostGalaxy, offset, discoverers, date;
    dms RA, Dec;
    float Magnitude;
};

#endif