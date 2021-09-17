/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "noprecessindex.h"

#include <QHash>
#include <QPolygonF>

class PolyList;
class ConstellationBoundary;
class KSFileReader;

typedef QVector<std::shared_ptr<PolyList>> PolyListList;
typedef QVector<std::shared_ptr<PolyListList>> PolyIndex;

/**
 * @class ConstellationBoundary
 * Collection of lines comprising the borders between constellations
 *
 * @author Jason Harris
 * @version 0.1
 */
class ConstellationBoundaryLines : public NoPrecessIndex
{
  public:
    /**
     * @short Constructor
     * Simply adds all of the coordinate grid circles (meridians and parallels)
     * @p parent Pointer to the parent SkyComposite object
     *
     * Reads the constellation boundary data from cbounds.dat. The boundary data is defined by
     * a series of RA,Dec coordinate pairs defining the "nodes" of the boundaries. The nodes are
     * organized into "segments", such that each segment represents a continuous series
     * of boundary-line intervals that divide two particular constellations.
     */
    explicit ConstellationBoundaryLines(SkyComposite *parent);
    virtual ~ConstellationBoundaryLines() override = default;

    QString constellationName(const SkyPoint *p) const;

    bool selected() override;

    void preDraw(SkyPainter *skyp) override;

  private:
    void appendPoly(const std::shared_ptr<PolyList> &polyList, int debug = 0);

    /**
     * @short reads the indices from the KSFileReader instead of using
     * the SkyMesh to create them.  If the file pointer is null or if
     * debug == -1 then we fall back to using the index.
     */
    void appendPoly(std::shared_ptr<PolyList> &polyList, KSFileReader *file, int debug);

    PolyList *ContainingPoly(const SkyPoint *p) const;

    SkyMesh *m_skyMesh { nullptr };
    PolyIndex m_polyIndex;
    int m_polyIndexCnt { 0 };
};
