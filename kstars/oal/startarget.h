#ifndef STARTARGET_H
#define STARTARGET_H

#include "oal.h"
#include "observationtarget.h"

using namespace OAL;

class OAL::StarTarget : public ObservationTarget
{
public:
    StarTarget(const QString &id, const QString &datasourceObserver, bool fromDatasource, const QString &name,
               const QStringList &aliases, const SkyPoint &pos, const QString &constellation, const QString &notes, const double apparentMag,
               const QString &classification) : ObservationTarget(id, datasourceObserver, fromDatasource, name, aliases, pos, constellation, notes)
    {
        m_ApparentMag = apparentMag;
        m_Classification = classification;
    }

    double apparentMag() const { return m_ApparentMag; }
    QString classification() const { return m_Classification; }

    void setTarget(const QString &id, const QString &datasourceObserver, const bool fromDatasource, const QString &name,
                   const QStringList &aliases, const SkyPoint &pos, const QString &constellation, const QString &notes, const double apparentMag,
                   const QString &classification);

private:
    double m_ApparentMag;
    QString m_Classification;
};

#endif // STARTARGET_H
