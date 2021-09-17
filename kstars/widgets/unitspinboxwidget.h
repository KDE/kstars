/*
    SPDX-FileCopyrightText: 2015 Utkarsh Simha <utkarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef UNITSPINBOXWIDGET_H
#define UNITSPINBOXWIDGET_H

#include "ui_unitspinboxwidget.h"

/**
 * @brief The UnitSpinBoxWidget class
    * It is a widget that provides a DoubleSpinBox
    * and a ComboBox for conversions from different
    * units.
 * @author Utkarsh Simha
 */
class UnitSpinBoxWidget : public QWidget
{
    Q_OBJECT

  public:
    explicit UnitSpinBoxWidget(QWidget *parent = nullptr);
    ~UnitSpinBoxWidget() override;

    /**
         * @brief addUnit Adds a item to the combo box
         * @param unitName The name of the unit to be displayed
         * @param conversionFactor The factor the value of a unit must be multiplied by
         */
    void addUnit(const QString &unitName, double conversionFactor);

    /**
         * @brief value Returns value upon conversion
         */
    double value() const;

  private:
    Ui::UnitSpinBoxWidget *ui;
    QComboBox *comboBox;
    QDoubleSpinBox *doubleSpinBox;
};

#endif // UNITSPINBOXWIDGET_H
