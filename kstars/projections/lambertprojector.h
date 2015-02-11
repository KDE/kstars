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

#ifndef LAMBERTPROJECTOR_H
#define LAMBERTPROJECTOR_H

#include "projector.h"
#include "skymap.h"

/**
 * @class LambertProjector
 *
 * Implememntation of <a href="http://en.wikipedia.org/wiki/Lambert_azimuthal_equal-area_projection">Lambert azimuthal equal-area projection</a>
 *
 */
class LambertProjector : public Projector
{
public:
    explicit LambertProjector(const ViewParams& p);
    virtual ~LambertProjector() {}
    virtual SkyMap::Projection type() const;
    virtual double radius() const;
    virtual double projectionK(double x) const;
    virtual double projectionL(double x) const;
};

#endif // LAMBERTPROJECTOR_H
