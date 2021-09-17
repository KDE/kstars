/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CATALOGCOLOREDITOR_H
#define CATALOGCOLOREDITOR_H

#include <QDialog>
#include "catalogsdb.h"

namespace Ui
{
class CatalogColorEditor;
}

/**
 * A form for editing catalog color scheme mappings.
 *
 * If the constructor with the id is used, the dialog takes care of
 * editing the database. Otherwise `CatalogColorEditor::colors` may be
 * used.
 */
class CatalogColorEditor : public QDialog
{
    Q_OBJECT
    using color_map = CatalogsDB::CatalogColorMap;

  public:
    explicit CatalogColorEditor(const int id, QWidget *parent = nullptr);
    explicit CatalogColorEditor(color_map colors, QWidget *parent = nullptr);
    explicit CatalogColorEditor(const QString &colors, QWidget *parent = nullptr);

    ~CatalogColorEditor();

    color_map colors() { return m_colors; }
    QString color_string() { return CatalogsDB::to_color_string(m_colors); };

  private:
    Ui::CatalogColorEditor *ui;
    color_map m_colors;
    int m_id;

    void writeColors();
    void init();
    void make_color_button(const QString &name, const QColor &color);
};

#endif // CATALOGCOLOREDITOR_H
