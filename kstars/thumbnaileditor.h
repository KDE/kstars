/***************************************************************************
                          thumbnaileditor.h  -  description
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

#ifndef THUMBNAILEDITOR_H
#define THUMBNAILEDITOR_H

#include <kdialogbase.h>
#include <qlabel.h>

class ThumbnailEditorUI;
class ThumbnailPicker;
class QPoint;

class ThumbImage : public QLabel
{
Q_OBJECT
public:
	ThumbImage( QWidget *parent, const char *name = 0 );
	~ThumbImage();

	void setImage( QPixmap *pm ) { Image = pm; setFixedSize( Image->width(), Image->height() ); }
	QPixmap* image() { return Image; }
	QPixmap croppedImage();

	void setCropRect( int x, int y, int w, int h ) { CropRect->setRect( x, y, w, h ); }
	QRect* cropRect() const { return CropRect; }

signals:
	void cropRegionModified();

protected:
//	void resizeEvent( QResizeEvent *e);
	void paintEvent( QPaintEvent *);
	void mousePressEvent( QMouseEvent *e );
	void mouseReleaseEvent( QMouseEvent *e );
	void mouseMoveEvent( QMouseEvent *e );

private:
	QRect *CropRect;
	QPoint *Anchor;
	QPixmap *Image;
	
	bool bMouseButtonDown;
	bool bTopLeftGrab, bBottomLeftGrab, bTopRightGrab, bBottomRightGrab;
	int HandleSize;
};

class ThumbnailEditor : public KDialogBase
{
Q_OBJECT
public:
	ThumbnailEditor( QWidget *parent, const char *name=0 );
	~ThumbnailEditor();
	QPixmap thumbnail();

private slots:
	void slotUpdateCropLabel();

private:
	ThumbnailEditorUI *ui;
	ThumbnailPicker *tp;

};

#endif
