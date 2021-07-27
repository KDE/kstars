/***************************************************************************
                  catalogcoloreditor.cpp  -  K Desktop Planetarium
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

#include "catalogcoloreditor.h"
#include "ui_catalogcoloreditor.h"
#include "kstarsdata.h"
#include <QPushButton>
#include <qcolordialog.h>

CatalogColorEditor::CatalogColorEditor(color_map colors, QWidget *parent)
    : QDialog(parent), ui(new Ui::CatalogColorEditor), m_colors{ std::move(colors) }
{
    ui->setupUi(this);

    auto *data         = KStarsData::Instance();
    auto default_color = m_colors["default"];
    if (!default_color.isValid())
        default_color = data->colorScheme()->colorNamed("DSOColor");

    for (const auto &item : KStarsData::Instance()->color_schemes())
    {
        if (item.second != "default" && !data->hasColorScheme(item.second))
            continue;

        auto &color = m_colors[item.second];
        if (!color.isValid())
        {
            color = m_colors["default"];

            if (!color.isValid())
            {
                color = default_color;
            }
        }
    }

    for (const auto &item : m_colors)
    {
        make_color_button(item.first, item.second);
    }
}

CatalogColorEditor::~CatalogColorEditor()
{
    delete ui;
}

void CatalogColorEditor::make_color_button(const QString &name, const QColor &color)
{
    auto *button = new QPushButton(name == "default" ?
                                       i18n("Default") :
                                       KStarsData::Instance()->colorSchemeName(name));

    button->setStyleSheet(
        QString("background-color: %1; color: black").arg(color.name()));

    auto *layout = ui->colorButtons;
    layout->addWidget(button);

    connect(button, &QPushButton::clicked, this, [this, name, button] {
        QColorDialog picker{};
        if (picker.exec() != QDialog::Accepted)
            return;

        m_colors[name] = picker.currentColor();
        button->setStyleSheet(QString("background-color: %1; color: black")
                                  .arg(picker.currentColor().name()));
    });
};
