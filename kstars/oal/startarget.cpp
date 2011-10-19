#include "startarget.h"

using namespace OAL;

void StarTarget::setTarget(const QString &id, const QString &datasourceObserver, bool fromDatasource, const QString &name, const QStringList &aliases,
                           const SkyPoint &pos, const QString &constellation, const QString &notes, double apparentMag, const QString &classification)
{
    ObservationTarget::setTarget(id, datasourceObserver, fromDatasource, name, aliases, pos, constellation, notes);

    m_ApparentMag = apparentMag;
    m_Classification = classification;
}
