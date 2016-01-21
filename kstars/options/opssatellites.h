/***************************************************************************
                          opssatellites.h  -  K Desktop Planetarium
                             -------------------
    begin                : Mon 21 Mar 2011
    copyright            : (C) 2011 by Jérôme SONRIER
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

#ifndef OPSSATELLITES_H_
#define OPSSATELLITES_H_

#include "ui_opssatellites.h"

#include <QStandardItemModel>
#include <QSortFilterProxyModel>

#include <kconfigdialog.h>

class KStars;


class SatelliteSortFilterProxyModel : public QSortFilterProxyModel
{
public:
    explicit SatelliteSortFilterProxyModel( QObject* parent );
    bool filterAcceptsRow( int sourceRow, const QModelIndex &sourceParent ) const;
};

/** @class OpsSatellites
 *The Satellites Tab of the Options window.  In this Tab the user can configure
 *satellites options and select satellites that should be draw 
 *@author Jérôme SONRIER
 *@version 1.0
 */
class OpsSatellites : public QFrame, public Ui::OpsSatellites
{
    Q_OBJECT

public:
    /**
     * Constructor
     */
    explicit OpsSatellites();

    /**
     * Destructor
     */
    ~OpsSatellites();
    
private:
    /**
     * Refresh satellites list
     */
    void updateListView();
    
    KConfigDialog *m_ConfigDialog;
    QStandardItemModel *m_Model;
    QSortFilterProxyModel *m_SortModel;

private slots:
    void slotUpdateTLEs();
    void slotShowSatellites( bool on );
    void slotApply();
    void slotCancel();
    void slotFilterReg( const QString& );
    void slotItemChanged( QStandardItem* );
};

#endif  //OPSSATELLITES_H_
