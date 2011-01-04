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

#include <klistbox.h>

class TQDragEnterEvent;
class QDragDropEvent;

/**@class DragListBox
	*@short Extension of KListBox that allows Drag-and-Drop 
	*with other DragListBoxes
	*@author Jason Harris
	*@version 1.0
	*/

class DragListBox : public KListBox {
	Q_OBJECT
public:
/**@short Default constructor
 */
	DragListBox( TQWidget *parent = 0, const char *name = 0, WFlags = 0 );

/**@short Default destructor
 */
	~DragListBox();

	int ignoreIndex() const { return IgnoreIndex; }
	bool tqcontains( const TQString &s ) const;

	void dragEnterEvent( TQDragEnterEvent *evt );
	void dropEvent( TQDropEvent *evt );
	void mousePressEvent( TQMouseEvent *evt );
	void mouseMoveEvent( TQMouseEvent * );
private:
	bool dragging;
	int IgnoreIndex;

};

#endif //DRAGLISTBOX_H
