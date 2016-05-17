/** *************************************************************************
                          asteroidsitem.cpp  -  K Desktop Planetarium
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

#include "asteroidsitem.h"

#include "Options.h"
#include "projections/projector.h"
#include "ksasteroid.h"

#include "nodes/planetnode.h"
#include "nodes/planetrootnode.h"
#include "nodes/pointsourcenode.h"

AsteroidsItem::AsteroidsItem(QQuickItem* parent)
    :SkyItem(parent)
{

}

void AsteroidsItem::addAsteroid(KSAsteroid * asteroid) {
    if(!m_asteroids.contains(asteroid) && !m_toAdd.contains(asteroid))
        m_toAdd.append(asteroid);
}

void AsteroidsItem::clear() {
    m_clear = true;
    m_asteroids.clear();
    m_toAdd.clear();
}

QSGNode* AsteroidsItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) {

    Q_UNUSED(updatePaintNodeData);

    RootNode *n = static_cast<RootNode*>(oldNode);
    QRectF rect = boundingRect();

    if (!Options::showAsteroids() || rect.isEmpty()) {
        delete n;
        return 0;
    }

    if(!n) {
        n = new RootNode; // If no RootNode exists create one
        int pAstLen = m_asteroids.length();
        if(pAstLen > 0) {
            /* If there are some asteroids that have been already displayed once then recreate them
                 in new instance of RootNode*/
            for(int i = 0; i < pAstLen; ++i) {
                if (m_asteroids[i]->image().isNull() == false) {
                    n->appendSkyNode(new PlanetNode(m_asteroids[i], n));
                }
                else {
                    n->appendSkyNode(new PointSourceNode(m_asteroids[i],n));
                }
            }
        }
    }

    if(m_clear) {
        n->removeAllSkyNodes();
        m_clear = false;
    }

    int addLength = m_toAdd.length();
    if(addLength > 0) { // If there are some new asteroids to add
        for(int i = 0; i < addLength; ++i) {
            m_asteroids.append(m_toAdd[i]);
            if (m_asteroids[i]->image().isNull() == false) {
                n->appendSkyNode(new PlanetNode(m_toAdd[i], n));
            }
            else {
                n->appendSkyNode(new PointSourceNode(m_toAdd[i],n));
            }
        }
        m_toAdd.clear();
    }
    //Update clipping geometry. If m_clipPoly in SkyMapLite wasn't changed, geometry is not updated
    n->updateClipPoly();
    //Traverse all children nodes of PlanetRootNode
    for(int i = 0; i < n->skyNodesCount(); ++i) {
        SkyNode* pNode = static_cast<SkyNode*>(n->skyNodeAtIndex(i));
        //bool hideLabels =  ! Options::showAsteroidNames() ||
        //( SkyMapLite::Instance()->isSlewing() && Options::hideLabels() );

        /*double lgmin = log10(MINZOOM);
        double lgmax = log10(MAXZOOM);
        double lgz = log10(Options::zoomFactor());
        double labelMagLimit  = 2.5 + Options::asteroidLabelDensity() / 5.0;
        labelMagLimit += ( 15.0 - labelMagLimit ) * ( lgz - lgmin) / (lgmax - lgmin );
        if ( labelMagLimit > 10.0 ) labelMagLimit = 10.0;
        //printf("labelMagLim = %.1f\n", labelMagLimit );*/

        KSAsteroid *ast = static_cast<KSAsteroid *>(pNode->getSkyObject());

        if ( ast->mag() > Options::magLimitAsteroid() || std::isnan(ast->mag()) != 0) {
            pNode->hide();
            continue;
        }
        //bool drawn = false;
        pNode->update();
        //if ( drawn && !( hideLabels || ast->mag() >= labelMagLimit ) )
        //  SkyLabeler::AddLabel( ast, SkyLabeler::ASTEROID_LABEL );
    }
    return n;
}
