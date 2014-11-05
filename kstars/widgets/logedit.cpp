/***************************************************************************
                          logedit.cpp  -  description
                             -------------------
    begin                : Sat 03 Dec 2005
    copyright            : (C) 2005 by Jason Harris and Jasem Mutlaq
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

#include "logedit.h"

#include <QFrame>

LogEdit::LogEdit( QWidget *parent ) : QTextEdit( parent )
{
    setFrameStyle( QFrame::StyledPanel );
    setFrameShadow( QFrame::Plain );
    setLineWidth( 4 );
}

void LogEdit::focusOutEvent( QFocusEvent *e ) {
    emit focusOut();
    QWidget::focusOutEvent(e);
}


