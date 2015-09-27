/*  Artificial Horizon
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "artificialhorizoncomponent.h"

#include "ksnumbers.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyobjects/skypoint.h" 
#include "dms.h"
#include "Options.h"
#include "linelist.h"
#include "polylist.h"

#include "skymesh.h"
#include "skypainter.h"
#include "projections/projector.h"

ArtificialHorizonComponent::ArtificialHorizonComponent(SkyComposite *parent ) :
        NoPrecessIndex( parent, i18n("Artificial Horizon") )
{

    /*QMap<QString, LineList*> horizonList =KStarsData::Instance()->userdb()->GetAllHorizons();

    foreach(QString key, horizonList.keys())
    {
        m_RegionNames.append(key);
        LineList *list = horizonList.value(key);
        m_PolygonList.append(list);
        appendLine(list);
    }*/

    m_RegionNames.append("Example1");
    m_RegionNames.append("Example2");

    LineList *example1 = new LineList();
    SkyPoint *p = new SkyPoint();
    p->setAz(30);
    p->setAlt(30);
    example1->append(p);

    p = new SkyPoint();
    p->setAz(60);
    p->setAlt(60);
    example1->append(p);

    m_PolygonList.append(example1);

    LineList *example2 = new LineList();
    p = new SkyPoint();
    p->setAz(210);
    p->setAlt(50);
    example2->append(p);

    p = new SkyPoint();
    p->setAz(80);
    p->setAlt(34);
    example2->append(p);

    m_PolygonList.append(example2);


}


bool ArtificialHorizonComponent::selected()
{
    return Options::showHorizon();
}

void ArtificialHorizonComponent::preDraw( SkyPainter *skyp )
{    
    //QColor color( data->colorScheme()->colorNamed( "EqColor" ) );
    skyp->setBrush(QBrush(QColor(200, 40, 40, 40)));
    skyp->setPen(Qt::NoPen);

}

void ArtificialHorizonComponent::draw( SkyPainter *skyp )
{
    if ( ! selected() ) return;

    preDraw(skyp);

    DrawID   drawID   = skyMesh()->drawID();
    UpdateID updateID = KStarsData::Instance()->updateID();

    foreach ( LineList* lineList, listList() )
    {
        if ( lineList->drawID == drawID )
            continue;
        lineList->drawID = drawID;

        if ( lineList->updateID != updateID )
            JITupdate( lineList );

        skyp->drawSkyPolygon(lineList, false);
    }

}

LineList * ArtificialHorizonComponent::getListByRegion(const QString &regionName) const
{
    int index = m_RegionNames.indexOf(regionName);

    if (index == -1)
        return NULL;

    return listList().at(index);
}


