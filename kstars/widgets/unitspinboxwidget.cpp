/*
    SPDX-FileCopyrightText: 2015 Utkarsh Simha <utkarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "unitspinboxwidget.h"
#include<cmath>

UnitSpinBoxWidget::UnitSpinBoxWidget(QWidget *parent) : QWidget(parent), ui(new Ui::UnitSpinBoxWidget)
{
    ui->setupUi(this);
    doubleSpinBox = ui->doubleSpinBox;
    comboBox      = ui->comboBox;
    //connect(spinBox, SIGNAL(valueChanged(int)), this, SLOT(setText()));
    //connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setText()));
}

UnitSpinBoxWidget::~UnitSpinBoxWidget()
{
    delete ui;
}

void UnitSpinBoxWidget::addUnit(const QString &unitName, double conversionFactor)
{
    QVariant qv;
    qv.setValue(conversionFactor);
    comboBox->addItem(unitName, qv);
}

double UnitSpinBoxWidget::value() const
{
    int index               = comboBox->currentIndex();
    QVariant qv             = comboBox->itemData(index);
    double conversionFactor = qv.value<double>();
    double value            = doubleSpinBox->value();
    return value * conversionFactor;
}

void UnitSpinBoxWidget::setValue(const double value)
{
    if (value < 1e-20 && value > -1e-20)
    {
        // Practically zero
        doubleSpinBox->setValue(value);
        return;
    }
    std::vector<double> diffs;
    for (int index = 0; index < comboBox->count(); ++index)
    {
        QVariant qv = comboBox->itemData(index);
        double conversionFactor = qv.value<double>();
        diffs.push_back(std::abs(std::abs(value) / conversionFactor - 1.));
    }
    auto it = std::min_element(diffs.cbegin(), diffs.cend());
    int index = std::distance(diffs.cbegin(), it);
    comboBox->setCurrentIndex(index);
    QVariant qv = comboBox->itemData(index);
    double conversionFactor = qv.value<double>();
    doubleSpinBox->setValue(value / conversionFactor);
}
