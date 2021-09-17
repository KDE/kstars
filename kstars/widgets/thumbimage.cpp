/*
    SPDX-FileCopyrightText: 2005 Jason Harris and Jasem Mutlaq <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "thumbimage.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPen>

QPixmap ThumbImage::croppedImage()
{
    return Image->copy(*CropRect);
}

ThumbImage::ThumbImage(QWidget *parent, const char *name) : QLabel(parent)
{
    //FIXME: Deprecated?  Maybe we don't need this, since double-buffering is now built in
    //	setBackgroundMode( Qt::WA_NoBackground );

    setObjectName(name);

    CropRect.reset(new QRect());
    Anchor.reset(new QPoint());
    Image.reset(new QPixmap());
}

void ThumbImage::paintEvent(QPaintEvent *)
{
    QPainter p;
    p.begin(this);

    p.drawPixmap(0, 0, *Image);

    p.setPen(QPen(QColor("Grey"), 2));
    p.drawRect(*CropRect);

    p.setPen(QPen(QColor("Grey"), 0));
    p.drawRect(QRect(CropRect->left(), CropRect->top(), HandleSize, HandleSize));
    p.drawRect(QRect(CropRect->right() - HandleSize, CropRect->top(), HandleSize, HandleSize));
    p.drawRect(QRect(CropRect->left(), CropRect->bottom() - HandleSize, HandleSize, HandleSize));
    p.drawRect(QRect(CropRect->right() - HandleSize, CropRect->bottom() - HandleSize, HandleSize, HandleSize));

    if (CropRect->x() > 0)
        p.fillRect(0, 0, CropRect->x(), height(), QBrush(QColor("white"), Qt::Dense3Pattern));
    if (CropRect->right() < width())
        p.fillRect(CropRect->right(), 0, (width() - CropRect->right()), height(),
                   QBrush(QColor("white"), Qt::Dense3Pattern));
    if (CropRect->y() > 0)
        p.fillRect(0, 0, width(), CropRect->y(), QBrush(QColor("white"), Qt::Dense3Pattern));
    if (CropRect->bottom() < height())
        p.fillRect(0, CropRect->bottom(), width(), (height() - CropRect->bottom()),
                   QBrush(QColor("white"), Qt::Dense3Pattern));

    p.end();
}

void ThumbImage::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && CropRect->contains(e->pos()))
    {
        bMouseButtonDown = true;

        //The Anchor tells how far from the CropRect corner we clicked
        Anchor->setX(e->x() - CropRect->left());
        Anchor->setY(e->y() - CropRect->top());

        if (e->x() <= CropRect->left() + HandleSize && e->y() <= CropRect->top() + HandleSize)
        {
            bTopLeftGrab = true;
        }
        if (e->x() <= CropRect->left() + HandleSize && e->y() >= CropRect->bottom() - HandleSize)
        {
            bBottomLeftGrab = true;
            Anchor->setY(e->y() - CropRect->bottom());
        }
        if (e->x() >= CropRect->right() - HandleSize && e->y() <= CropRect->top() + HandleSize)
        {
            bTopRightGrab = true;
            Anchor->setX(e->x() - CropRect->right());
        }
        if (e->x() >= CropRect->right() - HandleSize && e->y() >= CropRect->bottom() - HandleSize)
        {
            bBottomRightGrab = true;
            Anchor->setX(e->x() - CropRect->right());
            Anchor->setY(e->y() - CropRect->bottom());
        }
    }
}

void ThumbImage::mouseReleaseEvent(QMouseEvent *)
{
    if (bMouseButtonDown)
        bMouseButtonDown = false;
    if (bTopLeftGrab)
        bTopLeftGrab = false;
    if (bTopRightGrab)
        bTopRightGrab = false;
    if (bBottomLeftGrab)
        bBottomLeftGrab = false;
    if (bBottomRightGrab)
        bBottomRightGrab = false;
}

void ThumbImage::mouseMoveEvent(QMouseEvent *e)
{
    if (bMouseButtonDown)
    {
        //If a corner was grabbed, we are resizing the box
        if (bTopLeftGrab)
        {
            if (e->x() >= 0 && e->x() <= width())
                CropRect->setLeft(e->x() - Anchor->x());
            if (e->y() >= 0 && e->y() <= height())
                CropRect->setTop(e->y() - Anchor->y());
            if (CropRect->left() < 0)
                CropRect->setLeft(0);
            if (CropRect->top() < 0)
                CropRect->setTop(0);
            if (CropRect->width() < 200)
                CropRect->setLeft(CropRect->left() - 200 + CropRect->width());
            if (CropRect->height() < 200)
                CropRect->setTop(CropRect->top() - 200 + CropRect->height());
        }
        else if (bTopRightGrab)
        {
            if (e->x() >= 0 && e->x() <= width())
                CropRect->setRight(e->x() - Anchor->x());
            if (e->y() >= 0 && e->y() <= height())
                CropRect->setTop(e->y() - Anchor->y());
            if (CropRect->right() > width())
                CropRect->setRight(width());
            if (CropRect->top() < 0)
                CropRect->setTop(0);
            if (CropRect->width() < 200)
                CropRect->setRight(CropRect->right() + 200 - CropRect->width());
            if (CropRect->height() < 200)
                CropRect->setTop(CropRect->top() - 200 + CropRect->height());
        }
        else if (bBottomLeftGrab)
        {
            if (e->x() >= 0 && e->x() <= width())
                CropRect->setLeft(e->x() - Anchor->x());
            if (e->y() >= 0 && e->y() <= height())
                CropRect->setBottom(e->y() - Anchor->y());
            if (CropRect->left() < 0)
                CropRect->setLeft(0);
            if (CropRect->bottom() > height())
                CropRect->setBottom(height());
            if (CropRect->width() < 200)
                CropRect->setLeft(CropRect->left() - 200 + CropRect->width());
            if (CropRect->height() < 200)
                CropRect->setBottom(CropRect->bottom() + 200 - CropRect->height());
        }
        else if (bBottomRightGrab)
        {
            if (e->x() >= 0 && e->x() <= width())
                CropRect->setRight(e->x() - Anchor->x());
            if (e->y() >= 0 && e->y() <= height())
                CropRect->setBottom(e->y() - Anchor->y());
            if (CropRect->right() > width())
                CropRect->setRight(width());
            if (CropRect->bottom() > height())
                CropRect->setBottom(height());
            if (CropRect->width() < 200)
                CropRect->setRight(CropRect->right() + 200 - CropRect->width());
            if (CropRect->height() < 200)
                CropRect->setBottom(CropRect->bottom() + 200 - CropRect->height());
        }
        else //no corner grabbed; move croprect
        {
            CropRect->moveTopLeft(QPoint(e->x() - Anchor->x(), e->y() - Anchor->y()));
            if (CropRect->left() < 0)
                CropRect->moveLeft(0);
            if (CropRect->right() > width())
                CropRect->moveRight(width());
            if (CropRect->top() < 0)
                CropRect->moveTop(0);
            if (CropRect->bottom() > height())
                CropRect->moveBottom(height());
        }

        emit cropRegionModified();
        update();
    }
}
