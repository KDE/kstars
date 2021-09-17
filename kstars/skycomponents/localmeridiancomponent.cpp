/*
    SPDX-FileCopyrightText: 2012 Jerome SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
    return (Options::showLocalMeridian() && !(Options::hideOnSlew() &&
    #ifndef KSTARS_LITE
    SkyMap::IsSlewing()));
    #else
    SkyMapLite::IsSlewing()));
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
