/***************************************************************************
                          skyobjlistmodel.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/26/05
    copyright            : (C) 2012 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SKYOBJ_LISTMODEL_H
#define SKYOBJ_LISTMODEL_H

#include "qabstractitemmodel.h"
#include "skyobject.h"
#include "skyobjitem.h"

/**
 * \class SkyObjListModel
 * Represents a model for the list of interesting sky-objects to be displayed in the QML interface.
 * \author Samikshan Bairagya
 */
class SkyObjListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    /**
     * \brief Constructor
     */
    explicit SkyObjListModel(SkyObjItem *soitem = 0, QObject *parent = 0);

    /**
     * \brief Add a sky-object to the model.
     * \param sobj    Pointer to sky-object to be added.
     */
    void addSkyObject(SkyObjItem *sobj);

    /**
     * \brief Overriden method from QAbstractItemModel.
     * \return The number of items in the sky-object list model.
     */
    int rowCount(const QModelIndex& parent = QModelIndex()) const;

    /**
     * \brief Overriden method from QAbstractItemModel.
     * \return Data stored under the given role for the sky-object item referred to by the index.
     */
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;

    /**
     * \brief Get the list of sky-object items in the model.
     * \return A QList of pointers to SkyObjItems which are there in the model.
     */
    QList<SkyObjItem *> getSkyObjItems();

    /**
     * \brief Get sky-object item referred to by index.
     * \return Pointer to SkyObjItem referred to by index.
     */
    SkyObjItem *getSkyObjItem(int index);

    /**
     * \brief Erase all data in model.
     */
    void resetModel();

private:
    QList<SkyObjItem *> m_SoItemList;     ///List of sky-object items in model.
};

#endif
