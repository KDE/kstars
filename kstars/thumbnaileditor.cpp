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
#include <qimage.h>
#include <qlayout.h>
#include <qpainter.h>
#include <qpoint.h>

#include <klocale.h>
#include <kdebug.h>
#include <kpushbutton.h>

#include "thumbnaileditor.h"
#include "thumbnaileditorui.h"
#include "thumbnailpicker.h"

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

QPixmap ThumbImage::croppedImage() {
	QPixmap result( CropRect->width(), CropRect->height() );
	bitBlt( &result, 0, 0, Image, 
			CropRect->left(), CropRect->top(),
			CropRect->width(), CropRect->height() );

	return result;
}

ThumbImage::ThumbImage( QWidget *parent, const char *name ) : QLabel( parent, name )
{
	setBackgroundMode( QWidget::NoBackground );
	bMouseButtonDown = false;
	bTopLeftGrab = false;
	bTopRightGrab = false;
	bBottomLeftGrab = false;
	bBottomRightGrab = false;
	HandleSize = 10;

	CropRect = new QRect();
	Anchor = new QPoint();
	Image = new QPixmap();
}

ThumbImage::~ThumbImage()
{}

void ThumbImage::paintEvent( QPaintEvent* ) {
	QPixmap c( *Image );
	QPainter p;
	p.begin( &c );
	p.setPen( QPen( QColor( "Grey" ), 2 ) );

	p.drawRect( *CropRect );

	p.setPen( QPen( QColor( "Grey" ), 0 ) );
	p.drawRect( QRect( CropRect->left(), CropRect->top(), 
		HandleSize, HandleSize ) );
	p.drawRect( QRect( CropRect->right() - HandleSize, CropRect->top(), 
		HandleSize, HandleSize ) );
	p.drawRect( QRect( CropRect->left(), CropRect->bottom() - HandleSize, 
		HandleSize, HandleSize ) );
	p.drawRect( QRect( CropRect->right() - HandleSize, CropRect->bottom() - HandleSize, 
		HandleSize, HandleSize ) );

	if ( CropRect->x() > 0 ) 
		p.fillRect( 0, 0, CropRect->x(), c.height(), 
				QBrush( QColor("white"), Dense3Pattern ) );
	if ( CropRect->right() < c.width() ) 
		p.fillRect( CropRect->right(), 0, (c.width() - CropRect->right()), c.height(), 
				QBrush( QColor("white"), Dense3Pattern ) );
	if ( CropRect->y() > 0 ) 
		p.fillRect( 0, 0, c.width(), CropRect->y(), 
				QBrush( QColor("white"), Dense3Pattern ) );
	if ( CropRect->bottom() < c.height() ) 
		p.fillRect( 0, CropRect->bottom(), c.width(), (c.height() - CropRect->bottom()), 
				QBrush( QColor("white"), Dense3Pattern ) );

	p.end();

	bitBlt( this, 0, 0, &c );
}

void ThumbImage::mousePressEvent( QMouseEvent *e ) {
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

void ThumbImage::mouseReleaseEvent( QMouseEvent *e ) {
	if ( bMouseButtonDown ) bMouseButtonDown = false;
	if ( bTopLeftGrab ) bTopLeftGrab = false;
	if ( bTopRightGrab ) bTopRightGrab = false;
	if ( bBottomLeftGrab ) bBottomLeftGrab = false;
	if ( bBottomRightGrab ) bBottomRightGrab = false;
}

void ThumbImage::mouseMoveEvent( QMouseEvent *e ) {
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
			CropRect->moveTopLeft( QPoint( e->x() - Anchor->x(), e->y() - Anchor->y() ) );
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
