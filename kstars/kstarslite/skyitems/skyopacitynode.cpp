/** *************************************************************************
                          skyopacitynode.cpp  -  K Desktop Planetarium
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

#include "skyopacitynode.h"

SkyOpacityNode::SkyOpacityNode() {

}

void SkyOpacityNode::show() {
    if(!opacity()) {
        setOpacity(1);
        markDirty(QSGNode::DirtyOpacity);
    }
}

void SkyOpacityNode::hide() {
    if(opacity()) {
        setOpacity(0);
        markDirty(QSGNode::DirtyOpacity);
    }
}

bool SkyOpacityNode::visible() {
    if(opacity() != 0) {
        return true;
    }
    return false;
}
