/*
    SPDX-FileCopyrightText: 2010 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KColorScheme>

#include <QWidget>
#include <QCalendarWidget>

class KSMoon;
class KSSun;
class KStarsDateTime;

class MoonPhaseCalendar : public QCalendarWidget
{
    Q_OBJECT

  public:
    /**
     * Constructor
     * @param moon A reference to a (non-const) KSMoon object, that will be updated
     * @param sun A reference to a (non-const) KSSun object, that will be updated
     * @param parent Parent widget
     */
    explicit MoonPhaseCalendar(KSMoon &moon, KSSun &sun, QWidget *parent = nullptr);

    ~MoonPhaseCalendar();

    /** @return a suggested size for the widget */
    virtual QSize sizeHint() const;

  public slots:

    /**
     * Set the geometry of the moon phase calendar (overloaded from QWidget).
     * Resizes the cells so as to fill the space of the calendar.
     * @note This is called automatically by resize events.
     * @p x the x-position of the widget
     * @p y the y-position of the widget
     * @p w the width of the widget
     * @p h the height of the widget
     */
    virtual void setGeometry(int x, int y, int w, int h);

    virtual void setGeometry(const QRect &r);

  protected:
    /**
     * Overrides KDateTable::paintEvent() to draw moon phases on the
     * calendar cells by calling this->paintCell()
     * @note Most of this code is copied from KDateTable::paintEvent()
     */
    virtual void paintEvent(QPaintEvent *e);

    /**
     * Replaces KDateTable::paintCell() to draw moon phases on the calendar cells
     * @note Most of this code is copied from KDateTable::paintCell()
     */
    void paintCell(QPainter *painter, int row, int col, const KColorScheme &colorScheme);

    /**
     * @short Loads the moon images, appropriately resized depending
     * on the current cell size.
     *
     * @note This method is very slow and one must avoid calling it more than once.
     */
    void loadImages();

    /** @short Computes the optimum moon image size */
    void computeMoonImageSize();

  private:
    /**
     * @short Computes the moon phase for the given date.
     * @param date  Date / Time of the computation
     * @return the _integer_ phase for the given date
     */
    unsigned short computeMoonPhase(const KStarsDateTime &date);

    QPixmap m_Images[36]; // Array storing moon images against integer phase

    double cellWidth { 0 };
    double cellHeight { 0 };
    int numWeekRows { 0 };
    int numDayColumns { 0 };
    int MoonImageSize { 0 };
    bool imagesLoaded { false };

    KSMoon &m_Moon;
    KSSun &m_Sun;
};
