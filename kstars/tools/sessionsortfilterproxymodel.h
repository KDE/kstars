/**************************************************************************
        sessionsortfilterproxymodel.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sat Apr 14 2012
    copyright            : (C) 2012 by Akarsh Simha
    email                : akarsh.simha@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include<QSortFilterProxyModel>

class QModelIndex;

/**
 *@class SessionSortFilterProxyModel
 *@short Sort best observation times by reimplementing lessThan() to work on the transit times of objects
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
    SessionSortFilterProxyModel( QObject *parent = 0 );

 protected:
    bool lessThan( const QModelIndex &left, const QModelIndex &right ) const;

};
