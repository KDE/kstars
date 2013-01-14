/***************************************************************************
                          logedit.h  -  description
                             -------------------
    begin                : Sat 03 Dec 2005
    copyright            : (C) 2005 by Jason Harris
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

#ifndef LOGEDIT_H
#define LOGEDIT_H

#include <ktextedit.h>
#include <QFocusEvent>

/**@class LogEdit is a simple derivative of KTextEdit, that just adds a
	*focusOut() signal, emitted when the edit loses focus.
	*@author Jason Harris
	*@version 1.0
	*/
class LogEdit : public KTextEdit {
    Q_OBJECT
public:
    explicit LogEdit( QWidget *parent=0 );
    ~LogEdit() {}

signals:
    void focusOut();

protected:
    void focusOutEvent( QFocusEvent *e );
};


#endif
