/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <bowlin@mindspring.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "equator.h"

#include "kstarsdata.h"
#include "linelist.h"
#include "Options.h"
#include "skylabeler.h"
#ifdef KSTARS_LITE
#include "skymaplite.h"
#else
#include "skymap.h"
#endif
#include "skypainter.h"
#include "projections/projector.h"

Equator::Equator(SkyComposite *parent) : NoPrecessIndex(parent, i18n("Equator")), m_label(LineListIndex::name())
{
    KStarsData *data = KStarsData::Instance();
    KSNumbers num(data->ut().djd());

    const double eps   = 0.1;
    const double minRa = 0.0;
    const double maxRa = 23.0;
    const double dRa   = 2.0;
    const double dRa2  = .5 / 5.;

    for (double ra = minRa; ra < maxRa; ra += dRa)
    {
        std::shared_ptr<LineList> lineList(new LineList());

        for (double ra2 = ra; ra2 <= ra + dRa + eps; ra2 += dRa2)
        {
            std::shared_ptr<SkyPoint> o(new SkyPoint(ra2, 0.0));

            o->EquatorialToHorizontal(data->lst(), data->geo()->lat());
            lineList->append(std::move(o));
        }
        appendLine(lineList);
    }
}

bool Equator::selected()
{
    return Options::showEquator();
}

void Equator::preDraw(SkyPainter *skyp)
{
    KStarsData *data = KStarsData::Instance();
    QColor color(data->colorScheme()->colorNamed("EqColor"));
    skyp->setPen(QPen(QBrush(color), 1, Qt::SolidLine));
}

void Equator::draw(SkyPainter *skyp)
{
    if (!selected())
        return;

    m_label.reset();
    NoPrecessIndex::draw(skyp);

    KStarsData *data = KStarsData::Instance();
    QColor color(data->colorScheme()->colorNamed("EqColor"));
    SkyLabeler::Instance()->setPen(QPen(QBrush(color), 1, Qt::SolidLine));
    m_label.draw();

    drawCompassLabels();
}

void Equator::drawCompassLabels()
{
#ifndef KSTARS_LITE
    QString label;

    const Projector *proj  = SkyMap::Instance()->projector();
    KStarsData *data       = KStarsData::Instance();
    SkyLabeler *skyLabeler = SkyLabeler::Instance();
    // Set proper color for labels
    QColor color(data->colorScheme()->colorNamed("CompassColor"));
    skyLabeler->setPen(QPen(QBrush(color), 1, Qt::SolidLine));

    KSNumbers num(data->ut().djd());
    for (int ra = 0; ra < 23; ra += 2)
    {
        SkyPoint o(ra, 0.0);
        o.EquatorialToHorizontal(data->lst(), data->geo()->lat());
        bool visible;
        QPointF cpoint = proj->toScreen(&o, false, &visible);
        if (visible && proj->checkVisibility(&o))
        {
            label.setNum(o.ra().hour());
            skyLabeler->drawGuideLabel(cpoint, label, 0.0);
        }
    }
#endif
}
