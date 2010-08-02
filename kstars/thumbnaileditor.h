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
#include <tqlabel.h>

class ThumbnailEditorUI;
class ThumbnailPicker;
class TQPoint;

class ThumbImage : public QLabel
{
Q_OBJECT
public:
	ThumbImage( TQWidget *parent, const char *name = 0 );
	~ThumbImage();

	void setImage( TQPixmap *pm ) { Image = pm; setFixedSize( Image->width(), Image->height() ); }
	TQPixmap* image() { return Image; }
	TQPixmap croppedImage();

	void setCropRect( int x, int y, int w, int h ) { CropRect->setRect( x, y, w, h ); }
	TQRect* cropRect() const { return CropRect; }

signals:
	void cropRegionModified();

protected:
//	void resizeEvent( TQResizeEvent *e);
	void paintEvent( TQPaintEvent *);
	void mousePressEvent( TQMouseEvent *e );
	void mouseReleaseEvent( TQMouseEvent *e );
	void mouseMoveEvent( TQMouseEvent *e );

private:
	TQRect *CropRect;
	TQPoint *Anchor;
	TQPixmap *Image;
	
	bool bMouseButtonDown;
	bool bTopLeftGrab, bBottomLeftGrab, bTopRightGrab, bBottomRightGrab;
	int HandleSize;
};

class ThumbnailEditor : public KDialogBase
{
Q_OBJECT
public:
	ThumbnailEditor( TQWidget *parent, const char *name=0 );
	~ThumbnailEditor();
	TQPixmap thumbnail();

private slots:
	void slotUpdateCropLabel();

private:
	ThumbnailEditorUI *ui;
	ThumbnailPicker *tp;

};

#endif
