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
    ~ShFovExporter();

    bool exportPath(const SkyPoint &src, const SkyPoint &dest, double fov, double maglim);


private:
    SimpleFovExporter *m_FovExporter;
    SkyMap *m_Map;
    StarHopper m_StarHopper;
    PrintingWizard *m_ParentWizard;
};

#endif // SHFOVEXPORTER_H
