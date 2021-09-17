/*
    SPDX-FileCopyrightText: 2016 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/* Project Includes */
#include "cachingdms.h"

/* KDE Includes */

/* Qt Includes */
#include <QString>

/* STL Includes */
#include <cmath>

#ifdef COUNT_DMS_SINCOS_CALLS
unsigned long CachingDms::cachingdms_constructor_calls = 0;
long CachingDms::cachingdms_delta             = 0; // difference of ( trig function calls ) - ( trig computations )
unsigned long CachingDms::cachingdms_bad_uses = 0;
#endif

CachingDms::CachingDms(const double &x) : dms(x)
{
    dms::SinCos(m_sin, m_cos);
#ifdef COUNT_DMS_SINCOS_CALLS
    ++cachingdms_constructor_calls;
    cachingdms_delta -= 2;
    m_cacheUsed = false;
#endif
}

#ifdef COUNT_DMS_SINCOS_CALLS
CachingDms::~CachingDms()
{
    if (!m_cacheUsed)
        ++cachingdms_bad_uses;
}
#endif

CachingDms::CachingDms(const QString &s, bool isDeg) : dms(s, isDeg)
{
    dms::SinCos(m_sin, m_cos);
#ifdef COUNT_DMS_SINCOS_CALLS
    ++cachingdms_constructor_calls;
    cachingdms_delta -= 2;
    m_cacheUsed = false;
#endif
}

CachingDms::CachingDms(const int &d, const int &m, const int &s, const int &ms) : dms(d, m, s, ms)
{
    dms::SinCos(m_sin, m_cos);
#ifdef COUNT_DMS_SINCOS_CALLS
    ++cachingdms_constructor_calls;
    cachingdms_delta -= 2;
    m_cacheUsed = false;
#endif
}

void CachingDms::setUsing_atan2(const double &y, const double &x)
{
    /*
     * NOTE: A bit of independent profiling shows that on my machine
     * (Intel Core i5, x86_64, glibc 2.24-2) the square-root based
     * computation below has some advantage, running ~ 70% faster on
     * average for some range of values.
     *
     */
    dms::setRadians(atan2(y, x));
    double r = sqrt(y * y + x * x);
    m_cos             = x / r;
    m_sin             = y / r;

#ifdef COUNT_DMS_SINCOS_CALLS
    if (!m_cacheUsed)
        ++cachingdms_bad_uses;
    m_cacheUsed = false;
#endif
    // One may be tempted to do the following:
    //   dms::setRadians( atan2( y, x ) );
    //   m_cos = dms::cos();
    //   m_sin = (y/x) * m_cos;
    // However, this has a problem when x = 0. The result for m_sin
    // must be 1, but instead the above code will result in NaN.
    // So we will need a conditional:
    //   m_sin = (x == 0) ? 1. : (y/x) * m_cos;
    // The conditional makes the performance worse than just setting
    // the angle and using sincos()
}

void CachingDms::setUsing_asin(const double &sine)
{
    dms::setRadians(asin(sine));
    m_sin = sine;
    // Note: The below is valid because in the range of asin, which is
    // [-pi/2, pi/2], cosine is always non-negative
    m_cos = std::sqrt(1 - sine * sine);
#ifdef COUNT_DMS_SINCOS_CALLS
    if (!m_cacheUsed)
        ++cachingdms_bad_uses;
    m_cacheUsed = false;
#endif
}

void CachingDms::setUsing_acos(const double &cosine)
{
    dms::setRadians(acos(cosine));
    m_cos = cosine;
    // Note: The below is valid because in the range of acos, which is
    // [0, pi], sine is always non-negative
    m_sin = std::sqrt(1 - cosine * cosine);
#ifdef COUNT_DMS_SINCOS_CALLS
    if (!m_cacheUsed)
        ++cachingdms_bad_uses;
    m_cacheUsed = false;
#endif
}

CachingDms CachingDms::fromString(const QString &s, bool deg)
{
    CachingDms result;
    result.setFromString(s, deg);
    return result;
}

CachingDms CachingDms::operator-()
{
    return CachingDms(-D, -m_sin, m_cos);
}

CachingDms::CachingDms(const dms &angle)
{
    D = angle.Degrees();
    dms::SinCos(m_sin, m_cos);
#ifdef COUNT_DMS_SINCOS_CALLS
    ++cachingdms_constructor_calls;
    cachingdms_delta -= 2;
    m_cacheUsed = false;
#endif
}

#ifdef COUNT_DMS_SINCOS_CALLS
CachingDms::CachingDms(const CachingDms &o)
{
    m_sin       = o.sin();
    m_cos       = o.cos();
    D           = o.D;
    m_cacheUsed = false;
}
CachingDms &CachingDms::operator=(const CachingDms &o)
{
    if (!m_cacheUsed)
        ++cachingdms_bad_uses;
    m_sin       = o.sin();
    m_cos       = o.cos();
    D           = o.D;
    m_cacheUsed = false;
    return (*this);
}
#endif

// Makes trig identities more readable:
#define sinA a.sin()
#define cosA a.cos()
#define sinB b.sin()
#define cosB b.cos()

// We use trigonometric addition / subtraction formulae to speed up
// computation. This way, we have no trigonometric function calls at
// all, but only floating point multiplications and
// addition/subtraction instead.
// The only caveat is that error can accumulate if used repeatedly!

CachingDms operator+(const CachingDms &a, const CachingDms &b)
{
    return CachingDms(a.Degrees() + b.Degrees(), sinA * cosB + cosA * sinB, cosA * cosB - sinA * sinB);
}

CachingDms operator-(const CachingDms &a, const CachingDms &b)
{
    return CachingDms(a.Degrees() - b.Degrees(), sinA * cosB - cosA * sinB, cosA * cosB + sinA * sinB);
}

CachingDms operator+(const dms &a, const CachingDms &b)
{
    return CachingDms(a + dms(b));
}

CachingDms operator-(const dms &a, const CachingDms &b)
{
    return CachingDms(a - dms(b));
}

CachingDms operator+(const CachingDms &a, const dms &b)
{
    return CachingDms(dms(a) + b);
}

CachingDms operator-(const CachingDms &a, const dms &b)
{
    return CachingDms(dms(a) - b);
}

#undef sinA
#undef cosA
#undef sinB
#undef cosB
