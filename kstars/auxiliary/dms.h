/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "../nan.h"

#include <QString>
#include <QDataStream>

#include <cmath>

//#define COUNT_DMS_SINCOS_CALLS true
//#define PROFILE_SINCOS true

#ifdef PROFILE_SINCOS
#include <ctime>
#endif

/** @class dms
 * @short An angle, stored as degrees, but expressible in many ways.
 * @author Jason Harris
 * @version 1.0
 *
 * dms encapsulates an angle.  The angle is stored as a double,
 * equal to the value of the angle in degrees.  Methods are available
 * for setting/getting the angle as a floating-point measured in
 * Degrees or Hours, or as integer triplets (degrees, arcminutes,
 * arcseconds or hours, minutes, seconds).  There is also a method
 * to set the angle according to a radian value, and to return the
 * angle expressed in radians.  Finally, a SinCos() method computes
 * the sin and cosine of the angle.
 */
class dms
{
  public:
    /** Default constructor. */
    dms()
        : D(NaN::d)
#ifdef COUNT_DMS_SINCOS_CALLS
          ,
          m_sinCosCalled(false), m_sinDirty(true), m_cosDirty(true)
#endif
    {
#ifdef COUNT_DMS_SINCOS_CALLS
        ++dms_constructor_calls;
#endif
    }

    /** Empty virtual destructor */
    virtual ~dms() = default;

    /** @short Set the floating-point value of the angle according to the four integer arguments.
         * @param d degree portion of angle (int).  Defaults to zero.
         * @param m arcminute portion of angle (int).  Defaults to zero.
         * @param s arcsecond portion of angle (int).  Defaults to zero.
         * @param ms arcsecond portion of angle (int).  Defaults to zero.
         */
    explicit dms(const int &d, const int &m = 0, const int &s = 0, const int &ms = 0)
#ifdef COUNT_DMS_SINCOS_CALLS
        : m_sinCosCalled(false), m_sinDirty(true), m_cosDirty(true)
#endif
    {
        dms::setD(d, m, s, ms);
#ifdef COUNT_DMS_SINCOS_CALLS
        ++dms_constructor_calls;
#endif
    }

    /** @short Construct an angle from a double value.
         *
         * Creates an angle whose value in Degrees is equal to the argument.
         * @param x angle expressed as a floating-point number (in degrees)
         */
    explicit dms(const double &x)
        : D(x)
#ifdef COUNT_DMS_SINCOS_CALLS
          ,
          m_sinCosCalled(false), m_sinDirty(true), m_cosDirty(true)
#endif
    {
#ifdef COUNT_DMS_SINCOS_CALLS
        ++dms_constructor_calls;
#endif
    }

    /** @short Construct an angle from a string representation.
         *
         * Attempt to create the angle according to the string argument.  If the string
         * cannot be parsed as an angle value, the angle is set to zero.
         *
         * @warning There is not an unambiguous notification that it failed to parse the string,
         * since the string could have been a valid representation of zero degrees.
         * If this is a concern, use the setFromString() function directly instead.
         *
         * @param s the string to parse as a dms value.
         * @param isDeg if true, value is in degrees; if false, value is in hours.
         * @sa setFromString()
         */
    explicit dms(const QString &s, bool isDeg = true)
#ifdef COUNT_DMS_SINCOS_CALLS
        : m_sinCosCalled(false), m_sinDirty(true), m_cosDirty(true)
#endif
    {
        setFromString(s, isDeg);
#ifdef COUNT_DMS_SINCOS_CALLS
        ++dms_constructor_calls;
#endif
    }

    /** @return integer degrees portion of the angle
         */
    inline int degree() const
    {
        if (std::isnan(D))
            return 0;

        return int(D);
    }

    /** @return integer arcminutes portion of the angle.
         * @note an arcminute is 1/60 degree.
         */
    int arcmin() const;

    /** @return integer arcseconds portion of the angle
         * @note an arcsecond is 1/60 arcmin, or 1/3600 degree.
         */
    int arcsec() const;

