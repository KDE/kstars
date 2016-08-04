/***************************************************************************
                          skyobjectlistmodel.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Jul 29 2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SKYOBJECTLISTMODEL_H_
#define SKYOBJECTLISTMODEL_H_

#include <QAbstractListModel>
#include <QDebug>

class SkyObject;

/** @class FindDialogLite
 * A backend of dialog decleared in QML.
 *
 * @short Backend for Find Object Dialog in QML
 * @author Artem Fedoskin, Jason Harris
 * @version 1.0
 */
class SkyObjectListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum DemoRoles {
        NameRole = Qt::UserRole + 1,
        SkyObjectRole
    };

    explicit SkyObjectListModel(QObject *parent = 0);

    virtual int rowCount(const QModelIndex&) const { return skyObjects.size(); }
    virtual QVariant data(const QModelIndex &index, int role) const;

    virtual QHash<int, QByteArray> roleNames() const;

    /*virtual bool removeRows(int row, int count, const QModelIndex &parent) {
        qDebug() << "TEST";
        return false;
    }*/

    void setSkyObjectsList(QVector<QPair<QString, const SkyObject *>> sObjects);

private:
    QVector<QPair<QString, const SkyObject *>> skyObjects;
};

#endif
