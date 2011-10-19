#ifndef OBSERVATIONTARGET_H
#define OBSERVATIONTARGET_H

#include "oal.h"

#include "QStringList"
#include "QList"
#include "skypoint.h"

class SkyObject;

using namespace OAL;

class OAL::ObservationTarget
{
public:
    ObservationTarget(const QString &id, const QString &datasourceObserver, bool fromDatasource, const QString &name,
                      const QStringList &aliases, const SkyPoint &pos, const QString &constellation, const QString &notes)
    {
        setTarget(id, datasourceObserver, fromDatasource, name, aliases, pos, constellation, notes);
    }

    ObservationTarget( const QString &id, SkyObject *obj )
    {
        setTarget(id, obj);
    }

    QString id() const { return m_Id; }
    bool fromDatasource() const { return m_FromDatasource; }
    QString datasource() const { return m_Datasource; }
    QString observer() const { return m_Observer; }
    QString name() const { return m_Name; }
    SkyPoint position() const { return m_Position; }
    QStringList aliases() const { return m_Aliases; }
    QString constellation() const { return m_Constellation; }
    QString notes() const { return m_Notes; }

    void setTarget(const QString &id, const QString &datasourceObserver, const bool fromDatasource, const QString &name,
                   const QStringList &aliases, const SkyPoint &pos, const QString &constellation, const QString &notes);
    void setTarget(const QString &id, SkyObject *obj);

    QList<Observation *>* observationsList() { return &m_ObservationsList; }

private:
    QString m_Id;
    bool m_FromDatasource;
    QString m_Datasource;
    QString m_Observer;
    QString m_Name;
    QStringList m_Aliases;
    SkyPoint m_Position;
    QString m_Constellation;
    QString m_Notes;

    QList<Observation *> m_ObservationsList;
};

#endif // OBSERVATIONTARGET_H
