/***************************************************************************
                          clicklabel.h  -  description
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

#ifndef CLICKLABEL_H
#define CLICKLABEL_H

#include <QLabel>
#include <QMouseEvent>

/** @class ClickLabel is a QLabel with a clicked() signal.
	*@author Jason Harris
	*@version 1.0
	*/
class ClickLabel : public QLabel {
    Q_OBJECT
public:
    explicit ClickLabel( QWidget *parent=0, const char *name=0 );
    ~ClickLabel() {}

signals:
    void clicked();

protected:
    void mousePressEvent( QMouseEvent *e ) { if ( e->button() == Qt::LeftButton ) emit clicked(); }
};

#endif
