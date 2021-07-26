/***************************************************************************
                  catalogcoloreditor.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2021-07-26
    copyright            : (C) 2021 by Valentin Boettcher
    email                : hiro at protagon.space; @hiro98:tchncs.de
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef CATALOGCOLOREDITOR_H
#define CATALOGCOLOREDITOR_H

#include <QDialog>
#include "catalogsdb.h"

namespace Ui
{
class CatalogColorEditor;
}

class CatalogColorEditor : public QDialog
{
    Q_OBJECT
    using color_map = CatalogsDB::CatalogColorMap;

  public:
    explicit CatalogColorEditor(color_map colors, QWidget *parent = nullptr);
    ~CatalogColorEditor();

    color_map colors() { return m_colors; }

  private:
    Ui::CatalogColorEditor *ui;
    color_map m_colors;

    void make_color_button(const QString &name, const QColor &color);
};

#endif // CATALOGCOLOREDITOR_H
