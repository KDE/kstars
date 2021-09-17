/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kplotwidget.h"

class PlanetViewer;

class PVPlotWidget : public KPlotWidget
{
    Q_OBJECT
  public:
    explicit PVPlotWidget(QWidget *parent = nullptr);

  public slots:
    void slotZoomIn();
    void slotZoomOut();

  signals:
    void doubleClicked(double, double);

  protected:
    void keyPressEvent(QKeyEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

  private:
    void updateFactor(const int modifier);

    bool mouseButtonDown { false };
    int oldx { 0 };
    int oldy { 0 };
    double factor { 2 };
    PlanetViewer *pv { nullptr };
};
