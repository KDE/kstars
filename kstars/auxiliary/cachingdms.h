/***************************************************************************
                        cachingdms.h  - KStars Planetarium
                             -------------------
    begin                : Sat 24 Sep 2016 02:18:26 CDT
    copyright            : (c) 2016 by Akarsh Simha
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



#ifndef CACHINGDMS_H
#define CACHINGDMS_H

#include "dms.h"

/**
 * @class CachingDms
 * @short a dms subclass that caches its sine and cosine values every time the angle is changed.
 * @note This is to be used for those angles where sin/cos is repeatedly computed.
 * @author Akarsh Simha <akarsh@kde.org>
 */

class CachingDms : public dms {

public:

    /**
     * @short Default Constructor
     */
    CachingDms() : dms(), m_sin( NaN::d ), m_cos( NaN::d ) {
#ifdef COUNT_DMS_SINCOS_CALLS
        m_cacheUsed = true;
        ++cachingdms_constructor_calls;
#endif
    };

    /**
     * @short Degree angle constructor
     * @param x is the angle in degrees
     */
    explicit CachingDms(const double &x );

    /**
     * @short QString constructor
     */
    explicit CachingDms( const QString &s, bool isDeg=true );

    /**
     * @short DMS representation constructor
     */
    explicit CachingDms( const int &d, const int &m=0, const int &s=0, const int &ms=0 );

    /**
     * @short Sets the angle in degrees supplied as a double
     * @note Re-implements dms::setD() with sine/cosine caching
     */
    inline void setD( const double &x ) { dms::setD( x ); dms::SinCos( m_sin, m_cos );
#ifdef COUNT_DMS_SINCOS_CALLS
        cachingdms_delta -= 2;
        if( !m_cacheUsed )
            ++cachingdms_bad_uses;
        m_cacheUsed = false;
#endif
    }

    /**
     * @short Sets the angle in hours, supplied as a double
     * @note Re-implements dms::setH() with sine/cosine caching
     * @note While this and other methods internally call setD, we want to avoid unnecessary vtable lookups. We'd rather have inline than virtual when speed matters in general.
     */
    inline void setH( const double &x ) { dms::setH( x ); dms::SinCos( m_sin, m_cos );
#ifdef COUNT_DMS_SINCOS_CALLS
        cachingdms_delta -= 2;
        if( !m_cacheUsed )
            ++cachingdms_bad_uses;
        m_cacheUsed = false;
#endif
    }

    /**
     * @short Sets the angle in HMS form
     * @note Re-implements dms::setH() with sine/cosine caching
     */
    inline void setH( const int &h, const int &m, const int &s, const int &ms=0 ) { dms::setH( h, m, s, ms ); dms::SinCos( m_sin, m_cos );
#ifdef COUNT_DMS_SINCOS_CALLS
        cachingdms_delta -= 2;
#endif
    }

    /**
     * @short Sets the angle from string
     * @note Re-implements dms::setFromString()
     */
    inline void setFromString( const QString &s, bool isDeg = true ) { dms::setFromString( s, isDeg ); dms::SinCos( m_sin, m_cos );
#ifdef COUNT_DMS_SINCOS_CALLS
        cachingdms_delta -= 2;
        if( !m_cacheUsed )
            ++cachingdms_bad_uses;
        m_cacheUsed = false;
#endif
    }

    /**
     * @short Sets the angle in radians
     */
    inline void setRadians( const double &a ) { dms::setRadians( a ); dms::SinCos( m_sin, m_cos );
#ifdef COUNT_DMS_SINCOS_CALLS
        cachingdms_delta -= 2;
        if( !m_cacheUsed )
            ++cachingdms_bad_uses;
        m_cacheUsed = false;
#endif
    }

    /**
     * @short Sets the angle using atan2()
     * @note The advantage is that we can calculate sin/cos faster because we know the tangent
     */
    void setUsing_atan2( const double & y, const double & x );

    /**
     * @short Sets the angle using asin()
     * @param sine Sine of the angle
     * @note The advantage is that we can cache the sine value supplied
     * @note The limited range of asin must be borne in mind
     */
    void setUsing_asin( const double & sine );

    /**
     * @short Sets the angle using acos()
     * @param cosine Cosine of the angle
     * @note The advantage is that we can cache the cosine value supplied
     * @note The limited range of acos must be borne in mind
     */
    void setUsing_acos( const double & cosine );

    /**
     * @short Get the sine and cosine together
     * @note Re-implements dms::SinCos()
     * @note This just uses the cached values assuming that they are good
     */
    inline void SinCos( double &s, double &c ) const { s = m_sin; c = m_cos;
#ifdef COUNT_DMS_SINCOS_CALLS
        cachingdms_delta += 2;
        m_cacheUsed = true;
#endif
    }

    /**
     * @short Get the sine of this angle
     * @note Re-implements dms::sin()
     * @note This just uses the cached value assuming that it is good
     */
    inline double sin() const {
#ifdef COUNT_DMS_SINCOS_CALLS
        ++cachingdms_delta;
        m_cacheUsed = true;
#endif
        return m_sin; }

    /**
     * @short Get the cosine of this angle
     * @note Re-implements dms::cos()
     * @note This just uses the cached value assuming that it is good
     */
    inline double cos() const {
#ifdef COUNT_DMS_SINCOS_CALLS
        ++cachingdms_delta;
        m_cacheUsed = true;
#endif
        return m_cos; }

    /**
     * @short Construct an angle from the given string
     * @note Re-implements dms::fromString()
     */
    static CachingDms fromString( const QString &s, bool deg );

    /**
     * @short operator -
     * @note In addition to negating the angle, we negate the sine value
     */
    CachingDms operator - ();

    /**
     * @short Casting constructor
     */
    CachingDms( const dms &angle );

private:
    double m_sin, m_cos; // Cached values

    /**
     * @short Private constructor used to create a CachingDms with known sine and cosine
     */
    explicit CachingDms( const double &degrees, const double &sine, const double &cosine )
        : dms( degrees ), m_sin( sine ), m_cos( cosine ) {
#ifdef COUNT_DMS_SINCOS_CALLS
        ++cachingdms_constructor_calls;
        m_cacheUsed = false;
#endif
            }

    /**
     * @short Addition and subtraction operators
     * @note Uses trigonometric identities to find the new trigonometric values
     * @note Avoid repeated use, as the round-off errors will accumulate!
     */
    friend CachingDms operator +(const CachingDms &, const CachingDms &);
    friend CachingDms operator -(const CachingDms &, const CachingDms &);

#ifdef COUNT_DMS_SINCOS_CALLS
private:
    mutable bool m_cacheUsed;
public:
    static unsigned long cachingdms_constructor_calls;
    static long cachingdms_delta; // difference of ( trig function calls ) - ( trig computations )
    static unsigned long cachingdms_bad_uses;
#endif
};

#endif
