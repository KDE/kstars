#include "shfovexporter.h"
#include "starhopper.h"
#include "targetlistcomponent.h"
#include "kstars/kstarsdata.h"
#include "skymapcomposite.h"
#include "kstars/skymap.h"
#include "skypoint.h"
#include "printingwizard.h"
#include "Options.h"

ShFovExporter::ShFovExporter(SimpleFovExporter *exporter, SkyMap *map, PrintingWizard *wizard) : m_FovExporter(exporter), m_Map(map), m_ParentWizard(wizard)
{}

ShFovExporter::~ShFovExporter()
{}

bool ShFovExporter::exportPath(const SkyPoint &src, const SkyPoint &dest, double fov, double maglim)
{
    KStarsData::Instance()->clock()->stop();

    QList<StarObject const *> path = m_StarHopper.computePath(src, dest, fov, maglim);
    if(path.isEmpty())
    {
        return false;
    }

    QList<SkyObject *> *mutablestarlist = new QList<SkyObject *>();
    foreach( const StarObject *conststar, path ) {
        StarObject *mutablestar = const_cast<StarObject *>(conststar);
        mutablestarlist->append( mutablestar );
    }

    TargetListComponent *t = KStarsData::Instance()->skyComposite()->getStarHopRouteList();
    delete t->list;
    t->list = mutablestarlist;
    m_Map->forceUpdate(true);


    double new_ra = src.ra().Degrees() + 0.5 * (path.at(0)->ra().Degrees() - src.ra().Degrees());
    double new_dec = src.dec().Degrees() + 0.5 * (path.at(0)->dec().Degrees() - src.dec().Degrees());
    dms ra(new_ra);
    dms dec(new_dec);
    SkyPoint pt(ra, dec);
    m_Map->setClickedPoint(&pt);
    m_Map->slotCenter();
    m_ParentWizard->captureFov();

    for(int i = 0 ; i < path.size() - 1; i++)
    {
        double new_ra = path.at(i)->ra().Degrees() + 0.5 * (path.at(i+1)->ra().Degrees() - path.at(i)->ra().Degrees());
        double new_dec = path.at(i)->dec().Degrees() + 0.5 * (path.at(i+1)->dec().Degrees() - path.at(i)->dec().Degrees());
        dms ra(new_ra);
        dms dec(new_dec);
        SkyPoint pt(ra, dec);
        m_Map->setClickedPoint(&pt);
        m_Map->slotCenter();
        m_ParentWizard->captureFov();
    }
}



