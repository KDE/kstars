/***************************************************************************
                          draglistbox.cpp  -  description
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

#include <qdragobject.h>
#include <klocale.h>
#include <kdebug.h>

#include "draglistbox.h"

DragListBox::DragListBox( QWidget *parent, const char *name, WFlags f ) 
		: KListBox( parent, name, f ) {

	setAcceptDrops( TRUE );
	dragging = FALSE;
}

DragListBox::~DragListBox() {}

void DragListBox::dragEnterEvent( QDragEnterEvent *evt )
{
	if ( QTextDrag::canDecode( evt ) ) 
		evt->accept();
}

bool DragListBox::contains( const QString &s ) const {
	for ( uint i=0; i<count(); ++i )
		if ( text(i) == s ) return true;

	return false;
}

void DragListBox::dropEvent( QDropEvent *evt ) {
	QString text;

	int i = int( float(evt->pos().y())/float(itemHeight()) + 0.5 ) + topItem();
	if ( i > count() + 1 ) i = count() + 1;

	if ( QTextDrag::decode( evt, text ) ) {
		//If we dragged an "Ignore item from the FieldList to the FieldPool, then we don't
		//need to insert the item, because FieldPool already has a persistent Ignore item.
		if ( !( text == i18n("Ignore" ) && QString(evt->source()->name()) == "FieldList" && 
				evt->source() != this )) {
			insertItem( text, i );
		}

		//If we dragged the "Ignore" item from FieldPool to FieldList, then we don't
		//want to remove the item from the FieldPool
		if ( !( text == i18n("Ignore" ) && QString(evt->source()->name()) == "FieldPool" && 
				evt->source() != this ) ) {
			DragListBox *fp = (DragListBox*)evt->source();
			fp->removeItem( fp->currentItem() );
		}
	}
}


void DragListBox::mousePressEvent( QMouseEvent *evt ) {
	QListBox::mousePressEvent( evt );
	dragging = TRUE;
	
	//Record position of the Ignore item; we may have to restore it.
	if ( currentText() == i18n("Ignore") )
		IgnoreIndex = currentItem();
	else 
		IgnoreIndex = -1;
}


void DragListBox::mouseMoveEvent( QMouseEvent * )
{
	if ( dragging ) {
		QDragObject *drag = new QTextDrag( currentText(), this );
		drag->dragMove();
		dragging = FALSE;
	}
}

#include "draglistbox.moc"
