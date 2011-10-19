#include "observationtarget.h"

using namespace OAL;

void ObservationTarget::setTarget(const QString &id, const QString &datasourceObserver, bool fromDatasource, const QString &name,
                                  const QStringList &aliases, const SkyPoint &pos, const QString &constellation, const QString &notes)
{
    m_Id = id;

    m_FromDatasource = fromDatasource;
    if(m_FromDatasource) {
        m_Datasource = datasourceObserver;
    } else {
        m_Observer = datasourceObserver;
    }

    m_Name = name;
    m_Aliases = aliases;
    m_Position = pos;
    m_Constellation = constellation;
    m_Notes = notes;
}

void ObservationTarget::setTarget(const QString &id, SkyObject *obj)
{

}
