/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDebug>

#include "starblock.h"
#include "skyobjects/starobject.h"
#include "starcomponent.h"
#include "skyobjects/stardata.h"
#include "skyobjects/deepstardata.h"

#ifdef KSTARS_LITE
#include "skymaplite.h"
#include "kstarslite/skyitems/skynodes/pointsourcenode.h"

StarNode::StarNode() : starNode(0)
{
}

StarNode::~StarNode()
{
    if (starNode)
    {
        SkyMapLite::Instance()->deleteSkyNode(starNode);
        qDebug() << "REAL NODE DESTRUCTOR";
    }
}
#endif

StarBlock::StarBlock(int nstars)
    : faintMag(-5), brightMag(35), parent(nullptr), prev(nullptr), next(nullptr), drawID(0), nStars(0),
#ifdef KSTARS_LITE
      stars(nstars, StarNode())
#else
      stars(nstars, StarObject())
#endif
{
}

void StarBlock::reset()
{
    if (parent)
        parent->releaseBlock(this);

    parent    = nullptr;
    faintMag  = -5.0;
    brightMag = 35.0;
    nStars    = 0;
}

#ifdef KSTARS_LITE
StarNode *StarBlock::addStar(const StarData &data)
{
    if (isFull())
        return 0;
    StarNode &node   = stars[nStars++];
    StarObject &star = node.star;

    star.init(&data);
    if (star.mag() > faintMag)
        faintMag = star.mag();
    if (star.mag() < brightMag)
        brightMag = star.mag();
    return &node;
}

StarNode *StarBlock::addStar(const DeepStarData &data)
{
    if (isFull())
        return 0;
    StarNode &node   = stars[nStars++];
    StarObject &star = node.star;

    star.init(&data);
    if (star.mag() > faintMag)
        faintMag = star.mag();
    if (star.mag() < brightMag)
        brightMag = star.mag();
    return &node;
}
#else
StarObject *StarBlock::addStar(const StarData &data)
{
    if (isFull())
        return nullptr;
    StarObject &star = stars[nStars++];

    star.init(&data);
    if (star.mag() > faintMag)
        faintMag = star.mag();
    if (star.mag() < brightMag)
        brightMag = star.mag();
    return &star;
}

StarObject *StarBlock::addStar(const DeepStarData &data)
{
    if (isFull())
        return nullptr;
    StarObject &star = stars[nStars++];

    star.init(&data);
    if (star.mag() > faintMag)
        faintMag = star.mag();
    if (star.mag() < brightMag)
        brightMag = star.mag();
    return &star;
}
#endif
