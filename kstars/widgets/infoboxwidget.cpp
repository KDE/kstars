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

#include "infoboxwidget.h"

#include "kstarsdata.h"
#include "colorscheme.h"
#include "ksutils.h"
#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>

const int InfoBoxWidget::padX = 6;
const int InfoBoxWidget::padY = 2;

InfoBoxes::~InfoBoxes()
{}

void InfoBoxes::addInfoBox(InfoBoxWidget* ibox)
{
    ibox->setParent(this);
    m_boxes.append(ibox);
}

void InfoBoxes::resizeEvent(QResizeEvent*) {
    foreach(InfoBoxWidget* w, m_boxes)
        w->adjust();
}

/* ================================================================ */

InfoBoxWidget::InfoBoxWidget(bool shade, QPoint pos, int anchor, QStringList str, QWidget* parent) :
    QWidget(parent),
    m_strings(str),
    m_adjusted(false),
    m_grabbed(false),
    m_shaded(shade),
    m_anchor(anchor)
{
    move(pos);
    updateSize();
}

InfoBoxWidget::~InfoBoxWidget()
{}

void InfoBoxWidget::updateSize() {
    QFontMetrics fm(font());
    int w = 0;
    foreach(QString str, m_strings)
        w = qMax(w, fm.width(str));
    int h = fm.height() * (m_shaded ? 1 : m_strings.size());
    // Add padding
    resize(w + 2*padX, h + 2*padY + 2);
    adjust();
}

void InfoBoxWidget::slotTimeChanged() {
    KStarsData* data = KStarsData::Instance();

    m_strings.clear();
    m_strings <<
        i18nc( "Local Time", "LT: " ) +
        KGlobal::locale()->formatTime( data->lt().time(), true ) + "   " +
        KGlobal::locale()->formatDate( data->lt().date() );
    m_strings <<
        i18nc( "Universal Time", "UT: " ) +
        KGlobal::locale()->formatTime( data->ut().time(), true ) + "   " +
        KGlobal::locale()->formatDate( data->ut().date() );

    QString STString;
    STString = STString.sprintf( "%02d:%02d:%02d   ", data->lst()->hour(), data->lst()->minute(), data->lst()->second() );
    //Don't use KLocale::formatNumber() for Julian Day because we don't want
    //thousands-place separators
    QString JDString = QString::number( data->ut().djd(), 'f', 2 );
    JDString.replace( '.', KGlobal::locale()->decimalSymbol() );
    m_strings <<
        i18nc( "Sidereal Time", "ST: " ) + STString +
        i18nc( "Julian Day", "JD: " ) + JDString;
    updateSize();
    update();
}

void InfoBoxWidget::slotGeoChanged() {
    GeoLocation* geo = KStarsData::Instance()->geo();

    m_strings.clear();
    m_strings << geo->fullName();
    m_strings <<
        i18nc( "Longitude", "Long:" ) + ' ' +
        KGlobal::locale()->formatNumber( geo->lng()->Degrees(),3) + "   " +
        i18nc( "Latitude", "Lat:" ) + ' ' +
        KGlobal::locale()->formatNumber( geo->lat()->Degrees(),3);
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
    m_strings.clear();
    m_strings << name;
    m_strings <<
        i18nc( "Right Ascension", "RA" ) + ": " + p->ra().toHMSString() + "  " +
        i18nc( "Declination", "Dec" )    + ": " + p->dec().toDMSString(true);
    m_strings <<
        i18nc( "Azimuth", "Az" )   + ": " + p->az().toDMSString(true) + "  " +
        i18nc( "Altitude", "Alt" ) + ": " + p->alt().toDMSString(true);
    updateSize();
    update();
}

void InfoBoxWidget::adjust() {
    if( !isVisible() )
        return;
    // X axis
    int newX = x();
    int maxX = parentWidget()->width() - width();
    if( m_anchor & AnchorRight ) {
        newX = maxX;
    } else {
        newX = KSUtils::clamp(newX, 0, maxX);
        if( newX == maxX )
            m_anchor |= AnchorRight;
    }
    // Y axis
    int newY = y();
    int maxY = parentWidget()->height() - height();
    if( m_anchor & AnchorBottom ) {
        newY = maxY;
    } else {
        newY = KSUtils::clamp(newY, 0, maxY);
        if( newY == maxY )
            m_anchor |= AnchorBottom;
    }
    // Do move
    m_adjusted = true;
    move(newX, newY);
}

void InfoBoxWidget::paintEvent(QPaintEvent*)
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

void InfoBoxWidget::mouseMoveEvent(QMouseEvent * event) {
    m_grabbed = true;
    // X axis
    int newX = x() + event->x();
    int maxX = parentWidget()->width() - width();
    if( newX > maxX ) {
        newX = maxX;
        m_anchor |= AnchorRight;
    } else {
        if( newX < 0 )
            newX = 0;
        m_anchor &= ~AnchorRight;
    }
    // Y axis
    int newY = y() + event->y();
    int maxY = parentWidget()->height() - height();
    if( newY > maxY ) {
        newY = maxY;
        m_anchor |= AnchorBottom;
    } else {
        if( newY < 0 )
            newY = 0;
        m_anchor &= ~AnchorBottom;
    }
    // Do move
    m_adjusted = true;
    move(newX, newY);
}

void InfoBoxWidget::mousePressEvent(QMouseEvent*) {
    emit clicked();
}

void InfoBoxWidget::showEvent(QShowEvent*) {
    if( !m_adjusted )
        adjust();
}

void InfoBoxWidget::mouseDoubleClickEvent(QMouseEvent*) {
    m_shaded = !m_shaded;
    updateSize();
    update();
}

void InfoBoxWidget::mouseReleaseEvent(QMouseEvent*) {
    m_grabbed = false;
}

#include "infoboxwidget.moc"
