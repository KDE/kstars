#include "horizonnode.h"
#include "nodes/polynode.h"
#include "Options.h"
#include "ksutils.h"

HorizonNode::HorizonNode(QList<SkyPoint*>& pointList)
    :m_pointList(pointList), m_polygonNode(new PolyNode)
{
    appendChildNode(m_polygonNode);
}

void HorizonNode::update() {
    if(!m_polygonNode->visible()) {
        m_polygonNode->show();
    }

    QColor color = KStarsData::Instance()->colorScheme()->colorNamed( "HorzColor" );

    m_polygonNode->setColor(color);
    m_polygonNode->setLineWidth(2);

    QVector<Vector2f> ground = SkyMapLite::Instance()->projector()->groundPoly();
    if( ground.size() ) {
        QPolygonF groundPoly(ground.size());
        for(int i = 0; i < ground.size(); ++i)
            groundPoly[i] = KSUtils::vecToPoint(ground[i]);
        if( Options::showGround() ) {
            m_polygonNode->updateGeometry(groundPoly,true);
        } else {
            groundPoly.append( groundPoly.first() );
            m_polygonNode->updateGeometry(groundPoly,false);
        }
    } else {
        m_polygonNode->hide();
        return;
    }
    m_polygonNode->markDirty(QSGNode::DirtyGeometry);
}

void HorizonNode::hide() {
    m_polygonNode->hide();
}
