/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "catalogcoloreditor.h"
#include "catalogsdb.h"
#include "ui_catalogcoloreditor.h"
#include "kstarsdata.h"
#include <QPushButton>
#include <qcolordialog.h>

CatalogColorEditor::CatalogColorEditor(const int id, QWidget *parent)
    : QDialog(parent), ui(new Ui::CatalogColorEditor), m_id{ id }
{
    CatalogsDB::DBManager manager{ CatalogsDB::dso_db_path() };
    const auto &cat = manager.get_catalog(m_id);
    if (!cat.first)
    {
        QMessageBox::critical(this, i18n("Critical error"),
                              i18n("Catalog with id %1 not found.", m_id));
        reject();
        return;
    }

    m_colors = manager.get_catalog_colors(m_id);
    init();
    ui->catalogName->setText(cat.second.name);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this,
            &CatalogColorEditor::writeColors);
}

CatalogColorEditor::CatalogColorEditor(const QString &colors, QWidget *parent)
    : QDialog(parent), ui(new Ui::CatalogColorEditor),
      m_colors{ CatalogsDB::parse_color_string(colors) }, m_id{ -1 }
{
    init();
    ui->catalogName->setText("");
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this,
            &CatalogColorEditor::accept);
};

CatalogColorEditor::CatalogColorEditor(color_map colors, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::CatalogColorEditor), m_colors{ std::move(colors) }, m_id{ -1 }
{
    init();
    ui->catalogName->setText("");
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this,
            &CatalogColorEditor::accept);
};

void CatalogColorEditor::init()
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
};

CatalogColorEditor::~CatalogColorEditor()
{
    delete ui;
}

QColor contrastingColor(QColor color)
{
    color.toHsl();
    color.setHsv(0, 0, color.lightness() < 100 ? 255 : 0);

    return color;
}

void setButtonStyle(QPushButton *button, const QColor &color)
{
    button->setStyleSheet(QString("background-color: %1; color: %2")
                              .arg(color.name())
                              .arg(contrastingColor(color).name()));
}

void CatalogColorEditor::make_color_button(const QString &name, const QColor &color)
{
    auto *button = new QPushButton(name == "default" ?
                                       i18n("Default") :
                                       KStarsData::Instance()->colorSchemeName(name));

    auto *layout = ui->colorButtons;
    layout->addWidget(button);
    setButtonStyle(button, color);

    connect(button, &QPushButton::clicked, this, [this, name, button] {
        QColorDialog picker{};
        if (picker.exec() != QDialog::Accepted)
            return;

        m_colors[name] = picker.currentColor();
        setButtonStyle(button, picker.currentColor());
    });
};

void CatalogColorEditor::writeColors()
{
    if (m_id < 0)
        return;

    CatalogsDB::DBManager manager{ CatalogsDB::dso_db_path() };
    const auto &insert_success = manager.insert_catalog_colors(m_id, m_colors);

    if (!insert_success.first)
    {
        QMessageBox::critical(
            this, i18n("Critical error"),
            i18n("Could not insert new colors.<br>", insert_success.second));
        reject();
        return;
    }

    accept();
};
