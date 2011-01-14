/***************************************************************************
                      objectlist.h  -  K Desktop Planetarium
                             -------------------
    begin                : 
    copyright            : (C) 2010 by Victor Carbune
    email                : victor.carbune@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef OBJECTLIST_H_
#define OBJECTLIST_H_

#include <QList>
#include <QHash>
#include <QSqlQueryModel>
#include <QSqlRecord>
#include <QAbstractTableModel>

#include <kdialog.h>
#include <kio/copyjob.h>

#include "ui_objectlist.h"
#include "kstarsdatetime.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/deepskyobject.h"

class KStars;


class ObjectListUI : public QFrame, public Ui::ObjectList {
    Q_OBJECT

public:
    /**@short Cunstructor
        */
    ObjectListUI( QWidget *parent );
};

/**@class ObservingList
    *Description will follow(just changed to adjust it for object list)
    *
    *@short Tool for displaying the list of objects from the database
    *@author Victor Carbune
    *@version 1.0
    */

class ObjectList : public KDialog
{
    Q_OBJECT

public:
    /**@short Constructor
        */
    ObjectList( KStars *_ks );
    /**@short Destuctor (empty)
        */
    ~ObjectList();

public slots:
    void enqueueSearch();

    void slotSelectObject(const QModelIndex &);
    void slotNewSelection();

private:
    void drawObject(qlonglong);

    QString processSearchText();

    KStars *ks;
    ObjectListUI *ui;
    QSqlQueryModel *m_TableModel;
};

#endif // OBJECTLIST_H_
