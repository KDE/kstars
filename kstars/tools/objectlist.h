/***************************************************************************
                          objectlist.h  -  K Desktop Planetarium
                             -------------------
    begin                : 29 Nov 2004
    copyright            : (C) 2004 by Jeff Woods, Jason Harris
    email                : jcwoods@bellsouth.net, jharris@30doradus.org
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
#include <QSqlQueryModel>
#include <QAbstractTableModel>

#include <kdialog.h>
#include <kio/copyjob.h>

#include "ui_objectlist.h"
#include "kstarsdatetime.h"
#include "skyobjects/skyobject.h"

class KStars;


class ObjectListUI : public QFrame, public Ui::ObjectList {
    Q_OBJECT

public:
    /**@short Cunstructor
        */
    ObjectListUI( QWidget *parent );
};

/**@class ObservingList
    *Tool window for managing a custom list of objects.  The window
    *displays the Name, RA, Dec, mag, and type of each object in the list.
    *
    *By selecting an object in the list, you can perform a number of functions
    *on that object:
    *+ Center it in the display 
    *+ Examine its Details Window 
    *+ Point the telescope at it 
    *+ Attach a custom icon or name label (TBD)
    *+ Attach a trail (solar system only) (TBD)
    *+ Open the AltVsTime tool 
    *
    *The user can also save/load their observing lists, and can export 
    *list data (TBD: as HTML table?  CSV format?  plain text?)
    *
    *The observing notes associated with the selected object are displayed 
    *below the list.
    *
    *TODO: 
    *+ Implement a "shaded" state, in which the UI is compressed to
    *  make it easier to float on the KStars window.  Displays only
    *  object names, and single-letter action buttons, and no user log.
    *+ Implement an InfoBox version (the ultimate shaded state)
    *
    *@short Tool for managing a custom list of objects
    *@author Jeff Woods, Jason Harris
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
    /*
    void slotOALExport(); 
    
    void slotAddVisibleObj();
    */
    void selectObject(const QModelIndex &);

private:
    QString processSearchText();

    KStars *ks;
    ObjectListUI *ui;
    QSqlQueryModel *m_TableModel;
};

#endif // OBJECTLIST_H_
