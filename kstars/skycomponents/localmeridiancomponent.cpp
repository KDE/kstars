/***************************************************************************
                      LocalMeridianComponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : Tue 01 Mar 2012
    copyright            : (C) 2012 by Jerome SONRIER
    email                : jsid@emor3j.fr.eu.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "localmeridiancomponent.h"

#include "kstarsdata.h"
#include "Options.h"
#include "linelist.h"
#ifdef KSTARS_LITE
#include "skymaplite.h"
#else
#include "skymap.h"
#endif
#include "skypainter.h"

LocalMeridianComponent::LocalMeridianComponent(SkyComposite *parent)
    : CoordinateGrid(parent, i18n("Local Meridian Component"))
{
    //KStarsData *data = KStarsData::Instance();

    intro();

    double eps    = 0.1;
    double maxAlt = 90.0;

    std::shared_ptr<LineList> lineList;    

    for (double az = 0; az <= 180; az += 180)
    {
        lineList.reset(new LineList());

        for (double alt = 0; alt < maxAlt; alt += eps)
        {
                std::shared_ptr<SkyPoint> p(new SkyPoint());
                p->setAz(az);
                p->setAlt(alt);
                lineList->append(std::move(p));
        }

        appendLine(lineList);
    }



    summary();
}

bool LocalMeridianComponent::selected()
{
    if (Options::autoSelectGrid())
        return (Options::useAltAz() && Options::showLocalMeridian());
    else
#ifndef KSTARS_LITE
        return (Options::showHorizontalGrid() && Options::showLocalMeridian() &&
                !(Options::hideOnSlew() && Options::hideGrids() && SkyMap::IsSlewing()));
#else
        return (Options::showHorizontalGrid() &&
                !(Options::hideOnSlew() && Options::hideGrids() && SkyMapLite::IsSlewing()));
#endif
}

void LocalMeridianComponent::preDraw(SkyPainter *skyp)
{
    KStarsData *data = KStarsData::Instance();
    QColor color     = data->colorScheme()->colorNamed("LocalMeridianColor");

    skyp->setPen(QPen(QBrush(color), 2, Qt::SolidLine));
}

void LocalMeridianComponent::update(KSNumbers *)
{
    KStarsData *data = KStarsData::Instance();

    for (int i = 0; i < listList().count(); i++)
    {
        for (int j = 0; j < listList().at(i)->points()->count(); j++)
        {
            listList().at(i)->points()->at(j)->HorizontalToEquatorial(data->lst(), data->geo()->lat());
        }
    }
}
