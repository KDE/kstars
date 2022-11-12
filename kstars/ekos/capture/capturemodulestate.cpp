/*  Ekos state machine for the Capture module
    SPDX-FileCopyrightText: 2022 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "capturemodulestate.h"
#include "ekos/manager/meridianflipstate.h"
#include "ekos/capture/sequencejob.h"

#include <ekos_capture_debug.h>

namespace Ekos
{
CaptureModuleState::CaptureModuleState(QObject *parent): QObject{parent} {
    m_refocusState.reset(new RefocusState());
}

void CaptureModuleState::setCaptureState(CaptureState value) {
    // handle new capture state
    switch (value)
    {
    case CAPTURE_IDLE:
    case CAPTURE_ABORTED:
    case CAPTURE_SUSPENDED:
        // meridian flip may take place if requested
        if (mf_state->getMeridianFlipStage() == MeridianFlipState::MF_REQUESTED)
            mf_state->updateMeridianFlipStage(MeridianFlipState::MF_READY);
        break;

    default:
        // do nothing
        break;
    }

    m_CaptureState = value;
    getMeridianFlipState()->setCaptureState(m_CaptureState);
    emit newStatus(m_CaptureState);
}

QSharedPointer<MeridianFlipState> CaptureModuleState::getMeridianFlipState()
{
    // lazy instantiation
    if (mf_state.isNull())
        mf_state.reset(new MeridianFlipState());

    return mf_state;
}

void CaptureModuleState::setMeridianFlipState(QSharedPointer<MeridianFlipState> state)
{
    // clear old state machine
    if (! mf_state.isNull())
        mf_state->deleteLater();

    mf_state = state;
}

void CaptureModuleState::decreaseDitherCounter()
{
    if (ditherCounter > 0)
        --ditherCounter;
}

bool CaptureModuleState::checkDithering()
{
    // No need if preview only
    if (m_activeJob && m_activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool())
        return false;

    if ( (Options::ditherEnabled() || Options::ditherNoGuiding())
         // 2017-09-20 Jasem: No need to dither after post meridian flip guiding
         && getMeridianFlipState()->getMeridianFlipStage() != MeridianFlipState::MF_GUIDING
         // We must be either in guide mode or if non-guide dither (via pulsing) is enabled
         && (getGuideState() == GUIDE_GUIDING || Options::ditherNoGuiding())
         // Must be only done for light frames
         && (m_activeJob != nullptr && m_activeJob->getFrameType() == FRAME_LIGHT)
         // Check dither counter
         && ditherCounter == 0)
    {
        ditherCounter = Options::ditherFrames();

        qCInfo(KSTARS_EKOS_CAPTURE) << "Dithering...";
        appendLogText(i18n("Dithering..."));

        setCaptureState(CAPTURE_DITHERING);
        setDitheringState(IPS_BUSY);

        return true;
    }
    // no dithering required
    return false;
}

bool CaptureModuleState::checkMeridianFlipReady()
{
    if (hasTelescope == false)
        return false;

    // If active job is taking flat field image at a wall source
    // then do not flip.
    if (m_activeJob && m_activeJob->getFrameType() == FRAME_FLAT && m_activeJob->getFlatFieldSource() == SOURCE_WALL)
        return false;

    if (getMeridianFlipState()->getMeridianFlipStage() != MeridianFlipState::MF_REQUESTED)
        // if no flip has been requested or is already ongoing
        return false;

    // meridian flip requested or already in action

    // Reset frame if we need to do focusing later on
    if (m_refocusState->isInSequenceFocus() ||
            (Options::enforceRefocusEveryN() && m_refocusState->getRefocusEveryNTimerElapsedSec() > 0))
        emit resetFocus();

    // signal that meridian flip may take place
    if (getMeridianFlipState()->getMeridianFlipStage() == MeridianFlipState::MF_REQUESTED)
        getMeridianFlipState()->updateMeridianFlipStage(MeridianFlipState::MF_READY);


    return true;
}

bool CaptureModuleState::checkGuidingAfterFlip()
{
    // if no meridian flip has completed, we do not touch guiding
    if (getMeridianFlipState()->getMeridianFlipStage() < MeridianFlipState::MF_COMPLETED)
        return false;
    // If we're not autoguiding then we're done
    if (getMeridianFlipState()->resumeGuidingAfterFlip() == false)
    {
        getMeridianFlipState()->updateMeridianFlipStage(MeridianFlipState::MF_NONE);
        return false;
    }

    // if we are waiting for a calibration, start it
    if (getMeridianFlipState()->getMeridianFlipStage() >= MeridianFlipState::MF_COMPLETED
            && getMeridianFlipState()->getMeridianFlipStage() < MeridianFlipState::MF_GUIDING)
    {
        appendLogText(i18n("Performing post flip re-calibration and guiding..."));

        setCaptureState(CAPTURE_CALIBRATING);

        getMeridianFlipState()->updateMeridianFlipStage(MeridianFlipState::MF_GUIDING);
        emit guideAfterMeridianFlip();
        return true;
    }
    else if (m_CaptureState == CAPTURE_CALIBRATING)
    {
        if (getGuideState() == GUIDE_CALIBRATION_ERROR || getGuideState() == GUIDE_ABORTED)
        {
            // restart guiding after failure
            appendLogText(i18n("Post meridian flip calibration error. Restarting..."));
            emit guideAfterMeridianFlip();
            return true;
        }
        else if (getGuideState() != GUIDE_GUIDING)
            // waiting for guiding to start
            return true;
        else
            // guiding is running
            return false;
    }
    else
        // in all other cases, do not touch
        return false;
}

bool CaptureModuleState::checkAlignmentAfterFlip()
{
    // if no meridian flip has completed, we do not touch guiding
    if (getMeridianFlipState()->getMeridianFlipStage() < MeridianFlipState::MF_COMPLETED)
        return false;
    // If we do not need to align then we're done
    if (getMeridianFlipState()->resumeAlignmentAfterFlip() == false)
        return false;

    // if we are waiting for a calibration, start it
    if (m_CaptureState < CAPTURE_ALIGNING)
    {
        appendLogText(i18n("Performing post flip re-alignment..."));

        resetAlignmentRetries();
        setCaptureState(CAPTURE_ALIGNING);

        getMeridianFlipState()->updateMeridianFlipStage(MeridianFlipState::MF_ALIGNING);
        return true;
    }
    else
        // in all other cases, do not touch
        return false;

}









void CaptureModuleState::appendLogText(QString message)
{
    qCInfo(KSTARS_EKOS_CAPTURE()) << message;
}

} // namespace
