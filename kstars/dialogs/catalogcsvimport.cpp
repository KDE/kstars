/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "catalogcsvimport.h"
#include "rapidcsv.h"
#include "ui_catalogcsvimport.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QLabel>
#include <QComboBox>
#include <QFormLayout>

/**
 * Maps the name of the field to a tuple [Tooltip, Unit, Can be ignored?]
 *
 * @todo Maybe centralize that in the catalogobject class.
 */
const std::vector<std::pair<QString, std::tuple<QString, QString, bool>>> fields{
    { "Type", { "", "", true } },
    { "Right Ascension", { "", "", false } },
    { "Declination", { "", "", false } },
    { "Magnitude", { "", "", false } },
    { "Name", { "", "", false } },
    { "Long Name", { "", "", true } },
    { "Identifier", { "", "", true } },
    { "Major Axis", { "", "arcmin", true } },
    { "Minor Axis", { "", "arcmin", true } },
    { "Position Angle", { "", "°", true } },
    { "Flux", { "", "", true } },
};

QWidget *type_selector_widget()
{
    auto *pWidget = new QWidget{}; // no parent as receiver takes posession
    auto *pLayout = new QHBoxLayout(pWidget);
    auto *pCombo  = new QComboBox{};

    for (int i = 0; i < SkyObject::TYPE::NUMBER_OF_KNOWN_TYPES; i++)
    {
        pCombo->addItem(SkyObject::typeName(i), i);
    }
    pLayout->setAlignment(Qt::AlignCenter);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->addWidget(pCombo);
    pWidget->setLayout(pLayout);

    return pWidget;
}

CatalogCSVImport::CatalogCSVImport(QWidget *parent)
    : QDialog(parent), ui(new Ui::CatalogCSVImport)
{
    ui->setupUi(this);
    reset_mapping();

    ui->separator->setText(QString(default_separator));
    ui->comment_prefix->setText(QString(default_comment));

    ui->preview->setModel(&m_preview_model);
    ui->preview->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeMode::Stretch);

    connect(ui->file_select_button, &QPushButton::clicked, this,
            &CatalogCSVImport::select_file);

    connect(ui->add_map, &QPushButton::clicked, this,
            &CatalogCSVImport::type_table_add_map);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this,
            &CatalogCSVImport::read_objects);

    connect(ui->remove_map, &QPushButton::clicked, this,
            &CatalogCSVImport::type_table_remove_map);

    connect(ui->preview_button, &QPushButton::clicked, this, [&]() {
        read_n_objects(default_preview_size);
        m_preview_model.setObjects(m_objects);
    });

    init_column_mapping();
    init_type_table();

    for (auto *box : { ui->ra_units, ui->dec_units })
    {
        box->addItem(i18n("Degrees"));
        box->addItem(i18n("Hours"));
    }
}

CatalogCSVImport::~CatalogCSVImport()
{
    delete ui;
}

void CatalogCSVImport::select_file()
{
    QFileDialog dialog(this, i18nc("@title:window", "Import Catalog"), QDir::homePath(),
                       QString("CSV") + i18n("File") + QString(" (*.csv);;") +
                           i18n("Any File") + QString(" (*);;"));
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setDefaultSuffix("csv");

    if (dialog.exec() != QDialog::Accepted)
        return;

    const auto &fileName = dialog.selectedUrls().value(0).toLocalFile();

    if (!QFile::exists(fileName))
    {
        reset_mapping();
        QMessageBox::warning(this, i18n("Warning"),
                             i18n("Could not open the csv file.<br>It does not exist."));

        return;
    }

    if (ui->separator->text().length() < 1)
    {
        ui->separator->setText(QString(default_separator));
    }

    if (ui->comment_prefix->text().length() < 1)
    {
        ui->separator->setText(QString(default_separator));
    }

    const auto &separator      = ui->separator->text();
    const auto &comment_prefix = ui->comment_prefix->text();

    ui->file_path_label->setText(fileName);

    m_doc.Load(fileName.toStdString(), rapidcsv::LabelParams(),
               rapidcsv::SeparatorParams(separator[0].toLatin1(), true),
               rapidcsv::ConverterParams(false),
               rapidcsv::LineReaderParams(true, comment_prefix[0].toLatin1()));

    init_mapping_selectors();
};

