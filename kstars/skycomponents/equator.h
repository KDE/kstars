/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <bowlin@mindspring.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "linelistlabel.h"
#include "noprecessindex.h"

/**
 * @class Equator
 * Represents the equator on the sky map.
 *
 * @author James B. Bowlin
 * @version 0.1
 */
class Equator : public NoPrecessIndex
{
  public:
    /**
     * @short Constructor
     * @p parent pointer to the parent SkyComposite object name is the name of the subclass
     */
    explicit Equator(SkyComposite *parent);

    bool selected() override;
    void draw(SkyPainter *skyp) override;
    virtual void drawCompassLabels();
    LineListLabel *label() override { return &m_label; }

  protected:
    void preDraw(SkyPainter *skyp) override;

  private:
    LineListLabel m_label;
};
