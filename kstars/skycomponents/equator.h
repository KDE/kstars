/***************************************************************************
                          equator.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-08-09
    copyright            : (C) 2007 by James B. Bowlin
    email                : bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

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
