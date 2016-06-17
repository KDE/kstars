/** *************************************************************************
                          skyopacitynode.h  -  K Desktop Planetarium
                             -------------------
    begin                : 16/06/2016
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

#ifndef SKYOPACITYNODE_H_
#define SKYOPACITYNODE_H_

#include <QSGOpacityNode>

/** @class SkyOpacityNode
 *
 *@short A wrapper for QSGOpacityNode that provides hide() and show() functions. If node is invisible
 * (opacity is 0) it won't be rendered.
 *@author Artem Fedoskin
 *@version 1.0
 */

class SkyOpacityNode : public QSGOpacityNode {
public:
    SkyOpacityNode();

    /**
     * @short makes this node visible
     */
    void show();

    /**
     * @short hides this node
     */
    void hide();

    /**
     * @return true if node is visible
     */
    bool visible();
};

#endif
