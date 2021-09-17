/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <bowlin@mindspring.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "typedef.h"

#include <QList>

class SkyPoint;
class KSNumbers;

/**
 * @class LineList
 * A simple data container used by LineListIndex.  It contains a list of
 * SkyPoints and integer drawID, updateID and updateNumID.
 *
 * @author James B. Bowlin
 * @version 0.2
*/
class LineList
{
  public:
    LineList() : drawID(0), updateID(0), updateNumID(0) {}
    virtual ~LineList() = default;

    /**
     * @short return the list of points for iterating or appending (or whatever).
     */
    SkyList *points() { return &pointList; }
    std::shared_ptr<SkyPoint> at(int i) { return pointList.at(i); }
    void append(std::shared_ptr<SkyPoint> p) { pointList.append(p); }

    /**
     * A global drawID (in SkyMesh) is updated at the start of each draw
     * cycle.  Since an extended object is often covered by more than one
     * trixel, the drawID is used to make sure each object gets drawn at
     * most once per draw cycle.  It is public because it is both set and
     * read by the LineListIndex class.
     */
    DrawID drawID;
    UpdateID updateID;
    UpdateID updateNumID;

  private:
    SkyList pointList;
};
