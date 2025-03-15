/*
    SPDX-FileCopyrightText: 2025 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QRubberBand>
#include <QMouseEvent>
#include <QScreen>

class ScreenCapture : public QWidget {
    Q_OBJECT
public:
  explicit ScreenCapture(QWidget *parent = nullptr);

  ~ScreenCapture();

protected:
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

signals:
    void areaSelected(const QImage &image);
    void aborted();

private:
    QPoint origin;
    QRubberBand *rubberBand;
};
