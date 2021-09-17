/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CATALOGDETAILS_H
#define CATALOGDETAILS_H

#include <QDialog>
#include "catalogsdb.h"
#include "catalogobject.h"
#include "catalogobjectlistmodel.h"

class QTimer;
class QListWidgetItem;
namespace Ui
{
class CatalogDetails;
}

/**
 * A dialog that shows catalog information and provides facilities to
 * edit catalog meta information and manage its contents (if the catalog
 * is mutable). It holds its own instance of `CatalogsDB::DBManager` and
 * can thus be instanciated with minimal dependencies on other parts of
 * KStars.
 *
 * The dialog displays the 100 most visible objects matching the
 * search query.
 *
 * Supported operations are insertion, deletion and editing of catalog
 * entries.
 *
 * Dedublication for custom catalogs still has to be implemented.
 */
class CatalogDetails : public QDialog
{
    Q_OBJECT

  public:
    /**
     * How many catalog entries to show in the list.
     */
    static constexpr int list_size{ 10000 };

    /**
     * \param parent the parent widget, `nullptr` allowed
     * \param db_path the path to the catalog database to be used
     * \param catalog_id the id of the catalog to be edited
     *
     * If the catalog is not found, an error message will be displayed
     * and the dialog will be closed.
     */
    explicit CatalogDetails(QWidget *parent, const QString &db_path,
                            const int catalog_id);
    ~CatalogDetails();

  private:
    Ui::CatalogDetails *ui;

    /**
     * The database instance for accessing a catalog database.
     */
    CatalogsDB::DBManager m_manager;

    /**
     * The id of the backing catalog.
     */
    const int m_catalog_id;

    /**
     * The backing catalog.
     */
    CatalogsDB::Catalog m_catalog;

    /**
     * A timer to check for idle in the filter input.
     */
    QTimer *m_timer;

    /**
     * A model that holds all the objects which are being viewed in a
     * `DetailDialog`.
     */
    CatalogObjectListModel m_model;

  private slots:
    /** Reload the catalog meta info display. */
    void reload_catalog();

    /** Reload the displayed list of objects. */
    void reload_objects();

    /** Shows a `DetailDialog` for the double clicked list item. */
    void show_object_details(const QModelIndex &index);

    /** Opens a `CatalogEditForm` to edit the currently selected catalog. */
    void edit_catalog_meta();

    /** Opens an `AddCatalogObject` dialog to add an object to the catalog. */
    void add_object();

    /** Removes the selected objects from the catalog. */
    void remove_objects();

    /** Opens an `AddCatalogObject` dialog to edit the selected objects in the catalog. */
    void edit_objects();

    /** Opens a `CatalogCSVImport` dialog and imports objects from a csv. */
    void import_csv();
};

#endif // CATALOGDETAILS_H
