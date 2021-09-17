/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_focusmanager.h"

#include "ekos/ekos.h"
#include "ekos/focus/focus.h"

#include <QWidget>
#include <QProgressIndicator.h>

class QProgressIndicator;

namespace Ekos
{

class FocusManager : public QWidget, public Ui::FocusManager
{
    Q_OBJECT

public:
    FocusManager(QWidget * parent);
    void init(Focus *focusProcess);
    void updateFocusDetailView();
    void stopAnimation();
    void reset();

public slots:
    void updateFocusStatus(FocusState status);
    void updateFocusStarPixmap(QPixmap &starPixmap);
    void updateCurrentHFR(double newHFR);

private:
    QProgressIndicator *focusPI { nullptr };
    std::unique_ptr<QPixmap> focusStarPixmap;

};

}
