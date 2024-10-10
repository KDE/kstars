/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "cameraprocess.h"
#include "QtWidgets/qstatusbar.h"
#include "capturedeviceadaptor.h"
#include "refocusstate.h"
#include "sequencejob.h"
#include "sequencequeue.h"
#include "ekos/manager.h"
#include "ekos/auxiliary/darklibrary.h"
#include "ekos/auxiliary/darkprocessor.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/profilesettings.h"
#include "ekos/guide/guide.h"
#include "indi/indilistener.h"
#include "indi/indirotator.h"
#include "indi/blobmanager.h"
#include "indi/indilightbox.h"
#include "ksmessagebox.h"
#include "kstars.h"

#ifdef HAVE_CFITSIO
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitstab.h"
#endif
#include "fitsviewer/fitsviewer.h"

#include "ksnotification.h"
#include <ekos_capture_debug.h>

#ifdef HAVE_STELLARSOLVER
#include "ekos/auxiliary/stellarsolverprofileeditor.h"
#endif

namespace Ekos
{
CameraProcess::CameraProcess(QSharedPointer<CameraState> newModuleState,
                             QSharedPointer<CaptureDeviceAdaptor> newDeviceAdaptor) : QObject(KStars::Instance())
{
    setObjectName("CameraProcess");
    m_State = newModuleState;
    m_DeviceAdaptor = newDeviceAdaptor;

    // connect devices to processes
    connect(devices().data(), &CaptureDeviceAdaptor::newCamera, this, &CameraProcess::selectCamera);

    //This Timer will update the Exposure time in the capture module to display the estimated download time left
    //It will also update the Exposure time left in the Summary Screen.
    //It fires every 100 ms while images are downloading.
    state()->downloadProgressTimer().setInterval(100);
    connect(&state()->downloadProgressTimer(), &QTimer::timeout, this, &CameraProcess::setDownloadProgress);

    // configure dark processor
    m_DarkProcessor = new DarkProcessor(this);
    connect(m_DarkProcessor, &DarkProcessor::newLog, this, &CameraProcess::newLog);
    connect(m_DarkProcessor, &DarkProcessor::darkFrameCompleted, this, &CameraProcess::darkFrameCompleted);

    // Pre/post capture/job scripts
    connect(&m_CaptureScript,
            static_cast<void (QProcess::*)(int exitCode, QProcess::ExitStatus status)>(&QProcess::finished),
            this, &CameraProcess::scriptFinished);
    connect(&m_CaptureScript, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error)
    {
        Q_UNUSED(error)
        emit newLog(m_CaptureScript.errorString());
        scriptFinished(-1, QProcess::NormalExit);
    });
    connect(&m_CaptureScript, &QProcess::readyReadStandardError, this,
            [this]()
    {
        emit newLog(m_CaptureScript.readAllStandardError());
    });
    connect(&m_CaptureScript, &QProcess::readyReadStandardOutput, this,
            [this]()
    {
        emit newLog(m_CaptureScript.readAllStandardOutput());
    });
}

bool CameraProcess::setMount(ISD::Mount *device)
{
    if (devices()->mount() && devices()->mount() == device)
    {
        updateTelescopeInfo();
        return false;
    }

    if (devices()->mount())
        devices()->mount()->disconnect(state().data());

    devices()->setMount(device);

    if (!devices()->mount())
        return false;

    devices()->mount()->disconnect(this);
    connect(devices()->mount(), &ISD::Mount::newTargetName, this, &CameraProcess::captureTarget);

    updateTelescopeInfo();
    return true;
}

bool CameraProcess::setRotator(ISD::Rotator *device)
{
    // do nothing if *real* rotator is already connected
    if ((devices()->rotator() == device) && (device != nullptr))
        return false;

    // real & manual rotator initializing depends on present mount process
    if (devices()->mount())
    {
        if (devices()->rotator())
            devices()->rotator()->disconnect(this);

        // clear initialisation.
        state()->isInitialized[CAPTURE_ACTION_ROTATOR] = false;

        if (device)
        {
            Manager::Instance()->createRotatorController(device);
            connect(devices().data(), &CaptureDeviceAdaptor::rotatorReverseToggled, this, &CameraProcess::rotatorReverseToggled,
                    Qt::UniqueConnection);
        }
        devices()->setRotator(device);
        return true;
    }
    return false;
}

bool CameraProcess::setDustCap(ISD::DustCap *device)
{
    if (devices()->dustCap() && devices()->dustCap() == device)
        return false;

    devices()->setDustCap(device);
    state()->setDustCapState(CAP_UNKNOWN);

    updateFilterInfo();
    return true;

}

bool CameraProcess::setLightBox(ISD::LightBox *device)
{
    if (devices()->lightBox() == device)
        return false;

    devices()->setLightBox(device);
    state()->setLightBoxLightState(CAP_LIGHT_UNKNOWN);

    return true;
}

bool CameraProcess::setDome(ISD::Dome *device)
{
    if (devices()->dome() == device)
        return false;

    devices()->setDome(device);

    return true;
}

bool CameraProcess::setCamera(ISD::Camera *device)
{
    if (devices()->getActiveCamera() == device)
        return false;

    // disable passing through new frames to the FITS viewer
    if (activeCamera())
        disconnect(activeCamera(), &ISD::Camera::newImage, this, &CameraProcess::showFITSPreview);

    devices()->setActiveCamera(device);

    // If we capturing, then we need to process capture timeout immediately since this is a crash recovery
    if (state()->getCaptureTimeout().isActive() && state()->getCaptureState() == CAPTURE_CAPTURING)
        QTimer::singleShot(100, this, &CameraProcess::processCaptureTimeout);

    // enable passing through new frames to the FITS viewer
    if (activeCamera())
        connect(activeCamera(), &ISD::Camera::newImage, this, &CameraProcess::showFITSPreview);

    return true;

}

void CameraProcess::toggleVideo(bool enabled)
{
    if (devices()->getActiveCamera() == nullptr)
        return;

    if (devices()->getActiveCamera()->isBLOBEnabled() == false)
    {
        if (Options::guiderType() != Guide::GUIDE_INTERNAL)
            devices()->getActiveCamera()->setBLOBEnabled(true);
        else
        {
            connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, enabled]()
            {
                KSMessageBox::Instance()->disconnect(this);
                devices()->getActiveCamera()->setBLOBEnabled(true);
                devices()->getActiveCamera()->setVideoStreamEnabled(enabled);
            });

            KSMessageBox::Instance()->questionYesNo(i18n("Image transfer is disabled for this camera. Would you like to enable it?"),
                                                    i18n("Image Transfer"), 15);

            return;
        }
    }

    devices()->getActiveCamera()->setVideoStreamEnabled(enabled);

}

void CameraProcess::toggleSequence()
{
    const CaptureState capturestate = state()->getCaptureState();
    if (capturestate == CAPTURE_PAUSE_PLANNED || capturestate == CAPTURE_PAUSED)
    {
        // change the state back to capturing only if planned pause is cleared
        if (capturestate == CAPTURE_PAUSE_PLANNED)
            state()->setCaptureState(CAPTURE_CAPTURING);

        emit newLog(i18n("Sequence resumed."));

        // Call from where ever we have left of when we paused
        switch (state()->getContinueAction())
        {
            case CAPTURE_CONTINUE_ACTION_CAPTURE_COMPLETE:
                resumeSequence();
                break;
            case CAPTURE_CONTINUE_ACTION_NEXT_EXPOSURE:
                startNextExposure();
                break;
            default:
                break;
        }
    }
    else if (capturestate == CAPTURE_IDLE || capturestate == CAPTURE_ABORTED || capturestate == CAPTURE_COMPLETE)
    {
        startNextPendingJob();
    }
    else
    {
        emit stopCapture(CAPTURE_ABORTED);
    }
}

void CameraProcess::startNextPendingJob()
{
    if (state()->allJobs().count() > 0)
    {
        SequenceJob *nextJob = findNextPendingJob();
        if (nextJob != nullptr)
        {
            startJob(nextJob);
            emit jobStarting();
        }
        else // do nothing if no job is pending
            emit newLog(i18n("No pending jobs found. Please add a job to the sequence queue."));
    }
    else
    {
        // Add a new job from the current capture settings.
        // If this succeeds, Capture will call this function again.
        emit createJob();
    }
}

void CameraProcess::jobCreated(SequenceJob *newJob)
{
    if (newJob == nullptr)
    {
        emit newLog(i18n("No new job created."));
        return;
    }
    // a job has been created successfully
    switch (newJob->jobType())
    {
        case SequenceJob::JOBTYPE_BATCH:
            startNextPendingJob();
            break;
        case SequenceJob::JOBTYPE_PREVIEW:
            state()->setActiveJob(newJob);
            capturePreview();
            break;
        default:
            // do nothing
            break;
    }
}

void CameraProcess::capturePreview(bool loop)
{
    if (state()->getFocusState() >= FOCUS_PROGRESS)
    {
        emit newLog(i18n("Cannot capture while focus module is busy."));
    }
    else if (activeJob() == nullptr)
    {
        if (loop && !state()->isLooping())
        {
            state()->setLooping(true);
            emit newLog(i18n("Starting framing..."));
        }
        // create a preview job
        emit createJob(SequenceJob::JOBTYPE_PREVIEW);
    }
    else
    {
        // job created, start capture preparation
        prepareJob(activeJob());
    }
}

void CameraProcess::stopCapturing(CaptureState targetState)
{
    clearFlatCache();

    state()->resetAlignmentRetries();
    //seqTotalCount   = 0;
    //seqCurrentCount = 0;

    state()->getCaptureTimeout().stop();
    state()->getCaptureDelayTimer().stop();
    if (activeJob() != nullptr)
    {
        if (activeJob()->getStatus() == JOB_BUSY)
        {
            QString stopText;
            switch (targetState)
            {
                case CAPTURE_SUSPENDED:
                    stopText = i18n("CCD capture suspended");
                    resetJobStatus(JOB_BUSY);
                    break;

                case CAPTURE_COMPLETE:
                    stopText = i18n("CCD capture complete");
                    resetJobStatus(JOB_DONE);
                    break;

                case CAPTURE_ABORTED:
                    stopText = state()->isLooping() ? i18n("Framing stopped") : i18n("CCD capture stopped");
                    resetJobStatus(JOB_ABORTED);
                    break;

                default:
                    stopText = i18n("CCD capture stopped");
                    resetJobStatus(JOB_IDLE);
                    break;
            }
            emit captureAborted(activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble());
            KSNotification::event(QLatin1String("CaptureFailed"), stopText, KSNotification::Capture, KSNotification::Alert);
            emit newLog(stopText);

            // special case: if pausing has been requested, we pause
            if (state()->getCaptureState() == CAPTURE_PAUSE_PLANNED &&
                    checkPausing(CAPTURE_CONTINUE_ACTION_NEXT_EXPOSURE))
                return;
            // in all other cases, abort
            activeJob()->abort();
            if (activeJob()->jobType() != SequenceJob::JOBTYPE_PREVIEW)
            {
                int index = state()->allJobs().indexOf(activeJob());
                state()->changeSequenceValue(index, "Status", "Aborted");
                emit updateJobTable(activeJob());
            }
        }

        // In case of batch job
        if (activeJob()->jobType() != SequenceJob::JOBTYPE_PREVIEW)
        {
        }
        // or preview job in calibration stage
        else if (activeJob()->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION)
        {
        }
        // or regular preview job
        else
        {
            state()->allJobs().removeOne(activeJob());
            // Delete preview job
            activeJob()->deleteLater();
            // Clear active job
            state()->setActiveJob(nullptr);
        }
    }

    // stop focusing if capture is aborted
    if (state()->getCaptureState() == CAPTURE_FOCUSING && targetState == CAPTURE_ABORTED)
        emit abortFocus();

    state()->setCaptureState(targetState);

    state()->setLooping(false);
    state()->setBusy(false);

    state()->getCaptureDelayTimer().stop();

    state()->setActiveJob(nullptr);

    // Turn off any calibration light, IF they were turned on by Capture module
    if (devices()->lightBox() && state()->lightBoxLightEnabled())
    {
        state()->setLightBoxLightEnabled(false);
        devices()->lightBox()->setLightEnabled(false);
    }

    // disconnect camera device
    setCamera(false);

    // In case of exposure looping, let's abort
    if (devices()->getActiveCamera() && devices()->getActiveChip()
            && devices()->getActiveCamera()->isFastExposureEnabled())
        devices()->getActiveChip()->abortExposure();

    // communicate successful stop
    emit captureStopped();
}

