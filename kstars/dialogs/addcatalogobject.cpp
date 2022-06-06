/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "addcatalogobject.h"
#include "ui_addcatalogobject.h"

#include <QPushButton>

AddCatalogObject::AddCatalogObject(QWidget *parent, const CatalogObject &obj)
    : QDialog(parent), ui(new Ui::AddCatalogObject), m_object{ obj }
{
    ui->setupUi(this);
    ui->ra->setDegType(false);
    fill_form_from_object();

    connect(ui->name, &QLineEdit::textChanged,
            [&](const auto &name) { m_object.setName(name); });

    connect(ui->long_name, &QLineEdit::textChanged,
            [&](const auto &name) { m_object.setLongName(name); });

    connect(ui->catalog_identifier, &QLineEdit::textChanged,
            [&](const auto &ident) { m_object.setCatalogIdentifier(ident); });

    connect(ui->type, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [&](const auto index) {
                if (index > SkyObject::TYPE_UNKNOWN)
                    m_object.setType(SkyObject::TYPE_UNKNOWN);

                m_object.setType(index);

                refresh_flux();
            });

    auto validateAndStoreCoordinates = [&]() {
        bool raOk(false), decOk(false);
        auto ra = ui->ra->createDms(&raOk);
        auto dec = ui->dec->createDms(&decOk);
        auto* okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
        Q_ASSERT(!!okButton);
        okButton->setEnabled(raOk && decOk);
        if (raOk && decOk) {
            m_object.setRA0(ra);
            m_object.setDec0(dec);
        }
    };

    connect(ui->ra, &dmsBox::textChanged, validateAndStoreCoordinates);
    connect(ui->dec, &dmsBox::textChanged, validateAndStoreCoordinates);

    auto updateMag = [&]()
    {
        m_object.setMag(
            ui->magUnknown->isChecked() ? NaN::f : static_cast<float>(ui->mag->value()));
    };
    connect(ui->mag, QOverload<double>::of(&QDoubleSpinBox::valueChanged), updateMag);
    connect(ui->magUnknown, &QCheckBox::stateChanged, updateMag);

    connect(ui->maj, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&](const auto value) { m_object.setMaj(static_cast<float>(value)); });

    connect(ui->min, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&](const auto value) { m_object.setMin(static_cast<float>(value)); });

    connect(ui->flux, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&](const auto value) { m_object.setFlux(static_cast<float>(value)); });

    connect(ui->position_angle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&](const auto value) { m_object.setPA(value); });
}

AddCatalogObject::~AddCatalogObject()
{
    delete ui;
}

void AddCatalogObject::fill_form_from_object()
{
    ui->name->setText(m_object.name());
    ui->long_name->setText(m_object.longname());
    ui->catalog_identifier->setText(m_object.catalogIdentifier());

    for (int k = 0; k < SkyObject::NUMBER_OF_KNOWN_TYPES; ++k)
    {
        ui->type->addItem(SkyObject::typeName(k));
    }
    ui->type->addItem(SkyObject::typeName(SkyObject::TYPE_UNKNOWN));
    ui->type->setCurrentIndex((int)(m_object.type()));

    dms ra0 = m_object.ra0();
    dms dec0 = m_object.dec0(); // Make a copy to avoid overwriting by signal-slot connection
    ui->ra->show(ra0);
    ui->dec->show(dec0);
    if (std::isnan(m_object.mag()))
    {
        ui->magUnknown->setChecked(true);
    }
    else
    {
        ui->mag->setValue(m_object.mag());
        ui->magUnknown->setChecked(false);
    }
    ui->flux->setValue(m_object.flux());
    ui->position_angle->setValue(m_object.pa());
    ui->maj->setValue(m_object.a());
    ui->min->setValue(m_object.b());
    refresh_flux();
}

void AddCatalogObject::refresh_flux()
{
    ui->flux->setEnabled(m_object.type() == SkyObject::RADIO_SOURCE);

    if (!ui->flux->isEnabled())
        ui->flux->setValue(0);
}
