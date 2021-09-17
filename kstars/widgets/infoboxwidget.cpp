/*
    SPDX-FileCopyrightText: 2009 Khudyakov Alexey <alexey.skladnoy@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "infoboxwidget.h"

#include "colorscheme.h"
#include "kstarsdata.h"
#include "ksutils.h"

#include <KLocalizedString>

#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>
#include <QLocale>

const int InfoBoxWidget::padX = 6;
const int InfoBoxWidget::padY = 2;

InfoBoxes::InfoBoxes(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
}

void InfoBoxes::addInfoBox(InfoBoxWidget *ibox)
{
    ibox->setParent(this);
    m_boxes.append(ibox);
}

void InfoBoxes::resizeEvent(QResizeEvent *)
{
    foreach (InfoBoxWidget *w, m_boxes)
        w->adjust();
}

/* ================================================================ */

InfoBoxWidget::InfoBoxWidget(bool shade, const QPoint &pos, int anchor, const QStringList &str, QWidget *parent)
    : QWidget(parent), m_strings(str), m_adjusted(false), m_grabbed(false), m_shaded(shade), m_anchor(anchor)
{
    move(pos);
    updateSize();
}

void InfoBoxWidget::updateSize()
{
    QFontMetrics fm(font());
    int w = 0;
    foreach (const QString &str, m_strings)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5,11,0)
        w = qMax(w, fm.horizontalAdvance(str));
#else
        w = qMax(w, fm.width(str));
#endif
    }

    int h = fm.height() * (m_shaded ? 1 : m_strings.size());
    // Add padding
    resize(w + 2 * padX, h + 2 * padY + 2);
    adjust();
}

void InfoBoxWidget::slotTimeChanged()
{
    KStarsData *data = KStarsData::Instance();

    m_strings.clear();
    m_strings
            << i18nc("Local Time", "LT: ") + data->lt().time().toString(QLocale().timeFormat().remove('t')) +
            "   " + // Remove timezone, as timezone of geolocation in KStars might not be same as system locale timezone
            QLocale().toString(data->lt().date());

    m_strings << i18nc("Universal Time", "UT: ") + data->ut().time().toString("HH:mm:ss") + "   " +
              QLocale().toString(data->ut().date()); // Do not format UTC according to locale

    const QString STString = QString::asprintf("%02d:%02d:%02d   ", data->lst()->hour(), data->lst()->minute(),
                             data->lst()->second());
    //Don't use KLocale::formatNumber() for Julian Day because we don't want
    //thousands-place separators
    QString JDString = QString::number(data->ut().djd(), 'f', 2);
    JDString.replace('.', QLocale().decimalPoint());
    m_strings << i18nc("Sidereal Time", "ST: ") + STString + i18nc("Julian Day", "JD: ") + JDString;
    updateSize();
    update();
}

void InfoBoxWidget::slotGeoChanged()
{
    GeoLocation *geo = KStarsData::Instance()->geo();

    m_strings.clear();
    m_strings << geo->fullName();

    //m_strings << i18nc("Longitude", "Long:") + ' ' + QLocale().toString(geo->lng()->Degrees(), 3) + "   " +
    //                 i18nc("Latitude", "Lat:") + ' ' + QLocale().toString(geo->lat()->Degrees(), 3);

    m_strings << i18nc("Longitude", "Long:") + ' ' + geo->lng()->toDMSString(true) + ' ' +
              i18nc("Latitude", "Lat:") + ' ' + geo->lat()->toDMSString(true);
    updateSize();
    update();
}

void InfoBoxWidget::slotObjectChanged(SkyObject *obj)
{
    setPoint(obj->translatedLongName(), obj);
}

void InfoBoxWidget::slotPointChanged(SkyPoint *p)
{
    setPoint(i18n("nothing"), p);
}

