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

/** @class SkyObjectListModel
 * A model used in Find Object Dialog in QML. Each entry is a QString (name of object) and pointer to
 * SkyObject itself
 *
 * @short Model that is used in Find Object Dialog
 * @author Artem Fedoskin, Jason Harris
 * @version 1.0
 */
class SkyObjectListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum DemoRoles {
        SkyObjectRole = Qt::UserRole + 1,
    };

    explicit SkyObjectListModel(QObject *parent = 0);

    virtual int rowCount(const QModelIndex&) const { return skyObjects.size(); }
    virtual QVariant data(const QModelIndex &index, int role) const;

    virtual QHash<int, QByteArray> roleNames() const;

    /**
     * @return index of object from skyObjects with name objectName. -1 if object with such
     * name was not found
     */
    int indexOf(QString objectName) const;

    /**
     * @short filter
     * @param regEx
     * @return
     */
    QStringList filter(QRegExp regEx);

    void setSkyObjectsList(QVector<QPair<QString, const SkyObject *>> sObjects);

private:
    QVector<QPair<QString, const SkyObject *>> skyObjects;
};

#endif
