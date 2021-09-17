/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "typedef.h"
#include "starblocklist.h"

#include <QVector>

class StarObject;
class StarBlockList;
class PointSourceNode;
struct StarData;
struct DeepStarData;

#ifdef KSTARS_LITE
#include "starobject.h"

struct StarNode
{
    StarNode();
    ~StarNode();

    StarObject star;
    PointSourceNode *starNode;
};
#endif

/**
 * @class StarBlock
 *
 * Holds a block of stars and various peripheral variables to mark its place in data structures
 *
 * @author  Akarsh Simha
 * @version 1.0
 */

class StarBlock
{
  public:
// StarBlockEntry is the data type held by the StarBlock's QVector
#ifdef KSTARS_LITE
    typedef StarNode StarBlockEntry;
#else
    typedef StarObject StarBlockEntry;
#endif

    /**
     * Constructor
     *
     * Initializes values of various parameters and creates nstars number of stars
     *
     * @param nstars   Number of stars to hold in this StarBlock
     */
    explicit StarBlock(int nstars = 100);

    ~StarBlock() = default;

    /**
     * @short Initialize another star with data.
     *
     * FIXME: StarObject::init doesn't reset object name(s). It
     * shouldn't be issue since stars which are swapped in/out do not
     * have names.
     *
     * @param  data    data to initialize star with.
     * @return pointer to star initialized with data. nullptr if block is full.
     */
    StarBlockEntry *addStar(const StarData &data);
    StarBlockEntry *addStar(const DeepStarData &data);

    /**
     * @short Returns true if the StarBlock is full
     *
     * @return true if full, false if not full
     */
    inline bool isFull() const { return nStars == size(); }

    /**
     * @short  Return the capacity of this StarBlock
     *
     * This is different from nStars. While nStars indicates the number of stars that this StarBlock
     * actually holds, this method returns the number of stars for which we have allocated memory.
     * Thus, this method should return a number >= nStars.
     *
     * @return The number of stars that this StarBlock can hold
     */
    inline int size() const { return stars.size(); }

    /**
     * @short  Return the i-th star in this StarBlock
     *
     * @param  i Index of StarBlock to return
     * @return A pointer to the i-th StarObject
     */
    inline StarBlockEntry *star(int i) { return &stars[i]; }

    /**
     * @return a reference to the internal container of this
     * @note This is bad -- is there a way of providing non-const access to the list's elements
     * without allowing altering of the list alone?
     */

    inline QVector<StarBlockEntry> &contents() { return stars; }

    // These methods are there because we might want to make faintMag and brightMag private at some point
    /**
     * @short  Return the magnitude of the brightest star in this StarBlock
     *
     * @return Magnitude of the brightest star
     */
    inline float getBrightMag() const { return brightMag; }

    /**
     * @short  Return the magnitude of the faintest star in this StarBlock
     *
     * @return Magnitude of the faintest star
     */
    inline float getFaintMag() const { return faintMag; }

    /**
     * @short  Return the number of stars currently filled in this StarBlock
     *
     * @return Number of stars filled in this StarBlock
     */
    inline int getStarCount() const { return nStars; }

    /** @short  Reset this StarBlock's data, for reuse of the StarBlock */
    void reset();

    float faintMag { 0 };
    float brightMag { 0 };
    StarBlockList *parent;
    std::shared_ptr<StarBlock> prev;
    std::shared_ptr<StarBlock> next;
    quint32 drawID { 0 };

  private:
    // Disallow copying and assignment. Just in case.
    StarBlock(const StarBlock &);
    StarBlock &operator=(const StarBlock &);

    /** Number of initialized stars in StarBlock. */
    int nStars { 0 };
    /** Array of stars. */
    QVector<StarBlockEntry> stars;
};
