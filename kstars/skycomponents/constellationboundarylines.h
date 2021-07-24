/***************************************************************************
               constellationboundary.h  -  K Desktop Planetarium
                             -------------------
    begin                : 25 Oct. 2005
    copyright            : (C) 2005 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

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
