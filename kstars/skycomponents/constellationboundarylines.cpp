/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "constellationboundarylines.h"

#include "ksfilereader.h"
#include "kstarsdata.h"
#include "linelist.h"
#include "Options.h"
#include "polylist.h"
#ifdef KSTARS_LITE
#include "skymaplite.h"
#else
#include "skymap.h"
#endif
#include "skypainter.h"
#include "htmesh/MeshIterator.h"
#include "skycomponents/skymapcomposite.h"

#include <QHash>

ConstellationBoundaryLines::ConstellationBoundaryLines(SkyComposite *parent)
    : NoPrecessIndex(parent, i18n("Constellation Boundaries"))
{
    m_skyMesh      = SkyMesh::Instance();
    m_polyIndexCnt = 0;
    for (int i = 0; i < m_skyMesh->size(); i++)
    {
        m_polyIndex.append(std::shared_ptr<PolyListList>(new PolyListList()));
    }

    KStarsData *data = KStarsData::Instance();
    int verbose      = 0; // -1 => create cbounds-$x.idx on stdout
    //  0 => normal
    const char *fname = "cbounds.dat";
    int flag = 0;
    double ra, dec = 0, lastRa, lastDec;
    std::shared_ptr<LineList> lineList;
    std::shared_ptr<PolyList> polyList;
    bool ok = false;

    intro();

    // Open the .idx file and skip past the first line
    KSFileReader idxReader, *idxFile = nullptr;
    QString idxFname = QString("cbounds-%1.idx").arg(SkyMesh::Instance()->level());
    if (idxReader.open(idxFname))
    {
        idxReader.readLine();
        idxFile = &idxReader;
    }

    // now open the file that contains the points
    KSFileReader fileReader;
    if (!fileReader.open(fname))
        return;

    fileReader.setProgress(i18n("Loading Constellation Boundaries"), 13124, 10);

    lastRa = lastDec = -1000.0;

    while (fileReader.hasMoreLines())
    {
        QString line = fileReader.readLine();
        fileReader.showProgress();

        if (line.at(0) == '#')
            continue;          // ignore comments
        if (line.at(0) == ':') // :constellation line
        {
            if (lineList.get())
                appendLine(lineList);
            lineList.reset();

            if (polyList.get())
                appendPoly(polyList, idxFile, verbose);
            QString cName = line.mid(1);
            polyList.reset(new PolyList(cName));
            if (verbose == -1)
                printf(":\n");
            lastRa = lastDec = -1000.0;
            continue;
        }

        // read in the data from the line
        ra = line.midRef(0, 12).toDouble(&ok);
        if (ok)
            dec = line.midRef(13, 12).toDouble(&ok);
        if (ok)
            flag = line.midRef(26, 1).toInt(&ok);
        if (!ok)
        {
            fprintf(stderr, "%s: conversion error on line: %d\n", fname, fileReader.lineNumber());
            continue;
        }

        if (ra == lastRa && dec == lastDec)
        {
            fprintf(stderr, "%s: tossing dupe on line %4d: (%f, %f)\n", fname, fileReader.lineNumber(), ra, dec);
            continue;
        }

        // always add the point to the boundary (and toss dupes)

        // By the time we come here, we should have polyList. Else we aren't doing good
        Q_ASSERT(polyList); // Is this the right fix?

        polyList->append(QPointF(ra, dec));
        if (ra < 0)
            polyList->setWrapRA(true);

        if (flag)
        {
            if (!lineList.get())
                lineList.reset(new LineList());

            std::shared_ptr<SkyPoint> point(new SkyPoint(ra, dec));

            point->EquatorialToHorizontal(data->lst(), data->geo()->lat());
            lineList->append(std::move(point));
            lastRa  = ra;
            lastDec = dec;
        }
        else
        {
            if (lineList.get())
                appendLine(lineList);
            lineList.reset();
            lastRa = lastDec = -1000.0;
        }
    }

    if (lineList.get())
        appendLine(lineList);
    if (polyList.get())
        appendPoly(polyList, idxFile, verbose);
}

bool ConstellationBoundaryLines::selected()
{
#ifndef KSTARS_LITE
    return Options::showCBounds() && !(Options::hideOnSlew() && Options::hideCBounds() && SkyMap::IsSlewing());
#else
    return Options::showCBounds() && !(Options::hideOnSlew() && Options::hideCBounds() && SkyMapLite::IsSlewing());
#endif
}

