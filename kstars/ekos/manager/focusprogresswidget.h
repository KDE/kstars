/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_focusprogresswidget.h"

#include "ekos/ekos.h"
#include "ekos/focus/focus.h"

#include <QWidget>
#include <QProgressIndicator.h>

class QProgressIndicator;

namespace Ekos
{

class FocusProgressWidget : public QWidget, public Ui::FocusProgressWidget
{
    Q_OBJECT

public:
    FocusProgressWidget(QWidget * parent);
    void init();
    void updateFocusDetailView();
    void reset();

public slots:
    void updateFocusStatus(FocusState status);
    void updateFocusStarPixmap(QPixmap &starPixmap);
    void updateCurrentHFR(double newHFR);

private:
    std::unique_ptr<QPixmap> focusStarPixmap;

};

}
