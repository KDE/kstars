/*
    SPDX-FileCopyrightText: 2008 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "calendarwidget.h"

#include "ksalmanac.h"
#include "kssun.h"
#include "kstarsdata.h"
#include "skycalendar.h"

#include <KLocalizedString>
#include <KPlotting/KPlotObject>

#include <QPainter>
#include <QDebug>

#define BIGTICKSIZE   10
#define SMALLTICKSIZE 4

CalendarWidget::CalendarWidget(QWidget *parent) : KPlotWidget(parent)
{
    setAntialiasing(true);

    setTopPadding(40);
    setLeftPadding(60);
    setRightPadding(100);

    maxRTime = 12.0;
    minSTime = -12.0;
}

void CalendarWidget::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e)

    QPainter p;

    p.begin(this);
    p.setRenderHint(QPainter::Antialiasing, antialiasing());
    p.fillRect(rect(), backgroundColor());
    p.translate(leftPadding(), topPadding());

    setPixRect();
    p.setClipRect(pixRect());
    p.setClipping(true);

    drawHorizon(&p);

    foreach (KPlotObject *po, plotObjects())
    {
        po->draw(&p, this);
    }

    p.setClipping(false);
    drawAxes(&p);
}

void CalendarWidget::setHorizon()
{
    KSSun thesun;
    SkyCalendar *skycal = (SkyCalendar *)topLevelWidget();
    KStarsDateTime kdt(QDate(skycal->year(), 1, 1), QTime(12, 0, 0));

    maxRTime = 0.0;
    minSTime = 0.0;

    // Clear date, rise and set time lists
    dateList.clear();
    riseTimeList.clear();
    setTimeList.clear();

    float rTime, sTime;

    // Get rise and set time every 7 days for 1 year
    while (skycal->year() == kdt.date().year())
    {
        QTime tmp_rTime = thesun.riseSetTime(KStarsDateTime(kdt.djd() + 1.0), skycal->get_geo(), true, true);
        QTime tmp_sTime = thesun.riseSetTime(KStarsDateTime(kdt.djd()), skycal->get_geo(), false, true);

        /* riseSetTime seems buggy since it sometimes returns the same time for rise and set (01:00:00).
         * In this case, we just reset tmp_rTime and tmp_sTime so they will be considered invalid
         * in the following lines.
         * NOTE: riseSetTime should be fix now, this test is no longer necessary*/
        if (tmp_rTime == tmp_sTime)
        {
            tmp_rTime = QTime();
            tmp_sTime = QTime();
        }

        // If rise and set times are valid, the sun rise and set...
        if (tmp_rTime.isValid() && tmp_sTime.isValid())
        {
            // Compute X-coordinate value for rise and set time
            QTime midday(12, 0, 0);
            rTime = tmp_rTime.secsTo(midday) * 24.0 / 86400.0;
            sTime = tmp_sTime.secsTo(midday) * 24.0 / 86400.0;

            if (tmp_rTime <= midday)
                rTime = 12.0 - rTime;
            else
                rTime = -12.0 - rTime;

            if (tmp_sTime <= midday)
                sTime = 12.0 - sTime;
            else
                sTime = -12.0 - sTime;
        }
        /* else, the sun don't rise and/or don't set.
         * we look at the altitude of the sun at transit time, if it is above the horizon,
         * there is no night, else there is no day. */
        else
        {
            if (thesun.transitAltitude(KStarsDateTime(kdt.djd()), skycal->get_geo()).degree() > 0)
            {
                rTime = -4.0;
                sTime = 4.0;
            }
            else
            {
                rTime = 12.0;
                sTime = -12.0;
            }
        }

        // Get max rise time and min set time
        if (rTime > maxRTime)
            maxRTime = rTime;
        if (sTime < minSTime)
            minSTime = sTime;

        // Keep the day, rise time and set time in lists
        dateList.append(kdt.date());
        riseTimeList.append(rTime);
        setTimeList.append(sTime);

        // Next week
        kdt = kdt.addDays(skycal->scUI->spinBox_Interval->value());
    }

    // Set widget limits
    maxRTime = ceil(maxRTime) + 1.0;
    if ((int)maxRTime % 2 != 0)
        maxRTime++;
    if (maxRTime > 12.0)
        maxRTime = 12.0;
    minSTime = floor(minSTime) - 1.0;
    if ((int)minSTime % 2 != 0)
        minSTime--;
    if (minSTime < -12.0)
        minSTime = -12.0;
    setLimits(minSTime, maxRTime, 0.0, 366.0);
    setPixRect();
}

