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

//TODO: Remove these when KStarsDateTime is added!
#define J2000 2451545.0 //Julian Date for noon on Jan 1, 2000 (epoch J2000)
                       //defined here because this file is included in every other class.
#define B1950 2433282.4235  // Julian date for Jan 0.9235, 1950

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
/**@short Default Constructor.
	*
	*Set the floating-point value of the angle according to the four integer arguments.
	*@param d degree portion of angle (int).  Defaults to zero.
	*@param m arcminute portion of angle (int).  Defaults to zero.
	*@param s arcsecond portion of angle (int).  Defaults to zero.
	*@param ms arcsecond portion of angle (int).  Defaults to zero.
	*/
	dms( const int &d=0, const int &m=0, const int &s=0, const int &ms=0 ) { setD( d, m, s, ms ); }

/**@short Construct an angle from a double value.
	*
	*Creates an angle whose value in Degrees is equal to the argument.
	*@param x angle expressed as a floating-point number (in degrees)
	*/
  dms( const double &x ) { setD( x ); }

/**@short Construct an angle from a string representation.
	*
	*Attempt to create the angle according to the string argument.  If the string
	*cannot be parsed as an angle value, the angle is set to zero.
	*
	*@warning There is not an unambiguous notification that it failed to parse the string,
	*since the string could have been a valid representation of zero degrees.
	*If this is a concern, use the setFromString() function directly instead.
	*
	*@param s the string to parse as a dms value.
	*@param isDeg if true, value is in degrees; if false, value is in hours.
	*@sa setFromString()
	*/
	dms( const QString &s, bool isDeg=true ) { setFromString( s, isDeg ); }

/**Destructor (empty).
	*/
	~dms() {}

/**@return integer degrees portion of the angle
	*/
  const int degree() const { return int( D ) ; }

/**@return integer arcminutes portion of the angle.
	*@note an arcminute is 1/60 degree.
	*/
  const int arcmin() const;

/**@return integer arcseconds portion of the angle
	*@note an arcsecond is 1/60 arcmin, or 1/3600 degree.
	*/
  const int arcsec() const;

/**@return integer milliarcseconds portion of the angle
	*@note a  milliarcsecond is 1/1000 arcsecond.
	*/
	const int marcsec() const;

/**@return angle in degrees expressed as a double.
	*/
	const double& Degrees() const { return D; }

/**@return integer hours portion of the angle
	*@note an angle can be measured in degrees/arcminutes/arcseconds
	*or hours/minutes/seconds.  An hour is equal to 15 degrees.
	*/
	const int hour() const { return int( reduce().Degrees()/15.0 ); }

/**@return integer minutes portion of the angle
	*@note a minute is 1/60 hour (not the same as an arcminute)
	*/
	const int minute() const;

/**@return integer seconds portion of the angle
	*@note a second is 1/3600 hour (not the same as an arcsecond)
	*/
	const int second() const;

/**@return integer milliseconds portion of the angle
	*@note a millisecond is 1/1000 second (not the same as a milliarcsecond)
	*/
	const int msecond() const;

/**@return angle in hours expressed as a double.
	*@note an angle can be measured in degrees/arcminutes/arcseconds
	*or hours/minutes/seconds.  An hour is equal to 15 degrees.
	*/
	const double Hours() const { return reduce().Degrees()/15.0; }

/**Sets integer degrees portion of angle, leaving the arcminute and
	*arcsecond values intact.
	*@param d new integer degrees value
	*/
  void setDeg( const int &d ) { setD( d, arcmin(), arcsec() ); }

/**Sets integer arcminutes portion of angle, leaving the degrees
	*and arcsecond values intact.
	*@param m new integer arcminutes value
	*/
  void setArcMin( const int &m ) { setD( degree(), m, arcsec() ); }

/**Sets integer arcseconds portion of angle, leaving the degrees
	*and arcminute values intact.
	*@param s new integer arcseconds value
	*/
  void setArcSec( const int &s ) { setD( degree(), arcmin(), s ); }

/**Sets floating-point value of angle, in degrees.
	*@param x new angle (double)
	*/
  void setD( const double &x );

/**@short Sets floating-point value of angle, in degrees.
	*
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

/**Sets integer hours portion of angle, leaving the minutes and
	*seconds values intact.
	*@param h new integer hours value
	*@sa setH() setDeg()
	*/
	void setHour( const int &h ) { setH( h, minute(), second() ); }

/**Sets integer minutes portion of angle, leaving the hours and
	*seconds values intact.
	*@param m new integer minutes value
	*@sa setArcMin()
	*/
	void setHMin( const int &m ) { setH( hour(), m, second() ); }

