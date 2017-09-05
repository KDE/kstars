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

#pragma once

#include "linelistindex.h"

/**
 * @class NoPrecessIndex
 *
 * @author James B. Bowlin
 * @version 0.1
 */
class NoPrecessIndex : public LineListIndex
{
  public:
    /** @short Constructor */
    NoPrecessIndex(SkyComposite *parent, const QString &name);

    //Moved to public because KStars Lite uses it
    /**
     * @ short override JITupdate so we don't perform the precession
     * correction, only rotation.
     */
    void JITupdate(LineList *lineList) override;

  protected:
    /**
     * @short we need to use the buffer that does not have the
     * reverse-precession correction.
     */
    MeshBufNum_t drawBuffer() override { return NO_PRECESS_BUF; }
};
