/*
    SPDX-FileCopyrightText: 2013 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
