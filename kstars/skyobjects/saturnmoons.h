/***************************************************************************
                          saturnmoons.h  -  description
                             -------------------
    begin                : Sat Mar 13 2009
                         : by Vipul Kumar Singh
    email                : vipulkrsingh@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SATURNMOONS_H_
#define SATURNMOONS_H_

#include <QString>
#include <QVector>
#include "planetmoons.h"

class KSNumbers;
class KSPlanet;
class KSSun;
class TrailObject;
class dms;

/**
  *@class SaturnMoons
  *Implements the Eight largest moons of Saturn.
  *using Algorithms based on "Astronomical Algorithms"by Jean Meeus 
  *
  *TODO: make the moons SkyObjects, rather than just points.
  *
  *@author Vipul Kumar Singh
  *@version 1.0
  */
class SaturnMoons : public PlanetMoons {
public:
    /**
      *Constructor.  Assign the name of each moon,
      *and initialize their XYZ positions to zero.
      */
    SaturnMoons();

    /**
      *Destructor.  Delete moon objects.
      */
    virtual ~SaturnMoons();

    /**
      *@short Find the positions of each Moon, relative to Saturn.
      *We use an XYZ coordinate system, centered on Saturn, 
      *where the X-axis corresponds to Saturn's Equator, 
      *the Y-Axis is parallel to Saturn's Poles, and the 
      *Z-axis points along the line joining the Earth and 
      *Saturn.  Once the XYZ positions are known, this 
      *function also computes the RA,Dec positions of each 
      *Moon, and sets the inFront bool variable to indicate 
      *whether the Moon is nearer to us than Saturn or not
      *(this information is used to determine whether the 
      *Moon should be drawn on top of Saturn, or vice versa).
      *
      *See "Astronomical Algorithms" bu Jean Meeus.
      *
      *@param num pointer to the KSNumbers object describing 
      *the date/time at which to find the positions.
      *@param sat pointer to the Saturn object
      *@param sunptr pointer to the Sun object
      */
    virtual void findPosition( const KSNumbers *num, const KSPlanet *sat, const KSSun *sunptr );

  private:

    /** the subroutine helps in saturn moon calculations
    */
    void HelperSubroutine( double e, double lambdadash, double p, double a, double omega, double i, double c1, double s1, double& r, double& lambda, double& gamma, double& w );

    double MapTo0To360Range( double Degrees )
    {
      double Value = Degrees;
  
      //map it to the range 0 - 360
      while (Value < 0)
	Value += 360;
      while (Value > 360)
	Value -= 360;
      return Value;
    }
};

#endif
