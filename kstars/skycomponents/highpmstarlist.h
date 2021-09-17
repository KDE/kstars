/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <bowlin@mindspring.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ksnumbers.h"
#include "typedef.h"

struct HighPMStar;
class SkyMesh;

/** @class HighPMStarList
 * Holds a list of stars with high proper motion along with the trixel each star
 * is currently located in.  The purpose of this class is to save some time by
 * only re-indexing the stars with high proper motion instead of the entire
 * collection of stars (which takes about 4 seconds on a AMD64 3600).
 *
 * Multiple HighPMStarList's can be used so we re-index a smaller number of
 * stars more frequently and a larger number of stars less frequently.
 *
 *
 * @author James B. Bowlin @version 0.1
*/

class HighPMStarList
{
  public:
    /** @short Constructor */
    explicit HighPMStarList(double threshold);
    ~HighPMStarList();

    /**
     * @short adds the star located at trixel to the list if the pm is
     * greater than the threshold.  We also use the pm to determine the
     * update interval.  Returns true if the star was appended and false
     * otherwise.
     */
    bool append(Trixel trixel, StarObject *star, double pm);

    /** @short returns the threshold. */
    double threshold() const { return m_threshold; }

    /** @short returns the number of stars in the list. */
    int size() const { return m_stars.size(); }

    /**
     * @short sets the time this list was last indexed to.  Normally this
     * is done automatically in the reindex() routine but this is useful
     * if the entire starIndex gets re-indexed.
     */
    void setIndexTime(KSNumbers *num);

    /**
     * @short if the date in num differs from the last time we indexed by
     * more than our update interval then we re-index all the stars in our
     * list that have actually changed trixels.
     */
    bool reindex(KSNumbers *num, StarIndex *starIndex);

    /** @short prints out some brief statistics. */
    void stats();

  private:
    QVector<HighPMStar *> m_stars;

    KSNumbers m_reindexNum;
    double m_reindexInterval { 0 };
    double m_threshold { 0 };
    double m_maxPM { 0 };

    SkyMesh *m_skyMesh { nullptr };
};
