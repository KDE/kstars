/*
    SPDX-FileCopyrightText: 2007 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kplotwidget.h"

class eqPlotWidget : public KPlotWidget
{
    Q_OBJECT
  public:
    explicit eqPlotWidget(QWidget *parent = nullptr);

  protected:
    void paintEvent(QPaintEvent *) override;
};
