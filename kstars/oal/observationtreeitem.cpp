#include "observationtreeitem.h"

ObservationTreeItem::ObservationTreeItem(Observation *observation)
    : QStandardItem("Observation"), m_Observation(observation), m_Target(0)
{}

ObservationTreeItem::ObservationTreeItem(ObservationTarget *target)
    : QStandardItem(target->name()), m_Observation(0), m_Target(target)
{}
