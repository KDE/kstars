/***************************************************************************
                          draglistbox.h  -  description
                             -------------------
    begin                : Sun May 29 2005
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

#ifndef DRAGLISTBOX_H
#define DRAGLISTBOX_H

#include <klistwidget.h>
//Added by qt3to4:
#include <QDragEnterEvent>
#include <QMouseEvent>
#include <QDropEvent>

class QDragEnterEvent;

/**@class DragListBox
	*@short Extension of KListWidget that allows Drag-and-Drop 
	*with other DragListBoxes
	*@author Jason Harris
	*@version 1.0
	*/

class DragListBox : public KListWidget {
    Q_OBJECT
public:
    /**@short Default constructor
     */
    explicit DragListBox( QWidget *parent = 0, const char* name = 0 );

    /**@short Default destructor
     */
    ~DragListBox();

    int ignoreIndex() const { return IgnoreIndex; }
    bool contains( const QString &s ) const;

    void dragEnterEvent( QDragEnterEvent *evt );
    void dragMoveEvent( QDragMoveEvent *evt );
    void dropEvent( QDropEvent *evt );
    void mousePressEvent( QMouseEvent *evt );
    void mouseMoveEvent( QMouseEvent * );
    void mouseReleaseEvent( QMouseEvent * );
private:
    bool leftButtonDown;
    int IgnoreIndex;

};

#endif //DRAGLISTBOX_H
