/*
    SPDX-FileCopyrightText: 2012 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sessionsortfilterproxymodel.h"

#include <QTime>
#include <QModelIndex>
#include <QSortFilterProxyModel>

namespace
{
inline int variantTypeId(const QVariant &v)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return v.typeId();
#else
    return v.userType();
#endif
}
}

SessionSortFilterProxyModel::SessionSortFilterProxyModel(QObject *parent) : QSortFilterProxyModel(parent)
{
}

bool SessionSortFilterProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    QVariant leftData  = sourceModel()->data(left);
    QVariant rightData = sourceModel()->data(right);
    if (variantTypeId(leftData) == QMetaType::QTime)
    {
        // We are sorting the observing time.
        return (leftData.toTime().addSecs(12 * 3600) <
                rightData.toTime().addSecs(12 * 3600)); // Note that QTime wraps, so this should work.
    }
    else
    {
        // Do default sorting for now
        // TODO: Need to add sorting by RA / Dec / magnitude etc, although these are not as important
        return QSortFilterProxyModel::lessThan(left, right);
    }
}
