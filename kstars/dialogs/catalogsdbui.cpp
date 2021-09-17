/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QCheckBox>
#include <QMessageBox>
#include <QFileDialog>
#include "catalogsdbui.h"
#include "ui_catalogsdbui.h"
#include "catalogeditform.h"
#include "catalogdetails.h"
#include "catalogcoloreditor.h"

CatalogsDBUI::CatalogsDBUI(QWidget *parent, const QString &db_path)
    : QDialog(parent), ui{ new Ui::CatalogsDBUI }, m_manager{ db_path }, m_last_dir{
          QDir::homePath()
      }
{
    ui->setupUi(this);
    ui->objectsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    const QStringList labels = {
        i18n("Enabled"),    i18n("ID"),      i18n("Name"),
        i18n("Precedence"), i18n("Author"),  i18n("Mutable"),
        i18n("Version"),    i18n("License"), i18n("Maintainer")
    };

    ui->objectsTable->setColumnCount(labels.size());
    ui->objectsTable->setHorizontalHeaderLabels(labels);

    refresh_db_table();

    connect(ui->objectsTable, &QTableWidget::cellClicked, this,
            &CatalogsDBUI::row_selected);

    connect(ui->objectsTable, &QTableWidget::itemSelectionChanged, this,
            &CatalogsDBUI::disable_buttons);

    connect(ui->activateButton, &QPushButton::clicked, this,
            &CatalogsDBUI::enable_disable_catalog);

    connect(ui->importButton, &QPushButton::clicked, this, &CatalogsDBUI::import_catalog);

    connect(ui->exportButton, &QPushButton::clicked, this, &CatalogsDBUI::export_catalog);

    connect(ui->removeButton, &QPushButton::clicked, this, &CatalogsDBUI::remove_catalog);

    connect(ui->createButton, &QPushButton::clicked, this,
            qOverload<>(&CatalogsDBUI::create_new_catalog));

    connect(ui->moreButton, &QPushButton::clicked, this, &CatalogsDBUI::show_more_dialog);

    connect(ui->dublicateButton, &QPushButton::clicked, this,
            &CatalogsDBUI::dublicate_catalog);

    connect(ui->colorButton, &QPushButton::clicked, this,
            &CatalogsDBUI::show_color_editor);
}

CatalogsDBUI::~CatalogsDBUI()
{
    delete ui;
}

QWidget *centered_check_box_widget(bool checked)
{
    auto *pWidget   = new QWidget{}; // no parent as receiver takes posession
    auto *pLayout   = new QHBoxLayout(pWidget);
    auto *pCheckBox = new QCheckBox(pWidget);

    pCheckBox->setChecked(checked);
    pCheckBox->setEnabled(false);
    pLayout->addWidget(pCheckBox);
    pLayout->setAlignment(Qt::AlignCenter);
    pLayout->setContentsMargins(0, 0, 0, 0);

    pWidget->setLayout(pLayout);

    return pWidget;
}

void CatalogsDBUI::refresh_db_table()
{
    const auto catalogs = m_manager.get_catalogs(true);

    m_catalogs.resize(catalogs.size());

    auto &table = *ui->objectsTable;
    table.setRowCount(catalogs.size());

    int row{ 0 };
    for (const auto &catalog : catalogs)
    {
        m_catalogs[row] = catalog.id;

        // the table takes ownership of its items
        table.setCellWidget(row, 0, centered_check_box_widget(catalog.enabled));

        table.setItem(row, 1, new QTableWidgetItem{ QString::number(catalog.id) });
        table.setItem(row, 2, new QTableWidgetItem{ catalog.name });
        table.setItem(row, 3,
                      new QTableWidgetItem{ QString::number(catalog.precedence) });
        table.setItem(row, 4, new QTableWidgetItem{ catalog.author });
        table.setCellWidget(row, 5, centered_check_box_widget(catalog.mut));
        table.setItem(row, 6, new QTableWidgetItem{ QString::number(catalog.version) });
        table.setItem(row, 7, new QTableWidgetItem{ catalog.license });
        table.setItem(row, 8, new QTableWidgetItem{ catalog.maintainer });
        row++;
    }
}

void CatalogsDBUI::row_selected(int row, int)
{
    const auto &success = m_manager.get_catalog(m_catalogs[row]);
    if (!success.first)
        return;

    const auto cat = success.second;
    ui->activateButton->setText(!cat.enabled ? i18n("Enable") : i18n("Disable"));
    ui->activateButton->setEnabled(true);
    ui->moreButton->setEnabled(true);
    ui->removeButton->setEnabled(true);
    ui->exportButton->setEnabled(true);
    ui->dublicateButton->setEnabled(true);
    ui->colorButton->setEnabled(true);
}

void CatalogsDBUI::disable_buttons()
{
    if (ui->objectsTable->selectedItems().length() > 0)
        return;

    ui->activateButton->setText(i18n("Enable"));
    ui->activateButton->setEnabled(false);
    ui->moreButton->setEnabled(false);
    ui->removeButton->setEnabled(false);
    ui->exportButton->setEnabled(false);
    ui->dublicateButton->setEnabled(false);
}

