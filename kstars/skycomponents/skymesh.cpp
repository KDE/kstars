/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <bowlin@mindspring.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skymesh.h"

#include "kstarsdata.h"
#ifndef KSTARS_LITE
#include "skymap.h"
#endif
#include "htmesh/MeshIterator.h"
#include "htmesh/MeshBuffer.h"
#include "projections/projector.h"
#include "skyobjects/starobject.h"

#include <QPainter>
#include <QPolygonF>
#include <QPointF>

QMap<int, SkyMesh *> SkyMesh::pinstances;
int SkyMesh::defaultLevel = -1;

SkyMesh *SkyMesh::Create(int level)
{
    SkyMesh *newInstance = pinstances.value(level, nullptr);
    if (newInstance != nullptr)
        return newInstance;

    newInstance = new SkyMesh(level);
    pinstances.insert(level, newInstance);
    if (defaultLevel < 0)
        defaultLevel = newInstance->level();

    return newInstance;
}

SkyMesh *SkyMesh::Instance()
{
    return pinstances.value(defaultLevel, nullptr);
}

SkyMesh *SkyMesh::Instance(int level)
{
    return pinstances.value(level, nullptr);
}

SkyMesh::SkyMesh(int level)
    : HTMesh(level, level, NUM_MESH_BUF), m_drawID(0), m_KSNumbers(0)
{
    errLimit = HTMesh::size() / 4;
    m_inDraw = false;
}

void SkyMesh::aperture(SkyPoint *p0, double radius, MeshBufNum_t bufNum)
{
    KStarsData *data = KStarsData::Instance();
    // FIXME: simple copying leads to incorrect results because RA0 && dec0 are both zero sometimes
    SkyPoint p1(p0->ra(), p0->dec());
    long double now = data->updateNum()->julianDay();
    p1.catalogueCoord(now);

    if (radius == 1.0)
    {
#ifndef KSTARS_LITE
        printf("\n ra0 = %8.4f   dec0 = %8.4f\n", p0->ra().Degrees(), p0->dec().Degrees());
        printf(" ra1 = %8.4f   dec1 = %8.4f\n", p1.ra().Degrees(), p1.dec().Degrees());
#endif
        SkyPoint p2 = p1;
        p2.updateCoordsNow(data->updateNum());

#ifndef KSTARS_LITE
        printf(" ra2 = %8.4f  dec2 = %8.4f\n", p2.ra().Degrees(), p2.dec().Degrees());
        printf("p0 - p1 = %6.4f degrees\n", p0->angularDistanceTo(&p1).Degrees());
        printf("p0 - p2 = %6.4f degrees\n", p0->angularDistanceTo(&p2).Degrees());
#endif
    }

    HTMesh::intersect(p1.ra().Degrees(), p1.dec().Degrees(), radius, (BufNum)bufNum);
    m_drawID++;
//    if (m_inDraw && bufNum != DRAW_BUF)
//        printf("Warning: overlapping buffer: %d\n", bufNum);
}

Trixel SkyMesh::index(const SkyPoint *p)
{
    return HTMesh::index(p->ra0().Degrees(), p->dec0().Degrees());
}

Trixel SkyMesh::indexStar(StarObject *star)
{
    double ra, dec;
    star->getIndexCoords(&m_KSNumbers, &ra, &dec);
    return HTMesh::index(ra, dec);
}

void SkyMesh::indexStar(StarObject *star1, StarObject *star2)
{
    double ra1, ra2, dec1, dec2;
    star1->getIndexCoords(&m_KSNumbers, &ra1, &dec1);
    star2->getIndexCoords(&m_KSNumbers, &ra2, &dec2);
    HTMesh::intersect(ra1, dec1, ra2, dec2);
}

void SkyMesh::index(const SkyPoint *p, double radius, MeshBufNum_t bufNum)
{
    HTMesh::intersect(p->ra().Degrees(), p->dec().Degrees(), radius, (BufNum)bufNum);
//    if (m_inDraw && bufNum != DRAW_BUF)
//        printf("Warning: overlapping buffer: %d\n", bufNum);
}

void SkyMesh::index(const SkyPoint *p1, const SkyPoint *p2)
{
    HTMesh::intersect(p1->ra0().Degrees(), p1->dec0().Degrees(), p2->ra0().Degrees(), p2->dec0().Degrees());
}

void SkyMesh::index(const SkyPoint *p1, const SkyPoint *p2, const SkyPoint *p3)
{
    HTMesh::intersect(p1->ra0().Degrees(), p1->dec0().Degrees(), p2->ra0().Degrees(), p2->dec0().Degrees(),
                      p3->ra0().Degrees(), p3->dec0().Degrees());
}

