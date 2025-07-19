/*  Ekos state machine for the Capture module
    SPDX-FileCopyrightText: 2022 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "camerastate.h"
#include "ekos/manager/meridianflipstate.h"
#include "ekos/capture/sequencejob.h"
#include "ekos/capture/sequencequeue.h"
#include "fitsviewer/fitsdata.h"

#include "ksnotification.h"
#include <ekos_capture_debug.h>

#define GD_TIMER_TIMEOUT    60000

namespace Ekos
{
void CameraState::init()
{
    m_sequenceQueue.reset(new SequenceQueue());
    m_refocusState.reset(new RefocusState());
    m_TargetADUTolerance = Options::calibrationADUValueTolerance();
    connect(m_sequenceQueue.get(), &SequenceQueue::newLog, this, &CameraState::newLog);
    connect(m_refocusState.get(), &RefocusState::newLog, this, &CameraState::newLog);

    getGuideDeviationTimer().setInterval(GD_TIMER_TIMEOUT);
    connect(&m_guideDeviationTimer, &QTimer::timeout, this, &CameraState::checkGuideDeviationTimeout);

    setCalibrationPreAction(Options::calibrationPreActionIndex());
    setFlatFieldDuration(static_cast<FlatFieldDuration>(Options::calibrationFlatDurationIndex()));
    wallCoord().setAz(Options::calibrationWallAz());
    wallCoord().setAlt(Options::calibrationWallAlt());
    setTargetADU(Options::calibrationADUValue());
    setSkyFlat(Options::calibrationSkyFlat());
}

CameraState::CameraState(QObject *parent): QObject{parent}
{
    init();
}

QList<QSharedPointer<SequenceJob>> &CameraState::allJobs()
{
    return m_sequenceQueue->allJobs();
}

const QUrl &CameraState::sequenceURL() const
{
    return m_sequenceQueue->sequenceURL();
}

void CameraState::setSequenceURL(const QUrl &newSequenceURL)
{
    m_sequenceQueue->setSequenceURL(newSequenceURL);
    placeholderPath().setSeqFilename(QFileInfo(newSequenceURL.toLocalFile()));
}

void CameraState::setActiveJob(const QSharedPointer<SequenceJob> &value)
{
    // do nothing if active job is not changed
    if (m_activeJob == value)
        return;

    // clear existing job connections
    if (m_activeJob != nullptr)
    {
        disconnect(this, nullptr, m_activeJob.get(), nullptr);
        disconnect(m_activeJob.get(), nullptr, this, nullptr);
        // ensure that the device adaptor does not send any new events
        m_activeJob->disconnectDeviceAdaptor();
    }

    // set the new value
    m_activeJob = value;

    // create job connections
    if (m_activeJob != nullptr)
    {
        // connect job with device adaptor events
        m_activeJob->connectDeviceAdaptor();
        // forward signals to the sequence job
        connect(this, &CameraState::newGuiderDrift, m_activeJob.get(), &SequenceJob::updateGuiderDrift);
        // react upon sequence job signals
        connect(m_activeJob.get(), &SequenceJob::prepareState, this, &CameraState::updatePrepareState);
        connect(m_activeJob.get(), &SequenceJob::prepareComplete, this, &CameraState::setPrepareComplete, Qt::UniqueConnection);
        connect(m_activeJob.get(), &SequenceJob::abortCapture, this, &CameraState::abortCapture);
        connect(m_activeJob.get(), &SequenceJob::captureStarted, this, &CameraState::captureStarted);
        connect(m_activeJob.get(), &SequenceJob::newLog, this, &CameraState::newLog);
        // forward the devices and attributes
        m_activeJob->updateDeviceStates();
        m_activeJob->setAutoFocusReady(getRefocusState()->isAutoFocusReady());
    }

}

int CameraState::activeJobID()
{
    if (m_activeJob.isNull())
        return -1;

    for (int i = 0; i < allJobs().count(); i++)
    {
        if (m_activeJob == allJobs().at(i))
            return i;
    }

    return -1;

}

void CameraState::initCapturePreparation()
{
    setStartingCapture(false);

    // Reset progress option if there is no captured frame map set at the time of start - fixes the end-user setting the option just before starting
    setIgnoreJobProgress(!hasCapturedFramesMap() && Options::alwaysResetSequenceWhenStarting());

    // Refocus timer should not be reset on deviation error
    if (isGuidingDeviationDetected() == false && getCaptureState() != CAPTURE_SUSPENDED)
    {
        // start timer to measure time until next forced refocus
        getRefocusState()->startRefocusTimer();
    }

    // Only reset these counters if we are NOT restarting from deviation errors
    // So when starting a new job or fresh then we reset them.
    if (isGuidingDeviationDetected() == false)
    {
        resetDitherCounter();
        getRefocusState()->resetInSequenceFocusCounter();
        getRefocusState()->setAdaptiveFocusDone(false);
    }

    setGuidingDeviationDetected(false);
    resetSpikesDetected();

    setCaptureState(CAPTURE_PROGRESS);
    setBusy(true);

    initPlaceholderPath();

    if (Options::enforceGuideDeviation() && isGuidingOn() == false)
        emit newLog(i18n("Warning: Guide deviation is selected but autoguide process was not started."));
}

void CameraState::setCaptureState(CaptureState value)
{
    bool pause_planned = false;
    // handle new capture state
    switch (value)
    {
        case CAPTURE_IDLE:
        case CAPTURE_ABORTED:
            emit resetNonGuidedDither();
        /* Fall through */
        case CAPTURE_SUSPENDED:
        case CAPTURE_PAUSED:
            // meridian flip may take place if requested
            if (mf_state->getMeridianFlipStage() == MeridianFlipState::MF_REQUESTED)
                mf_state->updateMeridianFlipStage(MeridianFlipState::MF_READY);
            break;
        case CAPTURE_IMAGE_RECEIVED:
            // remember pause planning before receiving an image
            pause_planned = (m_CaptureState == CAPTURE_PAUSE_PLANNED);
            break;
        case CAPTURE_DITHERING:
            emit requestAction(CAPTURE_ACTION_DITHER_REQUEST);
        default:
            // do nothing
            break;
    }

    // Only emit status if it changed
    if (m_CaptureState != value)
    {
        qCDebug(KSTARS_EKOS_CAPTURE()) << "Capture State changes from" << getCaptureStatusString(
                                           m_CaptureState) << "to" << getCaptureStatusString(value);
        m_CaptureState = value;
        getMeridianFlipState()->setCaptureState(m_CaptureState);
        emit newStatus(m_CaptureState);
        // reset to planned state if necessary
        if (pause_planned)
        {
            m_CaptureState = CAPTURE_PAUSE_PLANNED;
            emit newStatus(m_CaptureState);
        }
    }
}

