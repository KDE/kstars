/*
    SPDX-FileCopyrightText: 2006 Jason Harris <kstarss@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skyline.h"

#include "kstarsdata.h"
#include "ksnumbers.h"

#include <QDebug>

SkyLine::~SkyLine()
{
    clear();
}

void SkyLine::clear()
{
    qDeleteAll(m_pList);
    m_pList.clear();
}

void SkyLine::append(SkyPoint *p)
{
    m_pList.append(new SkyPoint(*p));
}

void SkyLine::setPoint(int i, SkyPoint *p)
{
    if (i < 0 || i >= m_pList.size())
    {
        qDebug() << "SkyLine index error: no such point: " << i;
        return;
    }
    *m_pList[i] = *p;
}

dms SkyLine::angularSize(int i) const
{
    if (i < 0 || i + 1 >= m_pList.size())
    {
        qDebug() << "SkyLine index error: no such segment: " << i;
        return dms();
    }

    SkyPoint *p1  = m_pList[i];
    SkyPoint *p2  = m_pList[i + 1];
    double dalpha = p1->ra().radians() - p2->ra().radians();
    double ddelta = p1->dec().radians() - p2->dec().radians();

    double sa = sin(dalpha / 2.);
    double sd = sin(ddelta / 2.);

    double hava = sa * sa;
    double havd = sd * sd;

    double aux = havd + cos(p1->dec().radians()) * cos(p2->dec().radians()) * hava;

    dms angDist;
    angDist.setRadians(2 * fabs(asin(sqrt(aux))));

    return angDist;
}

void SkyLine::update(KStarsData *d, KSNumbers *num)
{
    foreach (SkyPoint *p, m_pList)
    {
        if (num)
            p->updateCoords(num);
        p->EquatorialToHorizontal(d->lst(), d->geo()->lat());
    }
}