void CatalogCSVImport::reset_mapping()
{
    ui->column_mapping->setEnabled(false);
    ui->preview_button->setEnabled(false);
    ui->obj_count->setEnabled(false);
    m_doc.Clear();
    ui->buttonBox->buttons()[0]->setEnabled(false);
};

void CatalogCSVImport::init_mapping_selectors()
{
    const auto &columns = m_doc.GetColumnNames();

    for (const auto &field : fields)
    {
        const auto can_be_ignored = std::get<2>(field.second);
        auto *selector            = m_selectors[field.first];

        selector->clear();

        selector->setMaxCount(columns.size() + (can_be_ignored ? 0 : 1));

        if (can_be_ignored)
            selector->addItem(i18n("Ignore"), -2);

        int i = 0;
        for (const auto &col : columns)
            selector->addItem(col.c_str(), i++);
    }

    ui->column_mapping->setEnabled(true);
    ui->buttonBox->buttons()[0]->setEnabled(true);
    ui->obj_count->setEnabled(true);
    ui->preview_button->setEnabled(true);
    ui->obj_count->setText(i18np("%1 Object", "%1 Objects", m_doc.GetRowCount()));
};

void CatalogCSVImport::init_type_table()
{
    auto *const table = ui->type_table;
    table->setHorizontalHeaderLabels(QStringList() << i18n("Text") << i18n("Type"));

    table->setColumnCount(2);
    table->setRowCount(1);
    auto *item = new QTableWidgetItem{ i18n("default") };
    item->setFlags(Qt::NoItemFlags);
    table->setItem(0, 0, item);

    table->setCellWidget(0, 1, type_selector_widget());
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Stretch);
};

void CatalogCSVImport::type_table_add_map()
{
    auto *const table  = ui->type_table;
    const auto cur_row = table->rowCount();

    table->setRowCount(cur_row + 1);
    table->setItem(cur_row, 0, new QTableWidgetItem{ "" });
    table->setCellWidget(cur_row, 1, type_selector_widget());
};

void CatalogCSVImport::type_table_remove_map()
{
    auto *const table = ui->type_table;

    for (const auto *item : table->selectedItems())
    {
        const auto row = item->row();
        if (row > 0)
            table->removeRow(row);
    }
};

CatalogCSVImport::type_map CatalogCSVImport::get_type_mapping()
{
    auto *const table = ui->type_table;
    type_map map{};

    for (int i = 0; i <= table->rowCount(); i++)
    {
        const auto *key  = table->item(i, 0);
        const auto *type = table->cellWidget(i, 1);

        if (key == nullptr || type == nullptr)
            continue;

        map[key->text().toStdString()] = static_cast<SkyObject::TYPE>(
            dynamic_cast<QComboBox *>(type->layout()->itemAt(0)->widget())
                ->currentData()
                .toInt());
    }

    return map;
};

void CatalogCSVImport::init_column_mapping()
{
    auto *cmapping = new QFormLayout();
    for (const auto &field : fields)
    {
        auto name           = field.first;
        const auto &tooltip = std::get<0>(field.second);
        const auto &unit    = std::get<1>(field.second);

        if (unit.length() > 0)
            name += QString(" [%1]").arg(unit);

        auto *label = new QLabel(name);
        label->setToolTip(tooltip);

        auto *selector = new QComboBox();
        selector->setEditable(true);
        m_selectors[field.first] = selector;

        cmapping->addRow(label, selector);
    }

    ui->column_mapping->setLayout(cmapping);
};

CatalogCSVImport::column_map CatalogCSVImport::get_column_mapping()
{
    CatalogCSVImport::column_map map{};
    const auto &names = m_doc.GetColumnNames();

    for (const auto &item : m_selectors)
    {
        const auto &name     = item.first;
        const auto *selector = item.second;
        auto selected_value  = selector->currentData().toInt();

        QString val_string;
        if (selected_value >= 0 &&
            (selector->currentText() != names[selected_value].c_str()))
        {
            selected_value = -1;
            val_string     = selector->currentText();
        }
        else
        {
            val_string = "";
        }

        map[name] = { selected_value, val_string };
    }

    return map;
};

