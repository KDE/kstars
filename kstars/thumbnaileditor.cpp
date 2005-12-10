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

#include <QPixmap>
#include <QMouseEvent>
#include <QLabel>
#include <QHBoxLayout>
#include <QPaintEvent>

#include <klocale.h>
#include <kdebug.h>
#include <kpushbutton.h>

#include "thumbnaileditor.h"
#include "thumbnaileditorui.h"
#include "thumbnailpicker.h"

ThumbnailEditorUI::ThumbnailEditorUI( QWidget *parent ) : QFrame( parent ) {
	setupUi( parent );
}

ThumbnailEditor::ThumbnailEditor( QWidget *parent, const char *name )
 : KDialogBase( KDialogBase::Plain, i18n( "Edit Thumbnail Image" ), Ok|Cancel, Ok, parent, name )
{
	tp = (ThumbnailPicker*)parent;

	QFrame *page = plainPage();
	QHBoxLayout *hlay = new QHBoxLayout( page, 0, 0 );
	ui = new ThumbnailEditorUI( page );
	hlay->addWidget( ui );

	ui->ImageCanvas->setCropRect( tp->imageRect()->x(), tp->imageRect()->y(), 
		tp->imageRect()->width(), tp->imageRect()->height() );
	ui->ImageCanvas->setImage( tp->currentListImage() );

	connect( ui->ImageCanvas, SIGNAL(cropRegionModified()), SLOT( slotUpdateCropLabel() ) );
	slotUpdateCropLabel();

	update();
}

ThumbnailEditor::~ThumbnailEditor()
{}

QPixmap ThumbnailEditor::thumbnail() {
	QImage im = ui->ImageCanvas->croppedImage().convertToImage();
	im = im.smoothScale( 200, 200 );
	QPixmap pm;
	pm.convertFromImage( im );
	return pm;
}

void ThumbnailEditor::slotUpdateCropLabel() {
	QRect *r = ui->ImageCanvas->cropRect();
	ui->CropLabel->setText( i18n( "Crop region: [%1,%2  %3x%4]" )
			.arg( r->left() ).arg( r->top() ).arg( r->width() ).arg( r->height() ) );
}

#include "thumbnaileditor.moc"
