/*
    SPDX-FileCopyrightText: 2010 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QHash>
#include <QList>

class QStringList;

class SkyPoint;
class StarObject;

/**
 * @class StarHopper
 * @short Helps planning star hopping
 *
 * @version 1.0
 * @author Akarsh Simha
 */
class StarHopper
{
  public:
    /**
     * @short Computes path for Star Hop
     * @param src SkyPoint to source of the Star Hop
     * @param dest SkyPoint to destination of the Star Hop
     * @param fov__ Field of view within which stars are considered
     * @param maglim__ Magnitude limit of stars to consider
     * @param metadata_ Directions for starhopping
     * @return QList of StarObject pointers which are the resultant path to Star Hop
     * @note The StarObjects in the list returned are mutable and not constant
     */
    QList<StarObject *> *computePath(const SkyPoint &src, const SkyPoint &dest, float fov__, float maglim__,
                                     QStringList *metadata_ = nullptr);

  protected:
    // Returns a list of constant StarObject pointers which form the resultant path of Star Hop
    QList<const StarObject *> computePath_const(const SkyPoint &src, const SkyPoint &dest, float fov_, float maglim_,
                                                QStringList *metadata = nullptr);

  private:
    /**
     * @short The cost function for hopping from current position to the a given star, in view of the final destination
     * @param curr Source SkyPoint
     * @param next Next point in the hop.
     * @note If 'next' is neither the starting point of the hop, nor
     * the ending point, it _has_ to be a StarObject. A dynamic cast
     * followed by a Q_ASSERT will ensure this.
     */
    float cost(const SkyPoint *curr, const SkyPoint *next);

    /**
     * @short For internal use by the A* Search Algorithm. Completes
     * the star-hop path. See https://en.wikipedia.org/wiki/A*_search_algorithm for details
     */
    void reconstructPath(SkyPoint const *curr_node);

    float fov { 0 };
    float maglim { 0 };
    QString starHopDirections;
    // Useful for internal computations
    SkyPoint const *start { nullptr };
    SkyPoint const *end { nullptr };
    QHash<const SkyPoint *, const SkyPoint *> came_from; // Used by the A* search algorithm
    QList<StarObject const *> result_path;
    QHash<SkyPoint const *, QString> patternNames; // if patterns were identified, they are added to this hash.
};
