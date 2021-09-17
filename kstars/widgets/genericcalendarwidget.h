/*
    SPDX-FileCopyrightText: 2010 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_genericcalendarwidget.h"

#include <QDate>

/**
 *@class GenericCalendarWidget
 *@author Akarsh Simha
 *@version 1.0
 *@short Uses any KDateTable subclass for the date table and provides a calendar with options to choose a month / year.
 *@note This supports only the Gregorian calendar system for simplicity.
 *@note This shouldn't be the way to do things in the first place, but
 *      this is until KDatePicker starts supporting overridden
 *      KDateTables and the like
 */

class GenericCalendarWidget : public QWidget, public Ui::GenericCalendarWidgetUi
{
    Q_OBJECT

  public:
    /** @short Constructor. Sets up the Ui, connects signals to slots etc. */
    explicit GenericCalendarWidget(KDateTable &dateTable, QWidget *parent = nullptr);

    /** @short Empty destructor */
    virtual ~GenericCalendarWidget() override = default;

    /** @short Returns the selected date */
    const QDate &date() const;

  public slots:
    /**
     * @short Set the month
     * @param month  The month to choose
     */
    bool setMonth(int month);

    /**
     * @short Set the year
     * @param year  The year to choose
     */
    bool setYear(int year);

    /**
     * @short Set the selected day of month
     * @param date_ The date of the month to choose
     */
    bool setDate(int date_);

    /**
     * @short Set the selected date
     * @param date_ The date to set
     */
    bool setDate(const QDate &date_);

  signals:
    /**
     * @short Emitted when the date has been changed
     * @note Emitted by dateTableDateChanged() which subscribes to KDateTable::dateChanged()
     */
    void dateChanged(const QDate &date_);

  private slots:

    /** @short Called when the previous / next year / month buttons are clicked */
    void previousYearClicked();
    void nextYearClicked();
    void previousMonthClicked();
    void nextMonthClicked();

    /** @short Called when the year changes through the spin box */
    void yearChanged(int year);

    /** @short Called when the month changes through the combo box */
    void monthChanged(int month);

    /**
     * @short Subscribes to KDateTable::dateChanged() and echoes it by emitting this widget's dateChanged signals
     * @note For details on parameters, see KDateTable::dateChanged()
     */
    void dateChangedSlot(const QDate &date_);

  private:
    /** @short Fills the month combo box with month names */
    void populateMonthNames();

    /** @short Returns a link to the KCalendarSystem in use */
    const KCalendarSystem *calendar() const;

    QDate m_Date;
};
