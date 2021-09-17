/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ksnumbers.h"
#include "linelistindex.h"

class CultureList;

/**
 * @class ConstellationLines
 * Collection of lines making the 88 constellations
 *
 * @author Jason Harris
 * @version 0.1
 */

class ConstellationLines : public LineListIndex
{
  public:
    /**
     * @short Constructor
     * @p parent Pointer to the parent SkyComposite object
     *
     * Constellation lines data is read from clines.dat.
     * Each line in the file contains a command character ("M" means move to
     * this position without drawing a line, "D" means draw a line from
     * the previous position to this one), followed by the genetive name of
     * a star, which marks the position of the constellation node.
     */
    ConstellationLines(SkyComposite *parent, CultureList *cultures);

    void reindex(KSNumbers *num);

    bool selected() override;

  protected:
    const IndexHash &getIndexHash(LineList *lineList) override;

    /**
     * @short we need to override the update routine because stars are
     * updated differently from mere SkyPoints.
     */
    void JITupdate(LineList *lineList) override;

    /** @short Set the QColor and QPen for drawing. */
    void preDraw(SkyPainter *skyp) override;

  private:
    KSNumbers m_reindexNum;
    double m_reindexInterval { 0 };
};
