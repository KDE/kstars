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

#ifndef EQUIRECTANGULARPROJECTOR_H
#define EQUIRECTANGULARPROJECTOR_H

#include "projector.h"

/**
 * @class EquirectangularProjector
 *
 * Implememntation of <a href="http://en.wikipedia.org/wiki/Equirectangular_projection">Equirectangular projection</a>
 *
 */
class EquirectangularProjector : public Projector
{
  public:
    explicit EquirectangularProjector(const ViewParams &p);
    Projection type() const Q_DECL_OVERRIDE;
    double radius() const Q_DECL_OVERRIDE;
    bool unusablePoint(const QPointF &p) const Q_DECL_OVERRIDE;
    Vector2f toScreenVec(const SkyPoint *o, bool oRefract = true, bool *onVisibleHemisphere = 0) const Q_DECL_OVERRIDE;
    SkyPoint fromScreen(const QPointF &p, dms *LST, const dms *lat) const Q_DECL_OVERRIDE;
    QVector<Vector2f> groundPoly(SkyPoint *labelpoint = 0, bool *drawLabel = 0) const Q_DECL_OVERRIDE;
    void updateClipPoly() Q_DECL_OVERRIDE;
};

#endif // EQUIRECTANGULARPROJECTOR_H
