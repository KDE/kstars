/** *************************************************************************
                          horizonitem.h  -  K Desktop Planetarium
                             -------------------
    begin                : 28/05/2016
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
#ifndef HORIZONITEM_H_
#define HORIZONITEM_H_

#include "skyitem.h"

class HorizonComponent;

class HorizonItem : public SkyItem {
public:
    HorizonItem(QQuickItem* parent = 0);
    inline void setHorizonComp(HorizonComponent * hComp) { m_horizonComp = hComp; }

protected:
    virtual QSGNode* updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) override;
private:
    HorizonComponent *m_horizonComp;
};
#endif