void CameraProcess::pauseCapturing()
{
    if (state()->isCaptureRunning() == false)
    {
        // Ensure that the pause function is only called during frame capturing
        // Handling it this way is by far easier than trying to enable/disable the pause button
        // Fixme: make pausing possible at all stages. This makes it necessary to separate the pausing states from CaptureState.
        emit newLog(i18n("Pausing only possible while frame capture is running."));
        qCInfo(KSTARS_EKOS_CAPTURE) << "Pause button pressed while not capturing.";
        return;
    }
    // we do not decide at this stage how to resume, since pause is only planned here
    state()->setContinueAction(CAPTURE_CONTINUE_ACTION_NONE);
    state()->setCaptureState(CAPTURE_PAUSE_PLANNED);
    emit newLog(i18n("Sequence shall be paused after current exposure is complete."));
}

void CameraProcess::startJob(SequenceJob *job)
{
    state()->initCapturePreparation();
    prepareJob(job);
}

void CameraProcess::prepareJob(SequenceJob * job)
{
    if (activeCamera() == nullptr || activeCamera()->isConnected() == false)
    {
        emit newLog(i18n("No camera detected. Check train configuration and connection settings."));
        activeJob()->abort();
        return;
    }

    state()->setActiveJob(job);

    // If job is Preview and NO view is available, ask to enable it.
    // if job is batch job, then NO VIEW IS REQUIRED at all. It's optional.
    if (job->jobType() == SequenceJob::JOBTYPE_PREVIEW && Options::useFITSViewer() == false
            && Options::useSummaryPreview() == false)
    {
        // ask if FITS viewer usage should be enabled
        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [ = ]()
        {
            KSMessageBox::Instance()->disconnect(this);
            Options::setUseFITSViewer(true);
            // restart
            prepareJob(job);
        });
        connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [&]()
        {
            KSMessageBox::Instance()->disconnect(this);
            activeJob()->abort();
        });
        KSMessageBox::Instance()->questionYesNo(i18n("No view available for previews. Enable FITS viewer?"),
                                                i18n("Display preview"), 15);
        // do nothing because currently none of the previews is active.
        return;
    }

    if (state()->isLooping() == false)
        qCDebug(KSTARS_EKOS_CAPTURE) << "Preparing capture job" << job->getSignature() << "for execution.";

    if (activeJob()->jobType() != SequenceJob::JOBTYPE_PREVIEW)
    {
        // set the progress info

        if (activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
            state()->setNextSequenceID(1);

        // We check if the job is already fully or partially complete by checking how many files of its type exist on the file system
        // The signature is the unique identification path in the system for a particular job. Format is "<storage path>/<target>/<frame type>/<filter name>".
        // If the Scheduler is requesting the Capture tab to process a sequence job, a target name will be inserted after the sequence file storage field (e.g. /path/to/storage/target/Light/...)
        // If the end-user is requesting the Capture tab to process a sequence job, the sequence file storage will be used as is (e.g. /path/to/storage/Light/...)
        QString signature = activeJob()->getSignature();

        // Now check on the file system ALL the files that exist with the above signature
        // If 29 files exist for example, then nextSequenceID would be the NEXT file number (30)
        // Therefore, we know how to number the next file.
        // However, we do not deduce the number of captures to process from this function.
        state()->checkSeqBoundary();

        // Captured Frames Map contains a list of signatures:count of _already_ captured files in the file system.
        // This map is set by the Scheduler in order to complete efficiently the required captures.
        // When the end-user requests a sequence to be processed, that map is empty.
        //
        // Example with a 5xL-5xR-5xG-5xB sequence
        //
        // When the end-user loads and runs this sequence, each filter gets to capture 5 frames, then the procedure stops.
        // When the Scheduler executes a job with this sequence, the procedure depends on what is in the storage.
        //
        // Let's consider the Scheduler has 3 instances of this job to run.
        //
        // When the first job completes the sequence, there are 20 images in the file system (5 for each filter).
        // When the second job starts, Scheduler finds those 20 images but requires 20 more images, thus sets the frames map counters to 0 for all LRGB frames.
        // When the third job starts, Scheduler now has 40 images, but still requires 20 more, thus again sets the frames map counters to 0 for all LRGB frames.
        //
        // Now let's consider something went wrong, and the third job was aborted before getting to 60 images, say we have full LRG, but only 1xB.
        // When Scheduler attempts to run the aborted job again, it will count captures in storage, subtract previous job requirements, and set the frames map counters to 0 for LRG, and 4 for B.
        // When the sequence runs, the procedure will bypass LRG and proceed to capture 4xB.
        int count = state()->capturedFramesCount(signature);
        if (count > 0)
        {

            // Count how many captures this job has to process, given that previous jobs may have done some work already
            for (auto &a_job : state()->allJobs())
                if (a_job == activeJob())
                    break;
                else if (a_job->getSignature() == activeJob()->getSignature())
                    count -= a_job->getCompleted();

            // This is the current completion count of the current job
            updatedCaptureCompleted(count);
        }
        // JM 2018-09-24: Only set completed jobs to 0 IF the scheduler set captured frames map to begin with
        // If the map is empty, then no scheduler is used and it should proceed as normal.
        else if (state()->hasCapturedFramesMap())
        {
            // No preliminary information, we reset the job count and run the job unconditionally to clarify the behavior
            updatedCaptureCompleted(0);
        }
        // JM 2018-09-24: In case ignoreJobProgress is enabled
        // We check if this particular job progress ignore flag is set. If not,
        // then we set it and reset completed to zero. Next time it is evaluated here again
        // It will maintain its count regardless
        else if (state()->ignoreJobProgress()
                 && activeJob()->getJobProgressIgnored() == false)
        {
            activeJob()->setJobProgressIgnored(true);
            updatedCaptureCompleted(0);
        }
        // We cannot rely on sequenceID to give us a count - if we don't ignore job progress, we leave the count as it was originally

        // Check whether active job is complete by comparing required captures to what is already available
        if (activeJob()->getCoreProperty(SequenceJob::SJ_Count).toInt() <=
                activeJob()->getCompleted())
        {
            updatedCaptureCompleted(activeJob()->getCoreProperty(
                                        SequenceJob::SJ_Count).toInt());
            emit newLog(i18n("Job requires %1-second %2 images, has already %3/%4 captures and does not need to run.",
                             QString("%L1").arg(job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(), 0, 'f', 3),
                             job->getCoreProperty(SequenceJob::SJ_Filter).toString(),
                             activeJob()->getCompleted(),
                             activeJob()->getCoreProperty(SequenceJob::SJ_Count).toInt()));
            processJobCompletion2();

            /* FIXME: find a clearer way to exit here */
            return;
        }
        else
        {
            // There are captures to process
            emit newLog(i18n("Job requires %1-second %2 images, has %3/%4 frames captured and will be processed.",
                             QString("%L1").arg(job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(), 0, 'f', 3),
                             job->getCoreProperty(SequenceJob::SJ_Filter).toString(),
                             activeJob()->getCompleted(),
                             activeJob()->getCoreProperty(SequenceJob::SJ_Count).toInt()));

            // Emit progress update - done a few lines below
            // emit newImage(nullptr, activeJob());

            activeCamera()->setNextSequenceID(state()->nextSequenceID());
        }
    }

    if (activeCamera()->isBLOBEnabled() == false)
    {
        // FIXME: Move this warning pop-up elsewhere, it will interfere with automation.
        //        if (Options::guiderType() != Ekos::Guide::GUIDE_INTERNAL || KMessageBox::questionYesNo(nullptr, i18n("Image transfer is disabled for this camera. Would you like to enable it?")) ==
        //                KMessageBox::Yes)
        if (Options::guiderType() != Guide::GUIDE_INTERNAL)
        {
            activeCamera()->setBLOBEnabled(true);
        }
        else
        {
            connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this]()
            {
                KSMessageBox::Instance()->disconnect(this);
                activeCamera()->setBLOBEnabled(true);
                prepareActiveJobStage1();

            });
            connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this]()
            {
                KSMessageBox::Instance()->disconnect(this);
                activeCamera()->setBLOBEnabled(true);
                state()->setBusy(false);
            });

            KSMessageBox::Instance()->questionYesNo(i18n("Image transfer is disabled for this camera. Would you like to enable it?"),
                                                    i18n("Image Transfer"), 15);

            return;
        }
    }

    emit jobPrepared(job);

    prepareActiveJobStage1();

}

void CameraProcess::prepareActiveJobStage1()
{
    if (activeJob() == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "prepareActiveJobStage1 with null activeJob().";
    }
    else
    {
        // JM 2020-12-06: Check if we need to execute pre-job script first.
        // Only run pre-job script for the first time and not after some images were captured but then stopped due to abort.
        if (runCaptureScript(SCRIPT_PRE_JOB, activeJob()->getCompleted() == 0) == IPS_BUSY)
            return;
    }
    prepareActiveJobStage2();
}

void CameraProcess::prepareActiveJobStage2()
{
    // Just notification of active job stating up
    if (activeJob() == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "prepareActiveJobStage2 with null activeJob().";
    }
    else
        emit newImage(activeJob(), state()->imageData());


    /* Disable this restriction, let the sequence run even if focus did not run prior to the capture.
     * Besides, this locks up the Scheduler when the Capture module starts a sequence without any prior focus procedure done.
     * This is quite an old code block. The message "Manual scheduled" seems to even refer to some manual intervention?
     * With the new HFR threshold, it might be interesting to prevent the execution because we actually need an HFR value to
     * begin capturing, but even there, on one hand it makes sense for the end-user to know what HFR to put in the edit box,
     * and on the other hand the focus procedure will deduce the next HFR automatically.
     * But in the end, it's not entirely clear what the intent was. Note there is still a warning that a preliminary autofocus
     * procedure is important to avoid any surprise that could make the whole schedule ineffective.
     */
    // JM 2020-12-06: Check if we need to execute pre-capture script first.
    if (runCaptureScript(SCRIPT_PRE_CAPTURE) == IPS_BUSY)
        return;

    prepareJobExecution();
}

