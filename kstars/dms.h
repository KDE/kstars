/***************************************************************************
                          dms.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 11 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef DMS_H
#define DMS_H

#include <math.h>
#include <qstring.h>
#include <kdebug.h>

#define J2000 2451545.0 //Julian Date for noon on Jan 1, 2000 (epoch J2000)
                       //defined here because this file is included in every other class.
class SkyPoint;

/**
	*dms encapsulates an angle.  The angle is stored as a double,
	*the value of the angle in degrees.  Methods are available
	*for set/getting the angle as a floating-point measured in
	*Degrees or Hours, or as integer triplets (degrees, arcminutes,
	*arcseconds or hours, minutes, seconds).  There is also a method
	*to set the angle according to a radian value, and to return the
	*angle expressed in radians.
	*
	*@short An angle, stored as degrees, but expressible in many ways
  *@author Jason Harris
	*@version 0.9
  */

class dms {
public:
/**
	*Default Constructor.  Set the floating-point value of the angle
	*according to the three integer arguments (simply calls setD(d,m,s))
	*@param d degree portion of angle (int).  Defaults to zero.
	*@param m arcminute portion of angle (int).  Defaults to zero.
	*@param s arcsecond portion of angle (int).  Defaults to zero.
	*@param ms arcsecond portion of angle (int).  Defaults to zero.
	*/
	dms( const int &d=0, const int &m=0, const int &s=0, const int &ms=0 ) { setD( d, m, s, ms ); }

/**
	*Alternate constructor.  Sets the floating-point angle to the
	*argument, in degrees (simply calls setD( x ))
	*@param x angle expressed as a floating-point number (in degrees)
	*/
  dms( const double &x ) { setD( x ); }

/**
	*Destructor (empty).
	*/
	~dms() {}

/**
	*@returns integer degrees portion of the angle
	*/
  const int degree() const { return int( D ) ; }

/**
	*@returns integer arcminutes portion of the angle
	*/
  const int arcmin() const;

/**
	*@returns integer arcseconds portion of the angle
	*/
  const int arcsec() const;

