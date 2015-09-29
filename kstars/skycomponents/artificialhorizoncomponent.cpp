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
    load();
}

bool ArtificialHorizonComponent::load()
{
    m_RegionMap = KStarsData::Instance()->userdb()->GetAllHorizons();

    return true;
}

void ArtificialHorizonComponent::save()
{
    KStarsData::Instance()->userdb()->DeleteAllHorizons();

    foreach(QString regionName, m_RegionMap.keys())
        KStarsData::Instance()->userdb()->AddHorizon(regionName, m_RegionMap.value(regionName));
}

bool ArtificialHorizonComponent::selected()
{
    return Options::showHorizon();
}

void ArtificialHorizonComponent::preDraw( SkyPainter *skyp )
{    
    QColor color( KStarsData::Instance()->colorScheme()->colorNamed( "ArtificialHorizonColor" ) );
    color.setAlpha(40);
    skyp->setBrush(QBrush(color));
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

void ArtificialHorizonComponent::removeRegion(const QString &regionName)
{
    if (m_RegionMap.contains(regionName))
    {
        LineList *list = m_RegionMap.value(regionName);
        removeLine(list);
        while (list->points()->size() > 0)
            delete list->points()->takeAt(0);
        delete (list);

        m_RegionMap.remove(regionName);
    }
}

void ArtificialHorizonComponent::addRegion(const QString &regionName, LineList *list)
{
    m_RegionMap[regionName] = list;
    appendLine(list);
}
