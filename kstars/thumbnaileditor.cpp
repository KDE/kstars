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

#include <tqframe.h>
#include <tqimage.h>
#include <tqlayout.h>
#include <tqpainter.h>
#include <tqpoint.h>

#include <klocale.h>
#include <kdebug.h>
#include <kpushbutton.h>

#include "thumbnaileditor.h"
#include "thumbnaileditorui.h"
#include "thumbnailpicker.h"

ThumbnailEditor::ThumbnailEditor( TQWidget *parent, const char *name )
 : KDialogBase( KDialogBase::Plain, i18n( "Edit Thumbnail Image" ), Ok|Cancel, Ok, parent, name )
{
	tp = (ThumbnailPicker*)parent;

	TQFrame *page = plainPage();
	TQHBoxLayout *hlay = new TQHBoxLayout( page, 0, 0 );
	ui = new ThumbnailEditorUI( page );
	hlay->addWidget( ui );

	ui->ImageCanvas->setCropRect( tp->imageRect()->x(), tp->imageRect()->y(), 
		tp->imageRect()->width(), tp->imageRect()->height() );
	ui->ImageCanvas->setImage( tp->currentListImage() );

	connect( ui->ImageCanvas, TQT_SIGNAL(cropRegionModified()), TQT_SLOT( slotUpdateCropLabel() ) );
	slotUpdateCropLabel();

	update();
}

ThumbnailEditor::~ThumbnailEditor()
{}

TQPixmap ThumbnailEditor::thumbnail() {
	TQImage im = ui->ImageCanvas->croppedImage().convertToImage();
	im = im.smoothScale( 200, 200 );
	TQPixmap pm;
	pm.convertFromImage( im );
	return pm;
}

void ThumbnailEditor::slotUpdateCropLabel() {
	TQRect *r = ui->ImageCanvas->cropRect();
	ui->CropLabel->setText( i18n( "Crop region: [%1,%2  %3x%4]" )
			.arg( r->left() ).arg( r->top() ).arg( r->width() ).arg( r->height() ) );
}

TQPixmap ThumbImage::croppedImage() {
	TQPixmap result( CropRect->width(), CropRect->height() );
	bitBlt( &result, 0, 0, Image, 
			CropRect->left(), CropRect->top(),
			CropRect->width(), CropRect->height() );

	return result;
}

ThumbImage::ThumbImage( TQWidget *parent, const char *name ) : TQLabel( parent, name )
{
	setBackgroundMode( TQWidget::NoBackground );
	bMouseButtonDown = false;
	bTopLeftGrab = false;
	bTopRightGrab = false;
	bBottomLeftGrab = false;
	bBottomRightGrab = false;
	HandleSize = 10;

	CropRect = new TQRect();
	Anchor = new TQPoint();
	Image = new TQPixmap();
}

ThumbImage::~ThumbImage()
{}

void ThumbImage::paintEvent( TQPaintEvent* ) {
	TQPixmap c( *Image );
	TQPainter p;
	p.begin( &c );
	p.setPen( TQPen( TQColor( "Grey" ), 2 ) );

	p.drawRect( *CropRect );

	p.setPen( TQPen( TQColor( "Grey" ), 0 ) );
	p.drawRect( TQRect( CropRect->left(), CropRect->top(), 
		HandleSize, HandleSize ) );
	p.drawRect( TQRect( CropRect->right() - HandleSize, CropRect->top(), 
		HandleSize, HandleSize ) );
	p.drawRect( TQRect( CropRect->left(), CropRect->bottom() - HandleSize, 
		HandleSize, HandleSize ) );
	p.drawRect( TQRect( CropRect->right() - HandleSize, CropRect->bottom() - HandleSize, 
		HandleSize, HandleSize ) );

	if ( CropRect->x() > 0 ) 
		p.fillRect( 0, 0, CropRect->x(), c.height(), 
				TQBrush( TQColor("white"), Dense3Pattern ) );
	if ( CropRect->right() < c.width() ) 
		p.fillRect( CropRect->right(), 0, (c.width() - CropRect->right()), c.height(), 
				TQBrush( TQColor("white"), Dense3Pattern ) );
	if ( CropRect->y() > 0 ) 
		p.fillRect( 0, 0, c.width(), CropRect->y(), 
				TQBrush( TQColor("white"), Dense3Pattern ) );
	if ( CropRect->bottom() < c.height() ) 
		p.fillRect( 0, CropRect->bottom(), c.width(), (c.height() - CropRect->bottom()), 
				TQBrush( TQColor("white"), Dense3Pattern ) );

	p.end();

	bitBlt( this, 0, 0, &c );
}

