/*
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "catalogsdb.h"

#include <QObject>
#include <QModelIndex>
#include <QVector>
#include <QPair>
#include <QString>

class QLineEdit;
class QTimer;
class QCompleter;
class QSortFilterProxyModel;
class QAction;
class SkyObjectListModel;
class SkyObject;

/**
 * @class SkyObjectSearchAdapter
 * Attaches an incremental, debounced sky object name search to an existing
 * QLineEdit via QCompleter. Typing at least 2 characters triggers a 500ms
 * debounced search across all currently known sky objects (stars, solar
 * system bodies, DSOs, etc.), augmented by CatalogsDB for DSOs not yet
 * loaded into memory. Selecting an entry emits objectSelected(). A trailing
 * chevron icon opens a menu to restrict the search to a SearchCategory.
 *
 * Backend search logic is ported from FindDialog (dialogs/finddialog.cpp).
 */
class SkyObjectSearchAdapter : public QObject
{
        Q_OBJECT
    public:
        // mirrors FindDialog::FindDialogUI's FilterType category list
        enum class SearchCategory
        {
            Any, Stars, SolarSystem, OpenClusters, GlobularClusters, GaseousNebulae,
            PlanetaryNebulae, Galaxies, Comets, Asteroids, Constellations, Supernovae, Satellites
        };

        explicit SkyObjectSearchAdapter(QLineEdit *lineEdit, QObject *parent = nullptr);

    Q_SIGNALS:
        void objectSelected(SkyObject *object);

    private Q_SLOTS:
        void enqueueSearch();
        void filterList();
        void activated(const QModelIndex &index);

    private:
        void buildFilterMenu(QAction *filterAction);
        QVector<QPair<QString, const SkyObject *>> objectsForCategory() const;

        QLineEdit *m_lineEdit { nullptr };
        SkyObjectListModel *m_model { nullptr };
        QSortFilterProxyModel *m_sortModel { nullptr };
        QCompleter *m_completer { nullptr };
        QTimer *m_timer { nullptr };
        SearchCategory m_category { SearchCategory::Any };

        // DSO database, mirrors FindDialog owning its own connection
        CatalogsDB::DBManager m_dbManager;
};