void CameraState::setGuideState(GuideState state)
{
    if (state != m_GuideState)
        qCDebug(KSTARS_EKOS_CAPTURE) << "Guiding state changed from" <<
                                     Ekos::getGuideStatusString(m_GuideState)
                                     << "to" << Ekos::getGuideStatusString(state);
    switch (state)
    {
        case GUIDE_IDLE:
        case GUIDE_GUIDING:
        case GUIDE_CALIBRATION_SUCCESS:
            break;

        case GUIDE_ABORTED:
        case GUIDE_CALIBRATION_ERROR:
            processGuidingFailed();
            break;

        case GUIDE_DITHERING_SUCCESS:
            qCInfo(KSTARS_EKOS_CAPTURE) << "Dithering succeeded, capture state" << getCaptureStatusString(
                                            getCaptureState());
            // do nothing if something happened during dithering
            appendLogText(i18n("Dithering succeeded."));
            if (getCaptureState() != CAPTURE_DITHERING)
                break;

            if (Options::guidingSettle() > 0)
            {
                // N.B. Do NOT convert to i18np since guidingRate is DOUBLE value (e.g. 1.36) so we always use plural with that.
                appendLogText(i18n("Dither complete. Resuming in %1 seconds...", Options::guidingSettle()));
                getGuideSettleTimer().singleShot(Options::guidingSettle() * 1000, this, [this]()
                {
                    setDitheringState(IPS_OK);
                });
            }
            else
            {
                appendLogText(i18n("Dither complete."));
                setDitheringState(IPS_OK);
            }
            break;

        case GUIDE_DITHERING_ERROR:
            qCInfo(KSTARS_EKOS_CAPTURE) << "Dithering failed, capture state" << getCaptureStatusString(
                                            getCaptureState());
            if (getCaptureState() != CAPTURE_DITHERING)
                break;

            if (Options::guidingSettle() > 0)
            {
                // N.B. Do NOT convert to i18np since guidingRate is DOUBLE value (e.g. 1.36) so we always use plural with that.
                appendLogText(i18n("Warning: Dithering failed. Resuming in %1 seconds...", Options::guidingSettle()));
                // set dithering state to OK after settling time and signal to proceed
                QTimer::singleShot(Options::guidingSettle() * 1000, this, [this]()
                {
                    setDitheringState(IPS_OK);
                });
            }
            else
            {
                appendLogText(i18n("Warning: Dithering failed."));
                // signal OK so that capturing may continue although dithering failed
                setDitheringState(IPS_OK);
            }

            break;

        default:
            break;
    }

    m_GuideState = state;
    // forward it to the currently active sequence job
    if (m_activeJob != nullptr)
        m_activeJob->setCoreProperty(SequenceJob::SJ_GuiderActive, isActivelyGuiding());
}



void CameraState::setCurrentFilterPosition(int position, const QString &name, const QString &focusFilterName)
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

void CameraState::dustCapStateChanged(ISD::DustCap::Status status)
{
    switch (status)
    {
        case ISD::DustCap::CAP_ERROR:
            setDustCapState(CAP_ERROR);
            emit newLog(i18n("Dust cap error."));
            break;
        case ISD::DustCap::CAP_PARKED:
            setDustCapState(CAP_PARKED);
            emit newLog(i18n("Dust cap parked."));
            break;
        case ISD::DustCap::CAP_IDLE:
            setDustCapState(CAP_IDLE);
            emit newLog(i18n("Dust cap unparked."));
            break;
        case ISD::DustCap::CAP_UNPARKING:
            setDustCapState(CAP_UNPARKING);
            break;
        case ISD::DustCap::CAP_PARKING:
            setDustCapState(CAP_PARKING);
            break;
    }
}

QSharedPointer<MeridianFlipState> CameraState::getMeridianFlipState()
{
    // lazy instantiation
    if (mf_state.isNull())
        mf_state.reset(new MeridianFlipState());

    return mf_state;
}

void CameraState::setMeridianFlipState(QSharedPointer<MeridianFlipState> state)
{
    // clear old state machine
    if (! mf_state.isNull())
    {
        mf_state->disconnect(this);
    }

    mf_state = state;
    connect(mf_state.data(), &Ekos::MeridianFlipState::newMountMFStatus, this, &Ekos::CameraState::updateMFMountState,
            Qt::UniqueConnection);
}