    /** @return integer milliarcseconds portion of the angle
         * @note a  milliarcsecond is 1/1000 arcsecond.
         */
    int marcsec() const;

    /** @return angle in degrees expressed as a double.
        	*/
    inline const double &Degrees() const { return D; }

    /** @return integer hours portion of the angle
         * @note an angle can be measured in degrees/arcminutes/arcseconds
         * or hours/minutes/seconds.  An hour is equal to 15 degrees.
         */
    inline int hour() const { return int(reduce().Degrees() / 15.0); }

    /** @return integer minutes portion of the angle
         * @note a minute is 1/60 hour (not the same as an arcminute)
         */
    int minute() const;

    /** @return integer seconds portion of the angle
         * @note a second is 1/3600 hour (not the same as an arcsecond)
         */
    int second() const;

    /** @return integer milliseconds portion of the angle
         * @note a millisecond is 1/1000 second (not the same as a milliarcsecond)
         */
    int msecond() const;

    /** @return angle in hours expressed as a double in the range 0 to 23.999...
         * @note an angle can be measured in degrees/arcminutes/arcseconds
         * or hours/minutes/seconds.  An hour is equal to 15 degrees.
         */
    inline double Hours() const { return reduce().Degrees() / 15.0; }

    /** @return angle in hours expressed as a double in the range -11.999 to 0 to 12.0
         * @note an angle can be measured in degrees/arcminutes/arcseconds
         * or hours/minutes/seconds.  An hour is equal to 15 degrees.
         */
    inline double HoursHa() const { return Hours() <= 12.0 ? Hours() : Hours() - 24.0; }

    /** Sets floating-point value of angle, in degrees.
         * @param x new angle (double)
         */
    inline virtual void setD(const double &x)
    {
#ifdef COUNT_DMS_SINCOS_CALLS
        m_sinDirty = m_cosDirty = true;
#endif
        D = x;
    }

    /** @short Sets floating-point value of angle, in degrees.
         *
         * This is an overloaded member function; it behaves essentially
         * like the above function.  The floating-point value of the angle
         * (D) is determined from the following formulae:
         *
         * \f$ fabs(D) = fabs(d) + \frac{(m + (s/60))}{60} \f$
         * \f$ sgn(D) = sgn(d) \f$
         *
         * @param d integer degrees portion of angle
         * @param m integer arcminutes portion of angle
         * @param s integer arcseconds portion of angle
         * @param ms integer arcseconds portion of angle
         */
    virtual void setD(const int &d, const int &m, const int &s, const int &ms = 0);

    /** @short Sets floating-point value of angle, in hours.
         *
         * Converts argument from hours to degrees, then
         * sets floating-point value of angle, in degrees.
         * @param x new angle, in hours (double)
         * @sa setD()
         */
    inline virtual void setH(const double &x)
    {
        dms::setD(x * 15.0);
#ifdef COUNT_DMS_SINCOS_CALLS
        m_cosDirty = m_sinDirty = true;
#endif
    }

    /** @short Sets floating-point value of angle, in hours.
         *
         * Converts argument values from hours to degrees, then
         * sets floating-point value of angle, in degrees.
         * This is an overloaded member function, provided for convenience.  It
         * behaves essentially like the above function.
         * @param h integer hours portion of angle
         * @param m integer minutes portion of angle
         * @param s integer seconds portion of angle
         * @param ms integer milliseconds portion of angle
         * @sa setD()
         */
    virtual void setH(const int &h, const int &m, const int &s, const int &ms = 0);

    /** @short Attempt to parse the string argument as a dms value, and set the dms object
         * accordingly.
         * @param s the string to be parsed as a dms value.  The string can be an int or
         * floating-point value, or a triplet of values (d/h, m, s) separated by spaces or colons.
         * @param isDeg if true, the value is in degrees.  Otherwise, it is in hours.
         * @return true if sting was parsed successfully.  Otherwise, set the dms value
         * to 0.0 and return false.
         */
    virtual bool setFromString(const QString &s, bool isDeg = true);

