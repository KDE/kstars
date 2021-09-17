/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_guidestatewidget.h"

#include "ekos/ekos.h"

#include <QWidget>
#include <KLed>

namespace Ekos
{

class GuideStateWidget : public QWidget, public Ui::GuideStateWidget
{
    Q_OBJECT

public:
    GuideStateWidget(QWidget * parent = nullptr);
    void init();

public slots:
    void updateGuideStatus(GuideState state);

private:
    // State
    KLed * idlingStateLed { nullptr };
    KLed * preparingStateLed { nullptr };
    KLed * runningStateLed { nullptr };


};
}
