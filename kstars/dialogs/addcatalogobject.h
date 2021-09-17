/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef ADDCATALOGOBJECT_H
#define ADDCATALOGOBJECT_H

#include <QDialog>
#include "catalogobject.h"

namespace Ui
{
class AddCatalogObject;
}

/**
 * A simple data entry dialog to create and edit objects in
 * `CatalogDB` catalogs. It takes a `CatalogObject` and mutates it
 * according to user input.
 *
 * After the dialog is closed, the contained can then be retrieved by
 * calling `get_object`.
 */
class AddCatalogObject : public QDialog
{
    Q_OBJECT

  public:
    /**
     * \param parent the parent widget, nullptr allowed
     * \parame obj the objects to be mutated, default constructed by default
     */
    explicit AddCatalogObject(QWidget *parent, const CatalogObject &obj = {});
    ~AddCatalogObject();

    /**
     * Retrieve the edited object.
     */
    CatalogObject get_object() { return m_object; }

  private:
    Ui::AddCatalogObject *ui;

    /** The object to be edited. */
    CatalogObject m_object;

    /** Fill the form fields with the information from the object. */
    void fill_form_from_object();

    /** Enable/Disable the `flux` field depending on object type. */
    void refresh_flux();
};

#endif // ADDCATALOGOBJECT_H