    /** @short Compute Sine and Cosine of the angle simultaneously.
         * On machines using glibc >= 2.1, calling SinCos() is somewhat faster
         * than calling sin() and cos() separately.
         * The values are returned through the arguments (passed by reference).
         *
         * @param s Sine of the angle
         * @param c Cosine of the angle
         * @sa sin() cos()
         */
    inline void SinCos(double &s, double &c) const;

    /** @short Compute the Angle's Sine.
         *
         * @return the Sine of the angle.
         * @sa cos()
         */
    double sin() const
    {
#ifdef COUNT_DMS_SINCOS_CALLS
        if (!m_sinCosCalled)
        {
            m_sinCosCalled = true;
            ++dms_with_sincos_called;
        }
        if (m_sinDirty)
            m_sinDirty = false;
        else
            ++redundant_trig_function_calls;
        ++trig_function_calls;
#endif
#ifdef PROFILE_SINCOS
        std::clock_t start, stop;
        double s;
        start = std::clock();
        s     = ::sin(D * DegToRad);
        stop  = std::clock();
        seconds_in_trig += double(stop - start) / double(CLOCKS_PER_SEC);
        return s;
#else
        return ::sin(D * DegToRad);
#endif
    }

    /** @short Compute the Angle's Cosine.
         *
         * @return the Cosine of the angle.
         * @sa sin()
         */
    double cos() const
    {
#ifdef COUNT_DMS_SINCOS_CALLS
        if (!m_sinCosCalled)
        {
            m_sinCosCalled = true;
            ++dms_with_sincos_called;
        }
        if (m_cosDirty)
            m_cosDirty = false;
        else
            ++redundant_trig_function_calls;
        ++trig_function_calls;
#endif
#ifdef PROFILE_SINCOS
        std::clock_t start, stop;
        double c;
        start = std::clock();
        c     = ::cos(D * DegToRad);
        stop  = std::clock();
        seconds_in_trig += double(stop - start) / double(CLOCKS_PER_SEC);
        return c;
#else
        return ::cos(D * DegToRad);
#endif
    }

    /** @short Express the angle in radians.
         * @return the angle in radians (double)
         */
    inline double radians() const { return D * DegToRad; }

    /** @short Set angle according to the argument, in radians.
         *
         * This function converts the argument to degrees, then sets the angle
         * with setD().
         * @param Rad an angle in radians
         */
    inline virtual void setRadians(const double &Rad)
    {
        dms::setD(Rad / DegToRad);
#ifdef COUNT_DMS_SINCOS_CALLS
        m_cosDirty = m_sinDirty = true;
#endif
    }

    /** return the equivalent angle between 0 and 360 degrees.
         * @warning does not change the value of the parent angle itself.
         */
    const dms reduce() const;

    /**
     * @brief deltaAngle Return the shortest difference (path) between this angle and the supplied angle. The range is normalized to [-180,+180]
     * @param angle Angle to subtract from current angle.
     * @return Normalized angle in the range [-180,+180]
     */
    const dms deltaAngle(dms angle) const;

    /**
         * @short an enum defining standard angle ranges
         */
    enum AngleRanges
    {
        ZERO_TO_2PI,
        MINUSPI_TO_PI
    };

    /**
         * @short Reduce _this_ angle to the given range
         */
    void reduceToRange(enum dms::AngleRanges range);

    /** @return a nicely-formatted string representation of the angle
         * in degrees, arcminutes, and arcseconds.
         * @param forceSign if @c true then adds '+' or '-' to the string
         * @param machineReadable uses a colon separator and produces +/-dd:mm:ss format instead
         * @param highPrecision adds milliseconds, if @c false the seconds will be shown as an integer
         */
    const QString toDMSString(const bool forceSign = false, const bool machineReadable = false, const bool highPrecision=false) const;

