/*
    SPDX-FileCopyrightText: 2002 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>
#include <QAction>

class QToolButton;

#define TUB_ALLUNITS 8

/**
 * @class TimeUnitBox
 * @short Provides a second set of up/down buttons for TimeStepBox.
 * A pair of buttons, arranged one above the other, labeled "+"/"-".  These buttons
 * are to be paired with the TimeSpinBox widget.  Their function is to provide
 * a way to cycle through the possible time steps using larger intervals than the up/down
 * buttons of the TimeSpinBox.  For example, say the Spinbox currently shows a timestep of
 * "1 sec".  Increasing the timestep with the spinbox up-button will change it to
 * "2 sec", while using the "+" button of this widget will change it to "1 min".
 *
 * The idea is that these "outer" buttons always change to the next even unit of time.
 *
 * @note this widget is not to be used on its own; it is combined with the TimeSpinBox
 * widget to form the TimeStepBox composite widget.
 *
 * @author Jason Harris
 * @version 1.0
 */

class QToolButton;

class TimeUnitBox : public QWidget
{
    Q_OBJECT

  public:
    /** Constructor */
    explicit TimeUnitBox(QWidget *parent = nullptr, bool daysonly = false);

    /** @return the value of UnitStep for the current spinbox value() */
    int unitValue();

    /**
     * @short the same as unitValue, except you can get the UnitStep for any value, not just the current one.
     * @return the value of UnitStep for the index value given as an argument.
     */
    int getUnitValue(int);

    /**
     * Set the value which describes which time-unit is displayed in the TimeSpinBox.
     * @p value the new value
   	 */
    void setValue(int value) { Value = value; }
    /** @return the internal value describing the time-unit of the TimeSpinBox. */
    int value() const { return Value; }

    /** Set the minimum value for the internal time-unit value */
    void setMinimum(int minValue) { MinimumValue = minValue; }
    /** Set the maximum value for the internal time-unit value */
    void setMaximum(int maxValue) { MaximumValue = maxValue; }

    /** @return the minimum value for the internal time-unit value */
    int minValue() const { return MinimumValue; }
    /** @return the maximum value for the internal time-unit value */
    int maxValue() const { return MaximumValue; }

    bool daysOnly() const { return DaysOnly; }
    void setDaysOnly(bool daysonly);

    QAction *increaseUnitsAction() const { return IncreaseAction; }
    QAction *decreaseUnitsAction() const { return DecreaseAction; }

  signals:
    void valueChanged(int);

  private slots:
    /** Increment the internal time-unit value */
    void increase();
    /** Decrement the internal time-unit value */
    void decrease();

  private:
    bool DaysOnly { false };
    QToolButton *UpButton { nullptr };
    QToolButton *DownButton { nullptr };
    QAction *IncreaseAction { nullptr };
    QAction *DecreaseAction { nullptr };
    int MinimumValue { 0 };
    int MaximumValue { 0 };
    int Value { 0 };
    int UnitStep[TUB_ALLUNITS];
};
