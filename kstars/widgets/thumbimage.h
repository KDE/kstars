/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QLabel>
#include <QPixmap>

#include <memory>

class ThumbImage : public QLabel
{
    Q_OBJECT
  public:
    explicit ThumbImage(QWidget *parent, const char *name = nullptr);
    virtual ~ThumbImage() override = default;

    void setImage(QPixmap *pm)
    {
        *Image = *pm;
        setFixedSize(Image->width(), Image->height());
    }
    QPixmap *image() { return Image.get(); }
    QPixmap croppedImage();

    void setCropRect(int x, int y, int w, int h) { CropRect->setRect(x, y, w, h); }
    QRect *cropRect() const { return CropRect.get(); }

  signals:
    void cropRegionModified();

  protected:
    //	void resizeEvent( QResizeEvent *e);
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;

  private:
    std::unique_ptr<QRect> CropRect;
    std::unique_ptr<QPoint> Anchor;
    std::unique_ptr<QPixmap> Image;

    bool bMouseButtonDown { false };
    bool bTopLeftGrab { false };
    bool bBottomLeftGrab { false };
    bool bTopRightGrab { false };
    bool bBottomRightGrab { false };
    int HandleSize { 10 };
};
