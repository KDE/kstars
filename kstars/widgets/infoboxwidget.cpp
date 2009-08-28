/***************************************************************************
                          fovwidget.cpp  -  description
                             -------------------
    begin                : 20 Aug 2009
    copyright            : (C) 2009 by Khudyakov Alexey
    email                : alexey.skladnoy@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <algorithm>

#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>

#include <kdebug.h>

#include "ksutils.h"
#include "kstarsdata.h"
#include "colorscheme.h"

#include "widgets/infoboxwidget.h"

namespace {
    // Padding
    const int padX = 6;
    const int padY = 2;

    // Resize QStringList. For some reason. QList doesn't have resize.
    void resizeList(QStringList& list, int n) {
        while( list.size() > n )
            list.removeLast();
        while( list.size() < n )
            list.append( QString() );
    }
}


InfoBoxWidget::InfoBoxWidget(bool shade, QPoint pos, QStringList str, QWidget* parent) :
    QWidget(parent),
    m_strings(str),
    m_grabbed(false),
    m_shaded(shade)
{
    move(pos);
    updateSize();
}

InfoBoxWidget::~InfoBoxWidget()
{}

void InfoBoxWidget::updateSize() {
    int w = 0;
    int h = 0;
    QFontMetrics fm(font());
    foreach(QString str, m_strings)
        w = std::max(w, fm.width(str));
    h = fm.height() * (m_shaded ? 1 : m_strings.size());
    // Add padding
    resize(w + 2*padX, h + 2*padY + 2);
}

void InfoBoxWidget::slotTimeChanged() {
    KStarsData* data = KStarsData::Instance();

    resizeList(m_strings, 3);
    m_strings[0] =
        i18nc( "Local Time", "LT: " ) +
        KGlobal::locale()->formatTime( data->lt().time(), true ) + "   " +
        KGlobal::locale()->formatDate( data->lt().date() );
    m_strings[1] =
        i18nc( "Universal Time", "UT: " ) +
        KGlobal::locale()->formatTime( data->ut().time(), true ) + "   " +
        KGlobal::locale()->formatDate( data->ut().date() );

    QString STString;
    STString = STString.sprintf( "%02d:%02d:%02d   ", data->lst()->hour(), data->lst()->minute(), data->lst()->second() );
    //Don't use KLocale::formatNumber() for Julian Day because we don't want
    //thousands-place separators
    QString JDString = QString::number( data->ut().djd(), 'f', 2 );
    JDString.replace( '.', KGlobal::locale()->decimalSymbol() );
    m_strings[2] =
        i18nc( "Sidereal Time", "ST: " ) + STString +
        i18nc( "Julian Day", "JD: " ) + JDString;
    updateSize();
    update();
}

void InfoBoxWidget::slotGeoChanged() {
    KStarsData* data = KStarsData::Instance();

    resizeList(m_strings, 2);

    QString name = data->geo()->translatedName() + ", ";
    if ( ! data->geo()->province().isEmpty() )
        name += data->geo()->translatedProvince() + ",  ";
    name += data->geo()->translatedCountry();
    m_strings[0] = name;

    m_strings[1] =
        i18nc( "Longitude", "Long:" ) + ' ' +
        KGlobal::locale()->formatNumber( data->geo()->lng()->Degrees(),3) + "   " +
        i18nc( "Latitude", "Lat:" ) + ' ' +
        KGlobal::locale()->formatNumber( data->geo()->lat()->Degrees(),3);
    updateSize();
    update();
}

void InfoBoxWidget::slotObjectChanged(SkyObject* obj) {
    setPoint( obj->translatedLongName(), obj );
}

void InfoBoxWidget::slotPointChanged(SkyPoint* p) {
    setPoint( i18n("nothing"), p);
}

void InfoBoxWidget::setPoint(QString name, SkyPoint* p) {
    resizeList(m_strings, 3);
    m_strings[0] = name;
    m_strings[1] =
        i18nc( "Right Ascension", "RA" ) + ": " + p->ra()->toHMSString() + "  " +
        i18nc( "Declination", "Dec" )    +  ": " + p->dec()->toDMSString(true);
    m_strings[2] =
        i18nc( "Azimuth", "Az" )   + ": " + p->az()->toDMSString(true) + "  " +
        i18nc( "Altitude", "Alt" ) + ": " + p->alt()->toDMSString(true);
    updateSize();
    update();
}

void InfoBoxWidget::resizeEvent(QResizeEvent*) {
}

void InfoBoxWidget::paintEvent(QPaintEvent* e)
{
    // If widget contain no strings return
    if( m_strings.empty() )
        return;
    // Start with painting
    ColorScheme* cs = KStarsData::Instance()->colorScheme();
    QPainter p;
    p.begin( this );

    // Draw background
    QColor colBG = cs->colorNamed( "BoxBGColor"   );
    colBG.setAlpha(127);
    p.fillRect( contentsRect(), colBG );
    // Draw border
    if( m_grabbed ) {
        p.setPen( cs->colorNamed( "BoxGrabColor" ) );
        p.drawRect( 0, 0, width()-1, height()-1 );
    }
    // Draw text
    int h = QFontMetrics( font() ).height();
    int y = 0;
    p.setPen( cs->colorNamed( "BoxTextColor" ) );
    foreach(QString str, m_strings) {
        y += h;
        p.drawText( padX, padY + y, str);
    }
    // Done
    p.end();
}

void InfoBoxWidget::mouseMoveEvent ( QMouseEvent * event ) {
    m_grabbed = true;
    // Maximum allowed value
    int maxX = parentWidget()->width()  - width();
    int maxY = parentWidget()->height() - height();
    // New position
    int newX = KSUtils::clamp( x() + event->x(), 0, maxX );
    int newY = KSUtils::clamp( y() + event->y(), 0, maxY );
    move(newX, newY);
}

void InfoBoxWidget::mouseDoubleClickEvent(QMouseEvent * event ) {
    m_shaded = !m_shaded;
    updateSize();
    update();
}

void InfoBoxWidget::mouseReleaseEvent ( QMouseEvent * event ) {
    m_grabbed = false;
}

void InfoBoxWidget::mousePressEvent ( QMouseEvent * event ) {
}

#include "infoboxwidget.moc"
