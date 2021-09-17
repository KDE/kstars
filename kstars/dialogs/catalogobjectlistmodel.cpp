/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "catalogobjectlistmodel.h"

CatalogObjectListModel::CatalogObjectListModel(QObject *parent,
                                               std::vector<CatalogObject> objects)
    : QAbstractTableModel{ parent }, m_objects{ std::move(objects) } {};

QVariant CatalogObjectListModel::data(const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    const auto &obj = m_objects.at(index.row());

    switch (index.column())
    {
        case 0:
            return obj.typeName();
        case 1:
            return obj.ra().toHMSString();
        case 2:
            return obj.dec().toDMSString();
        case 3:
            return obj.mag();
        case 4:
            return obj.name();
        case 5:
            return obj.longname();
        case 6:
            return obj.catalogIdentifier();
        case 7:
            return obj.a();
        case 8:
            return obj.b();
        case 9:
            return obj.pa();
        case 10:
            return obj.flux();
        default:
            return "";
    }
};

void CatalogObjectListModel::setObjects(std::vector<CatalogObject> objects)
{
    beginResetModel();
    m_objects = std::move(objects);
    emit endResetModel();
}

QVariant CatalogObjectListModel::headerData(int section, Qt::Orientation orientation,
                                            int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QVariant();

    switch (section)
    {
        case 0:
            return i18n("Type");
        case 1:
            return i18n("RA");
        case 2:
            return i18n("Dec");
        case 3:
            return i18n("Mag");
        case 4:
            return i18n("Name");
        case 5:
            return i18n("Long Name");
        case 6:
            return i18n("Identifier");
        case 7:
            return i18n("Major Axis");
        case 8:
            return i18n("Minor Axis");
        case 9:
            return i18n("Position Angle");
        case 10:
            return i18n("Flux");
        default:
            return "";
    }
};