void CameraProcess::executeJob()
{
    if (activeJob() == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "executeJob with null activeJob().";
        return;
    }

    // Double check all pointers are valid.
    if (!activeCamera() || !devices()->getActiveChip())
    {
        checkCamera();
        QTimer::singleShot(1000, this, &CameraProcess::executeJob);
        return;
    }

    QList<FITSData::Record> FITSHeaders;
    if (Options::defaultObserver().isEmpty() == false)
        FITSHeaders.append(FITSData::Record("Observer", Options::defaultObserver(), "Observer"));
    if (activeJob()->getCoreProperty(SequenceJob::SJ_TargetName) != "")
        FITSHeaders.append(FITSData::Record("Object", activeJob()->getCoreProperty(SequenceJob::SJ_TargetName).toString(),
                                            "Object"));
    FITSHeaders.append(FITSData::Record("TELESCOP", m_Scope, "Telescope"));

    if (!FITSHeaders.isEmpty())
        activeCamera()->setFITSHeaders(FITSHeaders);

    // Update button status
    state()->setBusy(true);
    state()->setUseGuideHead((devices()->getActiveChip()->getType() == ISD::CameraChip::PRIMARY_CCD) ?
                             false : true);

    emit syncGUIToJob(activeJob());

    // If the job is a dark flat, let's find the optimal exposure from prior
    // flat exposures.
    if (activeJob()->jobType() == SequenceJob::JOBTYPE_DARKFLAT)
    {
        // If we found a prior exposure, and current upload more is not local, then update full prefix
        if (state()->setDarkFlatExposure(activeJob())
                && activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
        {
            auto placeholderPath = PlaceholderPath();
            // Make sure to update Full Prefix as exposure value was changed
            placeholderPath.processJobInfo(activeJob());
            state()->setNextSequenceID(1);
        }

    }

    updatePreCaptureCalibrationStatus();

}

void CameraProcess::prepareJobExecution()
{
    if (activeJob() == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "preparePreCaptureActions with null activeJob().";
        // Everything below depends on activeJob(). Just return.
        return;
    }

    state()->setBusy(true);

    // Update guiderActive before prepareCapture.
    activeJob()->setCoreProperty(SequenceJob::SJ_GuiderActive,
                                 state()->isActivelyGuiding());

    // signal that capture preparation steps should be executed
    activeJob()->prepareCapture();

    // update the UI
    emit jobExecutionPreparationStarted();
}

void CameraProcess::refreshOpticalTrain(QString name)
{
    state()->setOpticalTrain(name);

    auto mount = OpticalTrainManager::Instance()->getMount(name);
    setMount(mount);

    auto scope = OpticalTrainManager::Instance()->getScope(name);
    setScope(scope["name"].toString());

    auto camera = OpticalTrainManager::Instance()->getCamera(name);
    setCamera(camera);

    auto filterWheel = OpticalTrainManager::Instance()->getFilterWheel(name);
    setFilterWheel(filterWheel);

    auto rotator = OpticalTrainManager::Instance()->getRotator(name);
    setRotator(rotator);

    auto dustcap = OpticalTrainManager::Instance()->getDustCap(name);
    setDustCap(dustcap);

    auto lightbox = OpticalTrainManager::Instance()->getLightBox(name);
    setLightBox(lightbox);
}

IPState CameraProcess::checkLightFramePendingTasks()
{
    // step 1: did one of the pending jobs fail or has the user aborted the capture?
    if (state()->getCaptureState() == CAPTURE_ABORTED)
        return IPS_ALERT;

    // step 2: check if pausing has been requested
    if (checkPausing(CAPTURE_CONTINUE_ACTION_NEXT_EXPOSURE) == true)
        return IPS_BUSY;

    // step 3: check if a meridian flip is active
    if (state()->checkMeridianFlipActive())
        return IPS_BUSY;

    // step 4: check guide deviation for non meridian flip stages if the initial guide limit is set.
    //         Wait until the guide deviation is reported to be below the limit (@see setGuideDeviation(double, double)).
    if (state()->getCaptureState() == CAPTURE_PROGRESS &&
            state()->getGuideState() == GUIDE_GUIDING &&
            Options::enforceStartGuiderDrift())
        return IPS_BUSY;

    // step 5: check if dithering is required or running
    if ((state()->getCaptureState() == CAPTURE_DITHERING && state()->getDitheringState() != IPS_OK)
            || state()->checkDithering())
        return IPS_BUSY;

    // step 6: check if re-focusing is required
    //         Needs to be checked after dithering checks to avoid dithering in parallel
    //         to focusing, since @startFocusIfRequired() might change its value over time
    // Hint: CAPTURE_FOCUSING is not reliable, snce it might temporarily change to CAPTURE_CHANGING_FILTER
    //       Therefore, state()->getCaptureState() is not used here
    if (state()->checkFocusRunning() || state()->startFocusIfRequired())
        return IPS_BUSY;

    // step 7: resume guiding if it was suspended
    // JM 2023.12.20: Must make to resume if we have a light frame.
    if (state()->getGuideState() == GUIDE_SUSPENDED && activeJob()->getFrameType() == FRAME_LIGHT)
    {
        emit newLog(i18n("Autoguiding resumed."));
        emit resumeGuiding();
        // No need to return IPS_BUSY here, we can continue immediately.
        // In the case that the capturing sequence has a guiding limit,
        // capturing will be interrupted by setGuideDeviation().
    }

    // everything is ready for capturing light frames
    return IPS_OK;

}

void CameraProcess::captureStarted(CaptureResult rc)
{
    switch (rc)
    {
        case CAPTURE_OK:
        {
            state()->setCaptureState(CAPTURE_CAPTURING);
            state()->getCaptureTimeout().start(static_cast<int>(activeJob()->getCoreProperty(
                                                   SequenceJob::SJ_Exposure).toDouble()) * 1000 +
                                               CAPTURE_TIMEOUT_THRESHOLD);
            // calculate remaining capture time for the current job
            state()->imageCountDown().setHMS(0, 0, 0);
            double ms_left = std::ceil(activeJob()->getExposeLeft() * 1000.0);
            state()->imageCountDownAddMSecs(int(ms_left));
            state()->setLastRemainingFrameTimeMS(ms_left);
            state()->sequenceCountDown().setHMS(0, 0, 0);
            state()->sequenceCountDownAddMSecs(activeJob()->getJobRemainingTime(state()->averageDownloadTime()) * 1000);
            // ensure that the download time label is visible

            if (activeJob()->jobType() != SequenceJob::JOBTYPE_PREVIEW)
            {
                auto index = state()->allJobs().indexOf(activeJob());
                if (index >= 0 && index < state()->getSequence().count())
                    state()->changeSequenceValue(index, "Status", "In Progress");

                emit updateJobTable(activeJob());
            }
            emit captureRunning();
        }
        break;

        case CAPTURE_FRAME_ERROR:
            emit newLog(i18n("Failed to set sub frame."));
            emit stopCapturing(CAPTURE_ABORTED);
            break;

        case CAPTURE_BIN_ERROR:
            emit newLog((i18n("Failed to set binning.")));
            emit stopCapturing(CAPTURE_ABORTED);
            break;

        case CAPTURE_FOCUS_ERROR:
            emit newLog((i18n("Cannot capture while focus module is busy.")));
            emit stopCapturing(CAPTURE_ABORTED);
            break;
    }
}

void CameraProcess::checkNextExposure()
{
    IPState started = startNextExposure();
    // if starting the next exposure did not succeed due to pending jobs running,
    // we retry after 1 second
    if (started == IPS_BUSY)
        QTimer::singleShot(1000, this, &CameraProcess::checkNextExposure);
}

IPState CameraProcess::captureImageWithDelay()
{
    auto theJob = activeJob();

    if (theJob == nullptr)
        return IPS_IDLE;

    const int seqDelay = theJob->getCoreProperty(SequenceJob::SJ_Delay).toInt();
    // nothing pending, let's start the next exposure
    if (seqDelay > 0)
    {
        state()->setCaptureState(CAPTURE_WAITING);
    }
    state()->getCaptureDelayTimer().start(seqDelay);
    return IPS_OK;
}

IPState CameraProcess::startNextExposure()
{
    // Since this function is looping while pending tasks are running in parallel
    // it might happen that one of them leads to abort() which sets the #activeJob() to nullptr.
    // In this case we terminate the loop by returning #IPS_IDLE without starting a new capture.
    auto theJob = activeJob();

    if (theJob == nullptr)
        return IPS_IDLE;

    // check pending jobs for light frames. All other frame types do not contain mid-sequence checks.
    if (activeJob()->getFrameType() == FRAME_LIGHT)
    {
        IPState pending = checkLightFramePendingTasks();
        if (pending != IPS_OK)
            // there are still some jobs pending
            return pending;
    }

    return captureImageWithDelay();

    return IPS_OK;
}

IPState CameraProcess::resumeSequence()
{
    // before we resume, we will check if pausing is requested
    if (checkPausing(CAPTURE_CONTINUE_ACTION_CAPTURE_COMPLETE) == true)
        return IPS_BUSY;

    // If no job is active, we have to find if there are more pending jobs in the queue
    if (!activeJob())
    {
        return startNextJob();
    }
    // Otherwise, let's prepare for next exposure.

    // if we're done
    else if (activeJob()->getCoreProperty(SequenceJob::SJ_Count).toInt() <=
             activeJob()->getCompleted())
    {
        processJobCompletion1();
        return IPS_OK;
    }
    // continue the current job
    else
    {
        // If we suspended guiding due to primary chip download, resume guide chip guiding now - unless
        // a meridian flip is ongoing
        if (state()->getGuideState() == GUIDE_SUSPENDED && state()->suspendGuidingOnDownload() &&
                state()->getMeridianFlipState()->checkMeridianFlipActive() == false)
        {
            qCInfo(KSTARS_EKOS_CAPTURE) << "Resuming guiding...";
            emit resumeGuiding();
        }

        // If looping, we just increment the file system image count
        if (activeCamera()->isFastExposureEnabled())
        {
            if (activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
            {
                state()->checkSeqBoundary();
                activeCamera()->setNextSequenceID(state()->nextSequenceID());
            }
        }

        // ensure state image received to recover properly after pausing
        state()->setCaptureState(CAPTURE_IMAGE_RECEIVED);

        // JM 2020-12-06: Check if we need to execute pre-capture script first.
        if (runCaptureScript(SCRIPT_PRE_CAPTURE) == IPS_BUSY)
        {
            if (activeCamera()->isFastExposureEnabled())
            {
                state()->setRememberFastExposure(true);
                activeCamera()->setFastExposureEnabled(false);
            }
            return IPS_BUSY;
        }
        else
        {
            // Check if we need to stop fast exposure to perform any
            // pending tasks. If not continue as is.
            if (activeCamera()->isFastExposureEnabled())
            {
                if (activeJob() &&
                        activeJob()->getFrameType() == FRAME_LIGHT &&
                        checkLightFramePendingTasks() == IPS_OK)
                {
                    // Continue capturing seamlessly
                    state()->setCaptureState(CAPTURE_CAPTURING);
                    return IPS_OK;
                }

                // Stop fast exposure now.
                state()->setRememberFastExposure(true);
                activeCamera()->setFastExposureEnabled(false);
            }

            checkNextExposure();

        }
    }

    return IPS_OK;

}

bool Ekos::CameraProcess::checkSavingReceivedImage(const QSharedPointer<FITSData> &data, const QString &extension,
        QString &filename)
{
    // trigger saving the FITS file for batch jobs that aren't calibrating
    if (data && activeCamera() && activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
    {
        if (activeJob()->jobType() != SequenceJob::JOBTYPE_PREVIEW
                && activeJob()->getCalibrationStage() != SequenceJobState::CAL_CALIBRATION)
        {
            if (state()->generateFilename(extension, &filename) && activeCamera()->saveCurrentImage(filename))
            {
                data->setFilename(filename);
                KStars::Instance()->statusBar()->showMessage(i18n("file saved to %1", filename), 0);
                return true;
            }
            else
            {
                qCWarning(KSTARS_EKOS_CAPTURE) << "Saving current image failed!";
                connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [ = ]()
                {
                    KSMessageBox::Instance()->disconnect(this);
                });
                KSMessageBox::Instance()->error(i18n("Failed writing image to %1\nPlease check folder, filename & permissions.",
                                                     filename),
                                                i18n("Image Write Failed"), 30);
                return false;
            }
        }
    }
    return true;
}

void CameraProcess::processFITSData(const QSharedPointer<FITSData> &data, const QString &extension)
{
    ISD::CameraChip * tChip = nullptr;

    QString blobInfo;
    if (data)
    {
        state()->setImageData(data);
        blobInfo = QString("{Device: %1 Property: %2 Element: %3 Chip: %4}").arg(data->property("device").toString())
                   .arg(data->property("blobVector").toString())
                   .arg(data->property("blobElement").toString())
                   .arg(data->property("chip").toInt());
    }
    else
        state()->imageData().reset();

    const SequenceJob *job = activeJob();
    // If there is no active job, ignore
    if (job == nullptr)
    {
        if (data)
            qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring received FITS as active job is null.";

        emit processingFITSfinished(false);
        return;
    }

    if (state()->getMeridianFlipState()->getMeridianFlipStage() >= MeridianFlipState::MF_ALIGNING)
    {
        if (data)
            qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring Received FITS as meridian flip stage is" <<
                                           state()->getMeridianFlipState()->getMeridianFlipStage();
        emit processingFITSfinished(false);
        return;
    }

    const SequenceJob::SequenceJobType currentJobType = activeJob()->jobType();
    // If image is client or both, let's process it.
    if (activeCamera() && activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
    {

        if (state()->getCaptureState() == CAPTURE_IDLE || state()->getCaptureState() == CAPTURE_ABORTED)
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring Received FITS as current capture state is not active" <<
                                           state()->getCaptureState();

            emit processingFITSfinished(false);
            return;
        }

        if (data)
        {
            tChip = activeCamera()->getChip(static_cast<ISD::CameraChip::ChipType>(data->property("chip").toInt()));
            if (tChip != devices()->getActiveChip())
            {
                if (state()->getGuideState() == GUIDE_IDLE)
                    qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring Received FITS as it does not correspond to the target chip"
                                                   << devices()->getActiveChip()->getType();

                emit processingFITSfinished(false);
                return;
            }
        }

        if (devices()->getActiveChip()->getCaptureMode() == FITS_FOCUS ||
                devices()->getActiveChip()->getCaptureMode() == FITS_GUIDE)
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring Received FITS as it has the wrong capture mode" <<
                                           devices()->getActiveChip()->getCaptureMode();

            emit processingFITSfinished(false);
            return;
        }

        // If the FITS is not for our device, simply ignore
        if (data && data->property("device").toString() != activeCamera()->getDeviceName())
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring Received FITS as the blob device name does not equal active camera"
                                           << activeCamera()->getDeviceName();

            emit processingFITSfinished(false);
            return;
        }

        if (currentJobType == SequenceJob::JOBTYPE_PREVIEW)
        {
            QString filename;
            if (checkSavingReceivedImage(data, extension, filename))
            {
                FITSMode captureMode = tChip->getCaptureMode();
                FITSScale captureFilter = tChip->getCaptureFilter();
                updateFITSViewer(data, captureMode, captureFilter, filename, data->property("device").toString());
            }
        }

        // If dark is selected, perform dark substraction.
        if (data && Options::autoDark() && job->jobType() == SequenceJob::JOBTYPE_PREVIEW && state()->useGuideHead() == false)
        {
            QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::CaptureOpticalTrain);
            if (trainID.isValid())
            {
                m_DarkProcessor.data()->denoise(trainID.toUInt(),
                                                devices()->getActiveChip(),
                                                state()->imageData(),
                                                job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(),
                                                job->getCoreProperty(SequenceJob::SJ_ROI).toRect().x(),
                                                job->getCoreProperty(SequenceJob::SJ_ROI).toRect().y());
            }
            else
                qWarning(KSTARS_EKOS_CAPTURE) << "Invalid train ID for darks substraction:" << trainID.toUInt();

        }
        if (currentJobType == SequenceJob::JOBTYPE_PREVIEW)
        {
            // Set image metadata and emit captureComplete
            // Need to do this now for previews as the activeJob() will be set to null.
            updateImageMetadataAction(state()->imageData());
        }
    }

    // image has been received and processed successfully.
    state()->setCaptureState(CAPTURE_IMAGE_RECEIVED);
    // processing finished successfully
    SequenceJob *thejob = activeJob();

    if (thejob == nullptr)
        return;

    // If fast exposure is off, disconnect exposure progress
    // otherwise, keep it going since it fires off from driver continuous capture process.
    if (activeCamera()->isFastExposureEnabled() == false && state()->isLooping() == false)
    {
        disconnect(activeCamera(), &ISD::Camera::newExposureValue, this,
                   &CameraProcess::setExposureProgress);
        DarkLibrary::Instance()->disconnect(this);
    }

    QString filename;
    bool alreadySaved = false;
    switch (thejob->getFrameType())
    {
        case FRAME_BIAS:
        case FRAME_DARK:
            thejob->setCalibrationStage(SequenceJobState::CAL_CALIBRATION_COMPLETE);
            break;
        case FRAME_FLAT:
            /* calibration not completed, adapt exposure time */
            if (thejob->getFlatFieldDuration() == DURATION_ADU
                    && thejob->getCoreProperty(SequenceJob::SJ_TargetADU).toDouble() > 0)
            {
                if (checkFlatCalibration(state()->imageData(), state()->exposureRange().min, state()->exposureRange().max) == false)
                {
                    updateFITSViewer(data, tChip, filename);
                    return; /* calibration not completed */
                }
                thejob->setCalibrationStage(SequenceJobState::CAL_CALIBRATION_COMPLETE);
                // save current image since the image satisfies the calibration requirements
                if (checkSavingReceivedImage(data, extension, filename))
                    alreadySaved = true;
            }
            else
            {
                thejob->setCalibrationStage(SequenceJobState::CAL_CALIBRATION_COMPLETE);
            }
            break;
        case FRAME_LIGHT:
            // do nothing, continue
            break;
        case FRAME_NONE:
            // this should not happen!
            qWarning(KSTARS_EKOS_CAPTURE) << "Job completed with frametype NONE!";
            return;
    }
    // update counters
    // This will set activeJob to be a nullptr if it's a preview.
    updateCompletedCaptureCountersAction();

    if (thejob->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION_COMPLETE)
        thejob->setCalibrationStage(SequenceJobState::CAL_CAPTURING);

    if (activeJob() && currentJobType != SequenceJob::JOBTYPE_PREVIEW &&
            activeCamera() && activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
    {
        // Check to save and show the new image in the FITS viewer
        if (alreadySaved || checkSavingReceivedImage(data, extension, filename))
            updateFITSViewer(data, tChip, filename);

        // Set image metadata and emit captureComplete
        updateImageMetadataAction(state()->imageData());
    }

    // JM 2020-06-17: Emit newImage for LOCAL images (stored on remote host)
    //if (m_Camera->getUploadMode() == ISD::Camera::UPLOAD_LOCAL)
    emit newImage(thejob, state()->imageData());

    // Check if we need to execute post capture script first
    if (runCaptureScript(SCRIPT_POST_CAPTURE) == IPS_BUSY)
        return;

    // don't resume for preview jobs
    if (currentJobType != SequenceJob::JOBTYPE_PREVIEW)
        resumeSequence();

    // hand over to the capture module
    emit processingFITSfinished(true);
}

