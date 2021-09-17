/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QAbstractListModel>
#include <QDebug>

class SkyObject;

/**
 * @class SkyObjectListModel
 * A model used in Find Object Dialog in QML. Each entry is a QString (name of object) and pointer to
 * SkyObject itself
 *
 * @short Model that is used in Find Object Dialog
 * @author Artem Fedoskin, Jason Harris
 * @version 1.0
 */
class SkyObjectListModel : public QAbstractListModel
{
    Q_OBJECT
  public:
    enum DemoRoles
    {
        SkyObjectRole = Qt::UserRole + 1,
    };

    explicit SkyObjectListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &) const override { return skyObjects.size(); }
    QVariant data(const QModelIndex &index, int role) const override;

    QHash<int, QByteArray> roleNames() const override;

    /**
     * @return index of object from skyObjects with name objectName. -1 if object with such
     * name was not found
     */
    int indexOf(const QString &objectName) const;

    /**
     * @short Filter the model
     * @param regEx Regex
     * @return Filtered string list
     */
    QStringList filter(const QRegExp &regEx);

    void setSkyObjectsList(QVector<QPair<QString, const SkyObject *>> sObjects);

  public slots:
    void removeSkyObject(SkyObject *object);

  private:
    QVector<QPair<QString, const SkyObject *>> skyObjects;
};
