/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Options.h"

#include "shfovexporter.h"
#include "starhopper.h"
#include "targetlistcomponent.h"
#include "kstarsdata.h"
#include "skymapcomposite.h"
#include "skymap.h"
#include "printingwizard.h"

ShFovExporter::ShFovExporter(PrintingWizard *wizard, SkyMap *map) : m_Map(map), m_ParentWizard(wizard)
{
}

bool ShFovExporter::calculatePath(const SkyPoint &src, const SkyPoint &dest, double fov, double maglim)
{
    m_Src  = src;
    m_Dest = dest;

    auto list = m_StarHopper.computePath(src, dest, fov, maglim);
    m_skyObjList = KSUtils::castStarObjListToSkyObjList(list);
    delete list;

    m_Path = *m_skyObjList;
    if (m_Path.isEmpty())
    {
        return false;
    }

    return true;
}

bool ShFovExporter::exportPath()
{
    KStarsData::Instance()->clock()->stop();

    if (m_Path.isEmpty())
    {
        return false;
    }

    // Show path on SkyMap
    TargetListComponent *t = KStarsData::Instance()->skyComposite()->getStarHopRouteList();

    t->list.reset(m_skyObjList);

    // Update SkyMap now
    m_Map->forceUpdate(true);

    // Capture FOV snapshots
    centerBetweenAndCapture(m_Src, *m_Path.at(0));
    for (int i = 0; i < m_Path.size() - 1; i++)
    {
        centerBetweenAndCapture(*m_Path.at(i), *m_Path.at(i + 1));
    }
    centerBetweenAndCapture(*m_Path.last(), m_Dest);

    return true;
}

void ShFovExporter::centerBetweenAndCapture(const SkyPoint &ptA, const SkyPoint &ptB)
{
    // Calculate RA and Dec coordinates of point between ptA and ptB
    dms ra(ptA.ra().Degrees() + 0.5 * (ptB.ra().Degrees() - ptA.ra().Degrees()));
    dms dec(ptA.dec().Degrees() + 0.5 * (ptB.dec().Degrees() - ptA.dec().Degrees()));
    SkyPoint between(ra, dec);

    // Center SkyMap
    m_Map->setClickedPoint(&between);
    m_Map->slotCenter();

    // Capture FOV snapshot
    m_ParentWizard->captureFov();
}