void CalendarWidget::drawHorizon(QPainter *p)
{
    polySunRise.clear();
    polySunSet.clear();

    for (int date = 0; date < dateList.size(); date++)
    {
        int day = dateList.at(date).daysInYear() - dateList.at(date).dayOfYear();
        polySunRise << mapToWidget(QPointF(riseTimeList.at(date), day));
        polySunSet << mapToWidget(QPointF(setTimeList.at(date), day));
    }

    //Finish polygons by adding pixRect corners
    polySunRise << mapToWidget(QPointF(riseTimeList.last(), dataRect().top()))
                << mapToWidget(QPointF(dataRect().right(), dataRect().top()))
                << mapToWidget(QPointF(dataRect().right(), dataRect().bottom()))
                << mapToWidget(QPointF(riseTimeList.first(), dataRect().bottom()));
    polySunSet << mapToWidget(QPointF(setTimeList.last(), dataRect().top()))
               << mapToWidget(QPointF(dataRect().left(), pixRect().top()))
               << mapToWidget(QPointF(dataRect().left(), pixRect().bottom()))
               << mapToWidget(QPointF(setTimeList.first(), dataRect().bottom()));

    p->setPen(Qt::darkGreen);
    p->setBrush(Qt::darkGreen);
    p->drawPolygon(polySunRise);
    p->drawPolygon(polySunSet);
}

