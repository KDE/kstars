/*
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifdef _WIN32
#include <windows.h>
#endif

#include "azimuthalequidistantprojector.h"

AzimuthalEquidistantProjector::AzimuthalEquidistantProjector(const ViewParams &p) : Projector(p)
{
    updateClipPoly();
}

Projector::Projection AzimuthalEquidistantProjector::type() const
{
    return AzimuthalEquidistant;
}

double AzimuthalEquidistantProjector::radius() const
{
    return 1.57079633;
}

double AzimuthalEquidistantProjector::projectionK(double x) const
{
    double crad = acos(x);
    return ((crad != 0) ? crad / sin(crad) : 1); // This handles the 0/0 case. The limit of x / sin(x) is 1 as x -> 0.
}

double AzimuthalEquidistantProjector::projectionL(double x) const
{
    return x;
}
