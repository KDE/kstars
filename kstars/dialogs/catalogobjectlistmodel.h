/***************************************************************************
             catalogobjectlistmodel.h  -  K Desktop Planetarium
                             -------------------
    begin                : Jun 2021
    copyright            : (C) 2021 by Valentin Boettcher, Jason Harris
    email                : hiro at protagon.space; @hiro:tchncs.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

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
