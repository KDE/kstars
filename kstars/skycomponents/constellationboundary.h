/***************************************************************************
               constellationboundary.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-10
    copyright            : (C) 2007 James B. Bowlin
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

#ifndef CONSTELLATION_BOUNDARY_H
#define CONSTELLATION_BOUNDARY_H

#include "skycomposite.h"

#include <QHash>
#include <QPolygonF>

class SkyMesh;
class PolyList;
class KSFileReader;

typedef QVector<PolyList*>        PolyListList;
typedef QVector<PolyListList*>    PolyIndex;
typedef QHash<QString, PolyList*> PolyNameHash;


/* @class ConstellationBoundaryPoly
 * Holds the outlines of every constellation boundary in PolygonF's.
 *
 * @author James B. Bowlin
 * @version 0.1
*/

class ConstellationBoundary : public SkyComponent
{
public:
    explicit ConstellationBoundary( SkyComposite *parent );

    void appendPoly( PolyList* polyList, int debug=0);

    /* @short reads the indices from the KSFileReader instead of using
     * the SkyMesh to create them.  If the file pointer is null or if
     * debug == -1 then we fall back to using the index.
     */
    void appendPoly( PolyList* polyList, KSFileReader* file, int debug);

    void summary();

    QString constellationName( SkyPoint *p );

    const QPolygonF* constellationPoly( SkyPoint *p );

    const QPolygonF* constellationPoly( const QString& name );

    bool inConstellation( const QString &name, SkyPoint *p );

    virtual void init();
    virtual void draw(QPainter& psky);
private:
    PolyList* ContainingPoly( SkyPoint *p );

    SkyMesh*                 m_skyMesh;
    PolyIndex                m_polyIndex;
    int                      m_polyIndexCnt;
    PolyNameHash             m_nameHash;
};


#endif
