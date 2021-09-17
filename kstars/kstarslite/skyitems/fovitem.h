/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skyitem.h"

/**
 * @class FOVItem
 * This class handles representation of FOV symbols in SkyMapLite
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class FOVItem : public SkyItem
{
  public:
    /** FOV symbol types */
    enum Shape
    {
        SQUARE,
        CIRCLE,
        CROSSHAIRS,
        BULLSEYE,
        SOLIDCIRCLE,
        UNKNOWN
    };

    /** Constructor. Initialize default FOV symbols */
    explicit FOVItem(RootNode *rootNode);

    /**
     * @short Add information about new symbol to SkyMapLite and create FOVSymbolNode
     * SkyMapLite acts here as a bridge between FOVItem and QML. Here we call SkyMapLite::addFOVSymbol to add
     * information about new FOVSymbol to SkyMapLite and later in update() we check if user switched this
     * FOVSymbol on
     */
    void addSymbol(const QString &name, float a, float b, float xoffset, float yoffset, float rot, FOVItem::Shape shape,
                   const QString &color);

    /** Update FOVSymbol if user switched it on */
    virtual void update() override;
};
