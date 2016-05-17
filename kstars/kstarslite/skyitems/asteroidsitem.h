/** *************************************************************************
                          asteroidsitem.h  -  K Desktop Planetarium
                             -------------------
    begin                : 16/05/2016
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
#ifndef ASTEROIDSITEM_H_
#define ASTEROIDSITEM_H_

#include "skyitem.h"

class KSAsteroid;

class AsteroidsItem : public SkyItem {
public:

    AsteroidsItem(QQuickItem* parent = 0);

    /** Adds an object of type KSAsteroid to m_toAdd. In the next call to
     * updatePaintNode() the object of type PlanetNode will be created and asteroid
     * will be moved to m_asteroids. PlanetNode represents graphically KSAsteroid on SkyMapLite.
     * This function should be called whenever an object of class KSAsteroid is created.
     *
     * @param KSAsteroid that should be displayed on SkyMapLite
     */
    void addAsteroid(KSAsteroid * asteroid);
    /**
     * Sets m_clear to true. On next call to updatePaintNode all SkyNodes in RootNode and
     * all KSAsteroids in m_asteroids and m_toAdd will be deleted.
     */
    void clear();

protected:
    virtual QSGNode* updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) override;
private:
    bool m_clear;
    QList<KSAsteroid*> m_asteroids;
    QList<KSAsteroid*> m_toAdd;
};
#endif