void SkyMesh::index(const SkyPoint *p1, const SkyPoint *p2, const SkyPoint *p3, const SkyPoint *p4)
{
    HTMesh::intersect(p1->ra0().Degrees(), p1->dec0().Degrees(), p2->ra0().Degrees(), p2->dec0().Degrees(),
                      p3->ra0().Degrees(), p3->dec0().Degrees(), p4->ra0().Degrees(), p4->dec0().Degrees());
}

void SkyMesh::index(const QPointF &p1, const QPointF &p2, const QPointF &p3)
{
    HTMesh::intersect(p1.x() * 15.0, p1.y(), p2.x() * 15.0, p2.y(), p3.x() * 15.0, p3.y());
}

void SkyMesh::index(const QPointF &p1, const QPointF &p2, const QPointF &p3, const QPointF &p4)
{
    HTMesh::intersect(p1.x() * 15.0, p1.y(), p2.x() * 15.0, p2.y(), p3.x() * 15.0, p3.y(), p4.x() * 15.0, p4.y());
}

const IndexHash &SkyMesh::indexLine(SkyList *points)
{
    return indexLine(points, nullptr);
}

const IndexHash &SkyMesh::indexStarLine(SkyList *points)
{
    SkyPoint *pThis, *pLast;

    indexHash.clear();

    if (points->isEmpty())
        return indexHash;

    pLast = points->at(0).get();
    for (int i = 1; i < points->size(); i++)
    {
        pThis = points->at(i).get();

        indexStar((StarObject *)pThis, (StarObject *)pLast);
        MeshIterator region(this);

        while (region.hasNext())
        {
            indexHash[region.next()] = true;
        }
        pLast = pThis;
    }

    //printf("indexStarLine %d -> %d\n", points->size(), indexHash.size() );
    return indexHash;
}

const IndexHash &SkyMesh::indexLine(SkyList *points, IndexHash *skip)
{
    SkyPoint *pThis, *pLast;

    indexHash.clear();

    if (points->isEmpty())
        return indexHash;

    pLast = points->at(0).get();
    for (int i = 1; i < points->size(); i++)
    {
        pThis = points->at(i).get();

        if (skip != nullptr && skip->contains(i))
        {
            pLast = pThis;
            continue;
        }

        index(pThis, pLast);
        MeshIterator region(this);

        if (region.size() > errLimit)
        {
            printf("\nSkyMesh::indexLine: too many trixels: %d\n", region.size());
            printf("    ra1  = %f;\n", pThis->ra0().Degrees());
            printf("    ra2  = %f;\n", pLast->ra0().Degrees());
            printf("    dec1 = %f;\n", pThis->dec0().Degrees());
            printf("    dec2 = %f;\n", pLast->dec0().Degrees());
            HTMesh::setDebug(10);
            index(pThis, pLast);
            HTMesh::setDebug(0);
        }

        // This was used to track down a bug in my HTMesh code. The bug was caught
        // and fixed but I've left this debugging code in for now.  -jbb

        else
        {
            while (region.hasNext())
            {
                indexHash[region.next()] = true;
            }
        }
        pLast = pThis;
    }
    return indexHash;
}

// ----- Create HTMesh Index for Polygons -----
// Create (mostly) 4-point polygons that cover the mw polygon and
// all share the same first vertex.  Use indexHash to eliminate
// the many duplicate indices that are generated with this procedure.
// There are probably faster and better ways to do this.

const IndexHash &SkyMesh::indexPoly(SkyList *points)
{
    indexHash.clear();

    if (points->size() < 3)
        return indexHash;

    SkyPoint *startP = points->first().get();

    int end = points->size() - 2; // 1) size - 1  -> last index,
    // 2) minimum of 2 points

    for (int p = 1; p <= end; p += 2)
    {
        if (p == end)
        {
            index(startP, points->at(p).get(), points->at(p + 1).get());
        }
        else
        {
            index(startP, points->at(p).get(), points->at(p + 1).get(), points->at(p + 2).get());
        }

        MeshIterator region(this);

        if (region.size() > errLimit)
        {
            printf("\nSkyMesh::indexPoly: too many trixels: %d\n", region.size());

            printf("    ra1 = %f;\n", startP->ra0().Degrees());
            printf("    ra2 = %f;\n", points->at(p)->ra0().Degrees());
            printf("    ra3 = %f;\n", points->at(p + 1)->ra0().Degrees());
            if (p < end)
                printf("    ra4 = %f;\n", points->at(p + 2)->ra0().Degrees());

            printf("    dec1 = %f;\n", startP->dec0().Degrees());
            printf("    dec2 = %f;\n", points->at(p)->dec0().Degrees());
            printf("    dec3 = %f;\n", points->at(p + 1)->dec0().Degrees());
            if (p < end)
                printf("    dec4 = %f;\n", points->at(p + 2)->dec0().Degrees());

            printf("\n");
        }
        while (region.hasNext())
        {
            indexHash[region.next()] = true;
        }
    }
    return indexHash;
}

