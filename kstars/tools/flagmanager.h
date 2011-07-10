/***************************************************************************
                          flagmanager.h  -  Flag manager
                             -------------------
    begin                : Mon Feb 01 2009
    copyright            : (C) 2009 by Jerome SONRIER
    email                : jsid@emor3j.fr.eu.org
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef FLAGMANAGER_H_
#define FLAGMANAGER_H_

#include <QAbstractTableModel>

#include <kdialog.h>

#include "ui_flagmanager.h"

class QSortFilterProxyModel;
class QStandardItemModel;
class KStars;

class FlagManagerUI : public QFrame, public Ui::FlagManager
{
    Q_OBJECT

    public:
    /**@short Constructor
     */
        FlagManagerUI( QWidget *parent );
};


/**
 *@class FlagManager
 *@short Flag manager
 *Dialog box to add and remove flags
 *
 *@version 1.1
 *@author Jerome SONRIER
 */
class FlagManager : public KDialog
{
    Q_OBJECT
public:
    /**
     *@short Constructor.
     */
    FlagManager( QWidget *ks );

    /**
     *@short Destructor.
     */
    ~FlagManager();

    void setRaDec( const dms &ra, const dms &dec );
    void clearFields ();
    void showFlag( const int flagIdx );

    bool validatePoint();

    void deleteFlagItem ( int flagIdx );

public slots:
    /**
     *@short Verify coordinates and add a flag
     */
    void slotAddFlag();

    /**
     *@short Delete a flag
     */
    void slotDeleteFlag();

    /**
     *@short Center the selected object in the display
     */
    void slotCenterFlag();

private slots:
    void slotSaveChanges();
    void slotSetShownFlag( QModelIndex idx );

private:
    void insertFlag( bool isNew, int row = 0 );

    KStars *m_Ks;
    FlagManagerUI *ui;
    QStandardItemModel *m_Model;
    QSortFilterProxyModel *m_SortModel;
};

#endif
