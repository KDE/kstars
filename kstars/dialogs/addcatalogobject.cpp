/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "addcatalogobject.h"
#include "ui_addcatalogobject.h"

AddCatalogObject::AddCatalogObject(QWidget *parent, const CatalogObject &obj)
    : QDialog(parent), ui(new Ui::AddCatalogObject), m_object{ obj }
{
    ui->setupUi(this);
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

    connect(ui->ra, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&](const auto value) { m_object.setRA0({ value }); });

    connect(ui->dec, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&](const auto value) { m_object.setDec0({ value }); });

    connect(ui->mag, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&](const auto value) { m_object.setMag(static_cast<float>(value)); });

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

    ui->ra->setValue(m_object.ra0().Degrees());
    ui->dec->setValue(m_object.dec0().Degrees());
    ui->mag->setValue(m_object.mag());
    ui->flux->setValue(m_object.flux());
    ui->position_angle->setValue(m_object.pa());
    refresh_flux();
}

void AddCatalogObject::refresh_flux()
{
    ui->flux->setEnabled(m_object.type() == SkyObject::RADIO_SOURCE);

    if (!ui->flux->isEnabled())
        ui->flux->setValue(0);
}