void CameraState::setObserverName(const QString &value)
{
    m_ObserverName = value;
    Options::setDefaultObserver(value);
}

void CameraState::setBusy(bool busy)
{
    m_Busy = busy;
    emit captureBusy(busy);
}



bool CameraState::generateFilename(const QString &extension, QString *filename)
{
    *filename = placeholderPath().generateOutputFilename(true, true, nextSequenceID(), extension, "");

    QDir currentDir = QFileInfo(*filename).dir();
    if (currentDir.exists() == false)
        QDir().mkpath(currentDir.path());

    // Check if the file exists. We try not to overwrite capture files.
    if (QFile::exists(*filename))
    {
        QString oldFilename = *filename;
        *filename = placeholderPath().repairFilename(*filename);
        if (*filename != oldFilename)
            qCWarning(KSTARS_EKOS_CAPTURE) << "File over-write detected: changing" << oldFilename << "to" << *filename;
        else
            qCWarning(KSTARS_EKOS_CAPTURE) << "File over-write detected for" << oldFilename << "but could not correct filename";
    }

    QFile test_file(*filename);
    if (!test_file.open(QIODevice::WriteOnly))
        return false;
    test_file.flush();
    test_file.close();
    return true;
}

void CameraState::decreaseDitherCounter()
{
    if (m_ditherCounter > 0)
        --m_ditherCounter;
}

void CameraState::resetDitherCounter()
{
    uint value = 0;
    if (m_activeJob)
        value = m_activeJob->getCoreProperty(SequenceJob::SJ_DitherPerJobFrequency).toInt(0);

    if (value > 0)
        m_ditherCounter = value;
    else
        m_ditherCounter = Options::ditherFrames();
}

bool CameraState::checkDithering()
{
    // No need if preview only
    if (m_activeJob && m_activeJob->jobType() == SequenceJob::JOBTYPE_PREVIEW)
        return false;

    if ( (Options::ditherEnabled() || Options::ditherNoGuiding())
            && (m_activeJob != nullptr && m_activeJob->getCoreProperty(SequenceJob::SJ_DitherPerJobEnabled).toBool())
            // 2017-09-20 Jasem: No need to dither after post meridian flip guiding
            && getMeridianFlipState()->getMeridianFlipStage() != MeridianFlipState::MF_GUIDING
            // We must be either in guide mode or if non-guide dither (via pulsing) is enabled
            && (getGuideState() == GUIDE_GUIDING || Options::ditherNoGuiding())
            // Must be only done for light frames
            && (m_activeJob != nullptr && m_activeJob->getFrameType() == FRAME_LIGHT)
            // Check dither counter
            && m_ditherCounter == 0)
    {
        // reset the dither counter
        resetDitherCounter();

        appendLogText(i18n("Dithering requested..."));

        setCaptureState(CAPTURE_DITHERING);
        setDitheringState(IPS_BUSY);

        return true;
    }
    // no dithering required
    return false;
}

void CameraState::updateMFMountState(MeridianFlipState::MeridianFlipMountState status)
{
    qCDebug(KSTARS_EKOS_CAPTURE) << "updateMFMountState: " << MeridianFlipState::meridianFlipStatusString(status);

    switch (status)
    {
        case MeridianFlipState::MOUNT_FLIP_NONE:
            // MF_NONE as external signal ignored so that re-alignment and guiding are processed first
            if (getMeridianFlipState()->getMeridianFlipStage() < MeridianFlipState::MF_COMPLETED)
                updateMeridianFlipStage(MeridianFlipState::MF_NONE);
            break;

        case MeridianFlipState::MOUNT_FLIP_PLANNED:
            if (getMeridianFlipState()->getMeridianFlipStage() > MeridianFlipState::MF_REQUESTED)
            {
                // This should never happen, since a meridian flip seems to be ongoing
                qCritical(KSTARS_EKOS_CAPTURE) << "Accepting meridian flip request while being in stage " <<
                                               getMeridianFlipState()->getMeridianFlipStage();
            }

            // If we are autoguiding, we should resume autoguiding after flip
            getMeridianFlipState()->setResumeGuidingAfterFlip(isGuidingOn());

            // mark flip as requested
            updateMeridianFlipStage(MeridianFlipState::MF_REQUESTED);
            // if capture is not running, immediately accept it
            if (m_CaptureState == CAPTURE_IDLE || m_CaptureState == CAPTURE_ABORTED
                    || m_CaptureState == CAPTURE_COMPLETE || m_CaptureState == CAPTURE_PAUSED)
                getMeridianFlipState()->updateMFMountState(MeridianFlipState::MOUNT_FLIP_ACCEPTED);

            break;

        case MeridianFlipState::MOUNT_FLIP_RUNNING:
            updateMeridianFlipStage(MeridianFlipState::MF_INITIATED);
            setCaptureState(CAPTURE_MERIDIAN_FLIP);
            break;

        case MeridianFlipState::MOUNT_FLIP_COMPLETED:
            updateMeridianFlipStage(MeridianFlipState::MF_COMPLETED);
            break;

        default:
            break;

    }
}

