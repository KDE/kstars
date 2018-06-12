/***************************************************************************
                          timespinbox.h  -  description
                             -------------------
    begin                : Sun Mar 31 2002
    copyright            : (C) 2002 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#pragma once

#include <QSpinBox>
#include <QStringList>

/**
 * @class TimeSpinBox
 * Custom spinbox to handle selection of timestep values with variable units.
 * @note this should only be used internally, embedded in a TimeStepBox widget.
 *
 * @author Jason Harris
 * @version 1.0
 */
class TimeSpinBox : public QSpinBox
{
    Q_OBJECT
  public:
    /** Constructor */
    explicit TimeSpinBox(QWidget *parent, bool daysOnly = false);
    /** Destructor (empty) */
    ~TimeSpinBox() override = default;

    /**
     * Convert the internal value to a display string.
     * @note reimplemented from QSpinBox
     * @p value the internal value to convert to a display string
     * @return the display string
     */
    QString textFromValue(int value) const override;

    /**
     * Convert the displayed string to an internal value.
     * @note reimplemented from QSpinBox
     * @p ok bool pointer set to true if conversion was successful
     * @return internal value converted from displayed text
     */
    int valueFromText(const QString &text) const override;

    /** @return the current TimeStep setting */
    float timeScale() const;

    void setDaysOnly(bool daysonly);
    bool daysOnly() const { return DaysOnly; }

  signals:
    void scaleChanged(float s);

  public slots:
    void changeScale(float s);

  protected slots:
    void reportChange();

  private:
    bool DaysOnly { false };
    float TimeScale[43];
    QStringList TimeString;
};