/**Sets integer seconds portion of angle, leaving the hours and
	*minutes values intact.
	*@param s new integer seconds value
	*@sa setArcSec()
	*/
	void setHSec( const int &s ) { setH( hour(), minute(), s ); }

/**@short Sets floating-point value of angle, in hours.
	*
	*Converts argument from hours to degrees, then
	*sets floating-point value of angle, in degrees.
	*@param x new angle, in hours (double)
	*@sa setD()
	*/
	void setH( const double &x );

/**@short Sets floating-point value of angle, in hours.
	*
	*Converts argument values from hours to degrees, then
	*sets floating-point value of angle, in degrees.
	*This is an overloaded member function, provided for convenience.  It
	*behaves essentially like the above function.
	*@param h integer hours portion of angle
	*@param m integer minutes portion of angle
	*@param s integer seconds portion of angle
	*@param ms integer milliseconds portion of angle
	*@sa setD()
	*/
	void setH( const int &h, const int &m, const int &s, const int &ms=0 );

/**@short Copy value of another dms angle
	*@param d set angle according to this dms object
	*/
	void set( const dms &d ) { setD( d.Degrees() ); }

/**Copy value of another dms angle.  Differs from above function only
	*in argument type.  Identical to setD(double d).
	*@param d set angle according to this double value
	*@sa setD()
	*/
	void set( const double &d ) { setD( d ); }

/**@short Attempt to parse the string argument as a dms value, and set the dms object
	*accordingly.
	*@param s the string to be parsed as a dms value.  The string can be an int or
	*floating-point value, or a triplet of values (d/h, m, s) separated by spaces or colons.
	*@param isDeg if true, the value is in degrees.  Otherwise, it is in hours.
	*@return true if sting was parsed successfully.  Otherwise, set the dms value
	*to 0.0 and return false.
	*/
	bool setFromString( const QString &s, bool isDeg=true );

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

/**@short Compute Sine and Cosine of the angle simultaneously.
	*On machines using glibc >= 2.1, calling SinCos() is somewhat faster
	*than calling sin() and cos() separately.
	*The values are returned through the arguments (passed by reference).
	*The Sin and Cos values are stored internally; on subsequent calls
	*to SinCos(), the stored values are returned directly (unless the
	*angle's value has changed).
	*@param s Sine of the angle
	*@param c Cosine of the angle
	*@sa sin() cos()
	*/
	void SinCos( double &s, double &c ) const;

/**@short Compute the Angle's Sine.
	*
	*If the Sine/Cosine values have already been computed, then this
	*function simply returns the stored value.  Otherwise, it will compute
	*and store the values first.
	*@return the Sine of the angle.
	*@sa cos()
	*/
	const double& sin( void ) const;

/**@short Compute the Angle's Cosine.
	*
	*If the Sine/Cosine values have already been computed, then this
	*function simply returns the stored value.  Otherwise, it will compute
	*and store the values first.
	*@return the Cosine of the angle.
	*@sa sin()
	*/
	const double& cos( void ) const;

/**@short Express the angle in radians.
	*The computed Radians value is stored internally.  On subsequent calls,
	*the stored value is returned directly (unless the angle's value has
	*changed).
	*@return the angle in radians (double)
	*/
	const double& radians( void ) const;

/**@short Set angle according to the argument, in radians.
	*
	*This function converts the argument to degrees, then sets the angle
	*with setD().
	*@param a angle in radians
	*/
	void setRadians( const double &a );

/**return the equivalent angle between 0 and 360 degrees.
	*@warning does not change the value of the parent angle itself.
	*/
	const dms reduce( void ) const;

/**@return a nicely-formatted string representation of the angle
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

/**@short Static function to create a DMS object from a QString.
	*
	*There are several ways to specify the angle:
	*@li Integer numbers  ( 5 or -33 )
	*@li Floating-point numbers  ( 5.0 or -33.0 )
	*@li colon-delimited integers ( 5:0:0 or -33:0:0 )
	*@li colon-delimited with float seconds ( 5:0:0.0 or -33:0:0.0 )
	*@li colon-delimited with float minutes ( 5:0.0 or -33:0.0 )
	*@li space-delimited ( 5 0 0; -33 0 0 ) or ( 5 0.0 or -33 0.0 )
	*@li space-delimited, with unit labels ( 5h 0m 0s or -33d 0m 0s )
	*@param s the string to be parsed as an angle value
	*@param deg if TRUE, s is expressed in degrees; if FALSE, s is expressed in hours
	*@return a dms object whose value is parsed from the string argument
	*/
	static dms fromString(QString & s, bool deg);

private:
	double D;

	mutable double Radians;
	mutable double Sin, Cos;
	mutable bool scDirty, rDirty;
};

#endif
