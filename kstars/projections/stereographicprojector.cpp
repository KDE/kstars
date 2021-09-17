/*
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "stereographicprojector.h"

StereographicProjector::StereographicProjector(const ViewParams &p) : Projector(p)
{
    updateClipPoly();
}

Projector::Projection StereographicProjector::type() const
{
    return Stereographic;
}

double StereographicProjector::radius() const
{
    return 2.;
}

double StereographicProjector::projectionK(double x) const
{
    return 2.0 / (1.0 + x);
}

double StereographicProjector::projectionL(double x) const
{
    return 2.0 * atan2(x, 2.0);
}
