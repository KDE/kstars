/*
    SPDX-FileCopyrightText: 2024 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <cmath>
#include "exposurewidget.h"

ExposureWidget::ExposureWidget(QWidget *parent) : QDoubleSpinBox(parent)
{
}

ExposureWidget::~ExposureWidget()
{
}

QString ExposureWidget::textFromValue(double value) const
{
    int count = 0;
    const double delta = std::pow(10, -decimals() - 1);
    double v = value;
    for (int i = 0; i < decimals(); i++)
    {
        v = v - long(v + delta);
        if (v < std::pow(10, (-decimals() + i)))
            break;
        count++;
        v *= 10;
    }
    return QWidget::locale().toString(value, static_cast<char>(u'f'), std::max(3, count));
}
