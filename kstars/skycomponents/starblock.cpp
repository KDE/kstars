/***************************************************************************
                   starblock.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 9 Jun 2008
    copyright            : (C) 2008 by Akarsh Simha
    email                : akarshsimha@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QDebug>

#include "starblock.h"
#include "skyobjects/starobject.h"
#include "starcomponent.h"
#include "skyobjects/stardata.h"
#include "skyobjects/deepstardata.h"

#ifdef KSTARS_LITE
#include "skymaplite.h"
#include "kstarslite/skyitems/skynodes/pointsourcenode.h"

StarNode::StarNode()
    :starNode(0)
{

}

StarNode::~StarNode() {
    if(starNode) {
        SkyMapLite::Instance()->deleteSkyNode(starNode);
        qDebug() << "REAL NODE DESTRUCTOR";
    }
}
#endif

StarBlock::StarBlock( int nstars ) :
    faintMag(-5),
    brightMag(35),
    parent(0),
    prev(0),
    next(0),
    drawID(0),
    nStars(0),
#ifdef KSTARS_LITE
    stars(nstars,StarNode())
#else
    stars(nstars, StarObject())
#endif
{ }


void StarBlock::reset()
{
    if( parent )
        parent->releaseBlock( this );
    parent = NULL;
    faintMag = -5.0;
    brightMag = 35.0;
    nStars = 0;
}

StarBlock::~StarBlock()
{
    if( parent )
        parent -> releaseBlock( this );
}
#ifdef KSTARS_LITE
StarNode* StarBlock::addStar(const starData& data)
{
    if(isFull())
        return 0;
    StarNode& node = stars[nStars++];
    StarObject& star = node.star;

    star.init(&data);
    if( star.mag() > faintMag )
        faintMag = star.mag();
    if( star.mag() < brightMag )
        brightMag = star.mag();
    return &node;
}

StarNode* StarBlock::addStar(const deepStarData& data)
{
    if(isFull())
        return 0;
    StarNode& node = stars[nStars++];
    StarObject& star = node.star;

    star.init(&data);
    if( star.mag() > faintMag )
        faintMag = star.mag();
    if( star.mag() < brightMag )
        brightMag = star.mag();
    return &node;
}
#else
StarObject* StarBlock::addStar(const starData& data)
{
    if(isFull())
        return 0;
    StarObject& star = stars[nStars++];
    
    star.init(&data);
    if( star.mag() > faintMag )
        faintMag = star.mag();
    if( star.mag() < brightMag )
        brightMag = star.mag();
    return &star;
}

StarObject* StarBlock::addStar(const deepStarData& data)
{
    if(isFull())
        return 0;
    StarObject& star = stars[nStars++];
    
    star.init(&data);
    if( star.mag() > faintMag )
        faintMag = star.mag();
    if( star.mag() < brightMag )
        brightMag = star.mag();
    return &star;
}
#endif
