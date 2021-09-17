/*
    SPDX-FileCopyrightText: 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars:
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vect.h"

#include <cmath>
#include <cstdlib>

namespace GuiderUtils
{

Vector operator^(const Vector &u, const Vector &v)
{
    return Vector(u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x);
}

Vector RndVector()
{
    Vector v(rand() - 0.5 * RAND_MAX, rand() - 0.5 * RAND_MAX, rand() - 0.5 * RAND_MAX);
    return Normalize(v);
}

Vector &Clip(Vector &v)
{
    if (v.x < 0.0)
        v.x = 0.0;
    else if (v.x > 1.0)
        v.x = 1.0;
    if (v.y < 0.0)
        v.y = 0.0;
    else if (v.y > 1.0)
        v.y = 1.0;
    if (v.z < 0.0)
        v.z = 0.0;
    else if (v.z > 1.0)
        v.z = 1.0;

    return v;
}
}  // namespace GuiderUtils
