/***************************************************************************
                          eqplotwidget.cpp  -  description
                             -------------------
    begin                : Thu 16 Mar 2007
    copyright            : (C) 2007 by Jason Harris
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

#include "eqplotwidget.h"
#include <QPainter>
#include <KLocalizedString>
#include <QDebug>
#include "kstarsdatetime.h"
#include "modcalcvizequinox.h"

eqPlotWidget::eqPlotWidget( QWidget *parent )
        : KPlotWidget( parent )
{
}

//Draw rotated labels and month names on top axis
void eqPlotWidget::paintEvent( QPaintEvent *e ) {
    KPlotWidget::paintEvent(e);

    QPainter p;
    p.begin(this);

    modCalcEquinox *mc = (modCalcEquinox*)(parent()->parent()->parent()->parent());
    KStarsDateTime dt( QDate(mc->Year->value(), 1, 1), QTime(0,0,0) );
    long double jd0 = dt.djd(); //save JD on Jan 1st

    QPointF pSpring = mapToWidget( QPointF( mc->dSpring.djd() - jd0 - 4, -28.0 ) );
    QPointF pSummer = mapToWidget( QPointF( mc->dSummer.djd() - jd0 - 4, -28.0 ) );
    QPointF pAutumn = mapToWidget( QPointF( mc->dAutumn.djd() - jd0 - 4, -28.0 ) );
    QPointF pWinter = mapToWidget( QPointF( mc->dWinter.djd() - jd0 - 4, -28.0 ) );

    p.setPen( Qt::yellow );
    QFont f = p.font();
    f.setPointSize( f.pointSize() - 2 );
    p.setFont( f );

    p.save();
    p.translate( leftPadding() + pSpring.x(), topPadding() + pSpring.y() );
    p.rotate(-90);
    p.drawText( 0, 0, i18n("Vernal equinox:") );
    p.drawText( 0, 14, QLocale().toString( mc->dSpring, QLocale::LongFormat ) );
    p.restore();

    p.save();
    p.translate( leftPadding() + pSummer.x(), topPadding() + pSummer.y() );
    p.rotate(-90);
    p.drawText( 0, 0, i18n("Summer solstice:") );
    p.drawText( 0, 14, QLocale().toString( mc->dSummer, QLocale::LongFormat ) );
    p.restore();

    p.save();
    p.translate( leftPadding() + pAutumn.x(), topPadding() + pAutumn.y() );
    p.rotate(-90);
    p.drawText( 0, 0, i18n("Autumnal equinox:") );
    p.drawText( 0, 14, QLocale().toString( mc->dAutumn, QLocale::LongFormat ) );
    p.restore();

    p.save();
    p.translate( leftPadding() + pWinter.x(), topPadding() + pWinter.y() );
    p.rotate(-90);
    p.drawText( 0, 0, i18n("Winter solstice:") );
    p.drawText( 0, 14, QLocale().toString( mc->dWinter, QLocale::LongFormat ) );
    p.restore();

    //Draw month labels along top axis
    p.setPen( Qt::white );
    p.save();
    p.translate( leftPadding(), topPadding() );
    double y = mc->Plot->dataRect().bottom() + 1.5;
    for ( int i=0; i<12; i++ ) {
        QPoint c;
        if (i<11) {
            c = mc->Plot->mapToWidget( QPointF( 0.5*(mc->dmonth(i)+mc->dmonth(i+1)), y ) ).toPoint();
        } else {
            c = mc->Plot->mapToWidget( QPointF( 0.5*(mc->dmonth(i)+mc->Plot->dataRect().right()), y ) ).toPoint();
        }
        QRect r( -16, -8, 32, 16 );
        r.moveCenter( c );
        p.drawText( r, Qt::AlignCenter, QDate::shortMonthName(i+1) );
    }
    p.restore();
    p.end();
}

