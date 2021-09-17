/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CATALOGSDBUI_H
#define CATALOGSDBUI_H

#include <QDialog>
#include "catalogsdb.h"

namespace Ui
{
class CatalogsDBUI;
}

/**
 * A simple UI to manage downloaded and custom Catalogs.
 *
 * It holds it's own instance of `CatalogsDB::DBManager` and can import,
 * export, delete, enable, disable and clone catalogs. On request it
 * spawns a `CatalogDetails` dialog which can be be used to edit
 * `mutable` catalogs.
 */
class CatalogsDBUI : public QDialog
{
    Q_OBJECT
  public:
    explicit CatalogsDBUI(QWidget *parent, const QString &db_path);
    ~CatalogsDBUI();

  private slots:
    /**
     * Activates the apropriate buttons.
     */
    void row_selected(int row, int);

    /**
     * Disables all catalog related buttons if no row is selected.c
     */
    void disable_buttons();

    /**
     * Enables or disables the currently selected catalog.
     */
    void enable_disable_catalog();

    /**
     * Opens a file selection dialog and exports the selected catalog.
     */
    void export_catalog();

    /**
     * Opens a file selection dialog and imports the selected catalog.
     */
    void import_catalog(bool force = false);

    /**
     * Removes the selected catalog.
     */
    void remove_catalog();

    /**
     * Creates a new catalog by prompting for the relevant details.
     * The smallest available id for custom catalogs is proposed.
     */
    void create_new_catalog();

    /**
     * Creates a new catalog based on \p `catalog` by prompting for the
     * relevant details.
     *
     * returns wether a catalog was created and the id of the created catalog
     */
    std::pair<bool, int> create_new_catalog(const CatalogsDB::Catalog &catalog);

    /**
     * Shows the `CatalogDetails` dialog for the currently selected
     * catalog.
     */
    void show_more_dialog();

    /**
     * Shows the `CatalogColorEditor` dialog for the currently selected
     * catalog.
     */
    void show_color_editor();

    /**
     * Dublicate the selected catalog, inserting it as a new catalog
     * and prompt for meta-data edit.
     */
    void dublicate_catalog();

    /**
     * Refresh the table by reloading the catalogs from the database.
     */
    void refresh_db_table();

  private:
    Ui::CatalogsDBUI *ui;

    /**
     * The database instance for accessing a catalog database.
     */
    CatalogsDB::DBManager m_manager;

    /**
     * The currently loaded catalogs. Relates row number to catalog
     * id.
     */
    std::vector<int> m_catalogs;

    /**
     * \returns if a catalog is selected and the catalog itself
     */
    const std::pair<bool, CatalogsDB::Catalog> get_selected_catalog();

    /** Remeber the directory where we last loaded a catalog from */
    QString m_last_dir;
};

#endif // CATALOGSDBUI_H
