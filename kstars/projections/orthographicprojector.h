/*
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef ORTHOGRAPHICPROJECTOR_H
#define ORTHOGRAPHICPROJECTOR_H

#include "projector.h"

/**
 * @class OrthographicProjector
 *
 * Implememntation of <a href="https://en.wikipedia.org/wiki/Orthographic_projection">Orthographic projection</a>
 *
 */
class OrthographicProjector : public Projector
{
  public:
    explicit OrthographicProjector(const ViewParams &p);
    Projection type() const override;
    double radius() const override;
    double projectionK(double x) const override;
    double projectionL(double x) const override;
};

#endif // ORTHOGRAPHICPROJECTOR_H
