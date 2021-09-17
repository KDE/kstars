/*
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef EQUIRECTANGULARPROJECTOR_H
#define EQUIRECTANGULARPROJECTOR_H

#include "projector.h"

/**
 * @class EquirectangularProjector
 *
 * Implememntation of <a href="https://en.wikipedia.org/wiki/Equirectangular_projection">Equirectangular projection</a>
 *
 */
class EquirectangularProjector : public Projector
{
    public:
        explicit EquirectangularProjector(const ViewParams &p);
        Projection type() const override;
        double radius() const override;
        bool unusablePoint(const QPointF &p) const override;
        Eigen::Vector2f toScreenVec(const SkyPoint *o, bool oRefract = true, bool *onVisibleHemisphere = nullptr) const override;
        SkyPoint fromScreen(const QPointF &p, dms *LST, const dms *lat, bool onlyAltAz = false) const override;
        QVector<Eigen::Vector2f> groundPoly(SkyPoint *labelpoint = nullptr, bool *drawLabel = nullptr) const override;
        void updateClipPoly() override;
};

#endif // EQUIRECTANGULARPROJECTOR_H
