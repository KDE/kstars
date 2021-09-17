/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <bowlin@mindspring.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/* It is a bit cheesy/crappy to have somewhat unrelated typedefs gathered in a
 * single typedef.h file like this but it has been incredibly useful For
 * example, I could convert the list structures below from QList to QVector by
 * simply making a single change in this file.  Also, typedefs can take people
 * by surprise so by putting most of them all in one place I hope to remove
 * some of the surprise.
 *
 * -- James B. Bowlin
 *
 * We may transition to the use of smart pointers whereever possible. Gathering
 * syntactic sugar here, takes the pain of it away.
 *
 * Conventions:
 *  - [x]_ptr<Type> => Type_[first letter of `x`]
 *    for example: typedef std::shared_ptr<SkyObject> SkyObject_s;
 *
 * -- Valentin Boettcher
 */

#pragma once

#include <qglobal.h>
#include <QHash>
#include <QVector>

#include <memory>

class SkyPoint;
class LineList;
class StarObject;
class StarBlock;
class SkyObject;
class KSPlanetBase;
class GeoLocation;
class EclipseEvent;

typedef quint32 DrawID;
typedef quint32 UpdateID;
typedef qint32 Trixel;
typedef unsigned short BufNum;

typedef QVector<std::shared_ptr<SkyPoint>> SkyList;
typedef QHash<Trixel, bool> IndexHash;
typedef QHash<Trixel, bool> SkyRegion;
typedef QList<StarObject *> StarList;
typedef QVector<StarList *> StarIndex;
typedef QVector<std::shared_ptr<LineList>> LineListList;
typedef QHash<Trixel, std::shared_ptr<LineListList>> LineListHash;
typedef QList<SkyObject *> SkyObjectList;

typedef std::shared_ptr<SkyObject> SkyObject_s;
typedef std::shared_ptr<KSPlanetBase> KSPlanetBase_s;
typedef std::shared_ptr<EclipseEvent> EclipseEvent_s;
