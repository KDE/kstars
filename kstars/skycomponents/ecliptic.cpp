/***************************************************************************
                             ecliptic.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-08-09
    copyright            : (C) 2007 by James B. Bowlin
    email                : bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "ecliptic.h"

#include "ksnumbers.h"
#include "kstarsdata.h"

#ifdef KSTARS_LITE
#include "skymaplite.h"
#else
#include "skymap.h"
#endif

#include "skyobjects/skypoint.h"
#include "dms.h"
#include "Options.h"
#include "linelist.h"
#include "skylabeler.h"

#include "skypainter.h"
#include "projections/projector.h"

Ecliptic::Ecliptic(SkyComposite *parent) : LineListIndex(parent, i18n("Ecliptic")), m_label(name())
{
    KStarsData *data = KStarsData::Instance();
    KSNumbers num(data->ut().djd());
    dms elat(0.0), elng(0.0);

    const double eps   = 0.1;
    const double minRa = 0.0;
    const double maxRa = 23.0;
    const double dRa   = 2.0;
    const double dRa2  = 2. / 5.;

    for (double ra = minRa; ra < maxRa; ra += dRa)
    {
        LineList *lineList = new LineList();
        for (double ra2 = ra; ra2 <= ra + dRa + eps; ra2 += dRa2)
        {
            elng.setH(ra2);
            SkyPoint *o = new SkyPoint();
            o->setFromEcliptic(num.obliquity(), elng, elat);
            o->setRA0(o->ra().Hours());
            o->setDec0(o->dec().Degrees());
            o->EquatorialToHorizontal(data->lst(), data->geo()->lat());
            lineList->append(o);
        }
        appendLine(lineList);
    }
}

bool Ecliptic::selected()
{
    return Options::showEcliptic();
}

void Ecliptic::draw(SkyPainter *skyp)
{
    if (!selected())
        return;

    KStarsData *data = KStarsData::Instance();
    QColor color(data->colorScheme()->colorNamed("EclColor"));
    skyp->setPen(QPen(QBrush(color), 1, Qt::SolidLine));

    m_label.reset();
    drawLines(skyp);
    SkyLabeler::Instance()->setPen(QPen(QBrush(color), 1, Qt::SolidLine));
    m_label.draw();

    drawCompassLabels();
}

void Ecliptic::drawCompassLabels()
{
#ifndef KSTARS_LITE
    const Projector *proj  = SkyMap::Instance()->projector();
    KStarsData *data       = KStarsData::Instance();
    SkyLabeler *skyLabeler = SkyLabeler::Instance();
    // Set proper color for labels
    QColor color(data->colorScheme()->colorNamed("CompassColor"));
    skyLabeler->setPen(QPen(QBrush(color), 1, Qt::SolidLine));

    KSNumbers num(data->ut().djd());
    dms elat(0.0), elng(0.0);
    QString label;
    for (int ra = 0; ra < 23; ra += 6)
    {
        elng.setH(ra);
        SkyPoint o;
        o.setFromEcliptic(num.obliquity(), elng, elat);
        o.setRA0(o.ra());
        o.setDec0(o.dec());
        o.EquatorialToHorizontal(data->lst(), data->geo()->lat());
        bool visible;
        QPointF cpoint = proj->toScreen(&o, false, &visible);
        if (visible && proj->checkVisibility(&o))
        {
            label.setNum(o.ra().reduce().Degrees());
            skyLabeler->drawGuideLabel(cpoint, label, 0.0);
        }
    }
#endif
}
