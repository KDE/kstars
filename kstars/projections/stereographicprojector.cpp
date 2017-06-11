/*
    Copyright (C) 2010 Henry de Valence <hdevalence@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

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
