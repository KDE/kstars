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

/**@class dms
	*@short An angle, stored as degrees, but expressible in many ways.
	*@author Jason Harris
	*@version 1.0
	
	*dms encapsulates an angle.  The angle is stored as a double,
	*equal to the value of the angle in degrees.  Methods are available
	*for setting/getting the angle as a floating-point measured in
	*Degrees or Hours, or as integer triplets (degrees, arcminutes,
	*arcseconds or hours, minutes, seconds).  There is also a method
	*to set the angle according to a radian value, and to return the
	*angle expressed in radians.  Finally, a SinCos() method computes
	*the sin and cosine of the angle.  Once computed, the sin and cos 
	*values are stored, so that subsequent SinCos() calls will be faster.
  */

class dms {
public:
/**Default Constructor.  Set the floating-point value of the angle
	*according to the three integer arguments.
	*@param d degree portion of angle (int).  Defaults to zero.
	*@param m arcminute portion of angle (int).  Defaults to zero.
	*@param s arcsecond portion of angle (int).  Defaults to zero.
	*@param ms arcsecond portion of angle (int).  Defaults to zero.
	*/
	dms( const int &d=0, const int &m=0, const int &s=0, const int &ms=0 ) { setD( d, m, s, ms ); }

/**
	*Alternate constructor.  Sets the floating-point angle to the
	*argument, in degrees.
	*@param x angle expressed as a floating-point number (in degrees)
	*/
  dms( const double &x ) { setD( x ); }

/**Destructor (empty).
	*/
	~dms() {}

/**@return integer degrees portion of the angle
	*/
  const int degree() const { return int( D ) ; }

/**@return integer arcminutes portion of the angle
	*/
  const int arcmin() const;

/**@return integer arcseconds portion of the angle
	*/
  const int arcsec() const;

/**@return integer milliarcseconds portion of the angle
	*/
	const int marcsec() const;

/**@return angle in degrees expressed as a double.
	*/
	const double& Degrees() const { return D; }

/**@return integer hours portion of the angle
	*/
	const int hour() const { return int( reduce().Degrees()/15.0 ); }

/**@return integer minutes portion of the angle
	*/
	const int minute() const;

/**@return integer seconds portion of the angle
	*/
	const int second() const;

/**@return integer milliseconds portion of the angle
	*/
	const int msecond() const;

/**
	*@return angle in hours expressed as a double.
	*/
	const double Hours() const { return reduce().Degrees()/15.0; }

/**Sets integer degrees portion of angle, leaving the ArcMin and
	*ArcSec values intact.
	*@param d new integer degrees value
	*/
  void setDeg( const int &d ) { setD( d, arcmin(), arcsec() ); }

/**Sets integer arcminutes portion of angle, leaving the Degrees
	*and ArcSec values intact.
	*@param m new integer arcminutes value
	*/
  void setArcMin( const int &m ) { setD( degree(), m, arcsec() ); }

/**Sets integer arcseconds portion of angle, leaving the Degrees
	*and ArcMin values intact.
	*@param s new integer arcseconds value
	*/
  void setArcSec( const int &s ) { setD( degree(), arcmin(), s ); }

/**Sets floating-point value of angle, in degrees.
	*@param x new angle (double)
	*/
  void setD( const double &x );

/**Sets floating-point value of angle, in degrees.
	*This is an overloaded member function; it behaves essentially 
	*like the above function.  The floating-point value of the angle
	*(D) is determined from the following formulae:
	*
	*\f$ fabs(D) = fabs(d) + \frac{(m + (s/60))}{60} \f$
	*\f$ sgn(D) = sgn(d) \f$
	*
	*@param d integer degrees portion of angle
	*@param m integer arcminutes portion of angle
	*@param s integer arcseconds portion of angle
	*@param ms integer arcseconds portion of angle
	*/
	void setD( const int &d, const int &m, const int &s, const int &ms=0 );

/**Sets integer hours portion of angle, leaving the Minutes and
	*Seconds values intact.
	*@param h new integer hours value
	*/
	void setHour( const int &h ) { setH( h, minute(), second() ); }

/**Sets integer minutes portion of angle, leaving the Hours and
	*Seconds values intact.
	*@param m new integer minutes value
	*/
	void setHMin( const int &m ) { setH( hour(), m, second() ); }

/**Sets integer seconds portion of angle, leaving the Hours and
	*Minutes values intact.
	*@param s new integer seconds value
	*/
	void setHSec( const int &s ) { setH( hour(), minute(), s ); }

/**converts argument from hours to degrees, then
	*sets floating-point value of angle, in degrees.
	*@param x new angle, in hours (double)
	*/
	void setH( const double &x );

/**Sets floating-point value of angle, first converting hours to degrees.
	*This is an overloaded member function, provided for convenience.  It 
	*behaves essentially like the above function.
	*@param h integer hours portion of angle
	*@param m integer minutes portion of angle
	*@param s integer seconds portion of angle
	*@param ms integer milliseconds portion of angle
	*/
	void setH( const int &h, const int &m, const int &s, const int &ms=0 );

/**Copy value of another dms angle
	*@param d set angle according to this dms object
	*/
  void set( const dms &d ) { setD( d.Degrees() ); }

/**Copy value of another dms angle.  Differs from above function only
	*in argument type.  Identical to setD(double d).
	*@param d set angle according to this double value
	*/
  void set( const double &d ) { setD( d ); }

/**
	*Addition operator.  Add two dms objects.
	*@param d add to current angle
	*@return sum of two angles, in a dms object
	*/
//  dms operator+ ( dms d );
/**
	*Subtraction operator.  Subtract two dms objects.
	*@param d subtract from current angle
	*@return difference of two angles, in a dms object
	*/
//  dms operator- ( dms d );
/**
	*Assignment operator.  Assign value of argument to current angle.
	*I wanted to pass the argument by reference, but I couldn't figure
	*out a good way to do it without generating an error or warning message.
	*@param a dms object to get angle value from
	*@return dms object, copy of argument.
	*/
//  dms operator= ( const dms a ) { return a; }
/**
	*Assignment operator.  Assign value of argument to current angle.
	*@param d floating-point number to get angle value from
	*@return dms object, same value as argument.
	*/
//  dms operator= ( const double &d ) { return (dms( d )); }

/**Compute Sine and Cosine of angle.  If you have glibc >= 2.1, then 
	*SinCos is a bit faster than calling sin() and cos() separately.  
	*The values are returned through the arguments (passed by reference).
	*The Sin and Cos values are stored internally; on subsequent calls,
	*the stored values are returned directly (unless the angle's value 
	*has changed).
	*@param s Sine of the angle
	*@param c Cosine of the angle
	*/
	void SinCos( double &s, double &c ) const;

/**@return the stored Sin value of the angle.
	*@warning must call SinCos() before using this function.
	*/
	const double& sin( void ) const { return Sin; }
	
/**@return the stored Cos value of the angle.
	*@warning must call SinCos() before using this function.
	*/
	const double& cos( void ) const { return Cos; }

/**Express the angle in radians.  The Radians value is stored internally.
	*On subsequent calls, the stored value is returned directly (unless
	*the angle's value has changed).
	*@return the angle in radians (double)
	*/
	const double& radians( void ) const;

/**Set angle according to the argument, which is in radians
	*@param a angle in radians
	*/
	void setRadians( const double &a );

/**return the equivalent angle between 0 and 360 degrees.
	*@warning does not change the value of the parent angle itself.
	*/
	const dms reduce( void ) const;

/**
	*@return a nicely-formatted string representation of the angle
	*in degrees, arcminutes, and arcseconds.
	*/
	const QString toDMSString(const bool forceSign = false) const;

/**@return a nicely-formatted string representation of the angle
	*in hours, minutes, and seconds.
	*/
	const QString toHMSString() const;

/**PI is a const static member; it's public so that it can be used anywhere,
	*as long as dms.h is included.
	*/
	static const double PI;

/**DegToRad is a const static member equal to the number of radians in
	*one degree (dms::PI/180.0).
	*/
	static const double DegToRad;

	/** Static function to create a DMS object from a QString; it is public so that it can
	be used anywhere. S similar function is present in other Q classes as QTime, QDate
	@param string.
	@param bool. If TRUE string represents degrees, if FALSE represents hours
	@returns a dms object from the formatted string
	//There are several ways to specify the RA and Dec:
	//(we always assume RA is in hours or h,m,s and Dec is in degrees or d,m,s)
	//1. Integer numbers  ( 5;  -33 )
	//2. Floating-point numbers  ( 5.0; -33.0 )
	//3. colon-delimited integers ( 5:0:0; -33:0:0 )
	//4. colon-delimited with float seconds ( 5:0:0.0; -33:0:0.0 )
	//5. colon-delimited with float minutes ( 5:0.0; -33:0.0 )
	//6. space-delimited ( 5 0 0; -33 0 0 ) or ( 5 0.0 -33 0.0 )
	//7. space-delimited, with unit labels ( 5h 0m 0s, -33d 0m 0s )
	**/

	static dms fromString(QString & s, bool deg);

private:
	double D;

	mutable double Radians;
	mutable double Sin, Cos;
	mutable bool scDirty, rDirty;
};

#endif
