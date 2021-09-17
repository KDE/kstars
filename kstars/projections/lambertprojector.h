/*
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef LAMBERTPROJECTOR_H
#define LAMBERTPROJECTOR_H

#include "projector.h"

/**
 * @class LambertProjector
 *
 * Implememntation of <a href="https://en.wikipedia.org/wiki/Lambert_azimuthal_equal-area_projection">Lambert azimuthal equal-area projection</a>
 *
 */
class LambertProjector : public Projector
{
  public:
    explicit LambertProjector(const ViewParams &p);
    ~LambertProjector() override = default;
    Projection type() const override;
    double radius() const override;
    double projectionK(double x) const override;
    double projectionL(double x) const override;
};

#endif // LAMBERTPROJECTOR_H
