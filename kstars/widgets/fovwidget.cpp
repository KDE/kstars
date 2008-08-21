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
#include "fovdialog.h"
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

    if ( m_FOV ) {
        if ( m_FOV->size() > 0 ) {
            m_FOV->draw( p, (float)( 0.3*contentsRect().width() ) );
            QFont smallFont = p.font();
            smallFont.setPointSize( p.font().pointSize() - 2 );
            p.setFont( smallFont );
            p.drawText( rect(), Qt::AlignHCenter|Qt::AlignBottom, i18nc("angular size in arcminutes", "%1 arcmin", QString::number( m_FOV->size(), 'f', 1 ) ) );
        }
    }

    p.end();
}

#include "fovwidget.moc"
