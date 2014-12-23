/***************************************************************************
                          shfovexporter.h  -  K Desktop Planetarium
                             -------------------
    begin                : Thu Aug 4 2011
    copyright            : (C) 2011 by Rafał Kułaga
    email                : rl.kulaga@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SHFOVEXPORTER_H
#define SHFOVEXPORTER_H

#include "starhopper.h"
#include "ksutils.h"

class SkyMap;
class PrintingWizard;

/**
  * \class ShFovExporter
  * \brief Helper class used as a wrapper for StarHopper when capturing FOV snapshots.
  * \author Rafał Kułaga
  */
class ShFovExporter
{
public:
    /**
      * \brief Constructor.
      */
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

    SkyMap *m_Map;
    StarHopper m_StarHopper;
    SkyPoint m_Src;
    SkyPoint m_Dest;
    PrintingWizard *m_ParentWizard;
    QList<SkyObject *> m_Path;
    QList<SkyObject *> *m_skyObjList;
};

#endif // SHFOVEXPORTER_H
