/*
    SPDX-FileCopyrightText: 2001 Heiko Evermann <heiko@evermann.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "magnitudespinbox.h"

MagnitudeSpinBox::MagnitudeSpinBox(QWidget *parent) : QDoubleSpinBox(parent)
{
    setRange(0, 10);
    setValue(0);
    setDecimals(1);
    setSingleStep(0.1);
}

MagnitudeSpinBox::MagnitudeSpinBox(double minValue, double maxValue, QWidget *parent) : QDoubleSpinBox(parent)
{
    setRange(minValue, maxValue);
    setValue(minValue);
    setDecimals(1);
    setSingleStep(0.1);
}
