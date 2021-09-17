/*
    SPDX-FileCopyrightText: 2002 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

class QHBoxLayout;

class TimeSpinBox;
class TimeUnitBox;

/**
 * @class TimeStepBox
 * @short Composite spinbox for specifying a time step.
 * This composite widget consists of a TimeSpinBox (a QSpinBox), coupled with a
 * TimeUnitBox (a second pair of up/down buttons).
 * The second set of buttons makes larger steps through the 82 possible
 * time-step values, skipping to the next even unit of time.
 *
 * @author Jason Harris
 * @version 1.0
 */
class TimeStepBox : public QWidget
{
    Q_OBJECT
  public:
    /** Constructor. */
    explicit TimeStepBox(QWidget *parent = nullptr, bool daysonly = false);

    /** @return a pointer to the child TimeSpinBox */
    TimeSpinBox *tsbox() const { return timeBox; }

    /** @return a pointer to the child TimeUnitBox */
    TimeUnitBox *unitbox() const { return unitBox; }

    bool daysOnly() const { return DaysOnly; }
    void setDaysOnly(bool daysonly);

  signals:
    void scaleChanged(float);

  private slots:
    /**
     * Set the TimeSpinBox value according to the current UnitBox value.
   	 * This is connected to the UnitBox valueChanged() Signal.
     */
    void changeUnits(void);

    /**
     * Make sure the current UnitBox value represents the correct units for the
     * current TimeBox value. This slot is connected to the TimeBox valueChanged() Slot.
     */
    void syncUnits(int);

  private:
    bool DaysOnly { false };
    QHBoxLayout *hlay { nullptr };
    TimeSpinBox *timeBox { nullptr };
    TimeUnitBox *unitBox { nullptr };
};
