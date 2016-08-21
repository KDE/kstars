/** *************************************************************************
                          fovitem.h  -  K Desktop Planetarium
                             -------------------
    begin                : 20/08/2016
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
#ifndef FOVITEM_H_
#define FOVITEM_H_

#include "skyitem.h"
    /**
     * @class FOVItem
     * This class handles FOV symbols in SkyMapLite
     *
     * @author Artem Fedoskin
     * @version 1.0
     */

class FOVItem : public SkyItem {
public:
    enum Shape { SQUARE,
                 CIRCLE,
                 CROSSHAIRS,
                 BULLSEYE,
                 SOLIDCIRCLE,
                 UNKNOWN };

    FOVItem(RootNode * rootNode);

    /**
     * @short Adds symbol to QVariantMap in SkyMap and creates new FOVSymbolNode
     */
    void addSymbol(const QString &name, float a, float b, float xoffset, float yoffset,
                            float rot, FOVItem::Shape shape, const QString &color);

    virtual void update() override;

private:
};
#endif
