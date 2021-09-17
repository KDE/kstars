/*
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gnomonicprojector.h"

GnomonicProjector::GnomonicProjector(const ViewParams &p) : Projector(p)
{
    updateClipPoly();
}

Projector::Projection GnomonicProjector::type() const
{
    return Gnomonic;
}

double GnomonicProjector::radius() const
{
    return 2 * M_PI;
}

double GnomonicProjector::projectionK(double x) const
{
    return 1.0 / x;
}

double GnomonicProjector::projectionL(double x) const
{
    return atan(x);
}

double GnomonicProjector::cosMaxFieldAngle() const
{
    //Don't let things approach infty.
    return 0.02;
}
