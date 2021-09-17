/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skypoint.h"
#include "starhopper.h"

class PrintingWizard;
class SkyMap;

/**
 * \class ShFovExporter
 * \brief Helper class used as a wrapper for StarHopper when capturing FOV snapshots.
 *
 * \author Rafał Kułaga
 */
class ShFovExporter
{
  public:
    /** Constructor */
    ShFovExporter(PrintingWizard *wizard, SkyMap *map);

    /**
     * \brief Calculate path between source and destination SkyPoints.
     * \param src SkyPoint at which StarHopper will begin.
     * \param dest SkyPoint at which StarHopper will end.
     * \param fov Star hopping field of view angle (in deg).
     * \param maglim Magnitude limit.
     * \return True if path has been found.
     */
    bool calculatePath(const SkyPoint &src, const SkyPoint &dest, double fov, double maglim);

    /**
     * \brief Export FOV snapshots across calculated path.
     * \return False if path is empty.
     * \note You should call ShFovExporter::calculatePath() before calling this method.
     */
    bool exportPath();

  private:
    /**
     * \brief Private method: center SkyMap between two SkyPoints and capture FOV snapshot.
     * \param ptA Beginning point.
     * \param ptB Ending point.
     */
    void centerBetweenAndCapture(const SkyPoint &ptA, const SkyPoint &ptB);

    SkyMap *m_Map { nullptr };
    StarHopper m_StarHopper;
    SkyPoint m_Src;
    SkyPoint m_Dest;
    PrintingWizard *m_ParentWizard { nullptr };
    QList<SkyObject *> m_Path;
    QList<SkyObject *> *m_skyObjList { nullptr };
};