void CameraProcess::showFITSPreview(const QSharedPointer<FITSData> &data)
{
    ISD::CameraChip * tChip  = activeCamera()->getChip(static_cast<ISD::CameraChip::ChipType>(data->property("chip").toInt()));

    updateFITSViewer(data, tChip->getCaptureMode(), tChip->getCaptureFilter(), "", data->property("device").toString());
}

void CameraProcess::processNewRemoteFile(QString file)
{
    emit newLog(i18n("Remote image saved to %1", file));
    // call processing steps without image data if the image is stored only remotely
    QString nothing("");
    if (activeCamera() && activeCamera()->getUploadMode() == ISD::Camera::UPLOAD_LOCAL)
    {
        QString ext("");
        processFITSData(nullptr, ext);
    }
}

IPState CameraProcess::processPreCaptureCalibrationStage()
{
    // in some rare cases it might happen that activeJob() has been cleared by a concurrent thread
    if (activeJob() == nullptr)
    {
        qCWarning(KSTARS_EKOS_CAPTURE) << "Processing pre capture calibration without active job, state = " <<
                                       getCaptureStatusString(state()->getCaptureState());
        return IPS_ALERT;
    }

    // If we are currently guide and the frame is NOT a light frame, then we shopld suspend.
    // N.B. The guide camera could be on its own scope unaffected but it doesn't hurt to stop
    // guiding since it is no longer used anyway.
    if (activeJob()->getFrameType() != FRAME_LIGHT
            && state()->getGuideState() == GUIDE_GUIDING)
    {
        emit newLog(i18n("Autoguiding suspended."));
        emit suspendGuiding();
    }

    // Run necessary tasks for each frame type
    switch (activeJob()->getFrameType())
    {
        case FRAME_LIGHT:
            return checkLightFramePendingTasks();

        // FIXME Remote flats are not working since the files are saved remotely and no
        // preview is done locally first to calibrate the image.
        case FRAME_FLAT:
        case FRAME_BIAS:
        case FRAME_DARK:
        case FRAME_NONE:
            // no actions necessary
            break;
    }

    return IPS_OK;

}

void CameraProcess::updatePreCaptureCalibrationStatus()
{
    // If process was aborted or stopped by the user
    if (state()->isBusy() == false)
    {
        emit newLog(i18n("Warning: Calibration process was prematurely terminated."));
        return;
    }

    IPState rc = processPreCaptureCalibrationStage();

    if (rc == IPS_ALERT)
        return;
    else if (rc == IPS_BUSY)
    {
        QTimer::singleShot(1000, this, &CameraProcess::updatePreCaptureCalibrationStatus);
        return;
    }

    captureImageWithDelay();
}

void CameraProcess::processJobCompletion1()
{
    if (activeJob() == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "procesJobCompletionStage1 with null activeJob().";
    }
    else
    {
        // JM 2020-12-06: Check if we need to execute post-job script first.
        if (runCaptureScript(SCRIPT_POST_JOB) == IPS_BUSY)
            return;
    }

    processJobCompletion2();
}

void CameraProcess::processJobCompletion2()
{
    if (activeJob() == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "procesJobCompletionStage2 with null activeJob().";
    }
    else
    {
        activeJob()->done();

        if (activeJob()->jobType() != SequenceJob::JOBTYPE_PREVIEW)
        {
            int index = state()->allJobs().indexOf(activeJob());
            QJsonArray seqArray = state()->getSequence();
            QJsonObject oneSequence = seqArray[index].toObject();
            oneSequence["Status"] = "Complete";
            seqArray.replace(index, oneSequence);
            state()->setSequence(seqArray);
            emit sequenceChanged(seqArray);
            emit updateJobTable(activeJob());
        }
    }
    // stopping clears the planned state, therefore skip if pause planned
    if (state()->getCaptureState() != CAPTURE_PAUSE_PLANNED)
        emit stopCapture();

    // Check if there are more pending jobs and execute them
    if (resumeSequence() == IPS_OK)
        return;
    // Otherwise, we're done. We park if required and resume guiding if no parking is done and autoguiding was engaged before.
    else
    {
        //KNotification::event(QLatin1String("CaptureSuccessful"), i18n("CCD capture sequence completed"));
        KSNotification::event(QLatin1String("CaptureSuccessful"), i18n("CCD capture sequence completed"),
                              KSNotification::Capture);

        emit stopCapture(CAPTURE_COMPLETE);

        //Resume guiding if it was suspended before
        //if (isAutoGuiding && currentCCD->getChip(ISD::CameraChip::GUIDE_CCD) == guideChip)
        if (state()->getGuideState() == GUIDE_SUSPENDED && state()->suspendGuidingOnDownload())
            emit resumeGuiding();
    }

}

IPState CameraProcess::startNextJob()
{
    SequenceJob * next_job = nullptr;

    for (auto &oneJob : state()->allJobs())
    {
        if (oneJob->getStatus() == JOB_IDLE || oneJob->getStatus() == JOB_ABORTED)
        {
            next_job = oneJob;
            break;
        }
    }

    if (next_job)
    {

        prepareJob(next_job);

        //Resume guiding if it was suspended before, except for an active meridian flip is running.
        //if (isAutoGuiding && currentCCD->getChip(ISD::CameraChip::GUIDE_CCD) == guideChip)
        if (state()->getGuideState() == GUIDE_SUSPENDED && state()->suspendGuidingOnDownload() &&
                state()->getMeridianFlipState()->checkMeridianFlipActive() == false)
        {
            qCDebug(KSTARS_EKOS_CAPTURE) << "Resuming guiding...";
            emit resumeGuiding();
        }

        return IPS_OK;
    }
    else
    {
        qCDebug(KSTARS_EKOS_CAPTURE) << "All capture jobs complete.";
        return IPS_BUSY;
    }
}

