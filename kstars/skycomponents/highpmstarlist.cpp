/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <bowlin@mindspring.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "highpmstarlist.h"

#include "skymesh.h"
#include "skyobjects/starobject.h"

#include <QDebug>

typedef struct HighPMStar
{
    HighPMStar(Trixel t, StarObject *s) : trixel(t), star(s) {}
    Trixel trixel;
    StarObject *star { nullptr };

} HighPMStar;

HighPMStarList::HighPMStarList(double threshold) : m_reindexNum(J2000), m_threshold(threshold)
{
    m_skyMesh = SkyMesh::Instance();
}

HighPMStarList::~HighPMStarList()
{
    qDeleteAll(m_stars);
    m_stars.clear();
}

bool HighPMStarList::append(Trixel trixel, StarObject *star, double pm)
{
    if (pm < m_threshold)
        return false;

    if (trixel >= m_skyMesh->size())
        qDebug() << "### Trixel ID out of range for the Mesh currently in use!" << trixel;

    m_stars.append(new HighPMStar(trixel, star));
    if (m_maxPM >= pm)
        return true;

    m_maxPM           = pm;
    m_reindexInterval = StarObject::reindexInterval(pm);

    return true;
}

void HighPMStarList::setIndexTime(KSNumbers *num)
{
    m_reindexNum = KSNumbers(*num);
}

bool HighPMStarList::reindex(KSNumbers *num, StarIndex *starIndex)
{
    if (fabs(num->julianCenturies() - m_reindexNum.julianCenturies()) < m_reindexInterval)
        return false;

    m_reindexNum = KSNumbers(*num);
    m_skyMesh->setKSNumbers(num);

    int cnt(0);

    for (auto &HPStar : m_stars)
    {
        Trixel trixel = m_skyMesh->indexStar(HPStar->star);

        if (trixel == HPStar->trixel)
            continue;
        cnt++;
        StarObject *star = HPStar->star;

        // out with the old ...
        if (HPStar->trixel >= m_skyMesh->size())
        {
            qDebug() << "### Expect an Index out-of-range error. star->trixel =" << HPStar->trixel;
        }

        StarList *old = starIndex->at(HPStar->trixel);
        old->removeAt(old->indexOf(star));

        float mag = star->mag();
        //printf("\n mag = %4.2f  trixel %d -> %d\n", mag, HPStar->trixel, trixel );

        // in with the new ...
        HPStar->trixel = trixel;
        if (trixel >= m_skyMesh->size())
            qDebug() << "### Expect an Index out-of-range error. trixel =" << trixel;

        StarList *list = starIndex->at(trixel);
        int j;
        for (j = 0; j < list->size(); j++)
        {
            if (list->at(j)->mag() < mag)
                continue;
            list->insert(j, star);
            break;
        }
        if (j == list->size())
            list->append(star);

        //for ( j = 0; j < list->size(); j++ ) {
        //    printf("    %4.2f\n", list->at(j)->mag() );
        //}
    }
    return true;
    //printf("Re-indexed %d stars at interval %6.1f\n", cnt, 100.0 * m_reindexInterval );
}

void HighPMStarList::stats()
{
    printf("\n");
    printf("maxPM: %6.1f  threshold %5.1f\n", m_maxPM, m_threshold);
    printf("stars: %d\n", size());
    printf("Update Interval: %6.1f years\n", 100.0 * m_reindexInterval);
    printf("Last Update: %6.1f\n", 2000.0 + 100.0 * (int)m_reindexNum.julianCenturies());
}
