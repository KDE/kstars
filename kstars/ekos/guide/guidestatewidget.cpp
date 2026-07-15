/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "guidestatewidget.h"
#include "internalguide/gmath.h"

#include <KLocalizedString>

namespace Ekos
{
GuideStateWidget::GuideStateWidget(QWidget * parent) : QWidget(parent)
{
    setupUi(this);
    init();
}

void GuideStateWidget::init()
{
    // Guiding state
    if (idlingStateLed == nullptr)
    {
        idlingStateLed = new KLed(Qt::gray, KLed::Off, KLed::Flat, KLed::Circular, this);
        idlingStateLed->setObjectName("idlingStateLed");
        guideStateLayout->insertWidget(0, idlingStateLed);
    }
    if (preparingStateLed == nullptr)
    {
        preparingStateLed = new KLed(Qt::gray, KLed::Off, KLed::Flat, KLed::Circular, this);
        preparingStateLed->setObjectName("preparingStateLed");
        guideStateLayout->insertWidget(2, preparingStateLed);
    }
    if (runningStateLed == nullptr)
    {
        runningStateLed = new KLed(Qt::gray, KLed::Off, KLed::Flat, KLed::Circular, this);
        runningStateLed->setObjectName("runningStateLed");
        guideStateLayout->insertWidget(4, runningStateLed);
    }
}

void GuideStateWidget::updateGuideStatus(GuideState state)
{
    m_lastGuideState = state;

    // Leaving an active guide session hands the strip back to the canonical display, even if the
    // AI never emitted a DISABLED transition (e.g. the user simply stopped guiding).
    if (m_aiMode && (state == GUIDE_IDLE || state == GUIDE_ABORTED
                     || state == GUIDE_DISCONNECTED || state == GUIDE_CONNECTED))
    {
        m_aiMode = false;
        setCanonicalLabels();
        setToolTip(QString());
    }

    // While AI-guiding, updateAIStatus() owns the LEDs and labels.
    if (m_aiMode)
        return;

    idlingStateLed->off();
    preparingStateLed->off();
    runningStateLed->off();
    switch (state)
    {
        case GUIDE_DISCONNECTED:
            idlingStateLed->setColor(Qt::red);
            idlingStateLed->on();
            break;
        case GUIDE_CONNECTED:
            idlingStateLed->setColor(Qt::green);
            preparingStateLed->setColor(Qt::gray);
            runningStateLed->setColor(Qt::gray);
            idlingStateLed->on();
            break;
        case GUIDE_CAPTURE:
        case GUIDE_LOOPING:
        case GUIDE_DARK:
        case GUIDE_SUBFRAME:
            preparingStateLed->setColor(Qt::green);
            runningStateLed->setColor(Qt::gray);
            preparingStateLed->on();
            break;
        case GUIDE_STAR_SELECT:
        case GUIDE_CALIBRATING:
            preparingStateLed->setColor(Qt::yellow);
            runningStateLed->setColor(Qt::gray);
            preparingStateLed->on();
            break;
        case GUIDE_CALIBRATION_ERROR:
            preparingStateLed->setColor(Qt::red);
            runningStateLed->setColor(Qt::red);
            preparingStateLed->on();
            break;
        case GUIDE_CALIBRATION_SUCCESS:
            preparingStateLed->setColor(Qt::green);
            runningStateLed->setColor(Qt::yellow);
            preparingStateLed->on();
            break;
        case GUIDE_GUIDING:
            preparingStateLed->setColor(Qt::green);
            runningStateLed->setColor(Qt::green);
            runningStateLed->setState(KLed::On);
            break;
        case GUIDE_MANUAL_DITHERING:
        case GUIDE_DITHERING:
        case GUIDE_DITHERING_SETTLE:
        case GUIDE_REACQUIRE:
        case GUIDE_SUSPENDED:
            runningStateLed->setColor(Qt::yellow);
            runningStateLed->setState(KLed::On);
            break;
        case GUIDE_DITHERING_ERROR:
        case GUIDE_ABORTED:
            idlingStateLed->setColor(Qt::green);
            preparingStateLed->setColor(Qt::red);
            runningStateLed->setColor(Qt::red);
            idlingStateLed->on();
            break;
        case GUIDE_IDLE:
        default:
            idlingStateLed->setColor(Qt::green);
            idlingStateLed->on();
            break;
    }

}

void GuideStateWidget::updateAIStatus(int aiState)
{
    const AIGuideState s = static_cast<AIGuideState>(aiState);

    if (s == AIGuideState::DISABLED)
    {
        // AI turned off (weights failed, algorithm not AI, or session ended): restore canonical view.
        if (m_aiMode)
        {
            m_aiMode = false;
            setCanonicalLabels();
            setToolTip(QString());
            updateGuideStatus(m_lastGuideState);
        }
        return;
    }

    if (!m_aiMode)
    {
        m_aiMode = true;
        setAILabels();
    }

    idlingStateLed->off();
    preparingStateLed->off();
    runningStateLed->off();
    switch (s)
    {
        case AIGuideState::WARMUP:
            preparingStateLed->setColor(Qt::yellow);   // "Warm up"
            preparingStateLed->on();
            break;
        case AIGuideState::SHADOW:
        case AIGuideState::ACTIVE:
            runningStateLed->setColor(Qt::green);       // "Active"
            runningStateLed->on();
            break;
        case AIGuideState::FALLBACK:
            idlingStateLed->setColor(QColor(255, 140, 0)); // "Idle" (amber): AI loaded but stepped back
            idlingStateLed->on();
            break;
        default:
            break;
    }
}

void GuideStateWidget::setCanonicalLabels()
{
    idlingStateLabel->setText(i18n("Idle"));
    preparingStateLabel->setText(i18n("Prep"));
    runningStateLabel->setText(i18n("Run"));
}

void GuideStateWidget::setAILabels()
{
    idlingStateLabel->setText(i18n("Idle"));
    preparingStateLabel->setText(i18n("Warm up"));
    runningStateLabel->setText(i18n("Active"));
}
}
