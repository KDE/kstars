/*
    SPDX-FileCopyrightText: 2005 Thomas Kabelmann <thomas.kabelmann@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "horizoncomponent.h"

#include "dms.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skylabeler.h"
#ifdef KSTARS_LITE
#include "skymaplite.h"
#else
#include "skymap.h"
#endif
#include "skypainter.h"
#include "projections/projector.h"

#include <QPointF>

#define NCIRCLE 360 //number of points used to define equator, ecliptic and horizon

HorizonComponent::HorizonComponent(SkyComposite *parent) : PointListComponent(parent)
{
    KStarsData *data = KStarsData::Instance();
    emitProgressText(i18n("Creating horizon"));

    //Define Horizon
    for (unsigned int i = 0; i < NCIRCLE; ++i)
    {
        std::shared_ptr<SkyPoint> o(new SkyPoint());

        o->setAz(i * 360. / NCIRCLE);
        o->setAlt(0.0);

        o->HorizontalToEquatorial(data->lst(), data->geo()->lat());
        pointList().append(o);
    }
}

bool HorizonComponent::selected()
{
    return Options::showHorizon() || Options::showGround();
}

void HorizonComponent::update(KSNumbers *)
{
    if (!selected())
        return;

    KStarsData *data = KStarsData::Instance();

    for (auto &p : pointList())
    {
        p->HorizontalToEquatorial(data->lst(), data->geo()->lat());
    }
}

//Only half of the Horizon circle is ever valid, the invalid half is "behind" the observer.
//To select the valid half, we start with the azimuth of the central focus point.
//The valid horizon points have azimuth between this az +- 90
//This is true for Equatorial or Horizontal coordinates
void HorizonComponent::draw(SkyPainter *skyp)
{
    if (!selected())
        return;

    KStarsData *data = KStarsData::Instance();

    skyp->setPen(QPen(QColor(data->colorScheme()->colorNamed("HorzColor")), 2, Qt::SolidLine));

    if (Options::showGround())
        skyp->setBrush(QColor(data->colorScheme()->colorNamed("HorzColor")));
    else
        skyp->setBrush(Qt::NoBrush);

    SkyPoint labelPoint;
    bool drawLabel;
    skyp->drawHorizon(Options::showGround(), &labelPoint, &drawLabel);

    if (drawLabel)
    {
        SkyPoint labelPoint2;
        labelPoint2.setAlt(0.0);
        labelPoint2.setAz(labelPoint.az().Degrees() + 1.0);
        labelPoint2.HorizontalToEquatorial(data->lst(), data->geo()->lat());
    }

    drawCompassLabels();
}

void HorizonComponent::drawCompassLabels()
{
#ifndef KSTARS_LITE
    SkyPoint c;
    QPointF cpoint;
    bool visible;

    const Projector *proj = SkyMap::Instance()->projector();
    KStarsData *data      = KStarsData::Instance();

    SkyLabeler *skyLabeler = SkyLabeler::Instance();
    // Set proper color for labels
    QColor color(data->colorScheme()->colorNamed("CompassColor"));
    skyLabeler->setPen(QPen(QBrush(color), 1, Qt::SolidLine));

    double az = -0.01;
    static QString name[8];
    name[0] = i18nc("Northeast", "NE");
    name[1] = i18nc("East", "E");
    name[2] = i18nc("Southeast", "SE");
    name[3] = i18nc("South", "S");
    name[4] = i18nc("Southwest", "SW");
    name[5] = i18nc("West", "W");
    name[6] = i18nc("Northwest", "NW");
    name[7] = i18nc("North", "N");

    for (const auto &item : name)
    {
        az += 45.0;
        c.setAz(az);
        c.setAlt(0.0);
        if (!Options::useAltAz())
        {
            c.HorizontalToEquatorial(data->lst(), data->geo()->lat());
        }

        cpoint = proj->toScreen(&c, false, &visible);
        if (visible && proj->onScreen(cpoint))
        {
            skyLabeler->drawGuideLabel(cpoint, item, 0.0);
        }
    }
#endif
}
