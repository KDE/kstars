/*
    SPDX-FileCopyrightText: 2010 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "genericcalendarwidget.h"

#include <KNotification>
#include <KCalendarSystem>

#include <QDebug>

GenericCalendarWidget::GenericCalendarWidget(KDateTable &datetable, QWidget *parent)
    : QWidget(parent), m_DateTable(datetable)
{
    setupUi(this);

    m_DateTable.setParent(DateTableFrame); // Put the date table in the QFrame that's meant to hold it

    // Set icons for the front / back buttons

    previousYear->setAutoRaise(true);
    nextYear->setAutoRaise(true);
    previousMonth->setAutoRaise(true);
    nextMonth->setAutoRaise(true);
    if (QApplication::isRightToLeft())
    {
        nextYear->setIcon(QIcon::fromTheme(QLatin1String("arrow-left-double")));
        previousYear->setIcon(QIcon::fromTheme(QLatin1String("arrow-right-double")));
        nextMonth->setIcon(QIcon::fromTheme(QLatin1String("arrow-left")));
        previousMonth->setIcon(QIcon::fromTheme(QLatin1String("arrow-right")));
    }
    else
    {
        nextYear->setIcon(QIcon::fromTheme(QLatin1String("arrow-right-double")));
        previousYear->setIcon(QIcon::fromTheme(QLatin1String("arrow-left-double")));
        nextMonth->setIcon(QIcon::fromTheme(QLatin1String("arrow-right")));
        previousMonth->setIcon(QIcon::fromTheme(QLatin1String("arrow-left")));
    }

    // Connects
    connect(&m_DateTable, SIGNAL(dateChanged(QDate)), SLOT(dateChangedSlot(QDate)));
    connect(nextMonth, SIGNAL(clicked()), SLOT(nextMonthClicked()));
    connect(previousMonth, SIGNAL(clicked()), SLOT(previousMonthClicked()));
    connect(nextYear, SIGNAL(clicked()), SLOT(nextYearClicked()));
    connect(previousYear, SIGNAL(clicked()), SLOT(previousYearClicked()));
    connect(selectMonth, SIGNAL(activated(int)), SLOT(monthChanged(int)));
    connect(selectYear, SIGNAL(valueChanged(int)), SLOT(yearChanged(int)));

    m_DateTable.setCalendar(); // Set global calendar

    populateMonthNames();

    //    qDebug() << calendar()->monthName( date(), KCalendarSystem::LongName );

    selectMonth->setCurrentIndex(date().month() - 1);
    selectYear->setValue(date().year());
    m_Date = date();

    show();
}

const QDate &GenericCalendarWidget::date() const
{
    return m_DateTable.date();
}

const KCalendarSystem *GenericCalendarWidget::calendar() const
{
    return m_DateTable.calendar();
}

void GenericCalendarWidget::populateMonthNames()
{
    // Populate the combobox with month names -- can change by year / calendar type
    selectMonth->clear();
    for (int m = 1; m <= calendar()->monthsInYear(date()); m++)
    {
        selectMonth->addItem(calendar()->monthName(m, calendar()->year(date())));
    }
}

void GenericCalendarWidget::dateChangedSlot(const QDate &date_)
{
    //    populateMonthNames(); // Not required for global calendar

    if (m_Date != date_)
    {
        // To avoid an infinite loop, we update the year spin box / month combo box only when the date has actually changed.
        selectMonth->setCurrentIndex(date().month() - 1);
        selectYear->setValue(date().year());
        m_Date = date_;
    }

    qDebug() << "Date = " << m_Date;

    emit(dateChanged(date_));
}

void GenericCalendarWidget::nextMonthClicked()
{
    if (!setDate(calendar()->addMonths(date(), 1)))
    {
        KNotification::beep();
    }
    m_DateTable.setFocus();
}

void GenericCalendarWidget::previousMonthClicked()
{
    qDebug() << "Previous month clicked!";
    if (!setDate(calendar()->addMonths(date(), -1)))
    {
        KNotification::beep();
    }
    m_DateTable.setFocus();
}

void GenericCalendarWidget::nextYearClicked()
{
    if (!setDate(calendar()->addYears(date(), 1)))
    {
        KNotification::beep();
    }
    m_DateTable.setFocus();
}

void GenericCalendarWidget::previousYearClicked()
{
    if (!setDate(calendar()->addYears(date(), -1)))
    {
        KNotification::beep();
    }
    m_DateTable.setFocus();
}

void GenericCalendarWidget::yearChanged(int year)
{
    if (!setYear(year))
    {
        KNotification::beep();
    }
    m_DateTable.setFocus();
}

void GenericCalendarWidget::monthChanged(int month)
{
    qDebug() << "Month = " << month;
    if (!setMonth(month + 1))
    {
        KNotification::beep();
    }
    m_DateTable.setFocus();
}

bool GenericCalendarWidget::setYear(int year)
{
    return setDate(QDate(year, date().month(), date().day()));
}

bool GenericCalendarWidget::setMonth(int month)
{
    return setDate(QDate(date().year(), month, date().day()));
}

bool GenericCalendarWidget::setDate(int date_)
{
    return setDate(QDate(date().year(), date().month(), date_));
}

bool GenericCalendarWidget::setDate(const QDate &date_)
{
    if (date_ == date())
        return true;
    return m_DateTable.setDate(date_);
}
