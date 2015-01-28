/***************************************************************************
               unitspinboxwidget.cpp  - A widget for providing multiple units
                             -------------------
    begin                : Sun 18th Jan 2015
    copyright            : (C) 2015 Utkarsh Simha
    email                : utkarshsimha@gmail.com
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "unitspinboxwidget.h"

UnitSpinBoxWidget::UnitSpinBoxWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::UnitSpinBoxWidget)
{
    ui->setupUi(this);
    doubleSpinBox = ui->doubleSpinBox;
    comboBox = ui->comboBox;
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
    qv.setValue( conversionFactor );
    comboBox->addItem( unitName, qv );
}

double UnitSpinBoxWidget::value() const
{
    int index = comboBox->currentIndex();
    QVariant qv = comboBox->itemData(index);
    double conversionFactor = qv.value<double>();
    double value = doubleSpinBox->value();
    return value*conversionFactor;
}

