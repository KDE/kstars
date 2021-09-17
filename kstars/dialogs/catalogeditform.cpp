/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "catalogeditform.h"
#include "dialogs/catalogcoloreditor.h"
#include "kstarsdata.h"
#include "ui_catalogeditform.h"
#include "catalogsdb.h"
#include <QColorDialog>

CatalogEditForm::CatalogEditForm(QWidget *parent, const CatalogsDB::Catalog &catalog,
                                 const int min_id, const bool allow_id_edit)
    : QDialog(parent), ui(new Ui::CatalogEditForm), m_catalog{ catalog }
{
    ui->setupUi(this);
    ui->id->setMinimum(min_id);
    ui->id->setEnabled(allow_id_edit);

    fill_form_from_catalog();
    connect(ui->id, QOverload<int>::of(&QSpinBox::valueChanged),
            [&](const int id) { m_catalog.id = id; });

    connect(ui->name, &QLineEdit::textChanged,
            [&](const QString &s) { m_catalog.name = s; });

    connect(ui->author, &QLineEdit::textChanged,
            [&](const QString &s) { m_catalog.author = s; });

    connect(ui->source, &QLineEdit::textChanged,
            [&](const QString &s) { m_catalog.source = s; });

    connect(ui->description, &QTextEdit::textChanged,
            [&]() { m_catalog.description = ui->description->toHtml(); });

    connect(ui->license, &QLineEdit::textChanged,
            [&](const QString &s) { m_catalog.license = s; });

    connect(ui->maintainer, &QLineEdit::textChanged,
            [&](const QString &s) { m_catalog.maintainer = s; });

    connect(ui->color, &QPushButton::clicked, [&]() {
        CatalogColorEditor editor{
            m_catalog.color == "" ?
                KStarsData::Instance()->colorScheme()->colorNamed("DSOColor").name() :
                m_catalog.color
        };

        if (editor.exec() != QDialog::Accepted)
            return;

        m_catalog.color = editor.color_string();
    });
}

CatalogEditForm::~CatalogEditForm()
{
    delete ui;
}

void CatalogEditForm::fill_form_from_catalog()
{
    ui->id->setValue(m_catalog.id);
    ui->name->setText(m_catalog.name);
    ui->author->setText(m_catalog.author);
    ui->source->setText(m_catalog.source);
    ui->description->setHtml(m_catalog.description);
    ui->license->setText(m_catalog.license);
    ui->maintainer->setText(m_catalog.maintainer);
}
