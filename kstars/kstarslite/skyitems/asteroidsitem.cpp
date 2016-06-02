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

#include "skynodes/rootnodes/rootnode.h"
#include "skynodes/planetnode.h"
#include "skynodes/pointsourcenode.h"

AsteroidsItem::AsteroidsItem(QQuickItem* parent)
    :SkyItem(parent), m_asteroidsList(0), m_clear(false)
{

}

void AsteroidsItem::setAsteroidsList(QList<SkyObject*> *asteroidsList) {
    m_asteroidsList = asteroidsList;
    m_addAsteroids = true;
}

void AsteroidsItem::clear() {
    m_clear = true;
}

QSGNode* AsteroidsItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) {

    Q_UNUSED(updatePaintNodeData);

    RootNode *n = static_cast<RootNode*>(oldNode);
    QRectF rect = boundingRect();

    if (!Options::showAsteroids() || rect.isEmpty()) {
        delete n;
        return 0;
    }

    if(m_clear) {
        if(n) n->removeAllSkyNodes();
        m_clear = false;
    }

    if(!n) {
        if(!m_asteroidsList) return 0;// AsteroidsComponent is not ready
        n = new RootNode; // If no RootNode exists create one
    }

    if(m_addAsteroids) {
        int pAstLen = m_asteroidsList->length();
        /* If there are some asteroids that have been already displayed once then recreate them
             in new instance of RootNode*/
        for(int i = 0; i < pAstLen; ++i) {
            KSAsteroid *ast = static_cast<KSAsteroid *>(m_asteroidsList->value(i));
            if (ast->image().isNull() == false) {
                n->appendSkyNode(new PlanetNode(ast, n));
            }
            else {
                n->appendSkyNode(new PointSourceNode(ast,n));
            }
        }
        m_addAsteroids = false;
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
