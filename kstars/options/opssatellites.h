/*
    SPDX-FileCopyrightText: 2011 Jérôme SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
 *
 * The Satellites Tab of the Options window.  In this Tab the user can configure
 * satellites options and select satellites that should be draw
 *
 * @author Jérôme SONRIER
 * @version 1.0
 */
class OpsSatellites : public QFrame, public Ui::OpsSatellites
{
    Q_OBJECT

  public:
    /** Constructor */
    OpsSatellites();

    virtual ~OpsSatellites() override = default;

  private:
    /** Refresh satellites list */
    void updateListView();

    /**
     * @brief saveSatellitesList Saves list of checked satellites in the configuration file
     */
    void saveSatellitesList();

  private slots:
    void slotUpdateTLEs();
    void slotShowSatellites(bool on);
    void slotApply();
    void slotCancel();
    void slotFilterReg(const QString &);
    void slotItemChanged(QStandardItem *);

private:
  KConfigDialog *m_ConfigDialog { nullptr };
  QStandardItemModel *m_Model { nullptr };
  QSortFilterProxyModel *m_SortModel { nullptr };
  bool isDirty { false };
};
