/***************************************************************************
                          thumbimage.h  -  description
                             -------------------
    begin                : Fri 09 Dec 2005
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

#ifndef THUMBIMAGE_H
#define THUMBIMAGE_H

#include <QLabel>
#include <QPixmap>

class ThumbImage : public QLabel {
    Q_OBJECT
public:
    explicit ThumbImage( QWidget *parent, const char *name = 0 );
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

#endif
