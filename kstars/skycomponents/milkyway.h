/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "linelistindex.h"

/**
 * @class MlkyWay
 *
 * Draw filled areas as Milky Way and Magellanic clouds.
 *
 * This class should store SkipLists instead of LineLists.  The two methods
 * are used inside of LineListIndex to access the SkipLists' skip hashes.
 * This way the same code in LineListIndex does double duty. Only subclassed
 * by MilkyWay.
 *
 * @author James B. Bowlin
 * @version 0.1
 */
class MilkyWay : public LineListIndex
{
    friend class MilkyWayItem;

  public:
    /**
     * @short Constructor
     * @p parent pointer to the parent SkyComposite
     */
    explicit MilkyWay(SkyComposite *parent);

    /** Load skiplists from file */
    void loadContours(QString fname, QString greeting);

    void draw(SkyPainter *skyp) override;
    bool selected() override;

  protected:
    /**
     * @short Returns an IndexHash from the SkyMesh that contains the set
     * of trixels that cover the _SkipList_ lineList excluding skipped
     * lines as specified in the SkipList.  SkipList is a subclass of
     * LineList.
     * FIXME: Implementation is broken!!
     */
    const IndexHash &getIndexHash(LineList *skipList) override;

    /**
     * @short Returns a boolean indicating whether to skip the i-th line
     * segment in the _SkipList_ skipList.  Note that SkipList is a
     * subclass of LineList.  This routine allows us to use the drawing
     * code in LineListIndex instead of repeating it all here.
     * FIXME: Implementation is broken!!
     */
    SkipHashList *skipList(LineList *lineList) override;
};
