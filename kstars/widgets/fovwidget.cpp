/***************************************************************************
                          fovwidget.cpp  -  description
                             -------------------
    begin                : Sat 22 Sept 2007
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

#include "fovwidget.h"
#include "dialogs/fovdialog.h"
#include "fov.h"

#include <QPaintEvent>
#include <QPainter>

FOVWidget::FOVWidget( QWidget *parent ) : QFrame( parent ), m_FOV(0) 
{}

FOVWidget::~FOVWidget()
{}

void FOVWidget::setFOV( FOV *f ) {
    m_FOV = f;
}

void FOVWidget::paintEvent( QPaintEvent * ) {
    QPainter p;
    p.begin( this );
    p.setRenderHint( QPainter::Antialiasing, true );
    p.fillRect( contentsRect(), QColor( "black" ) );

    if( m_FOV && m_FOV->sizeX() > 0 && m_FOV->sizeY() > 0 ) {
        m_FOV->draw(p, 0.6*contentsRect().width(), 0.6*contentsRect().height() );
        QFont smallFont = p.font();
        smallFont.setPointSize( p.font().pointSize() - 2 );
        p.setFont( smallFont );
        // TODO: Check if decimal points in this are localized (eg: It should read 1,5 x 1,5 in German rather than 1.5 x 1.5)
        p.drawText( rect(), Qt::AlignHCenter|Qt::AlignBottom,
                    i18nc("angular size in arcminutes", "%1 x %2 arcmin",
                          QString::number( m_FOV->sizeX(), 'f', 1 ),
                          QString::number( m_FOV->sizeY(), 'f', 1 ) ) );
    }
    
    p.end();
}

#include "fovwidget.moc"
