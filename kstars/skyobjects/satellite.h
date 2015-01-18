/***************************************************************************
                          satellite.h  -  K Desktop Planetarium
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

#ifndef SATELLITE_H
#define SATELLITE_H


#include <QString>

#include "skyobject.h"
#include "skypoint.h"

class KSPopupMenu;

/**
    *@class Satellite
    *Represents an artificial satellites.
    *@author Jérôme SONRIER
    *@version 0.1
    */
class Satellite : public SkyObject
{
public:
    /**
     *@short Constructor
     */
    Satellite( QString name, QString line1, QString line2 );

    /**
     *@return a clone of this object
     *@note See SkyObject::clone()
     */
    virtual Satellite *clone() const;

    /**
     *@short Destructor
     */
    ~Satellite();

    /**
     *@short Update satellite position
     */
    void updatePos();

    /**
     *@return True if the satellite is visible (above horizon, in the sunlight and sun at least 12° under horizon)
     */
    bool isVisible();

    /**
     *@return True if the satellite is selected
     */
    bool selected();

    /**
     *@short Select or not the satellite
     */
    void setSelected( bool selected );

    /**
     *@return Satellite velocity in km/s
     */
    double velocity();

    /**
     *@return Satellite altitude in km
     */
    double altitude();

    /**
     *@return Satellite range from observer in km
     */
    double range();

    /**
     *@return Satellite international designator
     */
    QString id();

private:
    /**
     *@short Compute non time dependant parameters
     */
    void init();

    /**
     *@short Compute satellite position
     */
    int sgp4( double tsince );

    /**
     *@return Arcsine of the argument
     */
    double arcSin( double arg );

    /**
     *Provides the difference between UT (approximately the same as UTC)
     *and ET (now referred to as TDT).
     *This function is based on a least squares fit of data from 1950
     *to 1991 and will need to be updated periodically.
     */
    double deltaET( double year );

    /**
     *@return arg1 mod arg2
     */
    double Modulus(double arg1, double arg2);

    
    virtual void initPopupMenu( KSPopupMenu *pmenu );
    
    // TLE
    int     m_number;           // Satellite Number
    QChar   m_class;            // Security Classification
    QString m_id;               // International Designator
    double  m_epoch_year;       // Epoch Year
    double  m_epoch;            // Epoch (Day of the year and fractional portion of the day)
    double  m_first_deriv;      // First Time Derivative of the Mean Motion
    double  m_second_deriv;     // Second Time Derivative of Mean Motion
    double  m_bstar;            // BSTAR drag term (decimal point assumed)
    int     m_ephem_type;       // Ephemeris type
    int     m_elem_number;      // Element number
    double  m_inclination;      // Inclination [Radians]
    double  m_ra;               // Right Ascension of the Ascending Node [Radians]
    double  m_eccentricity;     // Eccentricity
    double  m_arg_perigee;      // Argument of Perigee [Radians]
    double  m_mean_anomaly;     // Mean Anomaly [Radians]
    double  m_mean_motion;      // Mean Motion [Radians per minutes]
    int     m_nb_revolution;    // Revolution number at epoch [Revs]
    double  m_tle_jd;           // TLE epoch converted to julian date

    // Satellite
    bool m_is_visible;          // True if the satellite is visible
    bool m_is_eclipsed;         // True if the satellite is in the shadow of the earth
    bool m_is_selected;         // True if the satellite is selected
    double m_velocity;          // Satellite velocity in km/s
    double m_altitude;          // Satellite altitude in km
    double m_range;             // Satellite range from observer in km

    // Near Earth
    bool isimp;
    double aycof  , con41  , cc1    , cc4      , cc5    , d2      , d3   , d4    ,
           delmo  , eta    , argpdot, omgcof   , sinmao , t       , t2cof, t3cof ,
           t4cof  , t5cof  , x1mth2 , x7thm1   , mdot   , nodedot , xlcof, xmcof ,
           nodecf;

    // Deep Space
    int irez;
    double d2201  , d2211  , d3210  , d3222    , d4410  , d4422   , d5220 , d5232 ,
           d5421  , d5433  , dedt   , del1     , del2   , del3    , didt  , dmdt  ,
           dnodt  , domdt  , e3     , ee2      , peo    , pgho    , pho   , pinco ,
           plo    , se2    , se3    , sgh2     , sgh3   , sgh4    , sh2   , sh3   ,
           si2    , si3    , sl2    , sl3      , sl4    , gsto    , xfact , xgh2  ,
           xgh3   , xgh4   , xh2    , xh3      , xi2    , xi3     , xl2   , xl3   ,
           xl4    , xlamo  , zmol   , zmos     , atime  , xli     , xni;

    char method;
};

#endif
