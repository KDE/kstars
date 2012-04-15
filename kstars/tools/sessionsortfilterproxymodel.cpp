/**************************************************************************
        sessionsortfilterproxymodel.cpp  -  K Desktop Planetarium
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

#include "sessionsortfilterproxymodel.h"

#include <QTime>
#include <QModelIndex>
#include <QSortFilterProxyModel>

SessionSortFilterProxyModel::SessionSortFilterProxyModel( QObject *parent )
    : QSortFilterProxyModel( parent ) {
}

bool SessionSortFilterProxyModel::lessThan( const QModelIndex &left, const QModelIndex &right ) const {
    QVariant leftData = sourceModel()->data( left );
    QVariant rightData = sourceModel()->data( right );
    if( leftData.type() == QVariant::Time ) {
        // We are sorting the observing time.
        return( leftData.toTime().addSecs( 12 * 3600 ) < rightData.toTime().addSecs( 12 * 3600 ) ); // Note that QTime wraps, so this should work.
    }
    else {
        // Do default sorting for now
        // TODO: Need to add sorting by RA / Dec / magnitude etc, although these are not as important
        return QSortFilterProxyModel::lessThan( left, right );
    }
}
