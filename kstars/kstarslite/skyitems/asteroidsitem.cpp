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

#include "skynodes/planetnode.h"
#include "skynodes/pointsourcenode.h"
#include "kstarslite/skyitems/rootnode.h"

#include "labelsitem.h"

AsteroidsItem::AsteroidsItem(const QList<SkyObject*>& asteroidsList, RootNode *rootNode)
    :SkyItem(LabelsItem::label_t::ASTEROID_LABEL, rootNode), m_asteroidsList(asteroidsList)
{
    recreateList();
}

void AsteroidsItem::recreateList() {
    //Delete all child nodes
    while(QSGNode *n = firstChild()) { removeChildNode(n); delete n; }

    foreach(SkyObject *asteroid, m_asteroidsList) {
        KSAsteroid *ast = static_cast<KSAsteroid *>(asteroid);
        if (ast->image().isNull() == false) {
            appendChildNode(new PlanetNode(ast, rootNode()));
        } else {
            appendChildNode(new PointSourceNode(ast, rootNode(),labelType()));
        }
    }
}

void AsteroidsItem::update() {
    QSGNode *n = firstChild();
    while(n != 0) {
        SkyNode* pNode = static_cast<SkyNode*>(n);
        n = n->nextSibling();

        bool hideLabels =  ! Options::showAsteroidNames() ||
        ( SkyMapLite::Instance()->isSlewing() && Options::hideLabels() );

        double lgmin = log10(MINZOOM);
        double lgmax = log10(MAXZOOM);
        double lgz = log10(Options::zoomFactor());
        double labelMagLimit  = 2.5 + Options::asteroidLabelDensity() / 5.0;
        labelMagLimit += ( 15.0 - labelMagLimit ) * ( lgz - lgmin) / (lgmax - lgmin );
        if ( labelMagLimit > 10.0 ) labelMagLimit = 10.0;
        //printf("labelMagLim = %.1f\n", labelMagLimit );

        KSAsteroid *ast = static_cast<KSAsteroid *>(pNode->skyObject());

        bool drawLabel = false;

        if ( ast->mag() > Options::magLimitAsteroid() || std::isnan(ast->mag()) != 0) {
            pNode->hide();
            continue;
        }
        if (!( hideLabels || ast->mag() >= labelMagLimit ) ) {
            drawLabel = true;
        }
        pNode->update(drawLabel);
    }
}
