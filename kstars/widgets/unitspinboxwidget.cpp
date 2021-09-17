/*
    SPDX-FileCopyrightText: 2015 Utkarsh Simha <utkarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "unitspinboxwidget.h"

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
