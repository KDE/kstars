/*  Ekos guiding state widget
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
                  2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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