void ThumbImage::mousePressEvent( TQMouseEvent *e ) {
	if ( e->button() == LeftButton && CropRect->contains( e->pos() ) ) {
		bMouseButtonDown = true;

		//The Anchor tells how far from the CropRect corner we clicked
		Anchor->setX( e->x() - CropRect->left() );
		Anchor->setY( e->y() - CropRect->top() );

		if ( e->x() <= CropRect->left() + HandleSize && e->y() <= CropRect->top() + HandleSize ) {
			bTopLeftGrab = true;
		}
		if ( e->x() <= CropRect->left() + HandleSize && e->y() >= CropRect->bottom() - HandleSize ) {
			bBottomLeftGrab = true;
			Anchor->setY( e->y() - CropRect->bottom() );
		}
		if ( e->x() >= CropRect->right() - HandleSize && e->y() <= CropRect->top() + HandleSize ) {
			bTopRightGrab = true;
			Anchor->setX( e->x() - CropRect->right() );
		}
		if ( e->x() >= CropRect->right() - HandleSize && e->y() >= CropRect->bottom() - HandleSize ) {
			bBottomRightGrab = true;
			Anchor->setX( e->x() - CropRect->right() );
			Anchor->setY( e->y() - CropRect->bottom() );
		}
	}
}

void ThumbImage::mouseReleaseEvent( TQMouseEvent *e ) {
	if ( bMouseButtonDown ) bMouseButtonDown = false;
	if ( bTopLeftGrab ) bTopLeftGrab = false;
	if ( bTopRightGrab ) bTopRightGrab = false;
	if ( bBottomLeftGrab ) bBottomLeftGrab = false;
	if ( bBottomRightGrab ) bBottomRightGrab = false;
}

void ThumbImage::mouseMoveEvent( TQMouseEvent *e ) {
	if ( bMouseButtonDown ) {

		//If a corner was grabbed, we are resizing the box
		if ( bTopLeftGrab ) {
			if ( e->x() >= 0 && e->x() <= width() ) CropRect->setLeft( e->x() - Anchor->x() );
			if ( e->y() >= 0 && e->y() <= height() ) CropRect->setTop( e->y() - Anchor->y() );
			if ( CropRect->left() < 0 ) CropRect->setLeft( 0 );
			if ( CropRect->top() < 0 ) CropRect->setTop( 0 );
			if ( CropRect->width() < 200 ) CropRect->setLeft( CropRect->left() - 200 + CropRect->width() );
			if ( CropRect->height() < 200 ) CropRect->setTop( CropRect->top() - 200 + CropRect->height() );
		} else if ( bTopRightGrab ) {
			if ( e->x() >= 0 && e->x() <= width() ) CropRect->setRight( e->x() - Anchor->x() );
			if ( e->y() >= 0 && e->y() <= height() ) CropRect->setTop( e->y() - Anchor->y() );
			if ( CropRect->right() > width() ) CropRect->setRight( width() );
			if ( CropRect->top() < 0 ) CropRect->setTop( 0 );
			if ( CropRect->width() < 200 ) CropRect->setRight( CropRect->right() + 200 - CropRect->width() );
			if ( CropRect->height() < 200 ) CropRect->setTop( CropRect->top() - 200 + CropRect->height() );
		} else if ( bBottomLeftGrab ) {
			if ( e->x() >= 0 && e->x() <= width() ) CropRect->setLeft( e->x() - Anchor->x() );
			if ( e->y() >= 0 && e->y() <= height() ) CropRect->setBottom( e->y() - Anchor->y() );
			if ( CropRect->left() < 0 ) CropRect->setLeft( 0 );
			if ( CropRect->bottom() > height() ) CropRect->setBottom( height() );
			if ( CropRect->width() < 200 ) CropRect->setLeft( CropRect->left() - 200 + CropRect->width() );
			if ( CropRect->height() < 200 ) CropRect->setBottom( CropRect->bottom() + 200 - CropRect->height() );
		} else if ( bBottomRightGrab ) {
			if ( e->x() >= 0 && e->x() <= width() ) CropRect->setRight( e->x() - Anchor->x() );
			if ( e->y() >= 0 && e->y() <= height() ) CropRect->setBottom( e->y() - Anchor->y() );
			if ( CropRect->right() > width() ) CropRect->setRight( width() );
			if ( CropRect->bottom() > height() ) CropRect->setBottom( height() );
			if ( CropRect->width() < 200 ) CropRect->setRight( CropRect->right() + 200 - CropRect->width() );
			if ( CropRect->height() < 200 ) CropRect->setBottom( CropRect->bottom() + 200 - CropRect->height() );
		} else { //no corner grabbed; move croprect
			CropRect->moveTopLeft( TQPoint( e->x() - Anchor->x(), e->y() - Anchor->y() ) );
			if ( CropRect->left() < 0 ) CropRect->moveLeft( 0 );
			if ( CropRect->right() > width() ) CropRect->moveRight( width() );
			if ( CropRect->top() < 0 ) CropRect->moveTop( 0 );
			if ( CropRect->bottom() > height() ) CropRect->moveBottom( height() );
		}

		emit cropRegionModified();
		update();
	}
}

#include "thumbnaileditor.moc"
