/*
    SPDX-FileCopyrightText: 2010 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "moonphasecalendarwidget.h"

#include "ksnumbers.h"
#include "kstarsdatetime.h"
#include "ksutils.h"
#include "texturemanager.h"
#include "skyobjects/ksmoon.h"
#include "skyobjects/ksplanet.h"
#include "skyobjects/kssun.h"

#include <kcalendarsystem.h>
#include <kcolorscheme.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <KLocale>

#include <QActionEvent>
#include <QDebug>
#include <QFontDatabase>
#include <QPainter>
#include <QStyle>
#include <QtGui/QStyleOptionViewItem>

#include <cmath>

MoonPhaseCalendar::MoonPhaseCalendar(KSMoon &moon, KSSun &sun, QWidget *parent)
    : KDateTable(parent), m_Moon(moon), m_Sun(sun)
{
    // Populate moon images from disk into the hash
    numDayColumns = calendar()->daysInWeek(QDate::currentDate());
    numWeekRows   = 7;
    imagesLoaded  = false;
    // TODO: Set geometry.
}

MoonPhaseCalendar::~MoonPhaseCalendar()
{
}

QSize MoonPhaseCalendar::sizeHint() const
{
    const int suggestedMoonImageSize = 50;
    return QSize(qRound((suggestedMoonImageSize + 2) * numDayColumns),
                 (qRound(suggestedMoonImageSize + 4 + 12) * numWeekRows)); // FIXME: Using hard-coded fontsize
}

void MoonPhaseCalendar::loadImages()
{
    computeMoonImageSize();
    qDebug() << "Loading moon images. MoonImageSize = " << MoonImageSize;
    for (int i = 0; i < 36; ++i)
    {
        QString imName = QString().sprintf("moon%02d", i);
        m_Images[i]    = QPixmap::fromImage(TextureManager::getImage(imName))
                          .scaled(MoonImageSize, MoonImageSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    imagesLoaded = true;
}

void MoonPhaseCalendar::computeMoonImageSize()
{
    cellWidth  = width() / (double)numDayColumns;
    cellHeight = height() / (double)numWeekRows;
    qDebug() << cellWidth << cellHeight;
    MoonImageSize =
        ((cellWidth > cellHeight - 12) ? cellHeight - 12 : cellWidth) - 2; // FIXME: Using hard-coded fontsize
}

void MoonPhaseCalendar::setGeometry(int, int, int, int)
{
    imagesLoaded = false;
}

void MoonPhaseCalendar::setGeometry(const QRect &r)
{
    setGeometry(r.x(), r.y(), r.width(), r.height()); // FIXME: +1 / -1 pixel compensation. Not required at the moment.
}

void MoonPhaseCalendar::paintEvent(QPaintEvent *e)
{
    QPainter p(this);
    if (!imagesLoaded)
        loadImages();
    KColorScheme colorScheme(palette().currentColorGroup(), KColorScheme::View);
    const QRect &rectToUpdate = e->rect();
    int leftCol               = (int)std::floor(rectToUpdate.left() / cellWidth);
    int topRow                = (int)std::floor(rectToUpdate.top() / cellHeight);
    int rightCol              = (int)std::ceil(rectToUpdate.right() / cellWidth);
    int bottomRow             = (int)std::ceil(rectToUpdate.bottom() / cellHeight);
    bottomRow                 = qMin(bottomRow, numWeekRows - 1);
    rightCol                  = qMin(rightCol, numDayColumns - 1);
    p.translate(leftCol * cellWidth, topRow * cellHeight);
    for (int i = leftCol; i <= rightCol; ++i)
    {
        for (int j = topRow; j <= bottomRow; ++j)
        {
            this->paintCell(&p, j, i, colorScheme);
            p.translate(0, cellHeight);
        }
        p.translate(cellWidth, 0);
        p.translate(0, -cellHeight * (bottomRow - topRow + 1));
    }
    p.end();
}

void MoonPhaseCalendar::paintCell(QPainter *painter, int row, int col, const KColorScheme &colorScheme)
{
    double w    = cellWidth - 1;
    double h    = cellHeight - 1;
    QRectF cell = QRectF(0, 0, w, h);
    QString cellText;
    QPen pen;
    QColor cellBackgroundColor, cellTextColor;
    QFont cellFont  = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    bool workingDay = false;
    int cellWeekDay, pos;

    //Calculate the position of the cell in the grid
    pos = numDayColumns * (row - 1) + col;

    //Calculate what day of the week the cell is
    cellWeekDay = col + calendar()->weekStartDay();
    if (cellWeekDay > numDayColumns)
    {
        cellWeekDay -= numDayColumns;
    }

    //See if cell day is normally a working day
    if (KLocale::global()->workingWeekStartDay() <= KLocale::global()->workingWeekEndDay())
    {
        workingDay = cellWeekDay >= KLocale::global()->workingWeekStartDay() &&
                     cellWeekDay <= KLocale::global()->workingWeekEndDay();
    }
    else
    {
        workingDay = cellWeekDay >= KLocale::global()->workingWeekStartDay() ||
                     cellWeekDay <= KLocale::global()->workingWeekEndDay();
    }

    if (row == 0)
    {
        //We are drawing a header cell

        //If not a normal working day, then use "do not work today" color
        if (workingDay)
        {
            cellTextColor = palette().color(QPalette::WindowText);
        }
        else
        {
            KColorScheme colorScheme(palette().currentColorGroup(), KColorScheme::Window);
            cellTextColor = colorScheme.foreground(KColorScheme::NegativeText).color();
        }
        cellBackgroundColor = palette().color(QPalette::Window);

        //Set the text to the short day name and bold it
        cellFont.setBold(true);
        cellText = calendar()->weekDayName(cellWeekDay, KCalendarSystem::ShortDayName);
    }
    else
    {
        //We are drawing a day cell

        //Calculate the date the cell represents
        QDate cellDate = dateFromPos(pos);

        bool validDay = calendar()->isValid(cellDate);

        // Draw the day number in the cell, if the date is not valid then we don't want to show it
        if (validDay)
        {
            cellText = calendar()->dayString(cellDate, KCalendarSystem::ShortFormat);
        }
        else
        {
            cellText = "";
        }

        if (!validDay || calendar()->month(cellDate) != calendar()->month(date()))
        {
            // we are either
            // ° painting an invalid day
            // ° painting a day of the previous month or
            // ° painting a day of the following month or
            cellBackgroundColor = palette().color(backgroundRole());
            cellTextColor       = colorScheme.foreground(KColorScheme::InactiveText).color();
        }
        else
        {
            //Paint a day of the current month

            // Background Colour priorities will be (high-to-low):
            // * Selected Day Background Colour
            // * Customized Day Background Colour
            // * Normal Day Background Colour

            // Background Shape priorities will be (high-to-low):
            // * Customized Day Shape
            // * Normal Day Shape

            // Text Colour priorities will be (high-to-low):
            // * Customized Day Colour
            // * Day of Pray Colour (Red letter)
            // * Selected Day Colour
            // * Normal Day Colour

            //Determine various characteristics of the cell date
            bool selectedDay = (cellDate == date());
            bool currentDay  = (cellDate == QDate::currentDate());
            bool dayOfPray   = (calendar()->dayOfWeek(cellDate) == KLocale::global()->weekDayOfPray());

            //Default values for a normal cell
            cellBackgroundColor = palette().color(backgroundRole());
            cellTextColor       = palette().color(foregroundRole());

            // If we are drawing the current date, then draw it bold and active
            if (currentDay)
            {
                cellFont.setBold(true);
                cellTextColor = colorScheme.foreground(KColorScheme::ActiveText).color();
            }

            // if we are drawing the day cell currently selected in the table
            if (selectedDay)
            {
                // set the background to highlighted
                cellBackgroundColor = palette().color(QPalette::Highlight);
                cellTextColor       = palette().color(QPalette::HighlightedText);
            }

            //If the cell day is the day of religious observance, then always color text red unless Custom overrides
            if (dayOfPray)
            {
                KColorScheme colorScheme(palette().currentColorGroup(),
                                         selectedDay ? KColorScheme::Selection : KColorScheme::View);
                cellTextColor = colorScheme.foreground(KColorScheme::NegativeText).color();
            }
        }
    }

    //Draw the background
    if (row == 0)
    {
        painter->setPen(cellBackgroundColor);
        painter->setBrush(cellBackgroundColor);
        painter->drawRect(cell);
    }
    else if (cellBackgroundColor != palette().color(backgroundRole()))
    {
        QStyleOptionViewItemV4 opt;
        opt.initFrom(this);
        opt.rect = cell.toRect();
        if (cellBackgroundColor != palette().color(backgroundRole()))
        {
            opt.palette.setBrush(QPalette::Highlight, cellBackgroundColor);
            opt.state |= QStyle::State_Selected;
        }
        if (false && opt.state & QStyle::State_Enabled)
        {
            opt.state |= QStyle::State_MouseOver;
        }
        else
        {
            opt.state &= ~QStyle::State_MouseOver;
        }
        opt.showDecorationSelected = true;
        opt.viewItemPosition       = QStyleOptionViewItemV4::OnlyOne;
        style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, this);
    }

    if (row != 0)
    {
        // Paint the moon phase
        QDate cellDate = dateFromPos(pos);
        if (calendar()->isValid(cellDate))
        {
            int iPhase     = computeMoonPhase(KStarsDateTime(cellDate, QTime(0, 0, 0)));
            QRect drawRect = cell.toRect();
            painter->drawPixmap((drawRect.width() - MoonImageSize) / 2,
                                12 + ((drawRect.height() - 12) - MoonImageSize) / 2,
                                m_Images[iPhase]); // FIXME: Using hard coded fon
                                                   // +            painter
            // painter->drawPixmap( ( drawRect.width() - MoonImageSize )/2,
            // 12 + (( drawRect.height() - 12 ) - MoonImageSize)/2,
            // m_Images[ iPhase ] );
            // FIXME: Using hard coded fontsize
            //            qDebug() << "Drew moon image " << iPhase;
        }
    }

    //Draw the text
    painter->setPen(cellTextColor);
    painter->setFont(cellFont);
    painter->drawText(cell, (row == 0) ? Qt::AlignCenter : (Qt::AlignTop | Qt::AlignHCenter), cellText, &cell);

    //Draw the base line
    if (row == 0)
    {
        painter->setPen(palette().color(foregroundRole()));
        painter->drawLine(QPointF(0, h), QPointF(w, h));
    }

    // If the day cell we just drew is bigger than the current max cell sizes,
    // then adjust the max to the current cell

    /*
    if ( cell.width() > d->maxCell.width() ) d->maxCell.setWidth( cell.width() );
    if ( cell.height() > d->maxCell.height() ) d->maxCell.setHeight( cell.height() );
    */
}

unsigned short MoonPhaseCalendar::computeMoonPhase(const KStarsDateTime &date)
{
    KSNumbers num(date.djd());
    KSPlanet earth(I18N_NOOP("Earth"), QString(), QColor("white"), 12756.28 /*diameter in km*/);
    earth.findPosition(&num);

    m_Sun.findPosition(
        &num, 0, 0,
        &earth); // Find position is overkill for this purpose. Wonder if it is worth making findGeocentricPosition public instead of protected.
    m_Moon.findGeocentricPosition(&num, &earth);

    m_Moon.findPhase(&m_Sun);

    return m_Moon.getIPhase();
}