void ConstellationBoundaryLines::preDraw(SkyPainter *skyp)
{
    QColor color = KStarsData::Instance()->colorScheme()->colorNamed("CBoundColor");
    skyp->setPen(QPen(QBrush(color), 1, Qt::SolidLine));
}

void ConstellationBoundaryLines::appendPoly(std::shared_ptr<PolyList> &polyList, KSFileReader *file, int debug)
{
    if (!file || debug == -1)
        return appendPoly(polyList, debug);

    while (file->hasMoreLines())
    {
        QString line = file->readLine();
        if (line.at(0) == ':')
            return;
        Trixel trixel = line.toInt();

        m_polyIndex[trixel]->append(polyList);
    }
}

void ConstellationBoundaryLines::appendPoly(const std::shared_ptr<PolyList> &polyList, int debug)
{
    if (debug >= 0 && debug < m_skyMesh->debug())
        debug = m_skyMesh->debug();

    const IndexHash &indexHash     = m_skyMesh->indexPoly(polyList->poly());
    IndexHash::const_iterator iter = indexHash.constBegin();
    while (iter != indexHash.constEnd())
    {
        Trixel trixel = iter.key();
        iter++;

        if (debug == -1)
            printf("%d\n", trixel);

        m_polyIndex[trixel]->append(polyList);
    }

    if (debug > 9)
        printf("PolyList: %3d: %d\n", ++m_polyIndexCnt, indexHash.size());
}

PolyList *ConstellationBoundaryLines::ContainingPoly(const SkyPoint *p) const
{
    //printf("called ContainingPoly(p)\n");

    // we save the pointers in a hash because most often there is only one
    // constellation and we can avoid doing the expensive boundary calculations
    // and just return it if we know it is unique.  We can avoid this minor
    // complication entirely if we use index(p) instead of aperture(p, r)
    // because index(p) always returns a single trixel index.

    QHash<PolyList *, bool> polyHash;
    QHash<PolyList *, bool>::const_iterator iter;

    //printf("\n");

    // the boundaries don't precess so we use index() not aperture()
    m_skyMesh->index(p, 1.0, IN_CONSTELL_BUF);
    MeshIterator region(m_skyMesh, IN_CONSTELL_BUF);
    while (region.hasNext())
    {
        Trixel trixel = region.next();
        //printf("Trixel: %4d %s\n", trixel, m_skyMesh->indexToName( trixel ) );

        std::shared_ptr<PolyListList> polyListList = m_polyIndex[trixel];

        //printf("    size: %d\n", polyListList->size() );

        for (const auto &item : *polyListList)
        {
            polyHash.insert(item.get(), true);
        }
    }

    iter = polyHash.constBegin();

    // Don't bother with boundaries if there is only one
    if (polyHash.size() == 1)
        return iter.key();

    QPointF point(p->ra().Hours(), p->dec().Degrees());
    QPointF wrapPoint(p->ra().Hours() - 24.0, p->dec().Degrees());
    bool wrapRA = p->ra().Hours() > 12.0;

    while (iter != polyHash.constEnd())
    {
        PolyList *polyList = iter.key();
        iter++;

        //qDebug() << QString("checking %1 boundary\n").arg( polyList->name() );

        const QPolygonF *poly = polyList->poly();
        if (wrapRA && polyList->wrapRA())
        {
            if (poly->containsPoint(wrapPoint, Qt::OddEvenFill))
                return polyList;
        }
        else
        {
            if (poly->containsPoint(point, Qt::OddEvenFill))
                return polyList;
        }
    }

    return nullptr;
}

//-------------------------------------------------------------------
// The routines for providing public access to the boundary index
// start here.  (Some of them may not be needed (or working)).
//-------------------------------------------------------------------

QString ConstellationBoundaryLines::constellationName(const SkyPoint *p) const
{
    PolyList *polyList = ContainingPoly(p);
    if (polyList)
    {
        return (Options::useLocalConstellNames() ?
                    i18nc("Constellation name (optional)", polyList->name().toUpper().toLocal8Bit().data()) :
                    polyList->name());
    }
    return i18n("Unknown");
}
