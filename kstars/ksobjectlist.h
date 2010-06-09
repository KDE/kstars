/***************************************************************************
                          ksobjectlist.h  -  K Desktop Planetarium
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

#ifndef KSOBJECTLIST_H_
#define KSOBJECTLIST_H_

#include <QTableView>
#include <QWidget>
#include <QPoint>

#include "objlistpopupmenu.h"

class KSObjectList : public QTableView
{
    Q_OBJECT

public:
    KSObjectList(QWidget *parent);

public slots:
    void slotContextMenu(const QPoint &pos);
    void slotNewSelection();

private:
    ObjListPopupMenu *pmenu;
    bool singleSelection, noSelection;
};
#endif 
