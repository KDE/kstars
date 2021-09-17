/*
    SPDX-FileCopyrightText: 2007 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "avtplotwidget.h"

#include "kstarsdata.h"
#include "Options.h"

#include <QWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QTime>
#include <QLinearGradient>

#include <KLocalizedString>
#include <KPlotting/KPlotObject>
#include <QDebug>

AVTPlotWidget::AVTPlotWidget(QWidget *parent) : KPlotWidget(parent)
{
    setAntialiasing(true);

    MousePoint = QPoint(-1, -1);
}

void AVTPlotWidget::mousePressEvent(QMouseEvent *e)
{
    mouseMoveEvent(e);
}

void AVTPlotWidget::mouseDoubleClickEvent(QMouseEvent *)
{
    MousePoint = QPoint(-1, -1);
    update();
}

void AVTPlotWidget::mouseMoveEvent(QMouseEvent *e)
{
    QRect checkRect(leftPadding(), topPadding(), pixRect().width(), pixRect().height());
    int Xcursor = e->x();
    int Ycursor = e->y();

    if (!checkRect.contains(e->x(), e->y()))
    {
        if (e->x() < checkRect.left())
            Xcursor = checkRect.left();
        if (e->x() > checkRect.right())
            Xcursor = checkRect.right();
        if (e->y() < checkRect.top())
            Ycursor = checkRect.top();
        if (e->y() > checkRect.bottom())
            Ycursor = checkRect.bottom();
    }

    Xcursor -= leftPadding();
    Ycursor -= topPadding();

    MousePoint = QPoint(Xcursor, Ycursor);
    update();
}

void AVTPlotWidget::paintEvent(QPaintEvent *e)
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

    int pW = pixRect().width();
    int pH = pixRect().height();

    QColor SkyColor(0, 100, 200);
    /*
    if (Options::darkAppColors())
        SkyColor = QColor(200, 0, 0); // use something red, visible through a red filter
        */

    // Draw gradient representing lunar interference in the sky
    if (MoonIllum > 0.01) // do this only if Moon illumination is reasonable so it's important
    {
        int moonrise = int(pW * (0.5 + MoonRise));
        int moonset  = int(pW * (MoonSet - 0.5));
        if (moonset < 0)
            moonset += pW;
        if (moonrise > pW)
            moonrise -= pW;
        int moonalpha = int(10 + MoonIllum * 130);
        int fadewidth =
            pW *
            0.01; // pW * fraction of day to fade the moon brightness over (0.01 corresponds to roughly 15 minutes, 0.007 to 10 minutes), both before and after actual set.
        QColor MoonColor(255, 255, 255, moonalpha);

        if (moonset < moonrise)
        {
            QLinearGradient grad =
                QLinearGradient(QPointF(moonset - fadewidth, 0.0), QPointF(moonset + fadewidth, 0.0));
            grad.setColorAt(0, MoonColor);
            grad.setColorAt(1, Qt::transparent);
            p.fillRect(QRectF(0.0, 0.0, moonset + fadewidth, pH),
                       grad); // gradient should be padded until moonset - fadewidth (see QLinearGradient docs)
            grad.setStart(QPointF(moonrise + fadewidth, 0.0));
            grad.setFinalStop(QPointF(moonrise - fadewidth, 0.0));
            p.fillRect(QRectF(moonrise - fadewidth, 0.0, pW - moonrise + fadewidth, pH), grad);
        }
        else
        {
            p.fillRect(QRectF(moonrise + fadewidth, 0.0, moonset - moonrise - 2 * fadewidth, pH), MoonColor);
            QLinearGradient grad =
                QLinearGradient(QPointF(moonrise + fadewidth, 0.0), QPointF(moonrise - fadewidth, 0.0));
            grad.setColorAt(0, MoonColor);
            grad.setColorAt(1, Qt::transparent);
            p.fillRect(QRectF(0.0, 0.0, moonrise + fadewidth, pH), grad);
            grad.setStart(QPointF(moonset - fadewidth, 0.0));
            grad.setFinalStop(QPointF(moonset + fadewidth, 0.0));
            p.fillRect(QRectF(moonset - fadewidth, 0.0, pW - moonset, pH), grad);
        }
    }
    //draw daytime sky if the Sun rises for the current date/location
    if (SunMaxAlt > -18.0)
    {
        //Display centered on midnight, so need to modulate dawn/dusk by 0.5
        int rise = int(pW * (0.5 + SunRise));
        int set  = int(pW * (SunSet - 0.5));
        int da   = int(pW * (0.5 + Dawn));
        int du   = int(pW * (Dusk - 0.5));

        if (SunMinAlt > 0.0)
        {
            // The sun never set and the sky is always blue
            p.fillRect(rect(), SkyColor);
        }
        else if (SunMaxAlt < 0.0 && SunMinAlt < -18.0)
        {
            // The sun never rise but the sky is not completely dark
            QLinearGradient grad = QLinearGradient(QPointF(0.0, 0.0), QPointF(du, 0.0));

            QColor gradStartColor = SkyColor;
            gradStartColor.setAlpha((1 - (SunMaxAlt / -18.0)) * 255);

            grad.setColorAt(0, gradStartColor);
            grad.setColorAt(1, Qt::transparent);
            p.fillRect(QRectF(0.0, 0.0, du, pH), grad);
            grad.setStart(QPointF(pW, 0.0));
            grad.setFinalStop(QPointF(da, 0.0));
            p.fillRect(QRectF(da, 0.0, pW, pH), grad);
        }
        else if (SunMaxAlt < 0.0 && SunMinAlt > -18.0)
        {
            // The sun never rise but the sky is NEVER completely dark
            QLinearGradient grad = QLinearGradient(QPointF(0.0, 0.0), QPointF(pW, 0.0));

            QColor gradStartEndColor = SkyColor;
            gradStartEndColor.setAlpha((1 - (SunMaxAlt / -18.0)) * 255);
            QColor gradMidColor = SkyColor;
            gradMidColor.setAlpha((1 - (SunMinAlt / -18.0)) * 255);

            grad.setColorAt(0, gradStartEndColor);
            grad.setColorAt(0.5, gradMidColor);
            grad.setColorAt(1, gradStartEndColor);
            p.fillRect(QRectF(0.0, 0.0, pW, pH), grad);
        }
        else if (Dawn < 0.0)
        {
            // The sun sets and rises but the sky is never completely dark
            p.fillRect(0, 0, set, int(0.5 * pH), SkyColor);
            p.fillRect(rise, 0, pW, int(0.5 * pH), SkyColor);

            QLinearGradient grad = QLinearGradient(QPointF(set, 0.0), QPointF(rise, 0.0));

            QColor gradMidColor = SkyColor;
            gradMidColor.setAlpha((1 - (SunMinAlt / -18.0)) * 255);

            grad.setColorAt(0, SkyColor);
            grad.setColorAt(0.5, gradMidColor);
            grad.setColorAt(1, SkyColor);
            p.fillRect(QRectF(set, 0.0, rise - set, pH), grad);
        }
        else
        {
            p.fillRect(0, 0, set, pH, SkyColor);
            p.fillRect(rise, 0, pW, pH, SkyColor);

            QLinearGradient grad = QLinearGradient(QPointF(set, 0.0), QPointF(du, 0.0));
            grad.setColorAt(0, SkyColor);
            grad.setColorAt(
                1,
                Qt::transparent); // FIXME?: The sky appears black well before the actual end of twilight if the gradient is too slow (eg: latitudes above arctic circle)
            p.fillRect(QRectF(set, 0.0, du - set, pH), grad);

            grad.setStart(QPointF(rise, 0.0));
            grad.setFinalStop(QPointF(da, 0.0));
            p.fillRect(QRectF(da, 0.0, rise - da, pH), grad);
        }
    }

    //draw ground
    p.fillRect(0, int(0.5 * pH), pW, int(0.5 * pH),
               KStarsData::Instance()->colorScheme()->colorNamed(
                   "HorzColor")); // asimha changed to use color from scheme. Formerly was QColor( "#002200" )

    foreach (KPlotObject *po, plotObjects())
    {
        po->draw(&p, this);
    }

    p.setClipping(false);
    drawAxes(&p);

    //Add vertical line indicating "now"
    QFont smallFont = p.font();
    smallFont.setPointSize(smallFont.pointSize()); // wat?
    if (geo)
    {
        QTime t = geo->UTtoLT(KStarsDateTime::currentDateTimeUtc())
                      .time(); // convert the current system clock time to the TZ corresponding to geo
        double x = 12.0 + t.hour() + t.minute() / 60.0 + t.second() / 3600.0;
        while (x > 24.0)
            x -= 24.0;
        int ix = int(x * pW / 24.0); //convert to screen pixel coords
        p.setPen(QPen(QBrush("white"), 2.0, Qt::DotLine));
        p.drawLine(ix, 0, ix, pH);

        //Label this vertical line with the current time
        p.save();
        p.setFont(smallFont);
        p.translate(ix + 10, pH - 20);
        p.rotate(-90);
        p.drawText(
            0, 0,
            QLocale().toString(t, QLocale::ShortFormat)); // short format necessary to avoid false time-zone labeling
        p.restore();
    }

    //Draw crosshairs at clicked position
    if (MousePoint.x() > 0)
    {
        p.setPen(QPen(QBrush("gold"), 1.0, Qt::SolidLine));
        p.drawLine(QLineF(MousePoint.x() + 0.5, 0.5, MousePoint.x() + 0.5, pixRect().height() - 0.5));
        p.drawLine(QLineF(0.5, MousePoint.y() + 0.5, pixRect().width() - 0.5, MousePoint.y() + 0.5));

        //Label each crosshair line (time and altitude)
        p.setFont(smallFont);
        double a = (pH - MousePoint.y()) * 180.0 / pH - 90.0;
        p.drawText(20, MousePoint.y() + 10, QString::number(a, 'f', 2) + QChar(176));

        double h = MousePoint.x() * 24.0 / pW - 12.0;
        if (h < 0.0)
            h += 24.0;
        QTime t = QTime(int(h), int(60. * (h - int(h))));
        p.save();
        p.translate(MousePoint.x() + 10, pH - 20);
        p.rotate(-90);
        p.drawText(
            0, 0,
            QLocale().toString(t, QLocale::ShortFormat)); // short format necessary to avoid false time-zone labeling
        p.restore();
    }

    p.end();
}

void AVTPlotWidget::setDawnDuskTimes(double da, double du)
{
    Dawn = da;
    Dusk = du;
    update(); // fixme: should we always be calling update? It's probably cheap enough that we can.
}

void AVTPlotWidget::setMinMaxSunAlt(double min, double max)
{
    SunMinAlt = min;
    SunMaxAlt = max;
    update();
}

void AVTPlotWidget::setSunRiseSetTimes(double sr, double ss)
{
    SunRise = sr;
    SunSet  = ss;
    update();
}

void AVTPlotWidget::setMoonRiseSetTimes(double mr, double ms)
{
    MoonRise = mr;
    MoonSet  = ms;
    update();
}

void AVTPlotWidget::setMoonIllum(double mi)
{
    MoonIllum = mi;
    update();
}
