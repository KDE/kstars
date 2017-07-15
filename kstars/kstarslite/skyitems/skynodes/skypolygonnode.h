/** *************************************************************************
                          skypolygonnode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 23/06/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#pragma once

#include "skynode.h"

class PolyNode;

/**
 *
 * @class SkyPolygonNode
 *
 * @short A SkyNode derived class that represents polygon (either filled or non-filled)
 * One of the applications of this class is drawing of Milky Way.
 * @author Artem Fedoskin
 * @version 1.0
 */

class SkyPolygonNode : public SkyNode
{
  public:
    /**
     * @short Constructor.
     * @param list - Used of lines that comprise polygon
     */
    SkyPolygonNode(LineList *list);

    /**
     * @short Update position and visibility of this polygon.
     * @note This is not an overridden function because it requires a parameter
     * @param forceClip - true if a polygon should be clipped
     */
    void update(bool forceClip = true);
    virtual void hide() override;
    LineList *lineList() { return m_list; }

    void setColor(QColor color);

  private:
    LineList *m_list { nullptr };
    PolyNode *m_polygonNode { nullptr };
};
