/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "milkyway.h"

#include "ksfilereader.h"
#include "kstarsdata.h"
#ifdef KSTARS_LITE
#include "skymaplite.h"
#else
#include "skymap.h"
#endif
#include "Options.h"
#include "skypainter.h"
#include "skycomponents/skiphashlist.h"
#include "skymapcomposite.h"
#include "solarsystemcomposite.h"
#include "kssun.h"

#include <QtConcurrent>

MilkyWay::MilkyWay(SkyComposite *parent) : LineListIndex(parent, i18n("Milky Way"))
{
    intro();
    // Milky way
    //loadContours("milkyway.dat", i18n("Loading Milky Way"));
    // Magellanic clouds
    //loadContours("lmc.dat", i18n("Loading Large Magellanic Clouds"));
    //loadContours("smc.dat", i18n("Loading Small Magellanic Clouds"));
    //summary();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QtConcurrent::run(&MilkyWay::loadContours, this, QString("milkyway.dat"), i18n("Loading Milky Way"));
    QtConcurrent::run(&MilkyWay::loadContours, this, QString("lmc.dat"), i18n("Loading Large Magellanic Clouds"));
    QtConcurrent::run(&MilkyWay::loadContours, this, QString("smc.dat"), i18n("Loading Small Magellanic Clouds"));
#else
    QtConcurrent::run(this, &MilkyWay::loadContours, QString("milkyway.dat"), i18n("Loading Milky Way"));
    QtConcurrent::run(this, &MilkyWay::loadContours, QString("lmc.dat"), i18n("Loading Large Magellanic Clouds"));
    QtConcurrent::run(this, &MilkyWay::loadContours, QString("smc.dat"), i18n("Loading Small Magellanic Clouds"));
#endif
}

const IndexHash &MilkyWay::getIndexHash(LineList *lineList)
{
    SkipHashList *skipList = dynamic_cast<SkipHashList *>(lineList);
    return skyMesh()->indexLine(skipList->points(), skipList->skipHash());
}

SkipHashList *MilkyWay::skipList(LineList *lineList)
{
    return dynamic_cast<SkipHashList *>(lineList);
}

bool MilkyWay::selected()
{
#ifndef KSTARS_LITE
    return Options::showMilkyWay() && !(Options::hideOnSlew() && Options::hideMilkyWay() && SkyMap::IsSlewing());
#else
    return Options::showMilkyWay() && !(Options::hideOnSlew() && Options::hideMilkyWay() && SkyMapLite::IsSlewing());
#endif
}

void MilkyWay::draw(SkyPainter *skyp)
{
    if (!selected())
        return;

    QColor color = KStarsData::Instance()->colorScheme()->colorNamed("MWColor");
    if (Options::simulateDaytime())
    {
        KSSun *sun = KStarsData::Instance()->skyComposite()->solarSystemComposite()->sun();
        if (sun)
        {
            // No Milky Way in the daytime. Blends into sky color.
            QColor mwDayColor = KStarsData::Instance()->colorScheme()->colorNamed("SkyColorDaytime");
            const double nightFraction = sun->nightFraction();
            const double dayFraction = 1 - nightFraction;
            color = QColor(nightFraction * color.red()   + dayFraction * mwDayColor.red(),
                           nightFraction * color.green() + dayFraction * mwDayColor.green(),
                           nightFraction * color.blue()  + dayFraction * mwDayColor.blue());
        }
    }

    skyp->setPen(QPen(color, 3, Qt::SolidLine));
    skyp->setBrush(QBrush(color));

    if (Options::fillMilkyWay())
    {
        drawFilled(skyp);
    }
    else
    {
        drawLines(skyp);
    }
}

void MilkyWay::loadContours(QString fname, QString greeting)
{
    KSFileReader fileReader;
    std::shared_ptr<LineList> skipList;
    int iSkip = 0;

    if (!fileReader.open(fname))
        return;

    fileReader.setProgress(greeting, 2136, 5);
    while (fileReader.hasMoreLines())
    {
        QString line = fileReader.readLine();
        QChar firstChar = line.at(0);

        fileReader.showProgress();
        if (firstChar == '#')
            continue;

        bool okRA = false, okDec = false;
        double ra  = line.mid(2, 8).toDouble(&okRA);
        double dec = line.mid(11, 8).toDouble(&okDec);

        if (!okRA || !okDec)
        {
            qDebug() << Q_FUNC_INFO << QString("%1: conversion error on line: %2\n").arg(fname).arg(fileReader.lineNumber());
            continue;
        }

        if (firstChar == 'M')
        {
            if (skipList.get())
                appendBoth(skipList);
            skipList.reset();
            iSkip    = 0;
        }

        if (!skipList.get())
            skipList.reset(new SkipHashList());

        std::shared_ptr<SkyPoint> point(new SkyPoint(ra, dec));

        skipList->append(std::move(point));
        if (firstChar == 'S')
            static_cast<SkipHashList*>(skipList.get())->setSkip(iSkip);

        iSkip++;
    }
    if (skipList.get())
        appendBoth(skipList);
}