void CatalogCSVImport::read_n_objects(size_t n)
{
    const auto &type_map   = get_type_mapping();
    const auto &column_map = get_column_mapping();
    const CatalogObject defaults{};

    m_objects.clear();
    m_objects.reserve(std::min(m_doc.GetRowCount(), n));

    //  pure magic, it's like LISP macros
    const auto make_getter = [this, &column_map](const QString &field, auto def) {
        const auto &conf        = column_map.at(field);
        const auto &default_val = get_default(conf, def);
        const auto index        = conf.first;

        std::function<decltype(def)(const size_t)> getter;
        if (conf.first >= 0)
            getter = [=](const size_t row) {
                try
                {
                    return m_doc.GetCell<decltype(def)>(index, row);
                }
                catch (...)
                {
                    return default_val;
                };
            };
        else
            getter = [=](const size_t) { return default_val; };

        return getter;
    };

    const auto make_coord_getter = [this, &column_map](const QString &field, auto def,
                                                       coord_unit unit) {
        const auto &conf = column_map.at(field);
        const auto default_val =
            (unit == coord_unit::deg) ?
                get_default<typed_dms<coord_unit::deg>>(column_map.at(field), { def })
                    .data :
                get_default<typed_dms<coord_unit::hours>>(column_map.at(field), { def })
                    .data;
        const auto index = conf.first;

        std::function<decltype(def)(const size_t)> getter;
        if (conf.first >= 0)
        {
            if (unit == coord_unit::deg)
                getter = [=](const size_t row) {
                    try
                    {
                        return m_doc.GetCell<typed_dms<coord_unit::deg>>(index, row).data;
                    }
                    catch (...)
                    {
                        return default_val;
                    };
                };
            else
                getter = [=](const size_t row) {
                    try
                    {
                        return m_doc.GetCell<typed_dms<coord_unit::hours>>(index, row)
                            .data;
                    }
                    catch (...)
                    {
                        return default_val;
                    };
                };
        }
        else
            getter = [=](const size_t) { return default_val; };

        return getter;
    };

    const auto ra_type  = static_cast<coord_unit>(ui->ra_units->currentIndex());
    const auto dec_type = static_cast<coord_unit>(ui->dec_units->currentIndex());

    const auto get_ra   = make_coord_getter("Right Ascension", defaults.ra(), ra_type);
    const auto get_dec  = make_coord_getter("Declination", defaults.dec(), dec_type);
    const auto get_mag  = make_getter("Magnitude", defaults.mag());
    const auto get_name = make_getter("Name", defaults.name());
    const auto get_type = make_getter("Type", std::string{ "default" });
    const auto get_long_name  = make_getter("Long Name", defaults.name());
    const auto get_identifier = make_getter("Identifier", defaults.catalogIdentifier());
    const auto get_a          = make_getter("Major Axis", defaults.a());
    const auto get_b          = make_getter("Minor Axis", defaults.b());
    const auto get_pa         = make_getter("Position Angle", defaults.pa());
    const auto get_flux       = make_getter("Flux", defaults.flux());

    for (size_t i = 0; i < std::min(m_doc.GetRowCount(), n); i++)
    {
        const auto &raw_type = get_type(i);

        const auto type = parse_type(raw_type, type_map);

        const auto ra         = get_ra(i);
        const auto dec        = get_dec(i);
        const auto mag        = get_mag(i);
        const auto name       = get_name(i);
        const auto long_name  = get_long_name(i);
        const auto identifier = get_identifier(i);
        const auto a          = get_a(i);
        const auto b          = get_b(i);
        const auto pa         = get_pa(i);
        const auto flux       = get_flux(i);

        m_objects.emplace_back(CatalogObject::oid{}, type, ra, dec, mag, name, long_name,
                               identifier, -1, a, b, pa, flux);
    }
};

SkyObject::TYPE CatalogCSVImport::parse_type(const std::string &type,
                                             const type_map &type_map)
{
    if (type_map.count(type) == 0)
        return type_map.at("default");

    return type_map.at(type);
};