void InfoBoxWidget::setPoint(QString name, SkyPoint *p)
{
    m_strings.clear();
    m_strings << name;
    m_strings << i18nc("Right Ascension", "RA") + ": " + p->ra().toHMSString() + "  " + i18nc("Declination", "Dec") +
              ": " + p->dec().toDMSString(true);
    m_strings << i18nc("Azimuth", "Az") + ": " + p->az().toDMSString(true) + "  " + i18nc("Altitude", "Alt") + ": " +
              p->alt().toDMSString(true);

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    dms ha(lst - p->ra());
    QChar sign('+');
    if (ha.Hours() > 12.0)
    {
        ha.setH(24.0 - ha.Hours());
        sign = '-';
    }
    m_strings << i18nc("Hour Angle", "HA") + ": " + sign + ha.toHMSString() + "  " + i18nc("Zenith Angle", "ZA") + ": " +
              dms(90 - p->alt().Degrees()).toDMSString(true);
    updateSize();
    update();
}

void InfoBoxWidget::adjust()
{
    if (!isVisible())
        return;
    // X axis
    int newX = x();
    int maxX = parentWidget()->width() - width();
    if (m_anchor & AnchorRight)
    {
        newX = maxX;
    }
    else
    {
        newX = KSUtils::clamp(newX, 0, maxX);
        if (newX == maxX)
            m_anchor |= AnchorRight;
    }
    // Y axis
    int newY = y();
    int maxY = parentWidget()->height() - height();
    if (m_anchor & AnchorBottom)
    {
        newY = maxY;
    }
    else
    {
        newY = KSUtils::clamp(newY, 0, maxY);
        if (newY == maxY)
            m_anchor |= AnchorBottom;
    }
    // Do move
    m_adjusted = true;
    move(newX, newY);
}

void InfoBoxWidget::paintEvent(QPaintEvent *)
{
    // If widget contain no strings return
    if (m_strings.empty())
        return;
    // Start with painting
    ColorScheme *cs = KStarsData::Instance()->colorScheme();
    QPainter p;
    p.begin(this);

    // Draw background
    QColor colBG = cs->colorNamed("BoxBGColor");
    colBG.setAlpha(127);
    p.fillRect(contentsRect(), colBG);
    // Draw border
    if (m_grabbed)
    {
        p.setPen(cs->colorNamed("BoxGrabColor"));
        p.drawRect(0, 0, width() - 1, height() - 1);
    }
    // Draw text
    int h = QFontMetrics(font()).height();
    int y = 0;
    p.setPen(cs->colorNamed("BoxTextColor"));
    foreach (const QString &str, m_strings)
    {
        y += h;
        p.drawText(padX, padY + y, str);
    }
    // Done
    p.end();
}

void InfoBoxWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_grabbed = true;
    // X axis
    int newX = x() + event->x();
    int maxX = parentWidget()->width() - width();
    if (newX > maxX)
    {
        newX = maxX;
        m_anchor |= AnchorRight;
    }
    else
    {
        if (newX < 0)
            newX = 0;
        m_anchor &= ~AnchorRight;
    }
    // Y axis
    int newY = y() + event->y();
    int maxY = parentWidget()->height() - height();
    if (newY > maxY)
    {
        newY = maxY;
        m_anchor |= AnchorBottom;
    }
    else
    {
        if (newY < 0)
            newY = 0;
        m_anchor &= ~AnchorBottom;
    }
    // Do move
    m_adjusted = true;
    move(newX, newY);
}

void InfoBoxWidget::mousePressEvent(QMouseEvent *)
{
    emit clicked();
}

void InfoBoxWidget::showEvent(QShowEvent *)
{
    if (!m_adjusted)
        adjust();
}

void InfoBoxWidget::mouseDoubleClickEvent(QMouseEvent *)
{
    m_shaded = !m_shaded;
    updateSize();
    update();
}

void InfoBoxWidget::mouseReleaseEvent(QMouseEvent *)
{
    m_grabbed = false;
}
