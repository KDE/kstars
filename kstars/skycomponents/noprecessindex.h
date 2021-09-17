/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
