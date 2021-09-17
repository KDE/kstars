/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skyopacitynode.h"

SkyOpacityNode::SkyOpacityNode()
{
}

void SkyOpacityNode::show()
{
    if (opacity() == 0)
    {
        setOpacity(1);
        markDirty(QSGNode::DirtyOpacity);
    }
}

void SkyOpacityNode::hide()
{
    if (opacity() != 0)
    {
        setOpacity(0);
        markDirty(QSGNode::DirtyOpacity);
    }
}

bool SkyOpacityNode::visible()
{
    if (opacity() != 0)
    {
        return true;
    }
    return false;
}
