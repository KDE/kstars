/***************************************************************************
               unitspinboxwidget.h - A widget for providing multiple units
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
    explicit UnitSpinBoxWidget(QWidget *parent = 0);
    ~UnitSpinBoxWidget();

    /**
     * @brief addUnit Adds a item to the combo box
     * @param unitName The name of the unit to be displayed
     * @param conversionFactor The factor the value of a unit must be multiplied by
     */
    void addUnit( const QString &unitName, double conversionFactor );

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