  /**
	*@returns integer milliarcseconds portion of the angle
	*/
	const int marcsec() const;

/**
	*@returns angle in degrees expressed as a double.
	*/
	const double& Degrees() const { return D; }

/**
	*@returns integer hours portion of the angle
	*/
	const int hour() const { return int( reduce().Degrees()/15.0 ); }

/**
	*@returns integer minutes portion of the angle
	*/
	const int minute() const;

/**
	*@returns integer seconds portion of the angle
	*/
	const int second() const;

/**
	*@returns integer milliseconds portion of the angle
	*/
	const int msecond() const;

/**
	*@returns angle in hours expressed as a double.
	*/
	const double Hours() const { return reduce().Degrees()/15.0; }

/**
	*Sets integer degrees portion of angle, leaving the ArcMin and
	*ArcSec values intact.
	*@param d new integer degrees value
	*/
  void setDeg( const int &d ) { setD( d, arcmin(), arcsec() ); }

/**
	*Sets integer arcminutes portion of angle, leaving the Degrees
	*and ArcSec values intact.
	*@param m new integer arcminutes value
	*/
  void setArcMin( const int &m ) { setD( degree(), m, arcsec() ); }

/**
	*Sets integer arcseconds portion of angle, leaving the Degrees
	*and ArcMin values intact.
	*@param s new integer arcseconds value
	*/
  void setArcSec( const int &s ) { setD( degree(), arcmin(), s ); }

/**
	*Sets floating-point value of angle, in degrees.
	*@param x new angle (double)
	*/
  void setD( const double &x );

/**
	*Sets floating-point value of angle.
	*fabs(D) = fabs(d) + (m + (s/60))/60);
	*sgn(D) = sgn(d)
	*@param d integer degrees portion of angle
	*@param m integer arcminutes portion of angle
	*@param s integer arcseconds portion of angle
	*@param ms integer arcseconds portion of angle
	*/
	void setD( const int &d, const int &m, const int &s, const int &ms=0 );

/**
	*Sets integer hours portion of angle, leaving the Minutes and
	*Seconds values intact.
	*@param h new integer hours value
	*/
	void setHour( const int &h ) { setH( h, minute(), second() ); }

/**
	*Sets integer minutes portion of angle, leaving the Hours and
	*Seconds values intact.
	*@param m new integer minutes value
	*/
	void setHMin( const int &m ) { setH( hour(), m, second() ); }

/**
	*Sets integer seconds portion of angle, leaving the Hours and
	*Minutes values intact.
	*@param s new integer seconds value
	*/
	void setHSec( const int &s ) { setH( hour(), minute(), s ); }

/**
	*converts argument from hours to degrees, then
	*sets floating-point value of angle, in degrees.
	*@param x new angle, in hours (double)
	*/
	void setH( const double &x );

/**
	*Sets floating-point value of angle, first converting hours to degrees.
	*@param h integer hours portion of angle
	*@param m integer minutes portion of angle
	*@param s integer seconds portion of angle
	*@param ms integer milliseconds portion of angle
	*/
	void setH( const int &h, const int &m, const int &s, const int &ms=0 );

/**
	*Copy value of another dms angle
	*@param d set angle according to this dms object
	*/
  void set( const dms &d ) { setD( d.Degrees() ); }

/**
	*Copy value of another dms angle.  Differs from above function only
	*in argument type
	*@param d set angle according to this double value
	*/
  void set( const double &d ) { setD( d ); }

/**
	*Addition operator.  Add two dms objects.
	*@param d add to current angle
	*@returns sum of two angles, in a dms object
	*/
//  dms operator+ ( dms d );
/**
	*Subtraction operator.  Subtract two dms objects.
	*@param d subtract from current angle
	*@returns difference of two angles, in a dms object
	*/
//  dms operator- ( dms d );
/**
	*Assignment operator.  Assign value of argument to current angle.
	*I wanted to pass the argument by reference, but I couldn't figure
	*out a good way to do it without generating an error or warning message.
	*@param a dms object to get angle value from
	*@returns dms object, copy of argument.
	*/
//  dms operator= ( const dms a ) { return a; }
/**
	*Assignment operator.  Assign value of argument to current angle.
	*@param d floating-point number to get angle value from
	*@returns dms object, same value as argument.
	*/
//  dms operator= ( const double &d ) { return (dms( d )); }

/**
	*Compute Sine and Cosine of angle.  SinCos is a bit faster
	*than calling sin() and cos() separately.  The values are returned
	*through the arguments (passed by reference).
	*@param s Sine of the angle
	*@param c Cosine of the angle
	*/
	//DMS_SPEED
	void SinCos( double &s, double &c ) const;

	//DMS_SPEED
	const double& sin( void ) const { return Sin; }
	const double& cos( void ) const { return Cos; }

/**
	*Express the angle in radians
	*@returns the angle in radians (double)
	*/
	//DMS_SPEED
	const double& radians( void ) const;

/**
	*Set angle according to the argument, which is in radians
	*@param a angle in radians
	*/
	void setRadians( const double &a );

/**
	*return a version the angle that is between 0 and 360 degrees.
	*/
	const dms reduce( void ) const;

/**
 * convert to a D:M:S string representation
 */
	const QString toDMSString(const bool forceSign = false) const;

/**
 * convert to a H:M:S string representation
 */
	const QString toHMSString() const;

/**	
 * PI is a const static member; it's public so that it can be used anywhere,
 * as long as dms.h is included.
 */
	static const double PI;
	static const double DegToRad;

private:
	double D;
	
	//DMS_SPEED
	mutable double Radians;
	mutable double Sin, Cos;
	mutable bool scDirty, rDirty;
};

#endif