void CalendarWidget::drawAxes(QPainter *p)
{
    SkyCalendar *skycal = (SkyCalendar *)topLevelWidget();

    p->setPen(foregroundColor());
    p->setBrush(Qt::NoBrush);

    //Draw bounding box for the plot
    p->drawRect(pixRect());

    //set small font for axis labels
    QFont f = p->font();
    int s   = f.pointSize();
    f.setPointSize(s - 2);
    p->setFont(f);

    // Top axis label
    p->drawText(0, -38, pixRect().width(), pixRect().height(), Qt::AlignHCenter | Qt::AlignTop | Qt::TextDontClip,
                i18n("Local time"));
    // Bottom axis label
    p->drawText(0, 0, pixRect().width(), pixRect().height() + 35, Qt::AlignHCenter | Qt::AlignBottom | Qt::TextDontClip,
                i18n("Universal time"));
    // Left axis label
    p->save();
    p->rotate(90.0);
    p->drawText(0, 0, pixRect().height(), leftPadding() - 5, Qt::AlignHCenter | Qt::AlignBottom | Qt::TextDontClip,
                i18n("Month"));
    // Right axis label
    p->translate(0.0, -1 * frameRect().width() + 30);
    p->drawText(0, 0, pixRect().height(), leftPadding() - 5, Qt::AlignHCenter | Qt::AlignBottom | Qt::TextDontClip,
                i18n("Julian date"));
    p->restore();

    //Top/Bottom axis tickmarks and time labels
    for (float xx = minSTime; xx <= maxRTime; xx += 1.0)
    {
        int h = int(xx);
        if (h < 0)
            h += 24;
        QTime time(h, 0, 0);
        QString sTime = QLocale().toString(time, "hh:mm");

        QString sUtTime = QLocale().toString(time.addSecs(skycal->get_geo()->TZ() * -3600), "hh:mm");

        // Draw a small tick every hours and a big tick every two hours.
        QPointF pBottomTick = mapToWidget(QPointF(xx, dataRect().y()));
        QPointF pTopTick    = QPointF(pBottomTick.x(), 0.0);
        if (h % 2)
        {
            // Draw small bottom tick
            p->drawLine(pBottomTick, QPointF(pBottomTick.x(), pBottomTick.y() - SMALLTICKSIZE));
            // Draw small top tick
            p->drawLine(pTopTick, QPointF(pTopTick.x(), pTopTick.y() + SMALLTICKSIZE));
        }
        else
        {
            // Draw big bottom tick
            p->drawLine(pBottomTick, QPointF(pBottomTick.x(), pBottomTick.y() - BIGTICKSIZE));
            QRectF r(pBottomTick.x() - BIGTICKSIZE, pBottomTick.y() + 0.5 * BIGTICKSIZE, 2 * BIGTICKSIZE, BIGTICKSIZE);
            p->drawText(r, Qt::AlignCenter | Qt::TextDontClip, sUtTime);
            // Draw big top tick
            p->drawLine(pTopTick, QPointF(pTopTick.x(), pTopTick.y() + BIGTICKSIZE));
            r.moveTop(-2.0 * BIGTICKSIZE);
            p->drawText(r, Qt::AlignCenter | Qt::TextDontClip, sTime);
        }

        // Vertical grid
        if (skycal->scUI->checkBox_GridVertical->isChecked())
        {
            QColor c = p->pen().color();
            c.setAlpha(100);
            p->setPen(c);
            p->drawLine(pTopTick, pBottomTick);
            c.setAlpha(255);
            p->setPen(c);
        }
    }

    // Month dividers
    int y = skycal->year();
    for (int imonth = 2; imonth <= 12; ++imonth)
    {
        QDate dt(y, imonth, 1);
        float doy = float(dt.daysInYear() - dt.dayOfYear());

        // Draw a tick every months on left axis
        QPointF pMonthTick = mapToWidget(QPointF(dataRect().x(), doy));
        p->drawLine(pMonthTick, QPointF(pMonthTick.x() + BIGTICKSIZE, pMonthTick.y()));

        // Draw month labels
        QRectF rMonth(mapToWidget(QPointF(0.0, float(dt.daysInYear() - dt.addMonths(-1).dayOfYear()))),
                      mapToWidget(QPointF(dataRect().left() - 0.1, doy)));
        QLocale locale;
        p->drawText(rMonth, Qt::AlignRight | Qt::AlignVCenter | Qt::TextDontClip, locale.monthName(imonth - 1, QLocale::ShortFormat));
        if (imonth == 12) // December
        {
            rMonth = QRectF(mapToWidget(QPointF(0.0, doy)), mapToWidget(QPointF(dataRect().left() - 0.1, 0.0)));
            p->drawText(rMonth, Qt::AlignRight | Qt::AlignVCenter | Qt::TextDontClip, locale.monthName(imonth, QLocale::ShortFormat));
        }

        // Draw dividers
        if (skycal->scUI->checkBox_GridMonths->isChecked())
        {
            QColor c = p->pen().color();
            c.setAlpha(100);
            p->setPen(c);
            p->drawLine(pMonthTick, QPointF(pixRect().right(), pMonthTick.y()));
            c.setAlpha(255);
            p->setPen(c);
        }
    }

    // Interval dividers
    QFont origFont = p->font();
    p->setFont(QFont("Monospace", origFont.pointSize() - 1));
    for (KStarsDateTime kdt(QDate(y, 1, 1), QTime(12, 0, 0)); kdt.date().year() == y;
         kdt = kdt.addDays(skycal->scUI->spinBox_Interval->value() > 7 ? skycal->scUI->spinBox_Interval->value() : 7))
    {
        // Draw ticks
        float doy         = float(kdt.date().daysInYear() - kdt.date().dayOfYear());
        QPointF pWeekTick = mapToWidget(QPointF(dataRect().right(), doy));
        p->drawLine(pWeekTick, QPointF(pWeekTick.x() - BIGTICKSIZE, pWeekTick.y()));

        // Draw julian date
        QRectF rJd(mapToWidget(QPointF(dataRect().right() + 0.1, doy + 2)),
                   mapToWidget(QPointF(dataRect().right(), doy)));
        p->drawText(rJd, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip,
                    QString().setNum(double(kdt.djd()), 'f', 1));

        // Draw dividers
        if (skycal->scUI->checkBox_GridWeeks->isChecked())
        {
            QColor c = p->pen().color();
            c.setAlpha(50);
            p->setPen(c);
            p->drawLine(pWeekTick, QPointF(pixRect().left(), pWeekTick.y()));
            c.setAlpha(255);
            p->setPen(c);
        }
    }

    // Current day
    if (skycal->scUI->checkBox_GridToday->isChecked())
    {
        p->setPen(QColor(Qt::yellow));
        QDate today = QDate::currentDate();
        float doy   = float(today.daysInYear() - today.dayOfYear());
        p->drawLine(mapToWidget(QPointF(dataRect().left(), doy)), mapToWidget(QPointF(dataRect().right(), doy)));
        p->drawText(mapToWidget(QPointF(dataRect().left() + 0.1, doy + 2.0)), today.toString());
    }

    //Draw month labels along each horizon curve
    //     if ( skycal->scUI->checkBox_LabelMonths->isChecked() ) {
    //         p->setFont( QFont( "Monospace", origFont.pointSize() + 5 ) );
    //         int textFlags = Qt::TextSingleLine | Qt::AlignCenter;
    //         QFontMetricsF fm( p->font(), p->device() );
    //
    //         for ( int date=0; date<dateList.size(); date++ ) {
    //             if ( dateList.at( date ).day() < 12 || dateList.at( date ).day() > 18 )
    //                 continue;
    //
    //             bool noNight = false;
    //             if ( riseTimeList.at( date ) < setTimeList.at( date ) )
    //                 noNight = true;
    //
    //             int imonth = dateList.at( date ).month();
    //
    //             QString shortMonthName = QDate::shortMonthName( dateList.at( date ).month() );
    //             QRectF riseLabelRect = fm.boundingRect( QRectF(0,0,1,1), textFlags, shortMonthName );
    //             QRectF setLabelRect = fm.boundingRect( QRectF(0,0,1,1), textFlags, shortMonthName );
    //
    //             QDate dt( y, imonth, 15 );
    //             float doy = float( dt.daysInYear() - dt.dayOfYear() );
    //             float xRiseLabel, xSetLabel;
    //             if ( noNight ) {
    //                 xRiseLabel = 0.0;
    //                 xSetLabel = 0.0;
    //             } else {
    //                 xRiseLabel = riseTimeList.at( date ) + 0.6;
    //                 xSetLabel = setTimeList.at( date )- 0.6;
    //             }
    //             QPointF pRiseLabel = mapToWidget( QPointF( xRiseLabel, doy ) );
    //             QPointF pSetLabel = mapToWidget( QPointF( xSetLabel, doy ) );
    //
    //             //Determine rotation angle for month labels
    //             QDate dt1( y, imonth, 1 );
    //             float doy1 = float( dt1.daysInYear() - dt1.dayOfYear() );
    //             QDate dt2( y, imonth, dt1.daysInMonth() );
    //             float doy2 = float( dt2.daysInYear() - dt2.dayOfYear() );
    //
    //             QPointF p1, p2;
    //             float rAngle, sAngle;
    //             if ( noNight ) {
    //                 rAngle = 90.0;
    //             } else {
    //                 p1 = mapToWidget( QPointF( riseTimeList.at( date-2 ), doy1 ) );
    //                 p2 = mapToWidget( QPointF( riseTimeList.at( date+2 ), doy2 ) );
    //                 rAngle = atan2( p2.y() - p1.y(), p2.x() - p1.x() )/dms::DegToRad;
    //
    //                 p1 = mapToWidget( QPointF( setTimeList.at( date-2 ), doy1 ) );
    //                 p2 = mapToWidget( QPointF( setTimeList.at( date+2 ), doy2 ) );
    //                 sAngle = atan2( p2.y() - p1.y(), p2.x() - p1.x() )/dms::DegToRad;
    //             }
    //
    //             p->save();
    //             p->translate( pRiseLabel );
    //             p->rotate( rAngle );
    //             p->drawText( riseLabelRect, textFlags, shortMonthName );
    //             p->restore();
    //
    //             if ( ! noNight ) {
    //                 p->save();
    //                 p->translate( pSetLabel );
    //                 p->rotate( sAngle );
    //                 p->drawText( setLabelRect, textFlags, shortMonthName );
    //                 p->restore();
    //             }
    //         }
    //     }

    p->setFont(origFont);
}