    /** @return a nicely-formatted string representation of the angle
         * in hours, minutes, and seconds.
         * @param machineReadable uses a colon separator and produces hh:mm:ss format instead
         * @param highPrecision adds milliseconds, if @c false the seconds will be shown as an integer
         */
    const QString toHMSString(const bool machineReadable = false, const bool highPrecision=false) const;

    /** PI is a const static member; it's public so that it can be used anywhere,
         * as long as dms.h is included.
         */
    static constexpr double PI = { M_PI };

    /** DegToRad is a const static member equal to the number of radians in
         * one degree (dms::PI/180.0).
         */
    static constexpr double DegToRad = { M_PI / 180.0 };

    /** @short Static function to create a DMS object from a QString.
         *
         * There are several ways to specify the angle:
         * @li Integer numbers  ( 5 or -33 )
         * @li Floating-point numbers  ( 5.0 or -33.0 )
         * @li colon-delimited integers ( 5:0:0 or -33:0:0 )
         * @li colon-delimited with float seconds ( 5:0:0.0 or -33:0:0.0 )
         * @li colon-delimited with float minutes ( 5:0.0 or -33:0.0 )
         * @li space-delimited ( 5 0 0; -33 0 0 ) or ( 5 0.0 or -33 0.0 )
         * @li space-delimited, with unit labels ( 5h 0m 0s or -33d 0m 0s )
         * @param s the string to be parsed as an angle value
         * @param deg if true, s is expressed in degrees; if false, s is expressed in hours
         * @return a dms object whose value is parsed from the string argument
         */
    static dms fromString(const QString &s, bool deg);

    inline dms operator-() { return dms(-D); }
#ifdef COUNT_DMS_SINCOS_CALLS
    static long unsigned dms_constructor_calls; // counts number of DMS constructor calls
    static long unsigned dms_with_sincos_called;
    static long unsigned trig_function_calls;           // total number of trig function calls
    static long unsigned redundant_trig_function_calls; // counts number of redundant trig function calls
    static double seconds_in_trig;                      // accumulates number of seconds spent in trig function calls
#endif

  protected:
    double D;

  private:
#ifdef COUNT_DMS_SINCOS_CALLS
    mutable bool m_sinDirty, m_cosDirty, m_sinCosCalled;
#endif

    friend dms operator+(dms, dms);
    friend dms operator-(dms, dms);
    friend QDataStream &operator<<(QDataStream &out, const dms &d);
    friend QDataStream &operator>>(QDataStream &in, dms &d);
};

/// Add two angles
inline dms operator+(dms a, dms b)
{
    return dms(a.D + b.D);
}

/// Subtract angles
inline dms operator-(dms a, dms b)
{
    return dms(a.D - b.D);
}

// Inline sincos
inline void dms::SinCos(double &s, double &c) const
{
#ifdef PROFILE_SINCOS
    std::clock_t start, stop;
    start = std::clock();
#endif

#ifdef __GLIBC__
#if (__GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1 && !defined(__UCLIBC__))
    //GNU version
    sincos(radians(), &s, &c);
#else
    //For older GLIBC versions
    s = ::sin(radians());
    c = ::cos(radians());
#endif
#else
    //ANSI-compliant version
    s = ::sin(radians());
    c = ::cos(radians());
#endif

#ifdef PROFILE_SINCOS
    stop = std::clock();
    seconds_in_trig += double(stop - start) / double(CLOCKS_PER_SEC);
#endif

#ifdef COUNT_DMS_SINCOS_CALLS
    if (!m_sinCosCalled)
    {
        m_sinCosCalled = true;
        ++dms_with_sincos_called;
    }
    if (m_sinDirty)
        m_sinDirty = false;
    else
        ++redundant_trig_function_calls;

    if (m_cosDirty)
        m_cosDirty = false;
    else
        ++redundant_trig_function_calls;

    trig_function_calls += 2;
#endif
}

/** Overloaded equality operator */
inline bool operator==(const dms &a1, const dms &a2)
{
    return a1.Degrees() == a2.Degrees();
}