void CameraState::updateMeridianFlipStage(const MeridianFlipState::MFStage &stage)
{
    // forward the stage to the module state
    getMeridianFlipState()->updateMeridianFlipStage(stage);

    // handle state changes for other modules
    switch (stage)
    {
        case MeridianFlipState::MF_READY:
            break;

        case MeridianFlipState::MF_INITIATED:
            emit meridianFlipStarted();
            break;

        case MeridianFlipState::MF_COMPLETED:

            // Reset HFR Check counter after meridian flip
            if (getRefocusState()->isInSequenceFocus())
            {
                qCDebug(KSTARS_EKOS_CAPTURE) << "Resetting HFR Check counter after meridian flip.";
                //firstAutoFocus = true;
                getRefocusState()->setInSequenceFocusCounter(0);
            }

            // after a meridian flip we do not need to dither
            if ( Options::ditherEnabled() || Options::ditherNoGuiding())
                resetDitherCounter();

            // if requested set flag so it perform refocus before next frame
            if (Options::refocusAfterMeridianFlip() == true)
                getRefocusState()->setRefocusAfterMeridianFlip(true);

            KSNotification::event(QLatin1String("MeridianFlipCompleted"), i18n("Meridian flip is successfully completed"),
                                  KSNotification::Capture);

            getMeridianFlipState()->processFlipCompleted();

            // if the capturing has been paused before the flip, reset the state to paused, otherwise to idle
            setCaptureState(m_ContinueAction == CAPTURE_CONTINUE_ACTION_NONE ? CAPTURE_IDLE : CAPTURE_PAUSED);
            break;

        default:
            break;
    }
    // forward the new stage
    emit newMeridianFlipStage(stage);
}

bool CameraState::checkMeridianFlipActive()
{
    return (getMeridianFlipState()->checkMeridianFlipRunning() ||
            checkPostMeridianFlipActions() ||
            checkMeridianFlipReady());
}

bool CameraState::checkMeridianFlipReady()
{
    if (hasTelescope == false)
        return false;

    // If active job is taking flat field image at a wall source
    // then do not flip.
    if (m_activeJob && m_activeJob->getFrameType() == FRAME_FLAT
            && m_activeJob->getCalibrationPreAction() & CAPTURE_PREACTION_WALL)
        return false;

    if (getMeridianFlipState()->getMeridianFlipStage() != MeridianFlipState::MF_REQUESTED)
        // if no flip has been requested or is already ongoing
        return false;

    // meridian flip requested or already in action

    // Reset frame if we need to do focusing later on
    if (m_refocusState->isInSequenceFocus() ||
            (Options::enforceRefocusEveryN() && m_refocusState->getRefocusEveryNTimerElapsedSec() > 0))
        emit resetFocusFrame();

    // signal that meridian flip may take place
    if (getMeridianFlipState()->getMeridianFlipStage() == MeridianFlipState::MF_REQUESTED)
        getMeridianFlipState()->updateMeridianFlipStage(MeridianFlipState::MF_READY);


    return true;
}

bool CameraState::checkPostMeridianFlipActions()
{
    // step 1: If dome is syncing, wait until it stops
    if (hasDome && (m_domeState == ISD::Dome::DOME_MOVING_CW || m_domeState == ISD::Dome::DOME_MOVING_CCW))
        return true;

    // step 2: check if post flip alignment is running
    if (m_CaptureState == CAPTURE_ALIGNING || checkAlignmentAfterFlip())
        return true;

    // step 2: check if post flip guiding is running
    // MF_NONE is set as soon as guiding is running and the guide deviation is below the limit
    if (getMeridianFlipState()->getMeridianFlipStage() >= MeridianFlipState::MF_COMPLETED && m_GuideState != GUIDE_GUIDING
            && checkGuidingAfterFlip())
        return true;

    // step 4: in case that a meridian flip has been completed and a guide deviation limit is set, we wait
    //         until the guide deviation is reported to be below the limit (@see setGuideDeviation(double, double)).
    //         Otherwise the meridian flip is complete
    if (m_CaptureState == CAPTURE_CALIBRATING
            && getMeridianFlipState()->getMeridianFlipStage() == MeridianFlipState::MF_GUIDING)
    {
        if (Options::enforceGuideDeviation() || Options::enforceStartGuiderDrift())
            return true;
        else
            updateMeridianFlipStage(MeridianFlipState::MF_NONE);
    }

    // all actions completed or no flip running
    return false;
}

bool CameraState::checkGuidingAfterFlip()
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

void CameraState::processGuidingFailed()
{
    if (m_FocusState > FOCUS_PROGRESS)
    {
        appendLogText(i18n("Autoguiding stopped. Waiting for autofocus to finish..."));
    }
    // If Autoguiding was started before and now stopped, let's abort (unless we're doing a meridian flip)
    else if (isGuidingOn()
             && getMeridianFlipState()->getMeridianFlipStage() == MeridianFlipState::MF_NONE &&
             // JM 2022.08.03: Only abort if the current job is LIGHT. For calibration frames, we can ignore guide failures.
             ((m_activeJob && m_activeJob->getStatus() == JOB_BUSY && m_activeJob->getFrameType() == FRAME_LIGHT) ||
              m_CaptureState == CAPTURE_SUSPENDED || m_CaptureState == CAPTURE_PAUSED))
    {
        appendLogText(i18n("Autoguiding stopped. Aborting..."));
        emit abortCapture();
    }
    else if (getMeridianFlipState()->getMeridianFlipStage() == MeridianFlipState::MF_GUIDING)
    {
        if (increaseAlignmentRetries() >= 3)
        {
            appendLogText(i18n("Post meridian flip calibration error. Aborting..."));
            emit abortCapture();
        }
    }
}

