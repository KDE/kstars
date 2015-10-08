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
#include "skymapcomposite.h"
#include "skyobjects/skypoint.h" 
#include "dms.h"
#include "Options.h"
#include "linelist.h"
#include "polylist.h"

#include "skymesh.h"
#include "skypainter.h"
#include "projections/projector.h"

ArtificialHorizonEntity::ArtificialHorizonEntity()
{
    m_List = NULL;
}

ArtificialHorizonEntity::~ArtificialHorizonEntity()
{
  clearList();
}

QString ArtificialHorizonEntity::region() const
{
    return m_Region;
}

void ArtificialHorizonEntity::setRegion(const QString &Region)
{
    m_Region = Region;
}

bool ArtificialHorizonEntity::enabled() const
{
    return m_Enabled;
}

void ArtificialHorizonEntity::setEnabled(bool Enabled)
{
    m_Enabled = Enabled;
}

void ArtificialHorizonEntity::setList(LineList *List)
{
    m_List = List;
}

LineList *ArtificialHorizonEntity::list()
{
    return m_List;
}

void ArtificialHorizonEntity::clearList()
{
    if (m_List)
    {
        while (m_List->points()->size() > 0)
            delete m_List->points()->takeAt(0);
        delete (m_List);

       m_List = NULL;
    }
}

ArtificialHorizonComponent::ArtificialHorizonComponent(SkyComposite *parent ) :
        NoPrecessIndex( parent, i18n("Artificial Horizon") )
{
    livePreview=NULL;
    load();
}

bool ArtificialHorizonComponent::load()
{
    m_HorizonList = KStarsData::Instance()->userdb()->GetAllHorizons();

    foreach(ArtificialHorizonEntity *horizon, m_HorizonList)
        appendLine(horizon->list());

    return true;
}

void ArtificialHorizonComponent::save()
{
    KStarsData::Instance()->userdb()->DeleteAllHorizons();

    foreach(ArtificialHorizonEntity *horizon, m_HorizonList)
        KStarsData::Instance()->userdb()->AddHorizon(horizon);
}

bool ArtificialHorizonComponent::selected()
{
    return Options::showGround();
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

    if (livePreview)
    {
        skyp->setPen(QPen(Qt::white, 3));
        skyp->drawSkyPolyline(livePreview);
        return;
    }

    preDraw(skyp);

    DrawID   drawID   = skyMesh()->drawID();
    //UpdateID updateID = KStarsData::Instance()->updateID();

    //foreach ( LineList* lineList, listList() )
    for (int i=0; i < listList().count(); i++)
    {
        LineList* lineList = listList().at(i);

        if ( lineList->drawID == drawID || m_HorizonList.at(i)->enabled() == false)
            continue;
        lineList->drawID = drawID;

        /*if ( lineList->updateID != updateID )
            JITupdate( lineList );*/

        skyp->drawSkyPolygon(lineList, false);
    }

}

void ArtificialHorizonComponent::removeRegion(const QString &regionName, bool lineOnly)
{
    ArtificialHorizonEntity *regionHorizon = NULL;

    foreach(ArtificialHorizonEntity *horizon, m_HorizonList)
    {
        if (horizon->region() == regionName)
        {
            regionHorizon = horizon;
            break;
        }
    }

    if (regionHorizon == NULL)
        return;

    if (regionHorizon->list())
        removeLine(regionHorizon->list());

    if (lineOnly)
        regionHorizon->clearList();
    else
    {
        m_HorizonList.removeOne(regionHorizon);
        delete (regionHorizon);
    }
}

void ArtificialHorizonComponent::addRegion(const QString &regionName, bool enabled, LineList *list)
{
    ArtificialHorizonEntity *horizon = new ArtificialHorizonEntity;

    horizon->setRegion(regionName);
    horizon->setEnabled(enabled);
    horizon->setList(list);

    m_HorizonList.append(horizon);

    appendLine(list);
}

