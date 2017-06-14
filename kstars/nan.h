/***************************************************************************
                      nan.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sat 16 Mar 2013 17:50:49 CDT
    copyright            : (c) 2013 by Akarsh Simha
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

#pragma once

#include <limits>

// The way of doing this in C++ 11. Leaving it here for future purposes.
#if 0
namespace NaN
{
constexpr double d = std::numeric_limits::quiet_NaN();
constexpr float f = std::numeric_limits<float>::quiet_NaN();
constexpr long double ld = std::numeric_limits<long double>::quiet_NaN();
}
#endif

namespace NaN
{
const double d       = std::numeric_limits<double>::quiet_NaN();
const float f        = std::numeric_limits<float>::quiet_NaN();
const long double ld = std::numeric_limits<long double>::quiet_NaN();
}