void CameraProcess::captureImage()
{
    if (activeJob() == nullptr)
        return;

    // Bail out if we have no CCD anymore
    if (!activeCamera() || !activeCamera()->isConnected())
    {
        emit newLog(i18n("Error: Lost connection to CCD."));
        emit stopCapture(CAPTURE_ABORTED);
        return;
    }

    state()->getCaptureTimeout().stop();
    state()->getCaptureDelayTimer().stop();
    if (activeCamera()->isFastExposureEnabled())
    {
        int remaining = state()->isLooping() ? 100000 : (activeJob()->getCoreProperty(
                            SequenceJob::SJ_Count).toInt() -
                        activeJob()->getCompleted());
        if (remaining > 1)
            activeCamera()->setFastCount(static_cast<uint>(remaining));
    }

    setCamera(true);

    if (activeJob()->getFrameType() == FRAME_FLAT)
    {
        // If we have to calibrate ADU levels, first capture must be preview and not in batch mode
        if (activeJob()->jobType() != SequenceJob::JOBTYPE_PREVIEW
                && activeJob()->getFlatFieldDuration() == DURATION_ADU &&
                activeJob()->getCalibrationStage() == SequenceJobState::CAL_NONE)
        {
            if (activeCamera()->getEncodingFormat() != "FITS" &&
                    activeCamera()->getEncodingFormat() != "XISF")
            {
                emit newLog(i18n("Cannot calculate ADU levels in non-FITS images."));
                emit stopCapture(CAPTURE_ABORTED);
                return;
            }

            activeJob()->setCalibrationStage(SequenceJobState::CAL_CALIBRATION);
        }
    }

    // If preview, always set to UPLOAD_CLIENT if not already set.
    if (activeJob()->jobType() == SequenceJob::JOBTYPE_PREVIEW)
    {
        if (activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_CLIENT)
            activeCamera()->setUploadMode(ISD::Camera::UPLOAD_CLIENT);
    }
    // If batch mode, ensure upload mode mathces the active job target.
    else
    {
        if (activeCamera()->getUploadMode() != activeJob()->getUploadMode())
            activeCamera()->setUploadMode(activeJob()->getUploadMode());
    }

    if (activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
    {
        state()->checkSeqBoundary();
        activeCamera()->setNextSequenceID(state()->nextSequenceID());
    }

    // Re-enable fast exposure if it was disabled before due to pending tasks
    if (state()->isRememberFastExposure())
    {
        state()->setRememberFastExposure(false);
        activeCamera()->setFastExposureEnabled(true);
    }

    if (state()->frameSettings().contains(devices()->getActiveChip()))
    {
        const auto roi = activeJob()->getCoreProperty(SequenceJob::SJ_ROI).toRect();
        QVariantMap settings;
        settings["x"]    = roi.x();
        settings["y"]    = roi.y();
        settings["w"]    = roi.width();
        settings["h"]    = roi.height();
        settings["binx"] = activeJob()->getCoreProperty(SequenceJob::SJ_Binning).toPoint().x();
        settings["biny"] = activeJob()->getCoreProperty(SequenceJob::SJ_Binning).toPoint().y();

        state()->frameSettings()[devices()->getActiveChip()] = settings;
    }

    // If using DSLR, make sure it is set to correct transfer format
    activeCamera()->setEncodingFormat(activeJob()->getCoreProperty(
                                          SequenceJob::SJ_Encoding).toString());

    state()->setStartingCapture(true);
    state()->placeholderPath().setGenerateFilenameSettings(*activeJob());

    // update remote filename and directory filling all placeholders
    if (activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_CLIENT)
    {
        auto remoteUpload = state()->placeholderPath().generateSequenceFilename(*activeJob(), false, true, 1, "", "",  false,
                            false);

        auto lastSeparator = remoteUpload.lastIndexOf(QDir::separator());
        auto remoteDirectory = remoteUpload.mid(0, lastSeparator);
        auto remoteFilename = remoteUpload.mid(lastSeparator + 1);
        activeJob()->setCoreProperty(SequenceJob::SJ_RemoteFormatDirectory, remoteDirectory);
        activeJob()->setCoreProperty(SequenceJob::SJ_RemoteFormatFilename, remoteFilename);
    }

    // now hand over the control of capturing to the sequence job. As soon as capturing
    // has started, the sequence job will report the result with the captureStarted() event
    // that will trigger Capture::captureStarted()
    activeJob()->startCapturing(state()->getRefocusState()->isAutoFocusReady(),
                                activeJob()->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION ? FITS_CALIBRATE :
                                FITS_NORMAL);

    // Re-enable fast exposure if it was disabled before due to pending tasks
    if (state()->isRememberFastExposure())
    {
        state()->setRememberFastExposure(false);
        activeCamera()->setFastExposureEnabled(true);
    }

    emit captureTarget(activeJob()->getCoreProperty(SequenceJob::SJ_TargetName).toString());
    emit captureImageStarted();
}

void CameraProcess::resetFrame()
{
    devices()->setActiveChip(state()->useGuideHead() ?
                             devices()->getActiveCamera()->getChip(
                                 ISD::CameraChip::GUIDE_CCD) :
                             devices()->getActiveCamera()->getChip(ISD::CameraChip::PRIMARY_CCD));
    devices()->getActiveChip()->resetFrame();
    emit updateFrameProperties(1);
}

void CameraProcess::setExposureProgress(ISD::CameraChip *tChip, double value, IPState ipstate)
{
    // ignore values if not capturing
    if (state()->checkCapturing() == false)
        return;

    if (devices()->getActiveChip() != tChip ||
            devices()->getActiveChip()->getCaptureMode() != FITS_NORMAL
            || state()->getMeridianFlipState()->getMeridianFlipStage() >= MeridianFlipState::MF_ALIGNING)
        return;

    double deltaMS = std::ceil(1000.0 * value - state()->lastRemainingFrameTimeMS());
    emit updateCaptureCountDown(int(deltaMS));
    state()->setLastRemainingFrameTimeMS(state()->lastRemainingFrameTimeMS() + deltaMS);

    if (activeJob())
    {
        activeJob()->setExposeLeft(value);

        emit newExposureProgress(activeJob());
    }

    if (activeJob() && ipstate == IPS_ALERT)
    {
        int retries = activeJob()->getCaptureRetires() + 1;

        activeJob()->setCaptureRetires(retries);

        emit newLog(i18n("Capture failed. Check INDI Control Panel for details."));

        if (retries >= 3)
        {
            activeJob()->abort();
            return;
        }

        emit newLog((i18n("Restarting capture attempt #%1", retries)));

        state()->setNextSequenceID(1);

        captureImage();
        return;
    }

    if (activeJob() != nullptr && ipstate == IPS_OK)
    {
        activeJob()->setCaptureRetires(0);
        activeJob()->setExposeLeft(0);

        if (devices()->getActiveCamera()
                && devices()->getActiveCamera()->getUploadMode() == ISD::Camera::UPLOAD_LOCAL)
        {
            if (activeJob()->getStatus() == JOB_BUSY)
            {
                emit processingFITSfinished(false);
                return;
            }
        }

        if (state()->getGuideState() == GUIDE_GUIDING && Options::guiderType() == 0
                && state()->suspendGuidingOnDownload())
        {
            qCDebug(KSTARS_EKOS_CAPTURE) << "Autoguiding suspended until primary CCD chip completes downloading...";
            emit suspendGuiding();
        }

        emit downloadingFrame();

        //This will start the clock to see how long the download takes.
        state()->downloadTimer().start();
        state()->downloadProgressTimer().start();
    }
}

void CameraProcess::setDownloadProgress()
{
    if (activeJob())
    {
        double downloadTimeLeft = state()->averageDownloadTime() - state()->downloadTimer().elapsed() /
                                  1000.0;
        if(downloadTimeLeft >= 0)
        {
            state()->imageCountDown().setHMS(0, 0, 0);
            state()->imageCountDownAddMSecs(int(std::ceil(downloadTimeLeft * 1000)));
            emit newDownloadProgress(downloadTimeLeft);
        }
    }

}

IPState CameraProcess::continueFramingAction(const QSharedPointer<FITSData> &imageData)
{
    emit newImage(activeJob(), imageData);
    // If fast exposure is on, do not capture again, it will be captured by the driver.
    if (activeCamera()->isFastExposureEnabled() == false)
    {
        const int seqDelay = activeJob()->getCoreProperty(SequenceJob::SJ_Delay).toInt();

        if (seqDelay > 0)
        {
            QTimer::singleShot(seqDelay, this, [this]()
            {
                if (activeJob() != nullptr)
                    activeJob()->startCapturing(state()->getRefocusState()->isAutoFocusReady(), FITS_NORMAL);
            });
        }
        else if (activeJob() != nullptr)
            activeJob()->startCapturing(state()->getRefocusState()->isAutoFocusReady(), FITS_NORMAL);
    }
    return IPS_OK;

}

IPState CameraProcess::updateDownloadTimesAction()
{
    // Do not calculate download time for images stored on server.
    // Only calculate for longer exposures.
    if (activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL
            && state()->downloadTimer().isValid())
    {
        //This determines the time since the image started downloading
        double currentDownloadTime = state()->downloadTimer().elapsed() / 1000.0;
        state()->addDownloadTime(currentDownloadTime);
        // Always invalidate timer as it must be explicitly started.
        state()->downloadTimer().invalidate();

        QString dLTimeString = QString::number(currentDownloadTime, 'd', 2);
        QString estimatedTimeString = QString::number(state()->averageDownloadTime(), 'd', 2);
        emit newLog(i18n("Download Time: %1 s, New Download Time Estimate: %2 s.", dLTimeString, estimatedTimeString));
    }
    return IPS_OK;
}

IPState CameraProcess::previewImageCompletedAction()
{
    if (activeJob()->jobType() == SequenceJob::JOBTYPE_PREVIEW)
    {
        // Reset upload mode if it was changed by preview
        activeCamera()->setUploadMode(activeJob()->getUploadMode());
        // Reset active job pointer
        state()->setActiveJob(nullptr);
        emit stopCapture(CAPTURE_COMPLETE);
        if (state()->getGuideState() == GUIDE_SUSPENDED && state()->suspendGuidingOnDownload())
            emit resumeGuiding();
        return IPS_OK;
    }
    else
        return IPS_IDLE;

}

void CameraProcess::updateCompletedCaptureCountersAction()
{
    // stop timers
    state()->getCaptureTimeout().stop();
    state()->setCaptureTimeoutCounter(0);

    state()->downloadProgressTimer().stop();

    // In case we're framing, let's return quickly to continue the process.
    if (state()->isLooping())
    {
        continueFramingAction(state()->imageData());
        return;
    }

    // Update download times.
    updateDownloadTimesAction();

    // If it was initially set as pure preview job and NOT as preview for calibration
    if (previewImageCompletedAction() == IPS_OK)
        return;

    // do not update counters if in preview mode or calibrating
    if (activeJob()->jobType() == SequenceJob::JOBTYPE_PREVIEW
            || activeJob()->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION)
        return;

    /* Increase the sequence's current capture count */
    updatedCaptureCompleted(activeJob()->getCompleted() + 1);
    /* Decrease the counter for in-sequence focusing */
    state()->getRefocusState()->decreaseInSequenceFocusCounter();
    /* Reset adaptive focus flag */
    state()->getRefocusState()->setAdaptiveFocusDone(false);

    /* Decrease the dithering counter except for directly after meridian flip                                              */
    /* Hint: this isonly relevant when a meridian flip happened during a paused sequence when pressing "Start" afterwards. */
    if (state()->getMeridianFlipState()->getMeridianFlipStage() < MeridianFlipState::MF_FLIPPING)
        state()->decreaseDitherCounter();

    /* If we were assigned a captured frame map, also increase the relevant counter for prepareJob */
    state()->addCapturedFrame(activeJob()->getSignature());

    // report that the image has been received
    emit newLog(i18n("Received image %1 out of %2.", activeJob()->getCompleted(),
                     activeJob()->getCoreProperty(SequenceJob::SJ_Count).toInt()));
}

IPState CameraProcess::updateImageMetadataAction(QSharedPointer<FITSData> imageData)
{
    double hfr = -1, eccentricity = -1;
    int numStars = -1, median = -1;
    QString filename;
    if (imageData)
    {
        QVariant frameType;
        if (Options::autoHFR() && imageData && !imageData->areStarsSearched() && imageData->getRecordValue("FRAME", frameType)
                && frameType.toString() == "Light")
        {
#ifdef HAVE_STELLARSOLVER
            // Don't use the StellarSolver defaults (which allow very small stars).
            // Use the HFR profile--which the user can modify.
            QVariantMap extractionSettings;
            extractionSettings["optionsProfileIndex"] = Options::hFROptionsProfile();
            extractionSettings["optionsProfileGroup"] = static_cast<int>(Ekos::HFRProfiles);
            imageData->setSourceExtractorSettings(extractionSettings);
#endif
            QFuture<bool> result = imageData->findStars(ALGORITHM_SEP);
            result.waitForFinished();
        }
        hfr = imageData->getHFR(HFR_AVERAGE);
        numStars = imageData->getSkyBackground().starsDetected;
        median = imageData->getMedian();
        eccentricity = imageData->getEccentricity();
        filename = imageData->filename();

        // avoid logging that we captured a temporary file
        if (state()->isLooping() == false && activeJob() != nullptr && activeJob()->jobType() != SequenceJob::JOBTYPE_PREVIEW)
            emit newLog(i18n("Captured %1", filename));

        auto remainingPlaceholders = PlaceholderPath::remainingPlaceholders(filename);
        if (remainingPlaceholders.size() > 0)
        {
            emit newLog(
                i18n("WARNING: remaining and potentially unknown placeholders %1 in %2",
                     remainingPlaceholders.join(", "), filename));
        }
    }

    if (activeJob())
    {
        QVariantMap metadata;
        metadata["filename"] = filename;
        metadata["type"] = activeJob()->getFrameType();
        metadata["exposure"] = activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();
        metadata["filter"] = activeJob()->getCoreProperty(SequenceJob::SJ_Filter).toString();
        metadata["width"] = activeJob()->getCoreProperty(SequenceJob::SJ_ROI).toRect().width();
        metadata["height"] = activeJob()->getCoreProperty(SequenceJob::SJ_ROI).toRect().height();
        metadata["binx"] = activeJob()->getCoreProperty(SequenceJob::SJ_Binning).toPoint().x();
        metadata["biny"] = activeJob()->getCoreProperty(SequenceJob::SJ_Binning).toPoint().y();
        metadata["hfr"] = hfr;
        metadata["starCount"] = numStars;
        metadata["median"] = median;
        metadata["eccentricity"] = eccentricity;
        qCDebug(KSTARS_EKOS_CAPTURE) << "Captured frame metadata: filename =" << filename << ", type =" << metadata["type"].toInt()
                                     << "exposure =" <<  metadata["exposure"].toDouble() << "filter =" << metadata["filter"].toString() << "width =" <<
                                     metadata["width"].toInt() << "height =" << metadata["height"].toInt() << "hfr =" << metadata["hfr"].toDouble() <<
                                     "starCount =" << metadata["starCount"].toInt() << "median =" << metadata["median"].toInt() << "eccentricity =" <<
                                     metadata["eccentricity"].toDouble();

        emit captureComplete(metadata);
    }
    return IPS_OK;
}

IPState CameraProcess::runCaptureScript(ScriptTypes scriptType, bool precond)
{
    if (activeJob())
    {
        const QString captureScript = activeJob()->getScript(scriptType);
        if (captureScript.isEmpty() == false && precond)
        {
            state()->setCaptureScriptType(scriptType);
            m_CaptureScript.start(captureScript, generateScriptArguments());
            //m_CaptureScript.start("/bin/bash", QStringList() << captureScript);
            emit newLog(i18n("Executing capture script %1", captureScript));
            return IPS_BUSY;
        }
    }
    // no script execution started
    return IPS_OK;
}

void CameraProcess::scriptFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status)

    switch (state()->captureScriptType())
    {
        case SCRIPT_PRE_CAPTURE:
            emit newLog(i18n("Pre capture script finished with code %1.", exitCode));
            if (activeJob() && activeJob()->getStatus() == JOB_IDLE)
                prepareJobExecution();
            else
                checkNextExposure();
            break;

        case SCRIPT_POST_CAPTURE:
            emit newLog(i18n("Post capture script finished with code %1.", exitCode));

            // If we're done, proceed to completion.
            if (activeJob() == nullptr
                    || activeJob()->getCoreProperty(SequenceJob::SJ_Count).toInt() <=
                    activeJob()->getCompleted())
            {
                resumeSequence();
            }
            // Else check if meridian condition is met.
            else if (state()->checkMeridianFlipReady())
            {
                emit newLog(i18n("Processing meridian flip..."));
            }
            // Then if nothing else, just resume sequence.
            else
            {
                resumeSequence();
            }
            break;

        case SCRIPT_PRE_JOB:
            emit newLog(i18n("Pre job script finished with code %1.", exitCode));
            prepareActiveJobStage2();
            break;

        case SCRIPT_POST_JOB:
            emit newLog(i18n("Post job script finished with code %1.", exitCode));
            processJobCompletion2();
            break;

        default:
            // in all other cases do nothing
            break;
    }

}

