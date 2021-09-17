/*
    SPDX-FileCopyrightText: 2012 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QSortFilterProxyModel>

class QModelIndex;

/**
 * @class SessionSortFilterProxyModel
 * @short Sort best observation times by reimplementing lessThan() to work on the transit times of objects
 *
 * Any observing session starts at about sunset (~ 6 PM local time)
 * and goes on till sunrise (~ 6 AM local time). Thus, the correct
 * order to view objects in is to view those with meridian transit
 * times just after 12 noon local time first, working towards those
 * that transit in the evening, and finishing the ones that have
 * meridian transits just before 12 noon at the end of the
 * session. So, the observing session list should be sorted in a
 * special manner when sorting by time.  This class reimplements
 * lessThan() in QSortFilterProxyModel to obtain the required sorting.
 */
class SessionSortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT;

  public:
    explicit SessionSortFilterProxyModel(QObject *parent = nullptr);

  protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
};
