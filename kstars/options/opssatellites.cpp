/*
    SPDX-FileCopyrightText: 2011 Jérôme SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opssatellites.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "satellite.h"
#include "skymapcomposite.h"
#include "skycomponents/satellitescomponent.h"
#include "skymap.h"

#include <QStandardItemModel>
#include <QStatusBar>

static const char *satgroup_strings_context = "Satellite group name";

SatelliteSortFilterProxyModel::SatelliteSortFilterProxyModel(QObject *parent) : QSortFilterProxyModel(parent)
{
}

bool SatelliteSortFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

    if (sourceModel()->hasChildren(index))
    {
        for (int i = 0; i < sourceModel()->rowCount(index); ++i)
            if (filterAcceptsRow(i, index))
                return true;
        return false;
    }

    return sourceModel()->data(index).toString().contains(filterRegExp());
}

OpsSatellites::OpsSatellites() : QFrame(KStars::Instance())
{
    setupUi(this);

    m_ConfigDialog = KConfigDialog::exists("settings");

    //Set up the Table Views
    m_Model     = new QStandardItemModel(0, 1, this);
    m_SortModel = new SatelliteSortFilterProxyModel(this);
    m_SortModel->setSourceModel(m_Model);
    SatListTreeView->setModel(m_SortModel);
    SatListTreeView->setEditTriggers(QTreeView::NoEditTriggers);
    SatListTreeView->setSortingEnabled(false);

    // Populate satellites list
    updateListView();

    // Signals and slots connections
    connect(UpdateTLEButton, SIGNAL(clicked()), this, SLOT(slotUpdateTLEs()));
    connect(kcfg_ShowSatellites, SIGNAL(toggled(bool)), SLOT(slotShowSatellites(bool)));
    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), SLOT(slotCancel()));
    connect(FilterEdit, SIGNAL(textChanged(QString)), this, SLOT(slotFilterReg(QString)));
    connect(m_Model, SIGNAL(itemChanged(QStandardItem*)), this, SLOT(slotItemChanged(QStandardItem*)));

    connect(satelliteButtonGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonPressed), this,
            [&]() { isDirty = true; });

    isDirty = false;
}

void OpsSatellites::slotUpdateTLEs()
{
    // Save existing satellites
    saveSatellitesList();

    // Get new data files
    KStarsData::Instance()->skyComposite()->satellites()->updateTLEs();

    isDirty = true;

    // Refresh satellites list
    updateListView();
}

void OpsSatellites::updateListView()
{
    KStarsData *data = KStarsData::Instance();

    // Clear satellites list
    m_Model->clear();
    SatListTreeView->reset();

    m_Model->setHorizontalHeaderLabels(QStringList(i18n("Satellite Name")));

    // Add each groups and satellites in the list
    foreach (SatelliteGroup *sat_group, data->skyComposite()->satellites()->groups())
    {
        QStandardItem *group_item;
        QStandardItem *sat_item;
        bool all_sat_checked   = true;
        bool all_sat_unchecked = true;

        // Add the group
        group_item = new QStandardItem(i18nc(satgroup_strings_context, sat_group->name().toUtf8()));
        group_item->setCheckable(true);
        m_Model->appendRow(group_item);

        // Add all satellites of the group
        for (int i = 0; i < sat_group->count(); ++i)
        {
            sat_item = new QStandardItem(sat_group->at(i)->name());
            sat_item->setCheckable(true);
            if (Options::selectedSatellites().contains(sat_group->at(i)->name()))
            {
                sat_item->setCheckState(Qt::Checked);
                all_sat_unchecked = false;
            }
            else
                all_sat_checked = false;
            group_item->setChild(i, sat_item);
        }

        // If all satellites of the group are selected, select the group
        if (all_sat_checked)
            group_item->setCheckState(Qt::Checked);
        else if (all_sat_unchecked)
            group_item->setCheckState(Qt::Unchecked);
        else
            group_item->setCheckState(Qt::PartiallyChecked);
    }
}

void OpsSatellites::saveSatellitesList()
{
    KStarsData *data = KStarsData::Instance();
    QString sat_name;
    QStringList selected_satellites;
    QModelIndex group_index, sat_index;
    QStandardItem *group_item;
    QStandardItem *sat_item;

    // Retrieve each satellite in the list and select it if checkbox is checked
    for (int i = 0; i < m_Model->rowCount(SatListTreeView->rootIndex()); ++i)
    {
        group_index = m_Model->index(i, 0, SatListTreeView->rootIndex());
        group_item  = m_Model->itemFromIndex(group_index);

        for (int j = 0; j < m_Model->rowCount(group_item->index()); ++j)
        {
            sat_index = m_Model->index(j, 0, group_index);
            sat_item  = m_Model->itemFromIndex(sat_index);
            sat_name  = sat_item->data(0).toString();

            Satellite *sat = data->skyComposite()->satellites()->findSatellite(sat_name);
            if (sat)
            {
                if (sat_item->checkState() == Qt::Checked)
                {
                    int rc = sat->updatePos();
                    // If position calculation fails, unselect it
                    if (rc == 0)
                    {
                        sat->setSelected(true);
                        selected_satellites.append(sat_name);
                    }
                    else
                    {
                        KStars::Instance()->statusBar()->showMessage(
                            i18n("%1 position calculation error: %2.", sat->name(), sat->sgp4ErrorString(rc)), 0);
                        sat->setSelected(false);
                        sat_item->setCheckState(Qt::Unchecked);
                    }
                }
                else
                {
                    sat->setSelected(false);
                }
            }
        }
    }

    Options::setSelectedSatellites(selected_satellites);
}

void OpsSatellites::slotApply()
{
    if (isDirty == false)
        return;

    isDirty = false;

    saveSatellitesList();

    // update time for all objects because they might be not initialized
    // it's needed when using horizontal coordinates
    KStars::Instance()->data()->setFullTimeUpdate();
    KStars::Instance()->updateTime();
    KStars::Instance()->map()->forceUpdate();
}

void OpsSatellites::slotCancel()
{
    // Update satellites list
    updateListView();
}

void OpsSatellites::slotShowSatellites(bool on)
{
    isDirty = true;
    kcfg_ShowVisibleSatellites->setEnabled(on);
    kcfg_ShowSatellitesLabels->setEnabled(on);
    kcfg_DrawSatellitesLikeStars->setEnabled(on);
}

void OpsSatellites::slotFilterReg(const QString &filter)
{
    m_SortModel->setFilterRegExp(QRegExp(filter, Qt::CaseInsensitive, QRegExp::RegExp));
    m_SortModel->setFilterKeyColumn(-1);

    isDirty = true;

    // Expand all categories when the user use filter
    if (filter.length() > 0)
        SatListTreeView->expandAll();
    else
        SatListTreeView->collapseAll();
}

void OpsSatellites::slotItemChanged(QStandardItem *item)
{
    if (item->parent() == nullptr && !item->hasChildren())
    {
        return;
    }

    isDirty = true;

    QModelIndex sat_index;
    QStandardItem *sat_item;

    disconnect(m_Model, SIGNAL(itemChanged(QStandardItem*)), this, SLOT(slotItemChanged(QStandardItem*)));

    // If a group has been (un)checked, (un)check all satellites of the group
    // else a satellite has been (un)checked, (un)check his group
    if (item->hasChildren())
    {
        for (int i = 0; i < m_Model->rowCount(item->index()); ++i)
        {
            sat_index = m_Model->index(i, 0, item->index());
            sat_item  = m_Model->itemFromIndex(sat_index);

            if (item->checkState() == Qt::Checked)
                sat_item->setCheckState(Qt::Checked);
            else
                sat_item->setCheckState(Qt::Unchecked);
        }
    }
    else
    {
        bool all_sat_checked   = true;
        bool all_sat_unchecked = true;

        for (int i = 0; i < item->parent()->model()->rowCount(item->parent()->index()); ++i)
        {
            sat_index = m_Model->index(i, 0, item->parent()->index());
            sat_item  = m_Model->itemFromIndex(sat_index);

            if (sat_item->checkState() == Qt::Checked)
                all_sat_unchecked = false;
            else
                all_sat_checked = false;
        }

        if (all_sat_checked)
            item->parent()->setCheckState(Qt::Checked);
        else if (all_sat_unchecked)
            item->parent()->setCheckState(Qt::Unchecked);
        else
            item->parent()->setCheckState(Qt::PartiallyChecked);
    }

    connect(m_Model, SIGNAL(itemChanged(QStandardItem*)), this, SLOT(slotItemChanged(QStandardItem*)));
}
