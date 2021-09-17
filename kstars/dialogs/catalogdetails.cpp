/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QMessageBox>
#include "catalogdetails.h"
#include "detaildialog.h"
#include "kstarsdata.h"
#include "kstars.h"
#include "ui_catalogdetails.h"
#include "catalogeditform.h"
#include "addcatalogobject.h"
#include "catalogcsvimport.h"
#include "skymapcomposite.h"
#include "catalogscomponent.h"

constexpr int CatalogDetails::list_size;

CatalogDetails::CatalogDetails(QWidget *parent, const QString &db_path,
                               const int catalog_id)
    : QDialog(parent), ui(new Ui::CatalogDetails), m_manager{ db_path },
      m_catalog_id{ catalog_id }, m_timer{ new QTimer(this) }
{
    ui->setupUi(this);
    reload_catalog();
    reload_objects();
    ui->name_filter->setPlaceholderText(i18n("Showing <= %1 entries. Enter a name (case "
                                             "sensitive) to narrow down the search.",
                                             list_size));

    m_timer->setInterval(200);
    connect(m_timer, &QTimer::timeout, this, &CatalogDetails::reload_objects);

    connect(ui->name_filter, &QLineEdit::textChanged, this,
            [&](const auto) { m_timer->start(); });

    ui->object_table->setModel(&m_model);
    ui->object_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->object_table->adjustSize();
    ui->object_table->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeMode::ResizeToContents);

    connect(ui->object_table, &QTableView::doubleClicked, this,
            &CatalogDetails::show_object_details);

    connect(ui->object_table, &QTableView::clicked, [&](const auto) {
        if (!m_catalog.mut)
            return;

        ui->remove_object->setEnabled(true);
        ui->edit_object->setEnabled(true);
        ui->object_group->setEnabled(true);
    });

    connect(ui->edit, &QPushButton::clicked, this, &CatalogDetails::edit_catalog_meta);

    connect(ui->add_object, &QPushButton::clicked, this, &CatalogDetails::add_object);

    connect(ui->remove_object, &QPushButton::clicked, this,
            &CatalogDetails::remove_objects);

    connect(ui->edit_object, &QPushButton::clicked, this, &CatalogDetails::edit_objects);

    connect(ui->import_csv, &QPushButton::clicked, this, &CatalogDetails::import_csv);
}

CatalogDetails::~CatalogDetails()
{
    delete ui;
}

void CatalogDetails::reload_catalog()
{
    const auto &found = m_manager.get_catalog(m_catalog_id);
    if (!found.first)
    {
        QMessageBox::warning(this, i18n("Warning"),
                             i18n("Could not load the catalog with id=%1", m_catalog_id));
        reject();
    }

    m_catalog = found.second;

    ui->catalogInfo->setTitle(m_catalog.name);
    ui->id->setText(QString::number(m_catalog.id));
    ui->precedence->setText(QString::number(m_catalog.precedence));
    ui->author->setText(m_catalog.author);
    ui->maintainer->setText(m_catalog.maintainer);
    ui->source->setText(m_catalog.source);
    ui->description->setText(m_catalog.description);
    ui->version->setText(QString::number(m_catalog.version));
    ui->license->setText(m_catalog.license);

    ui->edit->setEnabled(m_catalog.mut);
    ui->add_object->setEnabled(m_catalog.mut);
    ui->import_csv->setEnabled(m_catalog.mut);
}

void CatalogDetails::reload_objects()
{
    const auto objects = m_manager.find_objects_by_name(
        m_catalog.id, ui->name_filter->displayText(), list_size);

    m_model.setObjects({ objects.cbegin(), objects.cend() });
    m_timer->stop();
}

void CatalogDetails::show_object_details(const QModelIndex &index)
{
    const auto &obj = m_model.getObject(index);
    auto &inserted_obj =
        KStarsData::Instance()->skyComposite()->catalogsComponent()->insertStaticObject(
            obj);

    auto *dialog = new DetailDialog(&inserted_obj, KStarsData::Instance()->lt(),
                                    KStarsData::Instance()->geo(), this);
    connect(this, &QDialog::finished, dialog, &QDialog::done);
    dialog->show();
}

void CatalogDetails::edit_catalog_meta()
{
    auto *dialog = new CatalogEditForm(this, m_catalog, 0, false);

    if (dialog->exec() != QDialog::Accepted)
        return;

    const auto &success = m_manager.update_catalog_meta(dialog->getCatalog());
    if (!success.first)
        QMessageBox::warning(this, i18n("Warning"),
                             i18n("Could not update the catalog.<br>%1", success.second));

    reload_catalog();
}

void CatalogDetails::add_object()
{
    auto *dialog = new AddCatalogObject(this, {});

    if (dialog->exec() != QDialog::Accepted)
        return;

    const auto &success = m_manager.add_object(m_catalog.id, dialog->get_object());
    if (!success.first)
        QMessageBox::warning(this, i18n("Warning"),
                             i18n("Could not add the object.<br>%1", success.second));

    reload_objects();
}

void CatalogDetails::remove_objects()
{
    const auto &items = ui->object_table->selectionModel()->selectedRows();

    if (items.length() == 0 || !m_catalog.mut)
        return;

    for (const auto &index : items)
    {
        const auto &success =
            m_manager.remove_object(m_catalog.id, m_model.getObject(index).getObjectId());
        if (!success.first)
            QMessageBox::warning(
                this, i18n("Warning"),
                i18n("Could not remove the object.<br>%1", success.second));
    }

    reload_objects();
}

void CatalogDetails::edit_objects()
{
    const auto &items = ui->object_table->selectionModel()->selectedRows();

    if (items.length() == 0 || !m_catalog.mut)
        return;

    for (const auto &index : items)
    {
        const auto &obj = m_model.getObject(index);
        auto *dialog    = new AddCatalogObject(this, obj);

        if (dialog->exec() != QDialog::Accepted)
            continue;

        const auto &success = m_manager.remove_object(m_catalog.id, obj.getObjectId());
        if (!success.first)
            QMessageBox::warning(this, i18n("Warning"),
                                 i18n("Could not remove the object.<br>%1", success.second));

        const auto &success_add =
            m_manager.add_object(m_catalog.id, dialog->get_object());
        if (!success_add.first)
            QMessageBox::warning(this, i18n("Warning"),
                                 i18n("Could not add the object.<br>%1", success_add.second));
    }

    reload_objects();
}

void CatalogDetails::import_csv()
{
    CatalogCSVImport dialog{};

    if (dialog.exec() != QDialog::Accepted)
        return;

    const auto &success_add = m_manager.add_objects(m_catalog.id, dialog.get_objects());

    if (!success_add.first)
        QMessageBox::warning(this, i18n("Warning"),
                             i18n("Could not add the objects.<br>%1", success_add.second));

    reload_objects();
};
