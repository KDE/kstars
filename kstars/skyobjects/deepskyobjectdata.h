/***************************************************************************
               deepskyobjectdata.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun 24 Aug 2014 03:36:32 CDT
    copyright            : (c) 2014 by Akarsh Simha
    email                : akarsh.simha@kdemail.net
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/



#ifndef DEEPSKYOBJECTDATA_H
#define DEEPSKYOBJECTDATA_H
/**
 * @struct DeepSkyObjectData
 * @short A struct to contain information about a deep sky object, independent of its existence as a DeepSkyObject
 * @author Akarsh Simha <akarsh.simha@kdemail.net>
 */

struct DeepSkyObjectData {

    /**
     * @short Constructor
     */
    inline DeepSkyObjectData(): catalogIDNumber(-1), B( NaN::f ), V( NaN::f ), type( SkyObject::TYPE::TYPE_UNKNOWN ), 
                                majorAxis( NaN::f ), minorAxis( NaN::f ), positionAngle( NaN::f ) {
    }

    /**
     * Data
     */

    /* Designation */
    QString name; // Primary designation
    QString altName; // Alternate designation
    QString longName; // Trivial name
    QString catalog; // Catalog corresponding to primary designation
    QString catalogID; // Catalog ID corresponding to primary designation
    int catalogIDNumber; // approximation to catalogID, by stripping suffixes and the like (eg: NGC6027B = 6027)

    /* Positional -- J2000.0 */
    dms RA;
    dms Dec;

    /* Fluxes */
    float B;
    float V;

    /* Type */
    QString typeString; // detailed type
    enum SkyObject::TYPE type; // type "rounded-off" to one of KStars' types
    QString classification; // morphology etc.

    /* Dimension */
    float majorAxis; // Major axis in arcmin
    float minorAxis; // Minor axis in arcmin
    float positionAngle;

};

#endif
