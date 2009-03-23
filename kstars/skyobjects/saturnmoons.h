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

class KSNumbers;
class KSPlanet;
class KSSun;
class TrailObject;
class dms;

/**
  *@class SAturnMoons
  *Implements the Eight largest moons of Saturn.
  *using Algorithms based on "Astronomical Algorithms"by Jean Meeus 
  *
  *TODO: make the moons SkyObjects, rather than just points.
  *
  *@author Vipul Kumar Singh
  *@version 1.0
  */
class SaturnMoons {
public:
    /**
      *Constructor.  Assign the name of each moon,
      *and initialize their XYZ positions to zero.
      */
    SaturnMoons();

    /**
      *Destructor.  Delete moon objects.
      */
    ~SaturnMoons();

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
    void findPosition( const KSNumbers *num, const KSPlanet *sat, const KSSun *sunptr );

    /**
      *@return pointer to a moon given the ID number.
      *@param id which moon? 0=Mimas,1=Enceladus,2=Tethys,3=Dione,4=Rhea,5=Titan,6=Hyperion,7=Lapetus
      */
    inline TrailObject* moon( int id ) { return Moon[id]; }

    /**
      *@return pointer to a moon, given its name:
      *@param name the name of the moon (Mimas,Enceladus,Tethys,Dione,Rhea,Titan,Hyperion,Lapetus )
      */
    TrailObject* moonNamed( const QString &name ) const;

    /**
      *@return true if the Moon is nearer to Earth than Saturn.
      *@param id which moon? 0=Mimas,1=Enceladus,2=Tethys,3=Dione,4=Rhea,5=Titan,6=Hyperion,7=Lapetus
      */
    inline bool inFront( int id ) const { return InFront[id]; }

    /**
      *@return the name of a moon.
      *@param id which moon? 0=Mimas,1=Enceladus,2=Tethys,3=Dione,4=Rhea,5=Titan,6=Hyperion,7=Lapetus
      */
    QString name( int id ) const;

    /**
      *Convert the RA,Dec coordinates of each moon to Az,Alt
      *@param LSTh pointer to the current local sidereal time
      *@param lat pointer to the geographic latitude
      */
    void EquatorialToHorizontal( const dms *LSTh, const dms *lat );

    /**
      *@return the X-coordinate in the Saturn-centered coord. system.
      *@param i which moon? 0=Mimas,1=Enceladus,2=Tethys,3=Dione,4=Rhea,5=Titan,6=Hyperion,7=Lapetus
      */
    double x( int i ) const { return XS[i]; }

    /**
      *@return the Y-coordinate in the Saturn-centered coord. system.
      *@param i which moon? 0=Mimas,1=Enceladus,2=Tethys,3=Dione,4=Rhea,5=Titan,6=Hyperion,7=Lapetus
      */
    double y( int i ) const { return YS[i]; }

    /**
      *@return the Z-coordinate in the Saturn-centered coord. system.
      *@param i which moon? 0=Mimas,1=Enceladus,2=Tethys,3=Dione,4=Rhea,5=Titan,6=Hyperion,7=Lapetus
      */
    double z( int i ) const { return ZS[i]; }

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
private:
    QVector<TrailObject*> Moon;
    bool InFront[8];
    //the rectangular position, relative to Saturn.  X-axis is equator of Saturn; usints are Sat. Radius
    double XS[8], YS[8], ZS[8];
};

#endif
