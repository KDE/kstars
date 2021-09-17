/*
    SPDX-FileCopyrightText: 2012 Samikshan Bairagya <samikshan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "qabstractitemmodel.h"

class SkyObjItem;

/**
 * @class SkyObjListModel
 * Represents a model for the list of interesting sky-objects to be displayed in the QML interface.
 *
 * @author Samikshan Bairagya
 */
class SkyObjListModel : public QAbstractListModel
{
    Q_OBJECT
  public:
    /** Constructor */
    explicit SkyObjListModel(SkyObjItem *soitem = nullptr, QObject *parent = nullptr);

    /**
     * @brief Add a sky-object to the model.
     * @param sobj
     * Pointer to sky-object to be added.
     */
    void addSkyObject(SkyObjItem *sobj);

    /**
     * @brief Create and return a QHash<int, QByteArray> of rolenames for the SkyObjItem.
     * @return QHash<int, QByteArray> of rolenames for the SkyObjItem.
     */
    QHash<int, QByteArray> roleNames() const override;

    /**
     * @brief Overridden method from QAbstractItemModel.
     * @return The number of items in the sky-object list model.
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief Overridden method from QAbstractItemModel.
     * @return Data stored under the given role for the sky-object item referred to by the index.
     */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /**
     * @brief Get the list of sky-object items in the model.
     * @return A QList of pointers to SkyObjItems which are there in the model.
     */
    QList<SkyObjItem *> getSkyObjItems();

    /**
     * @brief Get sky-object item referred to by index.
     * @return Pointer to SkyObjItem referred to by index.
     */
    SkyObjItem *getSkyObjItem(int index);

    /** Erase all data in model. */
    void resetModel();

    int getSkyObjIndex(SkyObjItem *item);

  private:
    QList<SkyObjItem *> m_SoItemList; ///List of sky-object items in model.
};
