/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QSGOpacityNode>

/**
 * @class SkyOpacityNode
 *
 * @short A wrapper for QSGOpacityNode that provides hide() and show() functions. If node is invisible
 * (opacity is 0) it won't be rendered.
 *
 * @author Artem Fedoskin
 * @version 1.0
 */

class SkyOpacityNode : public QSGOpacityNode
{
  public:
    SkyOpacityNode();

    /** @short makes this node visible */
    virtual void show();

    /** @short hides this node */
    virtual void hide();

    /** @return true if node is visible */
    bool visible();
};
