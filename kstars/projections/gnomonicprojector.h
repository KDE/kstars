/*
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
