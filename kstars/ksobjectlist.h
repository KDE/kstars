/***************************************************************************
                          ksobjectlist.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed May 12 2010
    copyright            : (C) 2010 by Victor Carbune
    email                : victor.carbune@kdemail.com
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

class KSObjectList : public QTableView
{
    public:
        KSObjectList(QWidget *parent);
};
#endif 
