/*
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "lambertprojector.h"

LambertProjector::LambertProjector(const ViewParams &p) : Projector(p)
{
    updateClipPoly();
}

Projector::Projection LambertProjector::type() const
{
    return Lambert;
}

double LambertProjector::radius() const
{
    return 1.41421356;
}

double LambertProjector::projectionK(double x) const
{
    return sqrt(2.0 / (1.0 + x));
}

double LambertProjector::projectionL(double x) const
{
    return 2.0 * asin(0.5 * x);
}
