/***************************************************************************
                        highpmstarlist.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-08-06
    copyright            : (C) 2007 by James B. Bowlin
    email                : bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef HIGHPMSTARLIST_H
#define HIGHPMSTARLIST_H

#include "ksnumbers.h"
#include "typedef.h"

class SkyMesh;
struct HighPMStar;

/* @class HighPMStarList
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
    /* @short Constructor
     */
    explicit HighPMStarList( double threshold );

    ~HighPMStarList();

    /* @short adds the star located at trixel to the list if the pm is
     * greater than the threshold.  We also use the pm to determine the
     * update interval.  Returns true if the star was appended and false
     * otherwise.
     */
    bool append( Trixel trixel, StarObject* star, double pm );

    /* @short returns the threshold.
     */
    double threshold() { return m_threshold; }

    /* @short returns the number of stars in the list.
     */
    int size() { return m_stars.size(); }

    /* @short sets the time this list was last indexed to.  Normally this
     * is done automatically in the reindex() routine but this is useful
     * if the entire starIndex gets re-indexed.
     */
    void setIndexTime( KSNumbers *num );

    /* @short if the date in num differs from the last time we indexed by
     * more than our update interval then we re-index all the stars in our
     * list that have actually changed trixels.
     */
    void reindex( KSNumbers *num, StarIndex* starIndex );

    /* @short prints out some brief statistics.
     */
    void stats();

private:
    QVector<HighPMStar*> m_stars;

    KSNumbers m_reindexNum;
    double    m_reindexInterval;
    double    m_threshold;
    double    m_maxPM;

    SkyMesh*  m_skyMesh;
};

#endif