void CameraState::updateAdaptiveFocusState(bool success)
{
    m_refocusState->setAdaptiveFocusDone(true);

    // Always process the adaptive focus state change, incl if a MF has also just started
    if (success)
        qCDebug(KSTARS_EKOS_CAPTURE) << "Adaptive focus completed successfully";
    else
        qCDebug(KSTARS_EKOS_CAPTURE) << "Adaptive focus failed";

    m_refocusState->setAutoFocusReady(true);
    // forward to the active job
    if (m_activeJob != nullptr)
        m_activeJob->setAutoFocusReady(true);

    setFocusState(FOCUS_COMPLETE);
    emit newLog(i18n(success ? "Adaptive focus complete." : "Adaptive focus failed. Continuing..."));
}

void CameraState::updateFocusState(FocusState state)
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
            // reset the timer if a full autofocus was run (rather than an HFR check)
            if (m_refocusState->getFocusHFRInAutofocus())
                m_refocusState->startRefocusTimer(true);

            // update HFR Threshold for those algorithms that use Autofocus as reference
            if (Options::hFRCheckAlgorithm() == HFR_CHECK_MEDIAN_MEASURE ||
                    (m_refocusState->getFocusHFRInAutofocus() && Options::hFRCheckAlgorithm() == HFR_CHECK_LAST_AUTOFOCUS))
            {
                m_refocusState->addHFRValue(getFocusFilterName());
                updateHFRThreshold();
            }
            emit newFocusStatus(state);
            break;
        default:
            break;
    }

    if (m_activeJob != nullptr)
    {
        // Account for in-sequence-focus action that may lead to guide suspension and resumption after it is completed successfully.
        // In this case, since the guiding was resumed, we should honor the guiding settle duration.
        // To satisfy this, the guide state must be guiding and focus suspend guiding should be active.
        if (state == FOCUS_COMPLETE && Options::guidingSettle() > 0 && Options::focusSuspendGuiding()
                && m_GuideState == GUIDE_GUIDING)
        {
            // N.B. Do NOT convert to i18np since guidingRate is DOUBLE value (e.g. 1.36) so we always use plural with that.
            appendLogText(i18n("Focus complete. Resuming in %1 seconds...", Options::guidingSettle()));
            QTimer::singleShot(Options::guidingSettle() * 1000, this, [this, state]()
            {
                if (m_activeJob != nullptr)
                    m_activeJob->setFocusStatus(state);
            });
        }
        else
            m_activeJob->setFocusStatus(state);
    }
}

AutofocusReason CameraState::getAFReason(RefocusState::RefocusReason state, QString &reasonInfo)
{
    AutofocusReason afReason;
    reasonInfo = "";
    switch (state)
    {
        case RefocusState::REFOCUS_USER_REQUEST:
            afReason = AutofocusReason::FOCUS_USER_REQUEST;
            break;
        case RefocusState::REFOCUS_TEMPERATURE:
            afReason = AutofocusReason::FOCUS_TEMPERATURE;
            reasonInfo = i18n("Limit: %1 °C", QString::number(Options::maxFocusTemperatureDelta(), 'f', 2));
            break;
        case RefocusState::REFOCUS_TIME_ELAPSED:
            afReason = AutofocusReason::FOCUS_TIME;
            reasonInfo = i18n("Limit: %1 mins", Options::refocusEveryN());
            break;
        case RefocusState::REFOCUS_POST_MF:
            afReason = AutofocusReason::FOCUS_MERIDIAN_FLIP;
            break;
        default:
            afReason = AutofocusReason::FOCUS_NONE;
    }
    return afReason;
}

