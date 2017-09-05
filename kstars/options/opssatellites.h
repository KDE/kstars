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

#pragma once

#include "ui_opssatellites.h"

#include <kconfigdialog.h>

#include <QFrame>
#include <QSortFilterProxyModel>

class QStandardItem;
class QStandardItemModel;
class KStars;

class SatelliteSortFilterProxyModel : public QSortFilterProxyModel
{
  Q_OBJECT

  public:
    explicit SatelliteSortFilterProxyModel(QObject *parent);
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
};

/**
 * @class OpsSatellites
 * The Satellites Tab of the Options window.  In this Tab the user can configure
 * satellites options and select satellites that should be draw
 * @author Jérôme SONRIER
 * @version 1.0
 */
class OpsSatellites : public QFrame, public Ui::OpsSatellites
{
    Q_OBJECT

  public:
    /** Constructor */
    OpsSatellites();

    /** Destructor */
    ~OpsSatellites() override;

  private:
    /** Refresh satellites list */
    void updateListView();

    /**
     * @brief saveSatellitesList Saves list of checked satellites in the configuration file
     */
    void saveSatellitesList();

    KConfigDialog *m_ConfigDialog;
    QStandardItemModel *m_Model;
    QSortFilterProxyModel *m_SortModel;
    bool isDirty = false;

  private slots:
    void slotUpdateTLEs();
    void slotShowSatellites(bool on);
    void slotApply();
    void slotCancel();
    void slotFilterReg(const QString &);
    void slotItemChanged(QStandardItem *);
};
