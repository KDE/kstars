/*
    SPDX-FileCopyrightText: 2013-2021 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2018-2020 Robert Lancaster <rlancaste@gmail.com>
    SPDX-FileCopyrightText: 2019-2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "align.h"
#include "align_p.h"
#include "alignview.h"
#include "alignadaptor.h"
#include "manualrotator.h"
#include "mountmodel.h"
#include "polaralignmentassistant.h"
#include "pushtoassistant.h"
#include "ekos/auxiliary/darkprocessor.h"

#include <ekos_align_debug.h>

namespace Ekos
{

using PAA = PolarAlignmentAssistant;

void Align::setupPolarAlignmentAssistant()
{
    // Create PAA instance
    m_PolarAlignmentAssistant = new PolarAlignmentAssistant(this, m_AlignView);
    connect(m_PolarAlignmentAssistant, &Ekos::PAA::captureAndSolve, this, [this]()
    {
        captureAndSolve(true);
    });
    connect(m_PolarAlignmentAssistant, &Ekos::PAA::newAlignTableResult, this, &Ekos::Align::setAlignTableResult);
    connect(m_PolarAlignmentAssistant, &Ekos::PAA::newFrame, this, &Ekos::Align::newFrame);
    connect(m_PolarAlignmentAssistant, &Ekos::PAA::newPAHStage, this, &Ekos::Align::processPAHStage);
    connect(m_PolarAlignmentAssistant, &Ekos::PAA::newLog, this, &Ekos::Align::appendLogText);

    tabWidget->addTab(m_PolarAlignmentAssistant, i18n("Polar Alignment"));
}

void Align::setupPushToAssistant()
{
    // push-to assistant
    PushToAssistant::Instance()->enableSolving(true);
    connect(this, &Align::newStatus, PushToAssistant::Instance(), &PushToAssistant::setAlignState);
    connect(this, &Align::newSolution, PushToAssistant::Instance(), &PushToAssistant::updateSolution);
    connect(PushToAssistant::Instance(), &PushToAssistant::captureAndSolve, [this](bool initialCall)
    {
        setSolverAction(GOTO_SYNC);
        captureAndSolve(initialCall);
    });
    connect(PushToAssistant::Instance(), &PushToAssistant::abort, this, &Align::abort);
}

void Align::setupManualRotator()
{
    if (m_ManualRotator)
        return;

    m_ManualRotator = new ManualRotator(this);
    connect(m_ManualRotator, &Ekos::ManualRotator::captureAndSolve, this, [this]()
    {
        captureAndSolve(false);
    });
    // If user cancel manual rotator, reset load slew target PA, otherwise it will keep popping up
    // for any subsequent solves.
    connect(m_ManualRotator, &Ekos::ManualRotator::rejected, this, [this]()
    {
        m_TargetPositionAngle = std::numeric_limits<double>::quiet_NaN();
        // If in progress stop it
        if (state > ALIGN_COMPLETE)
            stop(ALIGN_ABORTED);
    });
}

void Align::setuptDarkProcessor()
{
    if (m_DarkProcessor)
        return;

    m_DarkProcessor = new DarkProcessor(this);
    connect(m_DarkProcessor, &DarkProcessor::newLog, this, &Ekos::Align::appendLogText);
    connect(m_DarkProcessor, &DarkProcessor::darkFrameCompleted, this, [this](bool completed)
    {
        alignDarkFrame->setChecked(completed);
        m_AlignView->setProperty("suspended", false);
        if (completed)
        {
            m_AlignView->rescale(ZOOM_KEEP_LEVEL);
            m_AlignView->updateFrame();
        }
        setCaptureComplete();
    });
}

void Align::processPAHStage(int stage)
{
    switch (stage)
    {
        case PAA::PAH_IDLE:
            // Abort any solver that might be running.
            // Assumes this state change won't happen randomly (e.g. in the middle of align).
            // Alternatively could just let the stellarsolver finish naturally.
            if (m_Solver && m_Solver->isRunning())
                m_Solver->abort();
            break;
        case PAA::PAH_POST_REFRESH:
        {
            Options::setAstrometrySolverWCS(rememberSolverWCS);
            Options::setAutoWCS(rememberAutoWCS);
            stop(ALIGN_IDLE);
        }
        break;

        case PAA::PAH_FIRST_CAPTURE:
            nothingR->setChecked(true);
            m_CurrentGotoMode = GOTO_NOTHING;
            loadSlewB->setEnabled(false);

            rememberSolverWCS = Options::astrometrySolverWCS();
            rememberAutoWCS   = Options::autoWCS();

            Options::setAutoWCS(false);
            Options::setAstrometrySolverWCS(true);
            break;
        case PAA::PAH_SECOND_CAPTURE:
        case PAA::PAH_THIRD_CAPTURE:
            if (alignSettlingTime->value() >= DELAY_THRESHOLD_NOTIFY)
                Q_EMIT newLog(i18n("Settling..."));
            m_CaptureTimer.start(alignSettlingTime->value());
            break;

        default:
            break;
    }

    Q_EMIT newPAAStage(stage);
}

bool Align::matchPAHStage(uint32_t stage)
{
    return m_PolarAlignmentAssistant && m_PolarAlignmentAssistant->getPAHStage() == stage;
}

void Align::toggleManualRotator(bool toggled)
{
    if (toggled)
    {
        m_ManualRotator->show();
        m_ManualRotator->raise();
    }
    else
        m_ManualRotator->close();
}

}
