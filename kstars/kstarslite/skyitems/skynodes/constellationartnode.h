/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skynode.h"

class QSGSimpleTextureNode;

class ConstellationsArt;
class RootNode;

/**
 * @class ConstellationArtNode
 *
 * @short A SkyNode derived class that represents ConstellationsArt object.
 * @author Artem Fedoskin
 * @version 1.0
 */
class ConstellationArtNode : public SkyNode
{
  public:
    /**
     * @short Constructor
     * @param obj - a pointer to ConstellationsArt object that is represented by this node
     */
    explicit ConstellationArtNode(ConstellationsArt *obj);

    /**
     * @short changePos change the position of this node
     * @param pos - new position
     * @param positionangle - an angle of ConstellationsArt image rotation
     */
    void changePos(QPointF pos, double positionangle);

    virtual void update() override;
    virtual void hide() override;

  private:
    ConstellationsArt *m_art { nullptr };
    QSGSimpleTextureNode *m_texture { nullptr };
};
