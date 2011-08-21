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

class SkyMap;
class SimpleFovExporter;
class PrintingWizard;

class ShFovExporter
{
public:
    ShFovExporter(SimpleFovExporter *exporter, SkyMap *map, PrintingWizard *wizard);

    bool exportPath(const SkyPoint &src, const SkyPoint &dest, double fov, double maglim);

private:
    SimpleFovExporter *m_FovExporter;
    SkyMap *m_Map;
    StarHopper m_StarHopper;
    PrintingWizard *m_ParentWizard;
};

#endif // SHFOVEXPORTER_H
