/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skynode.h"

class PolyNode;

/**
 * @class HorizonNode
 * @short This node acts as a parent of nodes that comprise horizon and both filled and non-filled
 * ground
 *
 * @version 1.0
 * @author Artem Fedoskin
 */
class HorizonNode : public SkyNode
{
  public:
    /** @short Constructor */
    HorizonNode();

    /** @short Update child nodes based on user settings (filled/non-filled ground) and their visibility */
    virtual void update() override;
    virtual void hide() override;

  private:
    PolyNode *m_polygonNode { nullptr };
};
