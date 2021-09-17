/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher Jason Harris <hiro at protagon.space; @hiro:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include <QAbstractTableModel>
#include "catalogobject.h"

class CatalogObjectListModel : public QAbstractTableModel
{
    Q_OBJECT
  public:
    CatalogObjectListModel(QObject *parent                    = nullptr,
                           std::vector<CatalogObject> objects = {});

    inline int rowCount(const QModelIndex & = QModelIndex()) const override
    {
        return m_objects.size();
    };

    inline int columnCount(const QModelIndex & = QModelIndex()) const override
    {
        return 11;
    };

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setObjects(std::vector<CatalogObject> objects);

    inline const CatalogObject &getObject(const QModelIndex &index) const
    {
        return m_objects.at(index.row());
    };

  private:
    std::vector<CatalogObject> m_objects;
};
