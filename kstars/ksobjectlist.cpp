/***************************************************************************
                          ksobjectlist.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed June 8 2010
    copyright            : (C) 2010 by Victor Carbune
    email                : victor.carbune@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QDebug>
#include <QObject>
#include <QItemSelectionModel>
#include "ksobjectlist.h"
KSObjectList::KSObjectList(QWidget *parent):QTableView(parent)
{
    singleSelection = false;
    noSelection = true;

    setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(slotContextMenu(const QPoint &)));

    pmenu = new ObjListPopupMenu();
}

void KSObjectList::slotContextMenu(const QPoint &pos)
{
    int countRows = selectionModel()->selectedRows().count();
    qDebug() << countRows;
    if (countRows == 1) {
        pmenu->initPopupMenu( true, true, true, true, true, true );
        pmenu->popup(QWidget::mapToGlobal(pos));
    }
}

void KSObjectList::slotNewSelection() {
    singleSelection = false;
    noSelection = false;

    QModelIndexList selectedItems;
    QString newName;
    SkyObject *o;

    selectedItems = (selectionModel()->selection()).indexes();
    qDebug() << "New Selection works!";
/*
    //When one object is selected
    if (selectedItems.size() == selectionModel()->columnCount()) {
        newName = selectedItems[0].data().toString();
        singleSelection = true;

        qDebug() << newName;
    }

    if( singleSelection ) {
        qDebug() << "Single selection";

    } else if ( selectedItems.size() == 0 ) {//Nothing selected
        noSelection = true;
        qDebug() << "No selection has been made";
    } else { //more than one object selected.
        qDebug() << "A lot of objects were selected";
    }
    */
}
