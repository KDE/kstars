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

#ifndef GNOMONICPROJECTOR_H
#define GNOMONICPROJECTOR_H

#include "projector.h"

/**
 * @class GnomonicProjector
 *
 * Implememntation of <a href="https://en.wikipedia.org/wiki/Gnomonic_projection">Gnomonic projection</a>
 *
 */
class GnomonicProjector : public Projector
{
  public:
    explicit GnomonicProjector(const ViewParams &p);
    Projection type() const override;
    double radius() const override;
    double projectionK(double x) const override;
    double projectionL(double x) const override;
    double cosMaxFieldAngle() const override;
};

#endif // GNOMONICPROJECTOR_H
