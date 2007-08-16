/***************************************************************************
               polylistindex.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-17
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

#ifndef POLYLISTINDEX_H
#define POLYLISTINDEX_H

#include "skycomposite.h"

#include <QHash>
#include <QPolygonF>

class PolyList;
class KSFileReader;

typedef QVector<PolyList*>        PolyListList;
typedef QVector<PolyListList*>    PolyIndex;
typedef QHash<QString, PolyList*> PolyNameHash;

/* @class PolyListIndex
 * Holds the data in PolygonF's instead of QVector<SkyPoint*>.
 *
 * @author James B. Bowlin
 * @version 0.1
*/

class PolyListIndex : public SkyComposite
{
	public:
	/**
		*@short Constructor
		*@p parent Pointer to the parent SkyComponent object
		*/
		PolyListIndex( SkyComponent *parent );

        void appendPoly( PolyList* polyList, int debug=0);

		/* @short reads the indices from the KSFileReader instead of using
		 * the SkyMesh to create them.  If the file pointer is null or if
		 * debug == -1 then we fall back to using the index.
		 */
		void appendPoly( PolyList* polyList, KSFileReader* file, int debug);

        SkyMesh* skyMesh() { return m_skyMesh; }

        PolyList* ContainingPoly( SkyPoint *p );

        PolyIndex* polyIndex() { return & m_polyIndex; }

        const PolyNameHash& nameHash() { return m_nameHash; }

        void summary();

	private:
        SkyMesh*                 m_skyMesh;
        PolyIndex                m_polyIndex;
        int                      m_polyIndexCnt;
		PolyNameHash             m_nameHash;


};


#endif
