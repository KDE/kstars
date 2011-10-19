#ifndef OBSERVATIONTREEITEM_H
#define OBSERVATIONTREEITEM_H

#include <QStandardItem>
#include "observation.h"
#include "observationtarget.h"

class ObservationTreeItem : public QStandardItem
{
public:
    ObservationTreeItem(Observation *observation);
    ObservationTreeItem(ObservationTarget *target);

    bool holdsObservation() const { return m_Observation != 0; }
    Observation* getObservation() const { return m_Observation; }
    ObservationTarget* getTarget() const { return m_Target; }

private:
    Observation *m_Observation;
    ObservationTarget *m_Target;
};

#endif // OBSERVATIONTREEITEM_H
