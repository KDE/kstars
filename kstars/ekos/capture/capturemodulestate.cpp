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
CaptureModuleState::CaptureModuleState(QObject *parent): QObject{parent}
{
    m_refocusState.reset(new RefocusState());
    connect(m_refocusState.get(), &RefocusState::newLog, this, &CaptureModuleState::newLog);
}

void CaptureModuleState::setCaptureState(CaptureState value)
{
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



void CaptureModuleState::setCurrentFilterPosition(int position, const QString &name, const QString &focusFilterName)
{
    m_CurrentFilterPosition = position;
    if (position > 0)
    {
        m_CurrentFilterName = name;
        m_CurrentFocusFilterName = focusFilterName;
    }
    else
    {
        m_CurrentFilterName = "--";
        m_CurrentFocusFilterName = "--";
    }
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
    if (m_ditherCounter > 0)
        --m_ditherCounter;
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
            && m_ditherCounter == 0)
    {
        m_ditherCounter = Options::ditherFrames();

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

void CaptureModuleState::updateFocusState(FocusState state)
{
    if (state != m_FocusState)
        qCDebug(KSTARS_EKOS_CAPTURE) << "Focus State changed from" <<
                                     Ekos::getFocusStatusString(m_FocusState) <<
                                     "to" << Ekos::getFocusStatusString(state);
    setFocusState(state);

    // Do not process if meridian flip in progress
    if (getMeridianFlipState()->checkMeridianFlipRunning())
        return;

    switch (state)
    {
        // Do not process when aborted
        case FOCUS_ABORTED:
            if (!(getMeridianFlipState()->getMeridianFlipStage() == MeridianFlipState::MF_INITIATED))
            {
                // Meridian flip will abort focusing. In this case, after the meridian flip has completed capture
                // will restart the re-focus attempt. Therefore we only abort capture if meridian flip is not running.
                emit newFocusStatus(state);
                appendLogText(i18n("Autofocus failed. Aborting exposure..."));
                emit abortCapture();
            }
            break;
        case FOCUS_COMPLETE:
            // enable option to have a refocus event occur if HFR goes over threshold
            m_refocusState->setAutoFocusReady(true);
            // forward to the active job
            if (m_activeJob != nullptr)
                m_activeJob->setAutoFocusReady(true);
            // successful focus so reset elapsed time, force a reset
            m_refocusState->startRefocusTimer(true);

            if (m_fileHFR == 0.0)
            {
                m_refocusState->addHFRValue(getCurrentFilterName());
                updateHFRThreshold();
            }
            emit newFocusStatus(state);
            break;
        default:
            break;
    }
}

bool CaptureModuleState::startFocusIfRequired()
{
    // Do not start focus if:
    // 1. There is no active job, or
    // 2. Target frame is not LIGHT
    // 3. Capture is preview only
    if (m_activeJob == nullptr || m_activeJob->getFrameType() != FRAME_LIGHT
            || m_activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool())
        return false;

    RefocusState::RefocusReason reason = m_refocusState->checkFocusRequired();

    // no focusing necessary
    if (reason == RefocusState::REFOCUS_NONE)
        return false;

    // clear the flag for refocusing after the meridian flip
    m_refocusState->setRefocusAfterMeridianFlip(false);

    // Post meridian flip we need to reset filter _before_ running in-sequence focusing
    // as it could have changed for whatever reason (e.g. alignment used a different filter).
    // Then when focus process begins with the _target_ filter in place, it should take all the necessary actions to make it
    // work for the next set of captures. This is direct reset to the filter device, not via Filter Manager.
    if (getMeridianFlipState()->getMeridianFlipStage() != MeridianFlipState::MF_NONE)
    {
        int targetFilterPosition = m_activeJob->getTargetFilter();
        if (targetFilterPosition > 0 && targetFilterPosition != getCurrentFilterPosition())
            emit newFilterPosition(targetFilterPosition);
    }

    emit abortFastExposure();
    updateFocusState(FOCUS_PROGRESS);

    switch (reason)
    {
        case RefocusState::REFOCUS_HFR:
            m_refocusState->resetInSequenceFocusCounter();
            emit checkFocus(Options::hFRDeviation() == 0.0 ? 0.1 : Options::hFRDeviation());
            qCDebug(KSTARS_EKOS_CAPTURE) << "In-sequence focusing started...";
            break;
        case RefocusState::REFOCUS_TEMPERATURE:
        case RefocusState::REFOCUS_TIME_ELAPSED:
        case RefocusState::REFOCUS_POST_MF:
            // If we are over 30 mins since last autofocus, we'll reset frame.
            if (m_refocusState->getRefocusEveryNTimerElapsedSec() >= 1800)
                emit resetFocus();

            // force refocus
            emit checkFocus(0.1);
            // restart in sequence counting
            m_refocusState->resetInSequenceFocusCounter();
            qCDebug(KSTARS_EKOS_CAPTURE) << "Refocusing started...";
            break;
        default:
            // this should not happen, since this case is handled above
            return false;
    }

    setCaptureState(CAPTURE_FOCUSING);
    return true;
}

void CaptureModuleState::updateHFRThreshold()
{
    if (m_fileHFR != 0.0)
        return;

    QList<double> filterHFRList;
    if (m_CurrentFilterPosition > 0)
    {
        // If we are using filters, then we retrieve which filter is currently active.
        // We check if filter lock is used, and store that instead of the current filter.
        // e.g. If current filter HA, but lock filter is L, then the HFR value is stored for L filter.
        // If no lock filter exists, then we store as is (HA)
        QString finalFilter = (m_CurrentFocusFilterName == "--" ? m_CurrentFilterName : m_CurrentFocusFilterName);

        filterHFRList = m_refocusState->getHFRMap()[finalFilter];
    }
    // No filters
    else
    {
        filterHFRList = m_refocusState->getHFRMap()["--"];
    }

    // Update the limit only if HFR values have been measured for the current filter
    if (filterHFRList.empty() == false)
    {
        double median = 0;
        int count = filterHFRList.size();
        if (count > 1)
            median = (count % 2) ? filterHFRList[count / 2] : (filterHFRList[count / 2 - 1] + filterHFRList[count / 2]) / 2.0;
        else if (count == 1)
            median = filterHFRList[0];

        // Add 2.5% (default) to the automatic initial HFR value to allow for minute changes in HFR without need to refocus
        // in case in-sequence-focusing is used.
        median += median * (Options::hFRThresholdPercentage() / 100.0);
        Options::setHFRDeviation(median);
        emit newLimitFocusHFR(median);
    }
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

void CaptureModuleState::setGuideDeviation(double deviation_rms)
{
    const QString deviationText = QString("%1").arg(deviation_rms, 0, 'f', 3);

    // if guiding deviations occur and no job is active, check if a meridian flip is ready to be executed
    if (m_activeJob == nullptr && checkMeridianFlipReady())
        return;

    // if the job is in the startup phase and no flip is neither requested nor running, check if the deviation is below the initial guiding limit
    if (m_CaptureState == CAPTURE_PROGRESS &&
            getMeridianFlipState()->getMeridianFlipStage() != MeridianFlipState::MF_REQUESTED &&
            getMeridianFlipState()->checkMeridianFlipRunning() == false)
    {
        // initial guiding deviation irrelevant or below limit
        if (Options::enforceStartGuiderDrift() == false || deviation_rms < Options::startGuideDeviation())
        {
            setCaptureState(CAPTURE_CALIBRATING);
            if (Options::enforceStartGuiderDrift())
                appendLogText(i18n("Initial guiding deviation %1 below limit value of %2 arcsecs",
                                   deviationText, Options::startGuideDeviation()));
            setGuidingDeviationDetected(false);
            setStartingCapture(false);
        }
        else
        {
            // warn only once
            if (isGuidingDeviationDetected() == false)
                appendLogText(i18n("Initial guiding deviation %1 exceeded limit value of %2 arcsecs",
                                   deviationText, Options::startGuideDeviation()));

            setGuidingDeviationDetected(true);

            // Check if we need to start meridian flip. If yes, we need to start capturing
            // to ensure that capturing is recovered after the flip
            if (checkMeridianFlipReady())
                emit startCapture();
        }

        // in any case, do not proceed
        return;
    }

    // If guiding is started after a meridian flip we will start getting guide deviations again
    // if the guide deviations are within our limits, we resume the sequence
    if (getMeridianFlipState()->getMeridianFlipStage() == MeridianFlipState::MF_GUIDING)
    {
        // If the user didn't select any guiding deviation, the meridian flip is completed
        if (Options::enforceGuideDeviation() == false || deviation_rms < Options::guideDeviation())
        {
            appendLogText(i18n("Post meridian flip calibration completed successfully."));
            // N.B. Set meridian flip stage AFTER resumeSequence() always
            getMeridianFlipState()->updateMeridianFlipStage(MeridianFlipState::MF_NONE);
            return;
        }
    }

    // Check for initial deviation in the middle of a sequence (above just checks at the start of a sequence).
    if (m_activeJob && m_activeJob->getStatus() == JOB_BUSY && m_activeJob->getFrameType() == FRAME_LIGHT
            && isStartingCapture() && Options::enforceStartGuiderDrift())
    {
        setStartingCapture(false);
        if (deviation_rms > Options::startGuideDeviation())
        {
            appendLogText(i18n("Guiding deviation at capture startup %1 exceeded limit %2 arcsecs.",
                               deviationText, Options::startGuideDeviation()));
            emit suspendCapture();
            setGuidingDeviationDetected(true);

            // Check if we need to start meridian flip. If yes, we need to start capturing
            // to ensure that capturing is recovered after the flip
            if (checkMeridianFlipReady())
                emit startCapture();
            else
                getGuideDeviationTimer().start();
            return;
        }
        else
            appendLogText(i18n("Guiding deviation at capture startup %1 below limit value of %2 arcsecs",
                               deviationText, Options::startGuideDeviation()));
    }

    if (m_CaptureState != CAPTURE_SUSPENDED)
    {

        // We don't enforce limit on previews or non-LIGHT frames
        if ((Options::enforceGuideDeviation() == false)
                ||
                (m_activeJob  && (m_activeJob->getCoreProperty(SequenceJob::SJ_Preview).toBool() ||
                                  m_activeJob->getExposeLeft() == 0.0 ||
                                  m_activeJob->getFrameType() != FRAME_LIGHT)))
            return;

        // If we have an active busy job, let's abort it if guiding deviation is exceeded.
        // And we accounted for the spike
        if (m_activeJob && m_activeJob->getStatus() == JOB_BUSY && m_activeJob->getFrameType() == FRAME_LIGHT)
        {
            if (deviation_rms <= Options::guideDeviation())
                resetSpikesDetected();
            else
            {
                // Require several consecutive spikes to fail.
                constexpr int CONSECUTIVE_SPIKES_TO_FAIL = 3;
                if (increaseSpikesDetected() < CONSECUTIVE_SPIKES_TO_FAIL)
                    return;

                appendLogText(i18n("Guiding deviation %1 exceeded limit value of %2 arcsecs for %4 consecutive samples, "
                                   "suspending exposure and waiting for guider up to %3 seconds.",
                                   deviationText, Options::guideDeviation(),
                                   QString("%L1").arg(getGuideDeviationTimer().interval() / 1000.0, 0, 'f', 3),
                                   CONSECUTIVE_SPIKES_TO_FAIL));

                emit suspendCapture();

                resetSpikesDetected();
                setGuidingDeviationDetected(true);

                // Check if we need to start meridian flip. If yes, we need to start capturing
                // to ensure that capturing is recovered after the flip
                if (checkMeridianFlipReady())
                    emit startCapture();
                else
                    getGuideDeviationTimer().start();
            }
            return;
        }
    }

    // Find the first aborted job
    SequenceJob *abortedJob = nullptr;
    for(auto &job : m_allJobs)
    {
        if (job->getStatus() == JOB_ABORTED)
        {
            abortedJob = job;
            break;
        }
    }

    if (abortedJob != nullptr && isGuidingDeviationDetected())
    {
        if (deviation_rms <= Options::startGuideDeviation())
        {
            getGuideDeviationTimer().stop();

            // Start with delay if start hasn't been triggered before
            if (! m_captureDelayTimer.isActive())
            {
                // if capturing has been suspended, restart it
                if (m_CaptureState == CAPTURE_SUSPENDED)
                {
                    const int seqDelay = abortedJob->getCoreProperty(SequenceJob::SJ_Delay).toInt();
                    if (seqDelay == 0)
                        appendLogText(i18n("Guiding deviation %1 is now lower than limit value of %2 arcsecs, "
                                           "resuming exposure.",
                                           deviationText, Options::startGuideDeviation()));
                    else
                        appendLogText(i18n("Guiding deviation %1 is now lower than limit value of %2 arcsecs, "
                                           "resuming exposure in %3 seconds.",
                                           deviationText, Options::startGuideDeviation(), seqDelay / 1000.0));

                    m_captureDelayTimer.start(seqDelay);
                }
            }
            return;
        }
        else
        {
            // stop the delayed capture start if necessary
            if (m_captureDelayTimer.isActive())
                m_captureDelayTimer.stop();

            appendLogText(i18n("Guiding deviation %1 is still higher than limit value of %2 arcsecs.",
                               deviationText, Options::startGuideDeviation()));
        }
    }
}


void CaptureModuleState::appendLogText(const QString &message)
{
    qCInfo(KSTARS_EKOS_CAPTURE()) << message;
    emit newLog(message);
}

} // namespace
