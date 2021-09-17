/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "constellationlines.h"

#include "kstarsdata.h"
#include "kstars_debug.h"
#include "linelist.h"
#ifdef KSTARS_LITE
#include "skymaplite.h"
#else
#include "skymap.h"
#endif
#include "Options.h"
#include "skypainter.h"
#include "skycomponents/culturelist.h"
#include "skycomponents/starcomponent.h"
#include "ksparser.h"

ConstellationLines::ConstellationLines(SkyComposite *parent, CultureList *cultures)
    : LineListIndex(parent, i18n("Constellation Lines")), m_reindexNum(J2000)
{
    //Create the ConstellationLinesComponents.  Each is a series of points
    //connected by line segments.  A single constellation can be composed of
    //any number of these series, and we don't need to know which series
    //belongs to which constellation.

    //The constellation lines data file (clines.dat) contains lists
    //of abbreviated genetive star names in the same format as they
    //appear in the star data files (hipNNN.dat).
    //
    //Each constellation consists of a QList of SkyPoints,
    //corresponding to the stars at each "node" of the constellation.
    //These are pointers to the starobjects themselves, so the nodes
    //will automatically be fixed to the stars even as the star
    //positions change due to proper motions.  In addition, each node
    //has a corresponding flag that determines whether a line should
    //connect this node and the previous one.

    intro();

    bool culture = false;
    std::shared_ptr<LineList> lineList;
    double maxPM(0.0);
    KSFileReader fileReader;

    if (!fileReader.open("clines.dat"))
        return;

    while (fileReader.hasMoreLines())
    {
        QString line = fileReader.readLine();

        if (line.isEmpty())
            continue;

        QChar mode = line.at(0);

        //ignore lines beginning with "#":
        if (mode == '#')
            continue;
        //If the first character is "M", we are starting a new series.
        //In this case, add the existing clc component to the composite,
        //then prepare a new one.

        if (mode == 'C')
        {
            QString cultureName = line.mid(2).trimmed();
            culture             = cultureName == cultures->current();
            continue;
        }

        if (culture)
        {
            //Mode == 'M' starts a new series of line segments, joined end to end
            if (mode == 'M')
            {
                if (lineList.get())
                    appendLine(lineList);
                lineList.reset(new LineList());
            }

            int HDnum        = line.mid(2).trimmed().toInt();
            std::shared_ptr<SkyPoint> star;
            StarObject* tempStar = StarComponent::Instance()->findByHDIndex(HDnum);

            if (tempStar && lineList)
            {
                double pm = tempStar->pmMagnitude();

                star.reset(new StarObject(*tempStar));
                if (maxPM < pm)
                    maxPM = pm;

                lineList->append(std::move(star));
            }
            else if (!star.get())
                qCWarning(KSTARS) << i18n("Star HD%1 not found.", HDnum);
        }
    }
    //Add the last clc component
    if (lineList.get())
        appendLine(lineList);

    m_reindexInterval = StarObject::reindexInterval(maxPM);
    //printf("CLines:           maxPM = %6.1f milliarcsec/year\n", maxPM );
    //printf("CLines: Update Interval = %6.1f years\n", m_reindexInterval * 100.0 );
    summary();
}

bool ConstellationLines::selected()
{
#ifndef KSTARS_LITE
    return Options::showCLines() && !(Options::hideOnSlew() && Options::hideCLines() && SkyMap::IsSlewing());
#else
    return Options::showCLines() && !(Options::hideOnSlew() && Options::hideCLines() && SkyMapLite::IsSlewing());
#endif
}

void ConstellationLines::preDraw(SkyPainter *skyp)
{
    KStarsData *data = KStarsData::Instance();
    QColor color     = data->colorScheme()->colorNamed("CLineColor");
    skyp->setPen(QPen(QBrush(color), 1, Qt::SolidLine));
}

const IndexHash &ConstellationLines::getIndexHash(LineList *lineList)
{
    return skyMesh()->indexStarLine(lineList->points());
}

// JIT updating makes this simple.  Star updates are called from within both
// StarComponent and ConstellationLines.  If the update is redundant then
// StarObject::JITupdate() simply returns without doing any work.
void ConstellationLines::JITupdate(LineList *lineList)
{
    KStarsData *data   = KStarsData::Instance();
    lineList->updateID = data->updateID();

    SkyList *points = lineList->points();

    for (const auto &point : *points)
    {
        StarObject *star = (StarObject *)point.get();

        star->JITupdate();
    }
}

void ConstellationLines::reindex(KSNumbers *num)
{
    if (!num)
        return;

    if (fabs(num->julianCenturies() - m_reindexNum.julianCenturies()) < m_reindexInterval)
        return;

    //printf("Re-indexing CLines to year %4.1f...\n", 2000.0 + num->julianCenturies() * 100.0);

    m_reindexNum = KSNumbers(*num);
    skyMesh()->setKSNumbers(num);
    LineListIndex::reindexLines();

    //printf("Done.\n");
}
