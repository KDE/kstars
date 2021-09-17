/*
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef STEREOGRAPHICPROJECTOR_H
#define STEREOGRAPHICPROJECTOR_H

#include "projector.h"

/**
 * @class StereographicProjector
 *
 * Implememntation of <a href="https://en.wikipedia.org/wiki/Stereographic_projection">Stereographic projection</a>
 *
 */
class StereographicProjector : public Projector
{
  public:
    explicit StereographicProjector(const ViewParams &p);
    Projection type() const override;
    double radius() const override;
    double projectionK(double x) const override;
    double projectionL(double x) const override;
};

#endif // STEREOGRAPHICPROJECTOR_H