void CameraProcess::selectCamera(QString name)
{

    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::CaptureOpticalTrain);
    if (activeCamera() && trainID.isValid())
    {

        if (devices()->filterWheel())
            updateFilterInfo();
        if (activeCamera() && activeCamera()->getDeviceName() == name)
            checkCamera();

        emit refreshCamera(true);
    }
    else
        emit refreshCamera(false);

}

void CameraProcess::checkCamera()
{
    // Do not update any camera settings while capture is in progress.
    if (state()->getCaptureState() == CAPTURE_CAPTURING)
        return;

    // If camera is restarted, try again in 1 second
    if (!activeCamera())
    {
        QTimer::singleShot(1000, this, &CameraProcess::checkCamera);
        return;
    }

    devices()->setActiveChip(nullptr);

    // FIXME TODO fix guide head detection
    if (activeCamera()->getDeviceName().contains("Guider"))
    {
        state()->setUseGuideHead(true);
        devices()->setActiveChip(activeCamera()->getChip(ISD::CameraChip::GUIDE_CCD));
    }

    if (devices()->getActiveChip() == nullptr)
    {
        state()->setUseGuideHead(false);
        devices()->setActiveChip(activeCamera()->getChip(ISD::CameraChip::PRIMARY_CCD));
    }

    emit refreshCameraSettings();
}

void CameraProcess::syncDSLRToTargetChip(const QString &model)
{
    auto pos = std::find_if(state()->DSLRInfos().begin(),
                            state()->DSLRInfos().end(), [model](const QMap<QString, QVariant> &oneDSLRInfo)
    {
        return (oneDSLRInfo["Model"] == model);
    });

    // Sync Pixel Size
    if (pos != state()->DSLRInfos().end())
    {
        auto camera = *pos;
        devices()->getActiveChip()->setImageInfo(camera["Width"].toInt(),
                camera["Height"].toInt(),
                camera["PixelW"].toDouble(),
                camera["PixelH"].toDouble(),
                8);
    }
}

void CameraProcess::reconnectCameraDriver(const QString &camera, const QString &filterWheel)
{
    if (activeCamera() && activeCamera()->getDeviceName() == camera)
    {
        // Set camera again to the one we restarted
        auto rememberState = state()->getCaptureState();
        state()->setCaptureState(CAPTURE_IDLE);
        checkCamera();
        state()->setCaptureState(rememberState);

        // restart capture
        state()->setCaptureTimeoutCounter(0);

        if (activeJob())
        {
            devices()->setActiveChip(devices()->getActiveChip());
            captureImage();
        }
        return;
    }

    QTimer::singleShot(5000, this, [ &, camera, filterWheel]()
    {
        reconnectCameraDriver(camera, filterWheel);
    });
}

void CameraProcess::removeDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    auto name = device->getDeviceName();
    device->disconnect(this);

    // Mounts
    if (devices()->mount() && devices()->mount()->getDeviceName() == device->getDeviceName())
    {
        devices()->mount()->disconnect(this);
        devices()->setMount(nullptr);
        if (activeJob() != nullptr)
            activeJob()->addMount(nullptr);
    }

    // Domes
    if (devices()->dome() && devices()->dome()->getDeviceName() == device->getDeviceName())
    {
        devices()->dome()->disconnect(this);
        devices()->setDome(nullptr);
    }

    // Rotators
    if (devices()->rotator() && devices()->rotator()->getDeviceName() == device->getDeviceName())
    {
        devices()->rotator()->disconnect(this);
        devices()->setRotator(nullptr);
    }

    // Dust Caps
    if (devices()->dustCap() && devices()->dustCap()->getDeviceName() == device->getDeviceName())
    {
        devices()->dustCap()->disconnect(this);
        devices()->setDustCap(nullptr);
        state()->hasDustCap = false;
        state()->setDustCapState(CAP_UNKNOWN);
    }

    // Light Boxes
    if (devices()->lightBox() && devices()->lightBox()->getDeviceName() == device->getDeviceName())
    {
        devices()->lightBox()->disconnect(this);
        devices()->setLightBox(nullptr);
        state()->hasLightBox = false;
        state()->setLightBoxLightState(CAP_LIGHT_UNKNOWN);
    }

    // Cameras
    if (activeCamera() && activeCamera()->getDeviceName() == name)
    {
        activeCamera()->disconnect(this);
        devices()->setActiveCamera(nullptr);
        devices()->setActiveChip(nullptr);

        QSharedPointer<ISD::GenericDevice> generic;
        if (INDIListener::findDevice(name, generic))
            DarkLibrary::Instance()->removeDevice(generic);

        QTimer::singleShot(1000, this, [this]()
        {
            checkCamera();
        });
    }

    // Filter Wheels
    if (devices()->filterWheel() && devices()->filterWheel()->getDeviceName() == name)
    {
        devices()->filterWheel()->disconnect(this);
        devices()->setFilterWheel(nullptr);

        QTimer::singleShot(1000, this, [this]()
        {
            emit refreshFilterSettings();
        });
    }
}

void CameraProcess::processCaptureTimeout()
{
    state()->setCaptureTimeoutCounter(state()->captureTimeoutCounter() + 1);

    if (state()->deviceRestartCounter() >= 3)
    {
        state()->setCaptureTimeoutCounter(0);
        state()->setDeviceRestartCounter(0);
        emit newLog(i18n("Exposure timeout. Aborting..."));
        emit stopCapture(CAPTURE_ABORTED);
        return;
    }

    if (state()->captureTimeoutCounter() > 3 && activeCamera())
    {
        emit newLog(i18n("Exposure timeout. More than 3 have been detected, will restart driver."));
        QString camera = activeCamera()->getDeviceName();
        QString fw = (devices()->filterWheel() != nullptr) ?
                     devices()->filterWheel()->getDeviceName() : "";
        emit driverTimedout(camera);
        QTimer::singleShot(5000, this, [ &, camera, fw]()
        {
            state()->setDeviceRestartCounter(state()->deviceRestartCounter() + 1);
            reconnectCameraDriver(camera, fw);
        });
        return;
    }
    else
    {
        // Double check that m_Camera is valid in case it was reset due to driver restart.
        if (activeCamera() && activeJob())
        {
            setCamera(true);
            emit newLog(i18n("Exposure timeout. Restarting exposure..."));
            activeCamera()->setEncodingFormat("FITS");
            auto rememberState = state()->getCaptureState();
            state()->setCaptureState(CAPTURE_IDLE);
            checkCamera();
            state()->setCaptureState(rememberState);

            auto targetChip = activeCamera()->getChip(state()->useGuideHead() ?
                              ISD::CameraChip::GUIDE_CCD :
                              ISD::CameraChip::PRIMARY_CCD);
            targetChip->abortExposure();
            const double exptime = activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();
            targetChip->capture(exptime);
            state()->getCaptureTimeout().start(static_cast<int>((exptime) * 1000 + CAPTURE_TIMEOUT_THRESHOLD));
        }
        // Don't allow this to happen all night. We will repeat checking (most likely for activeCamera()
        // another 200s = 40 * 5s, but after that abort capture.
        else if (state()->captureTimeoutCounter() < 40)
        {
            qCDebug(KSTARS_EKOS_CAPTURE) << "Unable to restart exposure as camera is missing, trying again in 5 seconds...";
            QTimer::singleShot(5000, this, &CameraProcess::processCaptureTimeout);
        }
        else
        {
            state()->setCaptureTimeoutCounter(0);
            state()->setDeviceRestartCounter(0);
            emit newLog(i18n("Exposure timeout. Too many. Aborting..."));
            emit stopCapture(CAPTURE_ABORTED);
            return;
        }
    }

}

void CameraProcess::processCaptureError(ISD::Camera::ErrorType type)
{
    if (!activeJob())
        return;

    if (type == ISD::Camera::ERROR_CAPTURE)
    {
        int retries = activeJob()->getCaptureRetires() + 1;

        activeJob()->setCaptureRetires(retries);

        emit newLog(i18n("Capture failed. Check INDI Control Panel for details."));

        if (retries >= 3)
        {
            emit stopCapture(CAPTURE_ABORTED);
            return;
        }

        emit newLog(i18n("Restarting capture attempt #%1", retries));

        state()->setNextSequenceID(1);

        captureImage();
        return;
    }
    else
    {
        emit stopCapture(CAPTURE_ABORTED);
    }
}

