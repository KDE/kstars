/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <bowlin@mindspring.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/****************************************************************************
 * The filled polygon code in the innermost loops below in drawFilled*() below
 * implements the Sutherland-Hodgman's Polygon clipping algorithm.  Please
 * don't mess with it unless you ensure that the Milky Way clipping continues
 * to work.  The line clipping uses a similar but slightly less complicated
 * algorithm.
 *
 * Since the clipping code is a bit messy, there are four versions of the
 * inner loop for Filled/Outline * Integer/Float.  This moved these two
 * decisions out of the inner loops to make them a bit faster and less
 * messy.
 *
 * -- James B. Bowlin
 *
 ****************************************************************************/

#include "linelistindex.h"

#include "Options.h"
#include "kstarsdata.h"
#include "linelist.h"
#ifndef KSTARS_LITE
#include "skymap.h"
#endif
#include "skypainter.h"
#include "htmesh/MeshIterator.h"

LineListIndex::LineListIndex(SkyComposite *parent, const QString &name) : SkyComponent(parent), m_name(name)
{
    m_skyMesh   = SkyMesh::Instance();
    m_lineIndex.reset(new LineListHash());
    m_polyIndex.reset(new LineListHash());
}

// This is a callback for the indexLines() function below
const IndexHash &LineListIndex::getIndexHash(LineList *lineList)
{
    return skyMesh()->indexLine(lineList->points());
}

void LineListIndex::removeLine(const std::shared_ptr<LineList> &lineList)
{
    const IndexHash &indexHash     = getIndexHash(lineList.get());
    IndexHash::const_iterator iter = indexHash.constBegin();

    while (iter != indexHash.constEnd())
    {
        Trixel trixel = iter.key();
        iter++;

        if (m_lineIndex->contains(trixel))
            m_lineIndex->value(trixel)->removeOne(lineList);
    }
    m_listList.removeOne(lineList);
}

void LineListIndex::appendLine(const std::shared_ptr<LineList> &lineList)
{
    const IndexHash &indexHash     = getIndexHash(lineList.get());
    IndexHash::const_iterator iter = indexHash.constBegin();

    while (iter != indexHash.constEnd())
    {
        Trixel trixel = iter.key();

        iter++;
        if (!m_lineIndex->contains(trixel))
        {
            m_lineIndex->insert(trixel, std::shared_ptr<LineListList>(new LineListList()));
        }
        m_lineIndex->value(trixel)->append(lineList);
    }
    m_listList.append(lineList);
}

void LineListIndex::appendPoly(const std::shared_ptr<LineList> &lineList)
{
    const IndexHash &indexHash     = skyMesh()->indexPoly(lineList->points());
    IndexHash::const_iterator iter = indexHash.constBegin();

    while (iter != indexHash.constEnd())
    {
        Trixel trixel = iter.key();
        iter++;

        if (!m_polyIndex->contains(trixel))
        {
            m_polyIndex->insert(trixel, std::shared_ptr<LineListList>(new LineListList()));
        }
        m_polyIndex->value(trixel)->append(lineList);
    }
}

void LineListIndex::appendBoth(const std::shared_ptr<LineList> &lineList)
{
    QMutexLocker m1(&mutex);

    appendLine(lineList);
    appendPoly(lineList);
}

void LineListIndex::reindexLines()
{
    LineListHash *oldIndex = m_lineIndex.release();
    DrawID drawID = skyMesh()->incDrawID();

    m_lineIndex.reset(new LineListHash());
    for (auto &listList : *oldIndex)
    {
        for (auto &item : *listList)
        {
            if (item->drawID == drawID)
                continue;

            item->drawID = drawID;
            appendLine(item);
        }
        listList.reset();
    }
    delete oldIndex;
}

void LineListIndex::JITupdate(LineList *lineList)
{
    KStarsData *data   = KStarsData::Instance();
    lineList->updateID = data->updateID();
    SkyList *points    = lineList->points();

    if (lineList->updateNumID != data->updateNumID())
    {
        lineList->updateNumID = data->updateNumID();
        KSNumbers *num        = data->updateNum();

        for (const auto &point : *points)
        {
            point->updateCoords(num);
        }
    }

    for (const auto &point : *points)
    {
        point->EquatorialToHorizontal(data->lst(), data->geo()->lat());
    }
}

// This is a callback used in draw() below
void LineListIndex::preDraw(SkyPainter *skyp)
{
    skyp->setPen(QPen(QBrush(QColor("white")), 1, Qt::SolidLine));
}

void LineListIndex::draw(SkyPainter *skyp)
{
    if (!selected())
        return;
    preDraw(skyp);
    drawLines(skyp);
}

#ifdef KSTARS_LITE
MeshIterator LineListIndex::visibleTrixels()
{
    return MeshIterator(skyMesh(), drawBuffer());
}
#endif

// This is a callback used int drawLinesInt() and drawLinesFloat()
SkipHashList *LineListIndex::skipList(LineList *lineList)
{
    Q_UNUSED(lineList)
    return nullptr;
}

void LineListIndex::drawLines(SkyPainter *skyp)
{
    DrawID drawID     = skyMesh()->drawID();
    UpdateID updateID = KStarsData::Instance()->updateID();

    for (auto &lineListList : m_lineIndex->values())
    {
        for (int i = 0; i < lineListList->size(); i++)
        {
            std::shared_ptr<LineList> lineList = lineListList->at(i);

            if (lineList->drawID == drawID)
                continue;
            lineList->drawID = drawID;

            if (lineList->updateID != updateID)
                JITupdate(lineList.get());

            skyp->drawSkyPolyline(lineList.get(), skipList(lineList.get()), label());
        }
    }
}

void LineListIndex::drawFilled(SkyPainter *skyp)
{
    DrawID drawID     = skyMesh()->drawID();
    UpdateID updateID = KStarsData::Instance()->updateID();

    MeshIterator region(skyMesh(), drawBuffer());

    while (region.hasNext())
    {
        std::shared_ptr<LineListList> lineListList = m_polyIndex->value(region.next());

        if (lineListList == nullptr)
            continue;

        for (int i = 0; i < lineListList->size(); i++)
        {
            std::shared_ptr<LineList> lineList = lineListList->at(i);

            // draw each Linelist at most once
            if (lineList->drawID == drawID)
                continue;
            lineList->drawID = drawID;

            if (lineList->updateID != updateID)
                JITupdate(lineList.get());

            skyp->drawSkyPolygon(lineList.get());
        }
    }
}

void LineListIndex::intro()
{
    emitProgressText(i18n("Loading %1", m_name));
    if (skyMesh()->debug() >= 1)
        qDebug() << QString("Loading %1 ...").arg(m_name);
}

void LineListIndex::summary()
{
    if (skyMesh()->debug() < 2)
        return;

    int total    = skyMesh()->size();
    int polySize = m_polyIndex->size();
    int lineSize = m_lineIndex->size();

    if (lineSize > 0)
        printf("%4d out of %4d trixels in line index %3d%%\n", lineSize, total, 100 * lineSize / total);

    if (polySize > 0)
        printf("%4d out of %4d trixels in poly index %3d%%\n", polySize, total, 100 * polySize / total);
}
