/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "typedef.h"

class DeepStarComponent;
class StarBlock;

/**
 * @class StarBlockList
 * Maintains a list of StarBlocks that contain the stars lying in a single trixel.
 * Takes care of the dynamic loading of stars
 *
 * @author Akarsh Simha
 * @version 0.1
 */
class StarBlockList
{
  public:
    /**
     * Constructor for deep star catalogs.
     * @param trixel The trixel ID
     * @param parent Pointer to the parent DeepStarComponent
     */
    explicit StarBlockList(const Trixel &trixel, DeepStarComponent *parent = nullptr);

    /**
     * @short Ensures that the list is loaded with stars to given magnitude limit
     *
     * @param maglim Magnitude limit to load stars upto
     * @return true on success, false on failure (data file not found, bad seek etc)
     */
    bool fillToMag(float maglim);

    /**
     * @short Sets the first StarBlock in the list to point to the given StarBlock
     *
     * This function must ideally be used only once. Also, it does not make a copy
     * of the StarBlock supplied, but uses the pointer directly. StarBlockList will
     * take care of deleting the StarBlock when it is destroyed
     *
     * @param block Pointer to the StarBlock
     */
    void setStaticBlock(std::shared_ptr<StarBlock> &block);

    /**
     * @short  Drops the StarBlock with the given pointer from the list
     * @param  block Pointer to the StarBlock to remove
     * @return Number of entries removed from the QList
     */
    int releaseBlock(StarBlock *block);

    /**
     * @short  Returns the i-th block in this StarBlockList
     *
     * @param  i Index of the required block
     * @return The StarBlock requested for, nullptr if index out of bounds
     */
    inline std::shared_ptr<StarBlock> block(unsigned int i) { return ((i < nBlocks) ? blocks[i] : std::shared_ptr<StarBlock>()); }

    /**
     * @return a const reference to the contents of this StarBlockList
     */
    inline const QList<std::shared_ptr<StarBlock>> &contents() const { return blocks; }

    /**
     * @short  Returns the total number of stars in this StarBlockList
     * @return Total number of stars in this StarBlockList
     */
    inline long getStarCount() const { return nStars; }

    /**
     * @short  Returns the total number of blocks in theis StarBlockList
     * @return Number of blocks in this StarBlockList
     */
    inline int getBlockCount() const { return nBlocks; }

    /**
     * @short  Returns the magnitude of the faintest star currently stored
     * @return Magnitude of faintest star stored in this StarBlockList
     */
    inline float getFaintMag() const { return faintMag; }

    /**
     * @short  Returns the trixel that this SBL is meant for
     * @return The value of trixel
     */
    inline Trixel getTrixel() const { return trixel; }

  private:
    Trixel trixel;
    unsigned long nStars { 0 };
    long readOffset { 0 };
    float faintMag { -5 };
    QList<std::shared_ptr<StarBlock>> blocks;
    unsigned int nBlocks { 0 };
    bool staticStars { false };
    DeepStarComponent *parent { nullptr };
};
