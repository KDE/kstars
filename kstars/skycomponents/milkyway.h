/***************************************************************************
                           milkyway.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/07/08
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

#ifndef MILKYWAY_H
#define MILKYWAY_H

#include "linelistindex.h"

/** @class MlkyWay
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
public:
    /**@short Constructor
     * @p parent pointer to the parent SkyComposite
     */
    MilkyWay( SkyComposite *parent );

    /** Load skiplists from file */
    void loadContours(QString fname, QString greeting);
  
    virtual void draw( QPainter& psky );
    virtual bool selected();

protected:
    // FIXME: Implementation is broken!!
    /**@short Returns an IndexHash from the SkyMesh that contains the set
     * of trixels that cover the _SkipList_ lineList excluding skipped
     * lines as specified in the SkipList.  SkipList is a subclass of
     * LineList.
     */
    const IndexHash& getIndexHash( LineList* skipList );

    // FIXME: Implementation is broken!!
    /**@short Returns a boolean indicating whether to skip the i-th line
     * segment in the _SkipList_ skipList.  Note that SkipList is a
     * subclass of LineList.  This routine allows us to use the drawing
     * code in LineListIndex instead of repeating it all here.
     */
    virtual bool skipAt( LineList* skpiList, int i );

};
#endif