bool CameraProcess::checkFlatCalibration(QSharedPointer<FITSData> imageData, double exp_min, double exp_max)
{
    // nothing to do
    if (imageData.isNull())
        return true;

    double currentADU = imageData->getADU();
    bool outOfRange = false, saturated = false;

    switch (imageData->bpp())
    {
        case 8:
            if (activeJob()->getCoreProperty(SequenceJob::SJ_TargetADU).toDouble() > UINT8_MAX)
                outOfRange = true;
            else if (currentADU / UINT8_MAX > 0.95)
                saturated = true;
            break;

        case 16:
            if (activeJob()->getCoreProperty(SequenceJob::SJ_TargetADU).toDouble() > UINT16_MAX)
                outOfRange = true;
            else if (currentADU / UINT16_MAX > 0.95)
                saturated = true;
            break;

        case 32:
            if (activeJob()->getCoreProperty(SequenceJob::SJ_TargetADU).toDouble() > UINT32_MAX)
                outOfRange = true;
            else if (currentADU / UINT32_MAX > 0.95)
                saturated = true;
            break;

        default:
            break;
    }

    if (outOfRange)
    {
        emit newLog(i18n("Flat calibration failed. Captured image is only %1-bit while requested ADU is %2.",
                         QString::number(imageData->bpp())
                         , QString::number(activeJob()->getCoreProperty(SequenceJob::SJ_TargetADU).toDouble(), 'f', 2)));
        emit stopCapture(CAPTURE_ABORTED);
        return false;
    }
    else if (saturated)
    {
        double nextExposure = activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble() * 0.1;
        nextExposure = qBound(exp_min, nextExposure, exp_max);

        emit newLog(i18n("Current image is saturated (%1). Next exposure is %2 seconds.",
                         QString::number(currentADU, 'f', 0), QString("%L1").arg(nextExposure, 0, 'f', 6)));

        activeJob()->setCalibrationStage(SequenceJobState::CAL_CALIBRATION);
        activeJob()->setCoreProperty(SequenceJob::SJ_Exposure, nextExposure);
        if (activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_CLIENT)
        {
            activeCamera()->setUploadMode(ISD::Camera::UPLOAD_CLIENT);
        }
        startNextExposure();
        return false;
    }

    double ADUDiff = fabs(currentADU - activeJob()->getCoreProperty(
                              SequenceJob::SJ_TargetADU).toDouble());

    // If it is within tolerance range of target ADU
    if (ADUDiff <= state()->targetADUTolerance())
    {
        if (activeJob()->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION)
        {
            emit newLog(
                i18n("Current ADU %1 within target ADU tolerance range.", QString::number(currentADU, 'f', 0)));
            activeCamera()->setUploadMode(activeJob()->getUploadMode());
            auto placeholderPath = PlaceholderPath();
            // Make sure to update Full Prefix as exposure value was changed
            placeholderPath.processJobInfo(activeJob());
            // Mark calibration as complete
            activeJob()->setCalibrationStage(SequenceJobState::CAL_CALIBRATION_COMPLETE);

            // Must update sequence prefix as this step is only done in prepareJob
            // but since the duration has now been updated, we must take care to update signature
            // since it may include a placeholder for duration which would affect it.
            if (activeCamera() && activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
                state()->checkSeqBoundary();
        }

        return true;
    }

    double nextExposure = -1;

    // If value is saturated, try to reduce it to valid range first
    if (std::fabs(imageData->getMax(0) - imageData->getMin(0)) < 10)
        nextExposure = activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble() * 0.5;
    else
        nextExposure = calculateFlatExpTime(currentADU);

    if (nextExposure <= 0 || std::isnan(nextExposure))
    {
        emit newLog(
            i18n("Unable to calculate optimal exposure settings, please capture the flats manually."));
        emit stopCapture(CAPTURE_ABORTED);
        return false;
    }

    // Limit to minimum and maximum values
    nextExposure = qBound(exp_min, nextExposure, exp_max);

    emit newLog(i18n("Current ADU is %1 Next exposure is %2 seconds.", QString::number(currentADU, 'f', 0),
                     QString("%L1").arg(nextExposure, 0, 'f', 6)));

    activeJob()->setCalibrationStage(SequenceJobState::CAL_CALIBRATION);
    activeJob()->setCoreProperty(SequenceJob::SJ_Exposure, nextExposure);
    if (activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_CLIENT)
    {
        activeCamera()->setUploadMode(ISD::Camera::UPLOAD_CLIENT);
    }

    startNextExposure();
    return false;


}

double CameraProcess::calculateFlatExpTime(double currentADU)
{
    if (activeJob() == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "setCurrentADU with null activeJob().";
        // Nothing good to do here. Just don't crash.
        return currentADU;
    }

    double nextExposure = 0;
    double targetADU    = activeJob()->getCoreProperty(SequenceJob::SJ_TargetADU).toDouble();
    std::vector<double> coeff;

    // limit number of points to two so it can calibrate in intesity changing enviroment like shoting flats
    // at dawn/sunrise sky
    if(activeJob()->getCoreProperty(SequenceJob::SJ_SkyFlat).toBool() && ExpRaw.size() > 2)
    {
        int remove = ExpRaw.size() - 2;
        ExpRaw.remove(0, remove);
        ADURaw.remove(0, remove);
    }

    // Check if saturated, then take shorter capture and discard value
    ExpRaw.append(activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble());
    ADURaw.append(currentADU);

    qCDebug(KSTARS_EKOS_CAPTURE) << "Capture: Current ADU = " << currentADU << " targetADU = " << targetADU
                                 << " Exposure Count: " << ExpRaw.count();

    // Most CCDs are quite linear so 1st degree polynomial is quite sufficient
    // But DSLRs can exhibit non-linear response curve and so a 2nd degree polynomial is more appropriate
    if (ExpRaw.count() >= 2)
    {
        if (ExpRaw.count() >= 5)
        {
            double chisq = 0;

            coeff = gsl_polynomial_fit(ADURaw.data(), ExpRaw.data(), ExpRaw.count(), 2, chisq);
            qCDebug(KSTARS_EKOS_CAPTURE) << "Running polynomial fitting. Found " << coeff.size() << " coefficients.";
            if (std::isnan(coeff[0]) || std::isinf(coeff[0]))
            {
                qCDebug(KSTARS_EKOS_CAPTURE) << "Coefficients are invalid.";
                targetADUAlgorithm = ADU_LEAST_SQUARES;
            }
            else
            {
                nextExposure = coeff[0] + (coeff[1] * targetADU) + (coeff[2] * pow(targetADU, 2));
                // If exposure is not valid or does not make sense, then we fall back to least squares
                if (nextExposure < 0 || (nextExposure > ExpRaw.last() || targetADU < ADURaw.last())
                        || (nextExposure < ExpRaw.last() || targetADU > ADURaw.last()))
                {
                    nextExposure = 0;
                    targetADUAlgorithm = ADU_LEAST_SQUARES;
                }
                else
                {
                    targetADUAlgorithm = ADU_POLYNOMIAL;
                    for (size_t i = 0; i < coeff.size(); i++)
                        qCDebug(KSTARS_EKOS_CAPTURE) << "Coeff #" << i << "=" << coeff[i];
                }
            }
        }

        bool looping = false;
        if (ExpRaw.count() >= 10)
        {
            int size = ExpRaw.count();
            looping  = (std::fabs(ExpRaw[size - 1] - ExpRaw[size - 2] < 0.01)) &&
                       (std::fabs(ExpRaw[size - 2] - ExpRaw[size - 3] < 0.01));
            if (looping && targetADUAlgorithm == ADU_POLYNOMIAL)
            {
                qWarning(KSTARS_EKOS_CAPTURE) << "Detected looping in polynomial results. Falling back to llsqr.";
                targetADUAlgorithm = ADU_LEAST_SQUARES;
            }
        }

        // If we get invalid data, let's fall back to llsq
        // Since polyfit can be unreliable at low counts, let's only use it at the 5th exposure
        // if we don't have results already.
        if (targetADUAlgorithm == ADU_LEAST_SQUARES)
        {
            double a = 0, b = 0;
            llsq(ExpRaw, ADURaw, a, b);

            // If we have valid results, let's calculate next exposure
            if (a != 0.0)
            {
                nextExposure = (targetADU - b) / a;
                // If we get invalid value, let's just proceed iteratively
                if (nextExposure < 0)
                    nextExposure = 0;
            }
        }
    }

    // 2022.01.12 Put a hard limit to 180 seconds.
    // If it goes over this limit, the flat source is probably off.
    if (nextExposure == 0.0 || nextExposure > 180)
    {
        if (currentADU < targetADU)
            nextExposure = activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble() * 1.25;
        else
            nextExposure = activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble() * .75;
    }

    qCDebug(KSTARS_EKOS_CAPTURE) << "next flat exposure is" << nextExposure;

    return nextExposure;

}

void CameraProcess::clearFlatCache()
{
    ADURaw.clear();
    ExpRaw.clear();
}

void CameraProcess::updateTelescopeInfo()
{
    if (devices()->mount() && activeCamera() && devices()->mount()->isConnected())
    {
        // Camera to current telescope
        auto activeDevices = activeCamera()->getText("ACTIVE_DEVICES");
        if (activeDevices)
        {
            auto activeTelescope = activeDevices->findWidgetByName("ACTIVE_TELESCOPE");
            if (activeTelescope)
            {
                activeTelescope->setText(devices()->mount()->getDeviceName().toLatin1().constData());
                activeCamera()->sendNewProperty(activeDevices);
            }
        }
    }

}

void CameraProcess::updateFilterInfo()
{
    QList<ISD::ConcreteDevice *> all_devices;
    if (activeCamera())
        all_devices.append(activeCamera());
    if (devices()->dustCap())
        all_devices.append(devices()->dustCap());

    for (auto &oneDevice : all_devices)
    {
        auto activeDevices = oneDevice->getText("ACTIVE_DEVICES");
        if (activeDevices)
        {
            auto activeFilter = activeDevices->findWidgetByName("ACTIVE_FILTER");
            if (activeFilter)
            {
                QString activeFilterText = QString(activeFilter->getText());
                if (devices()->filterWheel())
                {
                    if (activeFilterText != devices()->filterWheel()->getDeviceName())
                    {
                        activeFilter->setText(devices()->filterWheel()->getDeviceName().toLatin1().constData());
                        oneDevice->sendNewProperty(activeDevices);
                    }
                }
                // Reset filter name in CCD driver
                else if (activeFilterText.isEmpty())
                {
                    // Add debug info since this issue is reported by users. Need to know when it happens.
                    qCDebug(KSTARS_EKOS_CAPTURE) << "No active filter wheel. " << oneDevice->getDeviceName() << " ACTIVE_FILTER is reset.";
                    activeFilter->setText("");
                    oneDevice->sendNewProperty(activeDevices);
                }
            }
        }
    }
}

QString Ekos::CameraProcess::createTabTitle(const FITSMode &captureMode, const QString &deviceName)
{
    const bool isPreview = (activeJob() == nullptr || (activeJob() && activeJob()->jobType() == SequenceJob::JOBTYPE_PREVIEW));
    if (isPreview && Options::singlePreviewFITS())
    {
        // If we are displaying all images from all cameras in a single FITS
        // Viewer window, then we prefix the camera name to the "Preview" string
        if (Options::singleWindowCapturedFITS())
            return (i18n("%1 Preview", deviceName));
        else
            // Otherwise, just use "Preview"
            return(i18n("Preview"));
    }
    else if (captureMode == FITS_CALIBRATE)
    {
        if (activeJob())
        {
            const QString filtername = activeJob()->getCoreProperty(SequenceJob::SJ_Filter).toString();
            if (filtername == "")
                return(QString(i18n("Flat Calibration")));
            else
                return(QString("%1 %2").arg(filtername).arg(i18n("Flat Calibration")));
        }
        else
            return(i18n("Calibration"));
    }
    return "";
}

void CameraProcess::updateFITSViewer(const QSharedPointer<FITSData> data, const FITSMode &captureMode,
                                     const FITSScale &captureFilter, const QString &filename, const QString &deviceName)
{
    // do nothing in case of empty data
    if (data.isNull())
        return;

    switch (captureMode)
    {
        case FITS_NORMAL:
        case FITS_CALIBRATE:
        {
            if (Options::useFITSViewer())
            {
                QUrl fileURL = QUrl::fromLocalFile(filename);
                bool success = false;
                // If image is preview and we should display all captured images in a
                // single tab called "Preview", then set the title to "Preview". Similar if we are calibrating flats.
                // Otherwise, the title will be the captured image name
                QString tabTitle = createTabTitle(captureMode, deviceName);

                int tabIndex = -1;
                int *tabID = &m_fitsvViewerTabIDs.normalTabID;
                if (*tabID == -1 || Options::singlePreviewFITS() == false)
                {

                    success = getFITSViewer()->loadData(data, fileURL, &tabIndex, captureMode, captureFilter, tabTitle);

                    //Setup any necessary connections
                    auto tabs = getFITSViewer()->tabs();
                    if (tabIndex < tabs.size() && captureMode == FITS_NORMAL)
                    {
                        emit newView(tabs[tabIndex]->getView());
                        tabs[tabIndex]->disconnect(this);
                        connect(tabs[tabIndex].get(), &FITSTab::updated, this, [this]
                        {
                            auto tab = qobject_cast<FITSTab *>(sender());
                            emit newView(tab->getView());
                        });
                    }
                }
                else
                {
                    success = getFITSViewer()->updateData(data, fileURL, *tabID, &tabIndex, captureMode, captureFilter, tabTitle);
                }

                if (!success)
                {
                    // If opening file fails, we treat it the same as exposure failure
                    // and recapture again if possible
                    qCCritical(KSTARS_EKOS_CAPTURE()) << "error adding/updating FITS";
                    return;
                }
                *tabID = tabIndex;
                if (Options::focusFITSOnNewImage())
                    getFITSViewer()->raise();

                return;
            }
        }
        break;
        default:
            break;
    }
}

void CameraProcess::updateFITSViewer(const QSharedPointer<FITSData> data, ISD::CameraChip *tChip, const QString &filename)
{
    FITSMode captureMode = tChip == nullptr ? FITS_UNKNOWN : tChip->getCaptureMode();
    FITSScale captureFilter = tChip == nullptr ? FITS_NONE : tChip->getCaptureFilter();
    updateFITSViewer(data, captureMode, captureFilter, filename, data->property("device").toString());
}

bool CameraProcess::loadSequenceQueue(const QString &fileURL,
                                      const QString &targetName, bool setOptions)
{
    state()->clearCapturedFramesMap();
    auto queue = state()->getSequenceQueue();
    if (!queue->load(fileURL, targetName, devices(), state()))
    {
        QString message = i18n("Unable to open file %1", fileURL);
        KSNotification::sorry(message, i18n("Could Not Open File"));
        return false;
    }

    if (setOptions)
    {
        queue->setOptions();
        // Set the HFR Check value appropriately for the conditions, e.g. using Autofocus
        state()->updateHFRThreshold();
    }

    for (auto j : state()->allJobs())
        emit addJob(j);

    return true;
}

bool CameraProcess::saveSequenceQueue(const QString &path, bool loadOptions)
{
    if (loadOptions)
        state()->getSequenceQueue()->loadOptions();
    return state()->getSequenceQueue()->save(path, state()->observerName());
}

void CameraProcess::setCamera(bool connection)
{
    if (connection)
    {
        // TODO: do not simply forward the newExposureValue
        connect(activeCamera(), &ISD::Camera::newExposureValue, this, &CameraProcess::setExposureProgress, Qt::UniqueConnection);
        connect(activeCamera(), &ISD::Camera::newImage, this, &CameraProcess::processFITSData, Qt::UniqueConnection);
        connect(activeCamera(), &ISD::Camera::newRemoteFile, this, &CameraProcess::processNewRemoteFile, Qt::UniqueConnection);
        connect(activeCamera(), &ISD::Camera::ready, this, &CameraProcess::cameraReady, Qt::UniqueConnection);
        // disable passing through new frames to the FITS viewer
        disconnect(activeCamera(), &ISD::Camera::newImage, this, &CameraProcess::showFITSPreview);
    }
    else
    {
        // enable passing through new frames to the FITS viewer
        connect(activeCamera(), &ISD::Camera::newImage, this, &CameraProcess::showFITSPreview);
        // TODO: do not simply forward the newExposureValue
        disconnect(activeCamera(), &ISD::Camera::newExposureValue, this, &CameraProcess::setExposureProgress);
        disconnect(activeCamera(), &ISD::Camera::newImage, this, &CameraProcess::processFITSData);
        disconnect(activeCamera(), &ISD::Camera::newRemoteFile, this, &CameraProcess::processNewRemoteFile);
        //    disconnect(m_Camera, &ISD::Camera::previewFITSGenerated, this, &Capture::setGeneratedPreviewFITS);
        disconnect(activeCamera(), &ISD::Camera::ready, this, &CameraProcess::cameraReady);
    }

}

bool CameraProcess::setFilterWheel(ISD::FilterWheel * device)
{
    if (devices()->filterWheel() && devices()->filterWheel() == device)
        return false;

    if (devices()->filterWheel())
        devices()->filterWheel()->disconnect(this);

    devices()->setFilterWheel(device);

    return (device != nullptr);
}

bool CameraProcess::checkPausing(CaptureContinueAction continueAction)
{
    if (state()->getCaptureState() == CAPTURE_PAUSE_PLANNED)
    {
        emit newLog(i18n("Sequence paused."));
        state()->setCaptureState(CAPTURE_PAUSED);
        // disconnect camera device
        setCamera(false);
        // save continue action
        state()->setContinueAction(continueAction);
        // pause
        return true;
    }
    // no pause
    return false;
}

SequenceJob *CameraProcess::findNextPendingJob()
{
    SequenceJob * first_job = nullptr;

    // search for idle or aborted jobs
    for (auto &job : state()->allJobs())
    {
        if (job->getStatus() == JOB_IDLE || job->getStatus() == JOB_ABORTED)
        {
            first_job = job;
            break;
        }
    }

    // If there are no idle nor aborted jobs, question is whether to reset and restart
    // Scheduler will start a non-empty new job each time and doesn't use this execution path
    if (first_job == nullptr)
    {
        // If we have at least one job that are in error, bail out, even if ignoring job progress
        for (auto &job : state()->allJobs())
        {
            if (job->getStatus() != JOB_DONE)
            {
                // If we arrived here with a zero-delay timer, raise the interval before returning to avoid a cpu peak
                if (state()->getCaptureDelayTimer().isActive())
                {
                    if (state()->getCaptureDelayTimer().interval() <= 0)
                        state()->getCaptureDelayTimer().setInterval(1000);
                }
                return nullptr;
            }
        }

        // If we only have completed jobs and we don't ignore job progress, ask the end-user what to do
        if (!state()->ignoreJobProgress())
            if(KMessageBox::warningContinueCancel(
                        nullptr,
                        i18n("All jobs are complete. Do you want to reset the status of all jobs and restart capturing?"),
                        i18n("Reset job status"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                        "reset_job_complete_status_warning") != KMessageBox::Continue)
                return nullptr;

        // If the end-user accepted to reset, reset all jobs and restart
        resetAllJobs();

        first_job = state()->allJobs().first();
    }
    // If we need to ignore job progress, systematically reset all jobs and restart
    // Scheduler will never ignore job progress and doesn't use this path
    else if (state()->ignoreJobProgress())
    {
        emit newLog(i18n("Warning: option \"Always Reset Sequence When Starting\" is enabled and resets the sequence counts."));
        resetAllJobs();
    }

    return first_job;
}

void CameraProcess::resetJobStatus(JOBStatus newStatus)
{
    if (activeJob() != nullptr)
    {
        activeJob()->resetStatus(newStatus);
        emit updateJobTable(activeJob());
    }
}

void CameraProcess::resetAllJobs()
{
    for (auto &job : state()->allJobs())
    {
        job->resetStatus();
    }
    // clear existing job counts
    m_State->clearCapturedFramesMap();
    // update the entire job table
    emit updateJobTable(nullptr);
}

void CameraProcess::updatedCaptureCompleted(int count)
{
    activeJob()->setCompleted(count);
    emit updateJobTable(activeJob());
}

void CameraProcess::llsq(QVector<double> x, QVector<double> y, double &a, double &b)
{
    double bot;
    int i;
    double top;
    double xbar;
    double ybar;
    int n = x.count();
    //
    //  Special case.
    //
    if (n == 1)
    {
        a = 0.0;
        b = y[0];
        return;
    }
    //
    //  Average X and Y.
    //
    xbar = 0.0;
    ybar = 0.0;
    for (i = 0; i < n; i++)
    {
        xbar = xbar + x[i];
        ybar = ybar + y[i];
    }
    xbar = xbar / static_cast<double>(n);
    ybar = ybar / static_cast<double>(n);
    //
    //  Compute Beta.
    //
    top = 0.0;
    bot = 0.0;
    for (i = 0; i < n; i++)
    {
        top = top + (x[i] - xbar) * (y[i] - ybar);
        bot = bot + (x[i] - xbar) * (x[i] - xbar);
    }

    a = top / bot;

    b = ybar - a * xbar;

}

QStringList CameraProcess::generateScriptArguments() const
{
    // TODO based on user feedback on what paramters are most useful to pass
    return QStringList();
}

bool CameraProcess::hasCoolerControl()
{
    if (devices()->getActiveCamera() && devices()->getActiveCamera()->hasCoolerControl())
        return true;

    return false;
}

bool CameraProcess::setCoolerControl(bool enable)
{
    if (devices()->getActiveCamera() && devices()->getActiveCamera()->hasCoolerControl())
        return devices()->getActiveCamera()->setCoolerControl(enable);

    return false;
}

void CameraProcess::restartCamera(const QString &name)
{
    connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, name]()
    {
        KSMessageBox::Instance()->disconnect(this);
        emit stopCapturing(CAPTURE_ABORTED);
        emit driverTimedout(name);
    });
    connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this]()
    {
        KSMessageBox::Instance()->disconnect(this);
    });

    KSMessageBox::Instance()->questionYesNo(i18n("Are you sure you want to restart %1 camera driver?", name),
                                            i18n("Driver Restart"), 5);
}

