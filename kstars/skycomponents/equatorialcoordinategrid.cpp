/*
    SPDX-FileCopyrightText: 2012 Jerome SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "equatorialcoordinategrid.h"

#include "kstarsdata.h"
#include "linelist.h"
#include "Options.h"
#ifdef KSTARS_LITE
#include "skymaplite.h"
#else
#include "skymap.h"
#endif
#include "skypainter.h"

EquatorialCoordinateGrid::EquatorialCoordinateGrid(SkyComposite *parent)
    : CoordinateGrid(parent, i18n("Equatorial Coordinate Grid"))
{
    KStarsData *data = KStarsData::Instance();

    intro();

    double eps    = 0.1;
    double minRa  = 0.0;
    double maxRa  = 23.0;
    double dRa    = 2.0;
    double minDec = -80.0;
    double maxDec = 90.0;
    double dDec   = 20.0;
    double dDec2  = 4.0;
    double dRa2   = 0.2;

    double max, dec, dec2, ra, ra2;

    std::shared_ptr<LineList> lineList;

    for (ra = minRa; ra < maxRa; ra += dRa)
    {
        for (dec = -90.0; dec < maxDec - eps; dec += dDec)
        {
            lineList.reset(new LineList());
            max      = dec + dDec;
            if (max > 90.0)
                max = 90.0;
            for (dec2 = dec; dec2 <= max + eps; dec2 += dDec2)
            {
                std::shared_ptr<SkyPoint> p(new SkyPoint(ra, dec2));

                p->EquatorialToHorizontal(data->lst(), data->geo()->lat());
                lineList->append(p);
            }
            appendLine(lineList);
        }
    }

    for (dec = minDec; dec < maxDec + eps; dec += dDec)
    {
        // Do not paint the line on the equator
        if (dec < 0.1 && dec > -0.1)
            continue;

        // Adjust point density
        int nPoints = int(round(fabs(cos(dec * dms::PI / 180.0)) * dRa / dRa2));
        if (nPoints < 5)
            nPoints = 5;
        double dRa3 = dRa / nPoints;

        for (ra = minRa; ra < maxRa + eps; ra += dRa)
        {
            lineList.reset(new LineList());
            for (ra2 = ra; ra2 <= ra + dRa + eps; ra2 += dRa3)
            {
                std::shared_ptr<SkyPoint> p(new SkyPoint(ra2, dec));

                p->EquatorialToHorizontal(data->lst(), data->geo()->lat());
                lineList->append(p);
            }
            appendLine(lineList);
        }
    }
    summary();
}

bool EquatorialCoordinateGrid::selected()
{
    if (Options::autoSelectGrid())
        return (!Options::useAltAz());
    else
#ifndef KSTARS_LITE
        return (Options::showEquatorialGrid() &&
                !(Options::hideOnSlew() && Options::hideGrids() && SkyMap::IsSlewing()));
#else
        return (Options::showEquatorialGrid() &&
                !(Options::hideOnSlew() && Options::hideGrids() && SkyMapLite::IsSlewing()));
#endif
}

void EquatorialCoordinateGrid::preDraw(SkyPainter *skyp)
{
    KStarsData *data = KStarsData::Instance();
    QColor color     = data->colorScheme()->colorNamed("EquatorialGridColor");
    skyp->setPen(QPen(QBrush(color), 1, Qt::DotLine));
}