void CatalogsDBUI::enable_disable_catalog()
{
    const auto catalog = get_selected_catalog();
    if (!catalog.first)
        return;

    const auto success =
        m_manager.set_catalog_enabled(catalog.second.id, !catalog.second.enabled);
    if (!success.first)
        QMessageBox::warning(
            this, i18n("Warning"),
            i18n("Could not enable/disable the catalog.<br>%1", success.second));

    refresh_db_table();
    row_selected(ui->objectsTable->selectedItems().first()->row(), 0);
}

const std::pair<bool, CatalogsDB::Catalog> CatalogsDBUI::get_selected_catalog()
{
    const auto items = ui->objectsTable->selectedItems();
    if (items.length() == 0)
        return { false, {} };

    return m_manager.get_catalog(m_catalogs[items.first()->row()]);
}

void CatalogsDBUI::export_catalog()
{
    const auto cat = get_selected_catalog();
    if (!cat.first)
        return;

    QFileDialog dialog(this, i18nc("@title:window", "Export Catalog"), m_last_dir,
                       i18n("Catalog") +
                           QString(" (*.%1);;").arg(CatalogsDB::db_file_extension));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix(CatalogsDB::db_file_extension);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const auto fileName = dialog.selectedUrls().value(0).toLocalFile();
    const auto success  = m_manager.dump_catalog(cat.second.id, fileName);
    m_last_dir          = QFileInfo(fileName).absolutePath();

    if (!success.first)
        QMessageBox::warning(this, i18n("Warning"),
                             i18n("Could not export the catalog.<br>%1", success.second));
}

void CatalogsDBUI::import_catalog(bool force)
{
    QFileDialog dialog(this, i18nc("@title:window", "Import Catalog"), m_last_dir,
                       i18n("Catalog") +
                           QString(" (*.%1);;").arg(CatalogsDB::db_file_extension));
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setDefaultSuffix(CatalogsDB::db_file_extension);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const auto fileName = dialog.selectedUrls().value(0).toLocalFile();
    const auto success  = m_manager.import_catalog(fileName, force);
    m_last_dir          = QFileInfo(fileName).absolutePath();

    if (!success.first && !force)
    {
        QMessageBox::warning(this, i18n("Warning"),
                             i18n("Could not import the catalog.<br>%1", success.second));

        if (QMessageBox::question(this, "Retry", "Retry and overwrite?",
                                  QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
            import_catalog(true);
    }

    refresh_db_table();
}

void CatalogsDBUI::remove_catalog()
{
    const auto cat = get_selected_catalog();
    if (!cat.first)
        return;

    const auto success = m_manager.remove_catalog(cat.second.id);

    if (!success.first)
        QMessageBox::warning(this, i18n("Warning"),
                             i18n("Could not remove the catalog.<br>%1", success.second));

    refresh_db_table();
}

std::pair<bool, int> CatalogsDBUI::create_new_catalog(const CatalogsDB::Catalog &catalog)
{
    auto *dialog = new CatalogEditForm(this, catalog);
    if (dialog->exec() != QDialog::Accepted)
        return { false, -1 };

    auto cat            = dialog->getCatalog();
    cat.mut             = true;
    const auto &success = m_manager.register_catalog(cat);

    if (!success.first)
    {
        QMessageBox::warning(this, i18n("Warning"),
                             i18n("Could not create the catalog.<br>%1", success.second));
        return create_new_catalog(cat);
    }

    refresh_db_table();
    return { true, cat.id };
}

void CatalogsDBUI::create_new_catalog()
{
    create_new_catalog({ m_manager.find_suitable_catalog_id() });
}

void CatalogsDBUI::dublicate_catalog()
{
    const auto &success = get_selected_catalog();
    if (!success.first)
        return;

    const auto &src = success.second;

    auto src_new       = src;
    src_new.id         = m_manager.find_suitable_catalog_id();
    src_new.enabled    = false;
    src_new.precedence = 1;

    const auto &create_success = create_new_catalog(src_new);

    if (!create_success.first)
        return;

    const auto copy_success = m_manager.copy_objects(src.id, create_success.second);
    if (!copy_success.first)
    {
        QMessageBox::warning(this, i18n("Warning"),
                             i18n("Could not copy the objects to the new catalog.<br>%1")
                                 .arg(copy_success.second));

        const auto &remove_success = m_manager.remove_catalog(create_success.second);
        if (!remove_success.first)
            QMessageBox::critical(
                this, i18n("Critical error"),
                i18n("Could not clean up and remove the new catalog.<br>%1",
                     remove_success.second));
    };
}

void CatalogsDBUI::show_more_dialog()
{
    const auto success = get_selected_catalog();
    if (!success.first)
        return;

    auto *dialog = new CatalogDetails(this, m_manager.db_file_name(), success.second.id);

    connect(this, &QDialog::finished, dialog, &QDialog::done);
    connect(dialog, &QDialog::finished, this, &CatalogsDBUI::refresh_db_table);

    dialog->show();
}

void CatalogsDBUI::show_color_editor()
{
    const auto &success = get_selected_catalog();
    if (!success.first)
        return;

    auto *dialog = new CatalogColorEditor(success.second.id, this);

    connect(this, &QDialog::finished, dialog, &QDialog::reject);
    dialog->show();
}
