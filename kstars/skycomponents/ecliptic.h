/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <bowlin@mindspring.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "linelistindex.h"
#include "linelistlabel.h"

/**
 * @class Ecliptic
 * Represents the ecliptic on the sky map.
 *
 * @author James B. Bowlin
 * @version 0.1
 */
class Ecliptic : public LineListIndex
{
  public:
    /**
     * @short Constructor
     * @p parent pointer to the parent SkyComposite object name is the name of the subclass
     */
    explicit Ecliptic(SkyComposite *parent);

    void draw(SkyPainter *skyp) override;
    virtual void drawCompassLabels();
    bool selected() override;

    LineListLabel *label() override { return &m_label; }

  private:
    LineListLabel m_label;
};