QStringList CameraProcess::frameTypes()
{
    if (!activeCamera())
        return QStringList();

    ISD::CameraChip *tChip = devices()->getActiveCamera()->getChip(ISD::CameraChip::PRIMARY_CCD);

    return tChip->getFrameTypes();
}

QStringList CameraProcess::filterLabels()
{
    if (devices()->getFilterManager().isNull())
        return QStringList();

    return devices()->getFilterManager()->getFilterLabels();
}

void CameraProcess::updateGain(double value, QMap<QString, QMap<QString, QVariant> > &propertyMap)
{
    if (devices()->getActiveCamera()->getProperty("CCD_GAIN"))
    {
        if (value >= 0)
        {
            QMap<QString, QVariant> ccdGain;
            ccdGain["GAIN"] = value;
            propertyMap["CCD_GAIN"] = ccdGain;
        }
        else
        {
            propertyMap["CCD_GAIN"].remove("GAIN");
            if (propertyMap["CCD_GAIN"].size() == 0)
                propertyMap.remove("CCD_GAIN");
        }
    }
    else if (devices()->getActiveCamera()->getProperty("CCD_CONTROLS"))
    {
        if (value >= 0)
        {
            QMap<QString, QVariant> ccdGain = propertyMap["CCD_CONTROLS"];
            ccdGain["Gain"] = value;
            propertyMap["CCD_CONTROLS"] = ccdGain;
        }
        else
        {
            propertyMap["CCD_CONTROLS"].remove("Gain");
            if (propertyMap["CCD_CONTROLS"].size() == 0)
                propertyMap.remove("CCD_CONTROLS");
        }
    }
}

void CameraProcess::updateOffset(double value, QMap<QString, QMap<QString, QVariant> > &propertyMap)
{
    if (devices()->getActiveCamera()->getProperty("CCD_OFFSET"))
    {
        if (value >= 0)
        {
            QMap<QString, QVariant> ccdOffset;
            ccdOffset["OFFSET"] = value;
            propertyMap["CCD_OFFSET"] = ccdOffset;
        }
        else
        {
            propertyMap["CCD_OFFSET"].remove("OFFSET");
            if (propertyMap["CCD_OFFSET"].size() == 0)
                propertyMap.remove("CCD_OFFSET");
        }
    }
    else if (devices()->getActiveCamera()->getProperty("CCD_CONTROLS"))
    {
        if (value >= 0)
        {
            QMap<QString, QVariant> ccdOffset = propertyMap["CCD_CONTROLS"];
            ccdOffset["Offset"] = value;
            propertyMap["CCD_CONTROLS"] = ccdOffset;
        }
        else
        {
            propertyMap["CCD_CONTROLS"].remove("Offset");
            if (propertyMap["CCD_CONTROLS"].size() == 0)
                propertyMap.remove("CCD_CONTROLS");
        }
    }
}

QSharedPointer<FITSViewer> CameraProcess::getFITSViewer()
{
    // if the FITS viewer exists, return it
    if (!m_FITSViewerWindow.isNull() && ! m_FITSViewerWindow.isNull())
        return m_FITSViewerWindow;

    // otherwise, create it
    m_fitsvViewerTabIDs = {-1, -1, -1, -1, -1};

    m_FITSViewerWindow = KStars::Instance()->createFITSViewer();

    // Check if ONE tab of the viewer was closed.
    connect(m_FITSViewerWindow.get(), &FITSViewer::closed, this, [this](int tabIndex)
    {
        if (tabIndex == m_fitsvViewerTabIDs.normalTabID)
            m_fitsvViewerTabIDs.normalTabID = -1;
        else if (tabIndex == m_fitsvViewerTabIDs.calibrationTabID)
            m_fitsvViewerTabIDs.calibrationTabID = -1;
        else if (tabIndex == m_fitsvViewerTabIDs.focusTabID)
            m_fitsvViewerTabIDs.focusTabID = -1;
        else if (tabIndex == m_fitsvViewerTabIDs.guideTabID)
            m_fitsvViewerTabIDs.guideTabID = -1;
        else if (tabIndex == m_fitsvViewerTabIDs.alignTabID)
            m_fitsvViewerTabIDs.alignTabID = -1;
    });

    // If FITS viewer was completed closed. Reset everything
    connect(m_FITSViewerWindow.get(), &FITSViewer::terminated, this, [this]()
    {
        m_fitsvViewerTabIDs = {-1, -1, -1, -1, -1};
        m_FITSViewerWindow.clear();
    });

    return m_FITSViewerWindow;
}

ISD::Camera *CameraProcess::activeCamera()
{
    return devices()->getActiveCamera();
}
} // Ekos namespace
