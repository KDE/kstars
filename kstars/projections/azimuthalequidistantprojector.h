/*
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef AZIMUTHALEQUIDISTANTPROJECTOR_H
#define AZIMUTHALEQUIDISTANTPROJECTOR_H

#include "projector.h"

/**
 * @class AzimuthalEquidistantProjector
 *
 * Implememntation of <a href="https://en.wikipedia.org/wiki/Azimuthal_equidistant_projection">Azimuthal equidistant projection</a>
 *
 */
class AzimuthalEquidistantProjector : public Projector
{
  public:
    explicit AzimuthalEquidistantProjector(const ViewParams &p);
    Projection type() const override;
    double radius() const override;
    double projectionK(double x) const override;
    double projectionL(double x) const override;
};

#endif // AZIMUTHALEQUIDISTANTPROJECTOR_H
