/***************************************************************************
                        noprecessindex.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-08-04
    copyright            : (C) 2007 James B. Bowlin
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

#ifndef NO_PRECESS_INDEX_H
#define NO_PRECESS_INDEX_H

#include "linelistindex.h"

/**@class NoPrecessIndex
 *
 * @author James B. Bowlin
 * @version 0.1
 */
class NoPrecessIndex : public LineListIndex
{
public:
    /* @short Constructor */
    NoPrecessIndex( SkyComposite *parent, const QString& name );

protected:
    /* @ short override JITupdate so we don't perform the precession
     * correction, only rotation.
     */
    virtual void JITupdate( LineList* lineList );

    /* @short we need to use the buffer that does not have the
     * reverse-precession correction.
     */
    virtual MeshBufNum_t drawBuffer() { return NO_PRECESS_BUF; }
};


#endif
