/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "../../skyopacitynode.h"

class QSGSimpleTextureNode;

class RootNode;

/**
 * @class PointNode
 * @short SkyOpacityNode derived class that represents stars and planets using cached QSGTexture
 *
 * A SkyOpacityNode derived class used for representing stars and planets as stars. Upon
 * construction loads the texture of star cached in parentNode
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class PointNode : public SkyOpacityNode
{
  public:
    /**
     * @short Constructor
     * @param p pointer to the top parent RootNode which holds texture cache
     * @param sp spectral type
     * @param size initial size of PointNode
     */
    explicit PointNode(RootNode *p, char sp = 'A', float size = 1);

    /**
     * @short setSize update size of PointNode with the given parameter
     * @param size new size of PointNode
     */
    void setSize(float size);

    QSizeF size() const;

  private:
    char spType { 0 };
    QSGSimpleTextureNode *texture { nullptr };
    //parentNode holds texture cache
    RootNode *m_rootNode { nullptr };
    float m_size { -1 };
    uint starColorMode { 0 };
};
