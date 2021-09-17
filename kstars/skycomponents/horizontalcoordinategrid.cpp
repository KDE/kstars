/*
    SPDX-FileCopyrightText: 2012 Jerome SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "horizontalcoordinategrid.h"

#include "kstarsdata.h"
#include "Options.h"
#include "linelist.h"
#ifdef KSTARS_LITE
#include "skymaplite.h"
#else
#include "skymap.h"
#endif
#include "skypainter.h"

HorizontalCoordinateGrid::HorizontalCoordinateGrid(SkyComposite *parent)
    : CoordinateGrid(parent, i18n("Horizontal Coordinate Grid"))
{
    //KStarsData *data = KStarsData::Instance();

    intro();

    double eps    = 0.1;
    double minAz  = 0.0;
    double maxAz  = 359.0;
    double dAz    = 30.0;
    double minAlt = -80.0;
    double maxAlt = 90.0;
    double dAlt   = 20.0;
    double dAlt2  = 4.0;
    double dAz2   = 0.2;

    double max, alt, alt2, az, az2;

    std::shared_ptr<LineList> lineList;

    for (az = minAz; az < maxAz; az += dAz)
    {
        for (alt = -90.0; alt < maxAlt - eps; alt += dAlt)
        {
            lineList.reset(new LineList());
            max      = alt + dAlt;
            if (max > 90.0)
                max = 90.0;
            for (alt2 = alt; alt2 <= max + eps; alt2 += dAlt2)
            {
                std::shared_ptr<SkyPoint> p(new SkyPoint());

                p->setAz(az);
                p->setAlt(alt2);
                //p->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
                lineList->append(std::move(p));
            }
            appendLine(lineList);
        }
    }

    for (alt = minAlt; alt < maxAlt + eps; alt += dAlt)
    {
        // Do not paint the line on the horizon
        if (alt < 0.1 && alt > -0.1)
            continue;

        // Adjust point density
        int nPoints = int(round(fabs(cos(alt * dms::PI / 180.0)) * dAz / dAz2));
        if (nPoints < 5)
            nPoints = 5;
        double dAz3 = dAz / nPoints;

        for (az = minAz; az < maxAz + eps; az += dAz)
        {
            lineList.reset(new LineList());
            for (az2 = az; az2 <= az + dAz + eps; az2 += dAz3)
            {
                std::shared_ptr<SkyPoint> p(new SkyPoint());

                p->setAz(az2);
                p->setAlt(alt);
                //p->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
                lineList->append(std::move(p));
            }
            appendLine(lineList);
        }
    }
    summary();
}

bool HorizontalCoordinateGrid::selected()
{
    if (Options::autoSelectGrid())
        return (Options::useAltAz());
    else
#ifndef KSTARS_LITE
        return (Options::showHorizontalGrid() &&
                !(Options::hideOnSlew() && Options::hideGrids() && SkyMap::IsSlewing()));
#else
        return (Options::showHorizontalGrid() &&
                !(Options::hideOnSlew() && Options::hideGrids() && SkyMapLite::IsSlewing()));
#endif
}

void HorizontalCoordinateGrid::preDraw(SkyPainter *skyp)
{
    KStarsData *data = KStarsData::Instance();
    QColor color     = data->colorScheme()->colorNamed("HorizontalGridColor");

    skyp->setPen(QPen(QBrush(color), 2, Qt::DotLine));
}

void HorizontalCoordinateGrid::update(KSNumbers *)
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
