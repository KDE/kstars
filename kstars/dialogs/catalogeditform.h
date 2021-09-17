/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>
#include "catalogsdb.h"

namespace Ui
{
class CatalogEditForm;
}

/**
 * A simple data entry form for creating/editing catalog meta
 * information.
 *
 * As this is intended to be used for custom catalogs only, the
 * minimum for the catalog `id` is set to
 * `CatalogsDB::custom_cat_min_id`.
 */
class CatalogEditForm : public QDialog
{
    Q_OBJECT

  public:
    explicit CatalogEditForm(QWidget *parent, const CatalogsDB::Catalog &catalog,
                             const int min_id         = CatalogsDB::custom_cat_min_id,
                             const bool allow_id_edit = true);
    ~CatalogEditForm();

    /**
     * Get the result of the catalog edit.
     */
    CatalogsDB::Catalog getCatalog() { return m_catalog; };

  private:
    Ui::CatalogEditForm *ui;
    CatalogsDB::Catalog m_catalog;

    /**
     * Fills the form fields with the given catalog information.
     */
    void fill_form_from_catalog();
};