bool CameraState::startFocusIfRequired()
{
    // Do not start focus or adaptive focus if:
    // 1. There is no active job, or
    // 2. Target frame is not LIGHT
    // 3. Capture is preview only
    if (m_activeJob == nullptr || m_activeJob->getFrameType() != FRAME_LIGHT
            || m_activeJob->jobType() == SequenceJob::JOBTYPE_PREVIEW)
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
    QString reasonInfo;
    AutofocusReason afReason;

    switch (reason)
    {
        case RefocusState::REFOCUS_HFR:
            m_refocusState->resetInSequenceFocusCounter();
            emit checkFocus(Options::hFRDeviation());
            qCDebug(KSTARS_EKOS_CAPTURE) << "In-sequence focusing started...";
            break;
        case RefocusState::REFOCUS_ADAPTIVE:
            m_refocusState->setAdaptiveFocusDone(true);
            emit adaptiveFocus();
            qCDebug(KSTARS_EKOS_CAPTURE) << "Adaptive focus started...";
            break;
        case RefocusState::REFOCUS_USER_REQUEST:
        case RefocusState::REFOCUS_TEMPERATURE:
        case RefocusState::REFOCUS_TIME_ELAPSED:
        case RefocusState::REFOCUS_POST_MF:
            // If we are over 30 mins since last autofocus, we'll reset frame.
            if (m_refocusState->getRefocusEveryNTimerElapsedSec() >= 1800)
                emit resetFocusFrame();

            // force refocus
            afReason = getAFReason(reason, reasonInfo);
            emit runAutoFocus(afReason, reasonInfo);
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

void CameraState::updateHFRThreshold()
{
    // For Ficed algo no need to update the HFR threshold
    if (Options::hFRCheckAlgorithm() == HFR_CHECK_FIXED)
        return;

    QString finalFilter = getFocusFilterName();
    QList<double> filterHFRList = m_refocusState->getHFRMap()[finalFilter];

    // Update the limit only if HFR values have been measured for the current filter
    if (filterHFRList.empty())
        return;

    double value = 0;
    if (Options::hFRCheckAlgorithm() == HFR_CHECK_LAST_AUTOFOCUS)
        value = filterHFRList.last();
    else // algo = Median Measure
    {
        int count = filterHFRList.size();
        if (count > 1)
            value = (count % 2) ? filterHFRList[count / 2] : (filterHFRList[count / 2 - 1] + filterHFRList[count / 2]) / 2.0;
        else if (count == 1)
            value = filterHFRList[0];
    }
    value += value * (Options::hFRThresholdPercentage() / 100.0);
    Options::setHFRDeviation(value);
    emit newLimitFocusHFR(value); // Updates the limits UI with the new HFR threshold
}

QString CameraState::getFocusFilterName()
{
    QString finalFilter;
    if (m_CurrentFilterPosition > 0)
        // If we are using filters, then we retrieve which filter is currently active.
        // We check if filter lock is used, and store that instead of the current filter.
        // e.g. If current filter HA, but lock filter is L, then the HFR value is stored for L filter.
        // If no lock filter exists, then we store as is (HA)
        finalFilter = (m_CurrentFocusFilterName == "--" ? m_CurrentFilterName : m_CurrentFocusFilterName);
    else
        // No filters
        finalFilter = "--";
    return finalFilter;
}

bool CameraState::checkAlignmentAfterFlip()
{
    // if no meridian flip has completed, we do not touch guiding
    if (getMeridianFlipState()->getMeridianFlipStage() < MeridianFlipState::MF_COMPLETED)
    {
        qCDebug(KSTARS_EKOS_CAPTURE) << "checkAlignmentAfterFlip too early, meridian flip stage =" <<
                                     getMeridianFlipState()->getMeridianFlipStage();
        return false;
    }
    // If we do not need to align then we're done
    if (getMeridianFlipState()->resumeAlignmentAfterFlip() == false)
    {
        qCDebug(KSTARS_EKOS_CAPTURE) << "No alignment after flip required.";
        return false;
    }

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

void CameraState::checkGuideDeviationTimeout()
{
    if (m_activeJob && m_activeJob->getStatus() == JOB_ABORTED
            && isGuidingDeviationDetected())
    {
        appendLogText(i18n("Guide module timed out."));
        setGuidingDeviationDetected(false);

        // If capture was suspended, it should be aborted (failed) now.
        if (m_CaptureState == CAPTURE_SUSPENDED)
        {
            setCaptureState(CAPTURE_ABORTED);
        }
    }
}

void CameraState::setGuideDeviation(double deviation_rms)
{
    // communicate the new guiding deviation
    emit newGuiderDrift(deviation_rms);

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
                (m_activeJob  && (m_activeJob->jobType() == SequenceJob::JOBTYPE_PREVIEW ||
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
                if (increaseSpikesDetected() < Options::guideDeviationReps())
                    return;

                appendLogText(i18n("Guiding deviation %1 exceeded limit value of %2 arcsecs for %4 consecutive samples, "
                                   "suspending exposure and waiting for guider up to %3 seconds.",
                                   deviationText, Options::guideDeviation(),
                                   QString("%L1").arg(getGuideDeviationTimer().interval() / 1000.0, 0, 'f', 3),
                                   Options::guideDeviationReps()));

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
    QSharedPointer<SequenceJob> abortedJob;
    for(auto &job : allJobs())
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
            if (! getCaptureDelayTimer().isActive())
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

                    emit startCapture();
                }
            }
            return;
        }
        else
        {
            // stop the delayed capture start if necessary
            if (getCaptureDelayTimer().isActive())
                getCaptureDelayTimer().stop();

            appendLogText(i18n("Guiding deviation %1 is still higher than limit value of %2 arcsecs.",
                               deviationText, Options::startGuideDeviation()));
        }
    }
}

void CameraState::addDownloadTime(double time)
{
    totalDownloadTime += time;
    downloadsCounter++;
}

int CameraState::pendingJobCount()
{
    int completedJobs = 0;

    foreach (auto job, allJobs())
    {
        if (job->getStatus() == JOB_DONE)
            completedJobs++;
    }

    return (allJobs().count() - completedJobs);

}

QString CameraState::jobState(int id)
{
    if (id < allJobs().count())
    {
        const QSharedPointer<SequenceJob> &job = allJobs().at(id);
        return job->getStatusString();
    }

    return QString();

}

QString CameraState::jobFilterName(int id)
{
    if (id < allJobs().count())
    {
        const QSharedPointer<SequenceJob> &job = allJobs().at(id);
        return job->getCoreProperty(SequenceJob::SJ_Filter).toString();
    }

    return QString();

}

CCDFrameType CameraState::jobFrameType(int id)
{
    if (id < allJobs().count())
    {
        const QSharedPointer<SequenceJob> &job = allJobs().at(id);
        return job->getFrameType();
    }

    return FRAME_NONE;
}

int CameraState::jobImageProgress(int id)
{
    if (id < allJobs().count())
    {
        const QSharedPointer<SequenceJob> &job = allJobs().at(id);
        return job->getCompleted();
    }

    return -1;
}

int CameraState::jobImageCount(int id)
{
    if (id < allJobs().count())
    {
        const QSharedPointer<SequenceJob> &job = allJobs().at(id);
        return job->getCoreProperty(SequenceJob::SJ_Count).toInt();
    }

    return -1;
}

double CameraState::jobExposureProgress(int id)
{
    if (id < allJobs().count())
    {
        const QSharedPointer<SequenceJob> &job = allJobs().at(id);
        return job->getExposeLeft();
    }

    return -1;
}

double CameraState::jobExposureDuration(int id)
{
    if (id < allJobs().count())
    {
        const QSharedPointer<SequenceJob> &job = allJobs().at(id);
        return job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();
    }

    return -1;
}

double CameraState::progressPercentage()
{
    int totalImageCount     = 0;
    int totalImageCompleted = 0;

    foreach (auto job, allJobs())
    {
        totalImageCount += job->getCoreProperty(SequenceJob::SJ_Count).toInt();
        totalImageCompleted += job->getCompleted();
    }

    if (totalImageCount != 0)
        return ((static_cast<double>(totalImageCompleted) / totalImageCount) * 100.0);
    else
        return -1;
}

bool CameraState::isActiveJobPreview()
{
    return m_activeJob && m_activeJob->jobType() == SequenceJob::JOBTYPE_PREVIEW;
}

int CameraState::activeJobRemainingTime()
{
    if (m_activeJob == nullptr)
        return -1;

    return m_activeJob->getJobRemainingTime(averageDownloadTime());
}

int CameraState::overallRemainingTime()
{
    int remaining = 0;
    double estimatedDownloadTime = averageDownloadTime();

    foreach (auto job, allJobs())
        remaining += job->getJobRemainingTime(estimatedDownloadTime);

    return remaining;
}

QString CameraState::sequenceQueueStatus()
{
    if (allJobs().count() == 0)
        return "Invalid";

    if (isBusy())
        return "Running";

    int idle = 0, error = 0, complete = 0, aborted = 0, running = 0;

    foreach (auto job, allJobs())
    {
        switch (job->getStatus())
        {
            case JOB_ABORTED:
                aborted++;
                break;
            case JOB_BUSY:
                running++;
                break;
            case JOB_DONE:
                complete++;
                break;
            case JOB_ERROR:
                error++;
                break;
            case JOB_IDLE:
                idle++;
                break;
        }
    }

    if (error > 0)
        return "Error";

    if (aborted > 0)
    {
        if (m_CaptureState == CAPTURE_SUSPENDED)
            return "Suspended";
        else
            return "Aborted";
    }

    if (running > 0)
        return "Running";

    if (idle == allJobs().count())
        return "Idle";

    if (complete == allJobs().count())
        return "Complete";

    return "Invalid";
}

QJsonObject CameraState::calibrationSettings()
{
    QJsonObject settings =
    {
        {"preAction", static_cast<int>(calibrationPreAction())},
        {"duration", flatFieldDuration()},
        {"az", wallCoord().az().Degrees()},
        {"al", wallCoord().alt().Degrees()},
        {"adu", targetADU()},
        {"tolerance", targetADUTolerance()},
        {"skyflat", skyFlat()},
    };

    return settings;
}

void CameraState::setCalibrationSettings(const QJsonObject &settings)
{
    const int preAction = settings["preAction"].toInt(calibrationPreAction());
    const int duration = settings["duration"].toInt(flatFieldDuration());
    const double az = settings["az"].toDouble(wallCoord().az().Degrees());
    const double al = settings["al"].toDouble(wallCoord().alt().Degrees());
    const int adu = settings["adu"].toInt(static_cast<int>(std::round(targetADU())));
    const int tolerance = settings["tolerance"].toInt(static_cast<int>(std::round(targetADUTolerance())));
    const int skyflat = settings["skyflat"].toBool();

    setCalibrationPreAction(static_cast<CalibrationPreActions>(preAction));
    setFlatFieldDuration(static_cast<FlatFieldDuration>(duration));
    wallCoord().setAz(az);
    wallCoord().setAlt(al);
    setTargetADU(adu);
    setTargetADUTolerance(tolerance);
    setSkyFlat(skyflat);
}

bool CameraState::setDarkFlatExposure(const QSharedPointer<SequenceJob> &job)
{
    const auto darkFlatFilter = job->getCoreProperty(SequenceJob::SJ_Filter).toString();
    const auto darkFlatBinning = job->getCoreProperty(SequenceJob::SJ_Binning).toPoint();
    const auto darkFlatADU = job->getCoreProperty(SequenceJob::SJ_TargetADU).toInt();

    for (auto &oneJob : allJobs())
    {
        if (oneJob->getFrameType() != FRAME_FLAT)
            continue;

        const auto filter = oneJob->getCoreProperty(SequenceJob::SJ_Filter).toString();

        // Match filter, if one exists.
        if (!darkFlatFilter.isEmpty() && darkFlatFilter != filter)
            continue;

        // Match binning
        const auto binning = oneJob->getCoreProperty(SequenceJob::SJ_Binning).toPoint();
        if (darkFlatBinning != binning)
            continue;

        // Match ADU, if used.
        const auto adu = oneJob->getCoreProperty(SequenceJob::SJ_TargetADU).toInt();
        if (job->getFlatFieldDuration() == DURATION_ADU)
        {
            if (darkFlatADU != adu)
                continue;
        }

        // Now get the exposure
        job->setCoreProperty(SequenceJob::SJ_Exposure, oneJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble());

        return true;
    }
    return false;
}

void CameraState::checkSeqBoundary()
{
    // No updates during meridian flip
    if (getMeridianFlipState()->getMeridianFlipStage() >= MeridianFlipState::MF_ALIGNING)
        return;

    setNextSequenceID(placeholderPath().checkSeqBoundary(*getActiveJob()));
}

bool CameraState::isModelinDSLRInfo(const QString &model)
{
    auto pos = std::find_if(m_DSLRInfos.begin(), m_DSLRInfos.end(), [model](QMap<QString, QVariant> &oneDSLRInfo)
    {
        return (oneDSLRInfo["Model"] == model);
    });

    return (pos != m_DSLRInfos.end());
}

void CameraState::setCapturedFramesCount(const QString &signature, uint16_t count)
{
    m_capturedFramesMap[signature] = count;
    qCDebug(KSTARS_EKOS_CAPTURE) <<
                                 QString("Client module indicates that storage for '%1' has already %2 captures processed.").arg(signature).arg(count);
    // Scheduler's captured frame map overrides the progress option of the Capture module
    setIgnoreJobProgress(false);
}

void CameraState::changeSequenceValue(int index, QString key, QString value)
{
    QJsonArray seqArray = getSequence();
    QJsonObject oneSequence = seqArray[index].toObject();
    oneSequence[key] = value;
    seqArray.replace(index, oneSequence);
    setSequence(seqArray);
    emit sequenceChanged(seqArray);
}

void CameraState::addCapturedFrame(const QString &signature)
{
    CapturedFramesMap::iterator frame_item = m_capturedFramesMap.find(signature);
    if (m_capturedFramesMap.end() != frame_item)
        frame_item.value()++;
    else m_capturedFramesMap[signature] = 1;
}

void CameraState::removeCapturedFrameCount(const QString &signature, uint16_t count)
{
    CapturedFramesMap::iterator frame_item = m_capturedFramesMap.find(signature);
    if (m_capturedFramesMap.end() != frame_item)
    {
        if (frame_item.value() <= count)
            // clear signature entry if none will be left
            m_capturedFramesMap.remove(signature);
        else
            // remove the frame count
            frame_item.value() = frame_item.value() - count;
    }
}

void CameraState::appendLogText(const QString &message)
{
    qCInfo(KSTARS_EKOS_CAPTURE()) << message;
    emit newLog(message);
}

bool CameraState::isGuidingOn()
{
    // In case we are doing non guiding dither, then we are not performing autoguiding.
    if (Options::ditherNoGuiding())
        return false;

    return (m_GuideState == GUIDE_GUIDING ||
            m_GuideState == GUIDE_CALIBRATING ||
            m_GuideState == GUIDE_CALIBRATION_SUCCESS ||
            m_GuideState == GUIDE_DARK ||
            m_GuideState == GUIDE_SUBFRAME ||
            m_GuideState == GUIDE_STAR_SELECT ||
            m_GuideState == GUIDE_REACQUIRE ||
            m_GuideState == GUIDE_DITHERING ||
            m_GuideState == GUIDE_DITHERING_SUCCESS ||
            m_GuideState == GUIDE_DITHERING_ERROR ||
            m_GuideState == GUIDE_DITHERING_SETTLE ||
            m_GuideState == GUIDE_SUSPENDED
           );
}

bool CameraState::isActivelyGuiding()
{
    return isGuidingOn() && (m_GuideState == GUIDE_GUIDING);
}

void CameraState::setAlignState(AlignState value)
{
    if (value != m_AlignState)
        qCDebug(KSTARS_EKOS_CAPTURE) << "Align State changed from" << Ekos::getAlignStatusString(
                                         m_AlignState) << "to" << Ekos::getAlignStatusString(value);
    m_AlignState = value;

    getMeridianFlipState()->setResumeAlignmentAfterFlip(true);

    switch (value)
    {
        case ALIGN_COMPLETE:
            if (getMeridianFlipState()->getMeridianFlipStage() == MeridianFlipState::MF_ALIGNING)
            {
                appendLogText(i18n("Post flip re-alignment completed successfully."));
                resetAlignmentRetries();
                // Trigger guiding if necessary.
                if (checkGuidingAfterFlip() == false)
                {
                    // If no guiding is required, the meridian flip is complete
                    updateMeridianFlipStage(MeridianFlipState::MF_NONE);
                    setCaptureState(CAPTURE_WAITING);
                }
            }
            break;

        case ALIGN_ABORTED:
        case ALIGN_FAILED:
            // TODO run it 3 times before giving up
            if (getMeridianFlipState()->getMeridianFlipStage() == MeridianFlipState::MF_ALIGNING)
            {
                if (increaseAlignmentRetries() >= 3)
                {
                    appendLogText(i18n("Post-flip alignment failed."));
                    emit abortCapture();
                }
                else
                {
                    appendLogText(i18n("Post-flip alignment failed. Retrying..."));
                    // Do not set back the stage to MF_COMPLETED here,
                    // as it causes repeated "Meridian flip completed" messages on alignment failures.
                    // The stage should remain MF_ALIGNING for the retry.
                }
            }
            break;

        default:
            break;
    }
}

void CameraState::setPrepareComplete(bool success)
{

    if (success)
    {
        qDebug(KSTARS_EKOS_CAPTURE) << "Capture preparation succeeded, start capturing.";
        setCaptureState(CAPTURE_PROGRESS);
        emit executeActiveJob();
    }
    else
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "Capture preparation failed, aborting.";
        setCaptureState(CAPTURE_ABORTED);
        emit abortCapture();
    }

}

} // namespace
