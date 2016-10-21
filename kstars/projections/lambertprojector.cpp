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

#include "lambertprojector.h"

LambertProjector::LambertProjector(const ViewParams& p) : Projector(p)
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
    return sqrt( 2.0/( 1.0 + x ) );
}

double LambertProjector::projectionL(double x) const
{
    return 2.0*asin(0.5*x);
}
