/***************************************************************************
                          thumbnaileditor.cpp  -  description
                             -------------------
    begin                : Thu Mar 2 2005
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

#include <qframe.h>
#include <qlayout.h>

#include <klocale.h>

#include "thumbnaileditor.h"
#include "thumbnaileditorui.h"
#include "thumbnailpicker.h"

ThumbnailEditor::ThumbnailEditor( QWidget *parent, const char *name )
 : KDialogBase( KDialogBase::Plain, i18n( "Edit Thumbnail Image" ), Ok|Cancel, Ok, parent, name )
{
	tp = (ThumbnailPicker*)parent;

	QFrame *page = plainPage();
	QVBoxLayout *vlay = new QVBoxLayout( page, 0, 0 );
	ui = new ThumbnailEditorUI( page );
	vlay->addWidget( ui );

	Image = *(tp->currentListImage());
	ui->ImageCanvas->resize( Image.width(), Image.height() );
	update();
}

ThumbnailEditor::~ThumbnailEditor()
{}

// void ThumbnailEditor::resizeEvent( QResizeEvent *e ) {
// 	update();
// }

void ThumbnailEditor::paintEvent( QPaintEvent* ) {
	bitBlt( ui->ImageCanvas, 0, 0, &Image );
}
