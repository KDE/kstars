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

    public Q_SLOTS:
        void updateGuideStatus(GuideState state);
        // AI lifecycle mirror. aiState is AIGuideState cast to int. DISABLED returns the strip
        // to the canonical Idle/Prep/Run display; any other state relabels it to Warmup/Active/Fallback.
        void updateAIStatus(int aiState);

    private:
        void setCanonicalLabels();   // Idle / Prep / Run
        void setAILabels();          // Warmup / Active / Fallback

        // State
        KLed * idlingStateLed { nullptr };
        KLed * preparingStateLed { nullptr };
        KLed * runningStateLed { nullptr };

        // When true the AI updater owns the LEDs/labels; the canonical updater stands aside.
        bool m_aiMode { false };
        GuideState m_lastGuideState { GUIDE_IDLE };
};
}
