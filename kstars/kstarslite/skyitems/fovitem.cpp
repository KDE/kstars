/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fovitem.h"
#include "labelsitem.h"
#include "skynodes/fovsymbolnode.h"
#include "Options.h"

FOVItem::FOVItem(RootNode *rootNode) : SkyItem(LabelsItem::label_t::NO_LABEL, rootNode)
{
    addSymbol(i18nc("use field-of-view for binoculars", "7x35 Binoculars"), 558, 558, 0, 0, 0, CIRCLE, "#AAAAAA");
    addSymbol(i18nc("use a Telrad field-of-view indicator", "Telrad"), 30, 30, 0, 0, 0, BULLSEYE, "#AA0000");
    addSymbol(i18nc("use 1-degree field-of-view indicator", "One Degree"), 60, 60, 0, 0, 0, CIRCLE, "#AAAAAA");
    addSymbol(i18nc("use HST field-of-view indicator", "HST WFPC2"), 2.4, 2.4, 0, 0, 0, SQUARE, "#AAAAAA");
    addSymbol(i18nc("use Radiotelescope HPBW", "30m at 1.3cm"), 1.79, 1.79, 0, 0, 0, SQUARE, "#AAAAAA");
}

void FOVItem::addSymbol(const QString &name, float a, float b, float xoffset, float yoffset, float rot,
                        FOVItem::Shape shape, const QString &color)
{
    SkyMapLite::Instance()->addFOVSymbol(name, false);
    appendChildNode(new FOVSymbolNode(name, a, b, xoffset, yoffset, rot, shape, color));
}

void FOVItem::update()
{
    float zoomFactor = Options::zoomFactor();
    SkyMapLite *map  = SkyMapLite::Instance();

    QSGNode *n = firstChild();
    int index  = 0;
    while (n != 0)
    {
        FOVSymbolNode *fov = static_cast<FOVSymbolNode *>(n);
        if (map->isFOVVisible(index))
        {
            fov->update(zoomFactor);
        }
        else
        {
            fov->hide();
        }
        n = n->nextSibling();
        index++;
    }
}