const IndexHash &SkyMesh::indexPoly(const QPolygonF *points)
{
    indexHash.clear();

    if (points->size() < 3)
        return indexHash;

    const QPointF startP = points->first();

    int end = points->size() - 2; // 1) size - 1  -> last index,
    // 2) minimum of 2 points
    for (int p = 1; p <= end; p += 2)
    {
        if (p == end)
        {
            index(startP, points->at(p), points->at(p + 1));
        }
        else
        {
            index(startP, points->at(p), points->at(p + 1), points->at(p + 2));
        }

        MeshIterator region(this);

        if (region.size() > errLimit)
        {
            printf("\nSkyMesh::indexPoly: too many trixels: %d\n", region.size());

            printf("    ra1 = %f;\n", startP.x());
            printf("    ra2 = %f;\n", points->at(p).x());
            printf("    ra3 = %f;\n", points->at(p + 1).x());
            if (p < end)
                printf("    ra4 = %f;\n", points->at(p + 2).x());

            printf("    dec1 = %f;\n", startP.y());
            printf("    dec2 = %f;\n", points->at(p).y());
            printf("    dec3 = %f;\n", points->at(p + 1).y());
            if (p < end)
                printf("    dec4 = %f;\n", points->at(p + 2).y());

            printf("\n");
        }
        while (region.hasNext())
        {
            indexHash[region.next()] = true;
        }
    }
    return indexHash;
}

// NOTE: SkyMesh::draw() is primarily used for debugging purposes, to
// show the trixels to enable visualizing them. Thus, it is not
// necessary that this be compatible with GL unless we abandon the
// QPainter some day, or the need arises to use this for some other
// purpose. -- asimha
void SkyMesh::draw(QPainter &psky, MeshBufNum_t bufNum)
{
#ifndef KSTARS_LITE
    SkyMap *map      = SkyMap::Instance();
    KStarsData *data = KStarsData::Instance();

    double r1, d1, r2, d2, r3, d3;

    MeshIterator region(this, bufNum);
    while (region.hasNext())
    {
        Trixel trixel = region.next();
        vertices(trixel, &r1, &d1, &r2, &d2, &r3, &d3);
        SkyPoint s1(r1 / 15.0, d1);
        SkyPoint s2(r2 / 15.0, d2);
        SkyPoint s3(r3 / 15.0, d3);
        s1.EquatorialToHorizontal(data->lst(), data->geo()->lat());
        s2.EquatorialToHorizontal(data->lst(), data->geo()->lat());
        s3.EquatorialToHorizontal(data->lst(), data->geo()->lat());
        QPointF q1 = map->projector()->toScreen(&s1);
        QPointF q2 = map->projector()->toScreen(&s2);
        QPointF q3 = map->projector()->toScreen(&s3);
        psky.drawLine(q1, q2);
        psky.drawLine(q2, q3);
        psky.drawLine(q3, q1);
        // Draw the name of the trixel
        QString TrixelNumberString;
        TrixelNumberString.setNum(trixel);
        psky.drawText((q1 + q2 + q3) / 3.0, TrixelNumberString);
    }
#else
    Q_UNUSED(psky);
    Q_UNUSED(bufNum);
#endif
}

const SkyRegion &SkyMesh::skyRegion(const SkyPoint &_p1, const SkyPoint &_p2)
{
    std::shared_ptr<SkyPoint> p1(new SkyPoint(_p1));
    std::shared_ptr<SkyPoint> p2(new SkyPoint(_p2));
    std::shared_ptr<SkyPoint> p3(new SkyPoint(p1->ra(), p2->dec()));
    std::shared_ptr<SkyPoint> p4(new SkyPoint(p2->ra(), p1->dec()));
    SkyList skylist;

    skylist.push_back(p1);
    skylist.push_back(p2);
    skylist.push_back(p3);
    skylist.push_back(p4);
    return indexPoly(&skylist);
}
