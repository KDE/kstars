/*
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "orthographicprojector.h"

OrthographicProjector::OrthographicProjector(const ViewParams &p) : Projector(p)
{
    updateClipPoly();
}

Projector::Projection OrthographicProjector::type() const
{
    return Orthographic;
}

double OrthographicProjector::radius() const
{
    return 1.0;
}

double OrthographicProjector::projectionK(double x) const
{
    Q_UNUSED(x);
    return 1.0;
}

double OrthographicProjector::projectionL(double x) const
{
    return asin(x);
}
