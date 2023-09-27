/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "captureprocess.h"
#include "sequencejob.h"
#include "ekos/manager.h"
#include "ekos/auxiliary/darklibrary.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/profilesettings.h"
#include "ekos/guide/guide.h"
#include "indi/indilistener.h"
#include "ksmessagebox.h"

#include "ksnotification.h"
#include <ekos_capture_debug.h>

#ifdef HAVE_STELLARSOLVER
#include "ekos/auxiliary/stellarsolverprofileeditor.h"
#endif

namespace Ekos
{
CaptureProcess::CaptureProcess(QSharedPointer<CaptureModuleState> newModuleState,
                               QSharedPointer<CaptureDeviceAdaptor> newDeviceAdaptor) : QObject()
{
    m_State = newModuleState;
    m_DeviceAdaptor = newDeviceAdaptor;

    // connect devices to processes
    connect(m_DeviceAdaptor.data(), &CaptureDeviceAdaptor::newCamera, this, &CaptureProcess::selectCamera);

    //This Timer will update the Exposure time in the capture module to display the estimated download time left
    //It will also update the Exposure time left in the Summary Screen.
    //It fires every 100 ms while images are downloading.
    m_State->downloadProgressTimer().setInterval(100);
    connect(&m_State->downloadProgressTimer(), &QTimer::timeout, this, &CaptureProcess::setDownloadProgress);

    // configure dark processor
    m_DarkProcessor = new DarkProcessor(this);
    connect(m_DarkProcessor, &DarkProcessor::newLog, this, &CaptureProcess::newLog);
    connect(m_DarkProcessor, &DarkProcessor::darkFrameCompleted, this, &CaptureProcess::darkFrameCompleted);

    // Pre/post capture/job scripts
    connect(&m_CaptureScript,
            static_cast<void (QProcess::*)(int exitCode, QProcess::ExitStatus status)>(&QProcess::finished),
            this, &CaptureProcess::scriptFinished);
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

bool CaptureProcess::setMount(ISD::Mount *device)
{
    if (m_DeviceAdaptor->mount() && m_DeviceAdaptor->mount() == device)
    {
        updateTelescopeInfo();
        return false;
    }

    if (m_DeviceAdaptor->mount())
        m_DeviceAdaptor->mount()->disconnect(m_State.data());

    m_DeviceAdaptor->setMount(device);

    if (!m_DeviceAdaptor->mount())
        return false;

    m_DeviceAdaptor->mount()->disconnect(this);
    connect(m_DeviceAdaptor->mount(), &ISD::Mount::newTargetName, m_State.data(), &CaptureModuleState::setTargetName);

    updateTelescopeInfo();
    return true;
}

bool CaptureProcess::setRotator(ISD::Rotator *device)
{
    // do nothing if *real* rotator is already connected
    if ((m_DeviceAdaptor->rotator() == device) && (device != nullptr))
        return false;

    // real & manual rotator initializing depends on present mount process
    if (m_DeviceAdaptor->mount())
    {
        if (m_DeviceAdaptor->rotator())
            m_DeviceAdaptor->rotator()->disconnect(this);

        // clear initialisation.
        m_State->isInitialized[CaptureModuleState::ACTION_ROTATOR] = false;

        if (device)
        {
            Manager::Instance()->createRotatorController(device);
            connect(m_DeviceAdaptor.data(), &CaptureDeviceAdaptor::rotatorReverseToggled, this, &CaptureProcess::rotatorReverseToggled,
                    Qt::UniqueConnection);
        }
        m_DeviceAdaptor->setRotator(device);
        return true;
    }
    return false;
}

bool CaptureProcess::setDustCap(ISD::DustCap *device)
{
    if (m_DeviceAdaptor->dustCap() && m_DeviceAdaptor->dustCap() == device)
        return false;

    m_DeviceAdaptor->setDustCap(device);
    m_State->setDustCapState(CaptureModuleState::CAP_UNKNOWN);

    updateFilterInfo();
    return true;

}

bool CaptureProcess::setLightBox(ISD::LightBox *device)
{
    if (m_DeviceAdaptor->lightBox() == device)
        return false;

    m_DeviceAdaptor->setLightBox(device);
    m_State->setLightBoxLightState(CaptureModuleState::CAP_LIGHT_UNKNOWN);

    return true;
}

bool CaptureProcess::setCamera(ISD::Camera *device)
{
    if (m_DeviceAdaptor->getActiveCamera() == device)
        return false;

    m_DeviceAdaptor->setActiveCamera(device);

    return true;

}

void CaptureProcess::toggleVideo(bool enabled)
{
    if (m_DeviceAdaptor->getActiveCamera() == nullptr)
        return;

    if (m_DeviceAdaptor->getActiveCamera()->isBLOBEnabled() == false)
    {
        if (Options::guiderType() != Guide::GUIDE_INTERNAL)
            m_DeviceAdaptor->getActiveCamera()->setBLOBEnabled(true);
        else
        {
            connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, enabled]()
            {
                KSMessageBox::Instance()->disconnect(this);
                m_DeviceAdaptor->getActiveCamera()->setBLOBEnabled(true);
                m_DeviceAdaptor->getActiveCamera()->setVideoStreamEnabled(enabled);
            });

            KSMessageBox::Instance()->questionYesNo(i18n("Image transfer is disabled for this camera. Would you like to enable it?"),
                                                    i18n("Image Transfer"), 15);

            return;
        }
    }

    m_DeviceAdaptor->getActiveCamera()->setVideoStreamEnabled(enabled);

}

void CaptureProcess::toggleSequence()
{
    const CaptureState capturestate = m_State->getCaptureState();
    if (capturestate == CAPTURE_PAUSE_PLANNED || capturestate == CAPTURE_PAUSED)
    {
        // change the state back to capturing only if planned pause is cleared
        if (capturestate == CAPTURE_PAUSE_PLANNED)
            m_State->setCaptureState(CAPTURE_CAPTURING);

        emit newLog(i18n("Sequence resumed."));

        // Call from where ever we have left of when we paused
        switch (m_State->getContinueAction())
        {
            case CaptureModuleState::CONTINUE_ACTION_CAPTURE_COMPLETE:
                resumeSequence();
                break;
            case CaptureModuleState::CONTINUE_ACTION_NEXT_EXPOSURE:
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

void CaptureProcess::startNextPendingJob()
{
    if (m_State->allJobs().count() > 0)
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
        emit addJob();
    }
}

void CaptureProcess::jobAdded(SequenceJob *newJob)
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
            m_State->setActiveJob(newJob);
            capturePreview();
            break;
        default:
            // do nothing
            break;
    }
}

void CaptureProcess::capturePreview(bool loop)
{
    if (m_State->getFocusState() >= FOCUS_PROGRESS)
    {
        emit newLog(i18n("Cannot capture while focus module is busy."));
    }
    else if (activeJob() == nullptr)
    {
        if (loop && !m_State->isLooping())
        {
            m_State->setLooping(true);
            emit newLog(i18n("Starting framing..."));
        }
        // create a preview job
        emit addJob(SequenceJob::JOBTYPE_PREVIEW);
    }
    else
    {
        // job created, start capture preparation
        prepareJob(activeJob());
    }
}

void CaptureProcess::stopCapturing(CaptureState targetState)
{
    clearFlatCache();

    m_State->resetAlignmentRetries();
    //seqTotalCount   = 0;
    //seqCurrentCount = 0;

    m_State->getCaptureTimeout().stop();
    m_State->getCaptureDelayTimer().stop();
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
                    stopText = i18n("CCD capture aborted");
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
            activeJob()->abort();
            if (activeJob()->jobType() != SequenceJob::JOBTYPE_PREVIEW)
            {
                int index = m_State->allJobs().indexOf(activeJob());
                m_State->changeSequenceValue(index, "Status", "Aborted");
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
            m_State->allJobs().removeOne(activeJob());
            // Delete preview job
            activeJob()->deleteLater();
            // Clear active job
            m_State->setActiveJob(nullptr);
        }
    }

    // stop focusing if capture is aborted
    if (m_State->getCaptureState() == CAPTURE_FOCUSING && targetState == CAPTURE_ABORTED)
        emit abortFocus();

    m_State->setCaptureState(targetState);

    m_State->setLooping(false);
    m_State->setBusy(false);

    m_State->getSeqDelayTimer().stop();

    m_State->setActiveJob(nullptr);

    // Turn off any calibration light, IF they were turned on by Capture module
    if (m_DeviceAdaptor->lightBox() && m_State->lightBoxLightEnabled())
    {
        m_State->setLightBoxLightEnabled(false);
        m_DeviceAdaptor->lightBox()->setLightEnabled(false);
    }

    // disconnect camera device
    setCamera(false);

    // In case of exposure looping, let's abort
    if (m_DeviceAdaptor->getActiveCamera() && m_DeviceAdaptor->getActiveChip()
            && m_DeviceAdaptor->getActiveCamera()->isFastExposureEnabled())
        m_DeviceAdaptor->getActiveChip()->abortExposure();

    // communicate successful stop
    emit captureStopped();
}

void CaptureProcess::pauseCapturing()
{
    if (m_State->getCaptureState() != CAPTURE_CAPTURING)
    {
        // Ensure that the pause function is only called during frame capturing
        // Handling it this way is by far easier than trying to enable/disable the pause button
        // Fixme: make pausing possible at all stages. This makes it necessary to separate the pausing states from CaptureState.
        emit newLog(i18n("Pausing only possible while frame capture is running."));
        qCInfo(KSTARS_EKOS_CAPTURE) << "Pause button pressed while not capturing.";
        return;
    }
    // we do not decide at this stage how to resume, since pause is only planned here
    m_State->setContinueAction(CaptureModuleState::CONTINUE_ACTION_NONE);
    m_State->setCaptureState(CAPTURE_PAUSE_PLANNED);
    emit newLog(i18n("Sequence shall be paused after current exposure is complete."));
}

void CaptureProcess::startJob(SequenceJob *job)
{
    m_State->initCapturePreparation();
    prepareJob(job);
}

void CaptureProcess::prepareJob(SequenceJob * job)
{
    m_State->setActiveJob(job);

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
            abort();
        });
        KSMessageBox::Instance()->questionYesNo(i18n("No view available for previews. Enable FITS viewer?"),
                                                i18n("Display preview"), 15);
        // do nothing because currently none of the previews is active.
        return;
    }

    if (m_State->isLooping() == false)
        qCDebug(KSTARS_EKOS_CAPTURE) << "Preparing capture job" << job->getSignature() << "for execution.";

    if (activeJob()->jobType() != SequenceJob::JOBTYPE_PREVIEW)
    {
        // set the progress info

        if (activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
            m_State->setNextSequenceID(1);

        // We check if the job is already fully or partially complete by checking how many files of its type exist on the file system
        // The signature is the unique identification path in the system for a particular job. Format is "<storage path>/<target>/<frame type>/<filter name>".
        // If the Scheduler is requesting the Capture tab to process a sequence job, a target name will be inserted after the sequence file storage field (e.g. /path/to/storage/target/Light/...)
        // If the end-user is requesting the Capture tab to process a sequence job, the sequence file storage will be used as is (e.g. /path/to/storage/Light/...)
        QString signature = activeJob()->getSignature();

        // Now check on the file system ALL the files that exist with the above signature
        // If 29 files exist for example, then nextSequenceID would be the NEXT file number (30)
        // Therefore, we know how to number the next file.
        // However, we do not deduce the number of captures to process from this function.
        m_State->checkSeqBoundary(m_State->sequenceURL());

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
        int count = m_State->capturedFramesCount(signature);
        if (count > 0)
        {

            // Count how many captures this job has to process, given that previous jobs may have done some work already
            for (auto &a_job : m_State->allJobs())
                if (a_job == activeJob())
                    break;
                else if (a_job->getSignature() == activeJob()->getSignature())
                    count -= a_job->getCompleted();

            // This is the current completion count of the current job
            updatedCaptureCompleted(count);
        }
        // JM 2018-09-24: Only set completed jobs to 0 IF the scheduler set captured frames map to begin with
        // If the map is empty, then no scheduler is used and it should proceed as normal.
        else if (m_State->hasCapturedFramesMap())
        {
            // No preliminary information, we reset the job count and run the job unconditionally to clarify the behavior
            updatedCaptureCompleted(0);
        }
        // JM 2018-09-24: In case ignoreJobProgress is enabled
        // We check if this particular job progress ignore flag is set. If not,
        // then we set it and reset completed to zero. Next time it is evaluated here again
        // It will maintain its count regardless
        else if (m_State->ignoreJobProgress()
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

            activeCamera()->setNextSequenceID(m_State->nextSequenceID());
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
                m_State->setBusy(false);
            });

            KSMessageBox::Instance()->questionYesNo(i18n("Image transfer is disabled for this camera. Would you like to enable it?"),
                                                    i18n("Image Transfer"), 15);

            return;
        }
    }

    emit jobPrepared(job);

    prepareActiveJobStage1();

}

void CaptureProcess::prepareActiveJobStage1()
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

void CaptureProcess::prepareActiveJobStage2()
{
    // Just notification of active job stating up
    if (activeJob() == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "prepareActiveJobStage2 with null activeJob().";
    }
    else
        emit newImage(activeJob(), m_State->imageData());


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

void CaptureProcess::executeJob()
{
    if (activeJob() == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "executeJob with null activeJob().";
        return;
    }

    QList<FITSData::Record> FITSHeaders;
    if (Options::defaultObserver().isEmpty() == false)
        FITSHeaders.append(FITSData::Record("Observer", Options::defaultObserver(), "Observer"));
    if (m_State->targetName().isEmpty() == false)
        FITSHeaders.append(FITSData::Record("Object", m_State->targetName(), "Object"));

    if (!FITSHeaders.isEmpty())
        activeCamera()->setFITSHeaders(FITSHeaders);

    // Update button status
    m_State->setBusy(true);

    m_State->setUseGuideHead((m_DeviceAdaptor->getActiveChip()->getType() == ISD::CameraChip::PRIMARY_CCD) ?
                             false : true);

    emit syncGUIToJob(activeJob());

    // If the job is a dark flat, let's find the optimal exposure from prior
    // flat exposures.
    if (activeJob()->jobType() == SequenceJob::JOBTYPE_DARKFLAT)
    {
        // If we found a prior exposure, and current upload more is not local, then update full prefix
        if (m_State->setDarkFlatExposure(activeJob())
                && activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
        {
            auto placeholderPath = PlaceholderPath();
            // Make sure to update Full Prefix as exposure value was changed
            placeholderPath.processJobInfo(activeJob(), m_State->targetName());
            m_State->setNextSequenceID(1);
        }

    }

    updatePreCaptureCalibrationStatus();

}

void CaptureProcess::prepareJobExecution()
{
    if (activeJob() == nullptr)
    {
        qWarning(KSTARS_EKOS_CAPTURE) << "preparePreCaptureActions with null activeJob().";
        // Everything below depends on activeJob(). Just return.
        return;
    }

    m_State->setBusy(true);

    // Update guiderActive before prepareCapture.
    activeJob()->setCoreProperty(SequenceJob::SJ_GuiderActive,
                                 m_State->isActivelyGuiding());

    // signal that capture preparation steps should be executed
    activeJob()->prepareCapture();

    // update the UI
    emit jobExecutionPreparationStarted();
}

void CaptureProcess::refreshOpticalTrain(QString name)
{

    auto mount = OpticalTrainManager::Instance()->getMount(name);
    setMount(mount);

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

IPState CaptureProcess::checkLightFramePendingTasks()
{
    // step 1: did one of the pending jobs fail or has the user aborted the capture?
    if (m_State->getCaptureState() == CAPTURE_ABORTED)
        return IPS_ALERT;

    // step 2: check if pausing has been requested
    if (checkPausing(CaptureModuleState::CONTINUE_ACTION_NEXT_EXPOSURE) == true)
        return IPS_BUSY;

    // step 3: check if a meridian flip is active
    if (m_State->checkMeridianFlipActive())
        return IPS_BUSY;

    // step 4: check guide deviation for non meridian flip stages if the initial guide limit is set.
    //         Wait until the guide deviation is reported to be below the limit (@see setGuideDeviation(double, double)).
    if (m_State->getCaptureState() == CAPTURE_PROGRESS &&
            m_State->getGuideState() == GUIDE_GUIDING &&
            Options::enforceStartGuiderDrift())
        return IPS_BUSY;

    // step 5: check if dithering is required or running
    if ((m_State->getCaptureState() == CAPTURE_DITHERING && m_State->getDitheringState() != IPS_OK)
            || m_State->checkDithering())
        return IPS_BUSY;

    // step 6: check if re-focusing is required
    //         Needs to be checked after dithering checks to avoid dithering in parallel
    //         to focusing, since @startFocusIfRequired() might change its value over time
    if ((m_State->getCaptureState() == CAPTURE_FOCUSING && m_State->checkFocusRunning())
            || m_State->startFocusIfRequired())
        return IPS_BUSY;

    // step 7: resume guiding if it was suspended
    if (m_State->getGuideState() == GUIDE_SUSPENDED)
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

void CaptureProcess::captureStarted(CaptureModuleState::CAPTUREResult rc)
{
    switch (rc)
    {
        case CaptureModuleState::CAPTURE_OK:
        {
            m_State->setCaptureState(CAPTURE_CAPTURING);
            m_State->getCaptureTimeout().start(static_cast<int>(activeJob()->getCoreProperty(
                                                   SequenceJob::SJ_Exposure).toDouble()) * 1000 +
                                               CAPTURE_TIMEOUT_THRESHOLD);
            // calculate remaining capture time for the current job
            m_State->imageCountDown().setHMS(0, 0, 0);
            double ms_left = std::ceil(activeJob()->getExposeLeft() * 1000.0);
            m_State->imageCountDownAddMSecs(int(ms_left));
            m_State->setLastRemainingFrameTimeMS(ms_left);
            m_State->sequenceCountDown().setHMS(0, 0, 0);
            m_State->sequenceCountDownAddMSecs(activeJob()->getJobRemainingTime(m_State->averageDownloadTime()) * 1000);
            // ensure that the download time label is visible

            if (activeJob()->jobType() != SequenceJob::JOBTYPE_PREVIEW)
            {
                auto index = m_State->allJobs().indexOf(activeJob());
                if (index >= 0 && index < m_State->getSequence().count())
                    m_State->changeSequenceValue(index, "Status", "In Progress");

                emit updateJobTable(activeJob());
            }
            emit captureRunning();
        }
        break;

        case CaptureModuleState::CAPTURE_FRAME_ERROR:
            emit newLog(i18n("Failed to set sub frame."));
            emit stopCapturing(CAPTURE_ABORTED);
            break;

        case CaptureModuleState::CAPTURE_BIN_ERROR:
            emit newLog((i18n("Failed to set binning.")));
            emit stopCapturing(CAPTURE_ABORTED);
            break;

        case CaptureModuleState::CAPTURE_FOCUS_ERROR:
            emit newLog((i18n("Cannot capture while focus module is busy.")));
            emit stopCapturing(CAPTURE_ABORTED);
            break;
    }
}

void CaptureProcess::checkNextExposure()
{
    IPState started = startNextExposure();
    // if starting the next exposure did not succeed due to pending jobs running,
    // we retry after 1 second
    if (started == IPS_BUSY)
        QTimer::singleShot(1000, this, &CaptureProcess::checkNextExposure);
}

IPState CaptureProcess::startNextExposure()
{
    // Since this function is looping while pending tasks are running in parallel
    // it might happen that one of them leads to abort() which sets the #activeJob() to nullptr.
    // In this case we terminate the loop by returning #IPS_IDLE without starting a new capture.
    SequenceJob *theJob = activeJob();

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

    const int seqDelay = theJob->getCoreProperty(SequenceJob::SJ_Delay).toInt();
    // nothing pending, let's start the next exposure
    if (seqDelay > 0)
    {
        m_State->setCaptureState(CAPTURE_WAITING);
    }
    m_State->getSeqDelayTimer().start(seqDelay);

    return IPS_OK;
}

IPState CaptureProcess::resumeSequence()
{
    // before we resume, we will check if pausing is requested
    if (checkPausing(CaptureModuleState::CONTINUE_ACTION_CAPTURE_COMPLETE) == true)
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
        if (m_State->getGuideState() == GUIDE_SUSPENDED && m_State->suspendGuidingOnDownload() &&
                m_State->getMeridianFlipState()->checkMeridianFlipActive() == false)
        {
            qCInfo(KSTARS_EKOS_CAPTURE) << "Resuming guiding...";
            emit resumeGuiding();
        }

        // If looping, we just increment the file system image count
        if (activeCamera()->isFastExposureEnabled())
        {
            if (activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
            {
                m_State->checkSeqBoundary(m_State->sequenceURL());
                activeCamera()->setNextSequenceID(m_State->nextSequenceID());
            }
        }

        // set state to capture preparation
        m_State->setCaptureState(CAPTURE_PROGRESS);

        // JM 2020-12-06: Check if we need to execute pre-capture script first.
        if (runCaptureScript(SCRIPT_PRE_CAPTURE) == IPS_BUSY)
        {
            if (activeCamera()->isFastExposureEnabled())
            {
                m_State->setRememberFastExposure(true);
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
                    m_State->setCaptureState(CAPTURE_CAPTURING);
                    return IPS_OK;
                }

                // Stop fast exposure now.
                m_State->setRememberFastExposure(true);
                activeCamera()->setFastExposureEnabled(false);
            }

            checkNextExposure();

        }
    }

    return IPS_OK;

}

void CaptureProcess::processFITSData(const QSharedPointer<FITSData> &data)
{
    ISD::CameraChip * tChip = nullptr;

    QString blobInfo;
    if (data)
    {
        m_State->setImageData(data);
        blobInfo = QString("{Device: %1 Property: %2 Element: %3 Chip: %4}").arg(data->property("device").toString())
                   .arg(data->property("blobVector").toString())
                   .arg(data->property("blobElement").toString())
                   .arg(data->property("chip").toInt());
    }
    else
        m_State->imageData().reset();

    const SequenceJob *job = activeJob();
    // If there is no active job, ignore
    if (job == nullptr)
    {
        if (data)
            qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring received FITS as active job is null.";

        emit processingFITSfinished(false);
        return;
    }

    if (m_State->getMeridianFlipState()->getMeridianFlipStage() >= MeridianFlipState::MF_ALIGNING)
    {
        if (data)
            qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring Received FITS as meridian flip stage is" <<
                                           m_State->getMeridianFlipState()->getMeridianFlipStage();
        emit processingFITSfinished(false);
        return;
    }

    // If image is client or both, let's process it.
    if (activeCamera() && activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
    {

        if (m_State->getCaptureState() == CAPTURE_IDLE || m_State->getCaptureState() == CAPTURE_ABORTED)
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring Received FITS as current capture state is not active" <<
                                           m_State->getCaptureState();

            emit processingFITSfinished(false);
            return;
        }

        if (data)
        {
            tChip = activeCamera()->getChip(static_cast<ISD::CameraChip::ChipType>
                                            (data->property("chip").toInt()));
            if (tChip != m_DeviceAdaptor->getActiveChip())
            {
                if (m_State->getGuideState() == GUIDE_IDLE)
                    qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring Received FITS as it does not correspond to the target chip"
                                                   << m_DeviceAdaptor->getActiveChip()->getType();

                emit processingFITSfinished(false);
                return;
            }
        }

        if (m_DeviceAdaptor->getActiveChip()->getCaptureMode() == FITS_FOCUS ||
                m_DeviceAdaptor->getActiveChip()->getCaptureMode() == FITS_GUIDE)
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << blobInfo << "Ignoring Received FITS as it has the wrong capture mode" <<
                                           m_DeviceAdaptor->getActiveChip()->getCaptureMode();

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

        // If dark is selected, perform dark substraction.
        if (data && Options::autoDark() && job->jobType() == SequenceJob::JOBTYPE_PREVIEW && m_State->useGuideHead() == false)
        {
            QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::CaptureOpticalTrain);
            if (trainID.isValid())
            {
                m_DarkProcessor.data()->denoise(trainID.toUInt(),
                                                m_DeviceAdaptor->getActiveChip(),
                                                m_State->imageData(),
                                                job->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(),
                                                job->getCoreProperty(SequenceJob::SJ_ROI).toRect().x(),
                                                job->getCoreProperty(SequenceJob::SJ_ROI).toRect().y());
            }
            else
                qWarning(KSTARS_EKOS_CAPTURE) << "Invalid train ID for darks substraction:" << trainID.toUInt();

        }

        // set image metadata
        updateImageMetadataAction(m_State->imageData());
    }

    // image has been received and processed successfully.
    m_State->setCaptureState(CAPTURE_IMAGE_RECEIVED);
    // processing finished successfully
    imageCapturingCompleted();
    // hand over to the capture module
    emit processingFITSfinished(true);
}

void CaptureProcess::processNewRemoteFile(QString file)
{
    emit newLog(i18n("Remote image saved to %1", file));
    // call processing steps without image data if the image is stored only remotely
    if (activeCamera() && activeCamera()->getUploadMode() == ISD::Camera::UPLOAD_LOCAL)
        processFITSData(nullptr);
}

void CaptureProcess::imageCapturingCompleted()
{
    SequenceJob *thejob = activeJob();

    if (thejob == nullptr)
        return;

    // If fast exposure is off, disconnect exposure progress
    // otherwise, keep it going since it fires off from driver continuous capture process.
    if (activeCamera()->isFastExposureEnabled() == false)
    {
        disconnect(activeCamera(), &ISD::Camera::newExposureValue, this,
                   &CaptureProcess::setExposureProgress);
        DarkLibrary::Instance()->disconnect(this);
    }
    // stop timers
    m_State->getCaptureTimeout().stop();
    m_State->setCaptureTimeoutCounter(0);

    m_State->downloadProgressTimer().stop();

    // In case we're framing, let's return quickly to continue the process.
    if (m_State->isLooping())
    {
        continueFramingAction(m_State->imageData());
        return;
    }

    // Update download times.
    updateDownloadTimesAction();

    // If it was initially set as pure preview job and NOT as preview for calibration
    if (previewImageCompletedAction(m_State->imageData()) == IPS_OK)
        return;

    // update counters
    updateCompletedCaptureCountersAction();

    switch (thejob->getFrameType())
    {
        case FRAME_BIAS:
        case FRAME_DARK:
            thejob->setCalibrationStage(SequenceJobState::CAL_CALIBRATION_COMPLETE);
            break;
        case FRAME_FLAT:
            /* calibration not completed, adapt exposure time */
            if (thejob->getFlatFieldDuration() == DURATION_ADU
                    && thejob->getCoreProperty(SequenceJob::SJ_TargetADU).toDouble() > 0 &&
                    checkFlatCalibration(m_State->imageData(), m_State->exposureRange().min,
                                         m_State->exposureRange().max) == false)
                return; /* calibration not completed */
            thejob->setCalibrationStage(SequenceJobState::CAL_CALIBRATION_COMPLETE);
            break;
        case FRAME_LIGHT:
            // don nothing, continue
            break;
        case FRAME_NONE:
            // this should not happen!
            qWarning(KSTARS_EKOS_CAPTURE) << "Job completed with frametype NONE!";
            return;
    }

    if (thejob->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION_COMPLETE)
        thejob->setCalibrationStage(SequenceJobState::CAL_CAPTURING);

    // JM 2020-06-17: Emit newImage for LOCAL images (stored on remote host)
    //if (m_Camera->getUploadMode() == ISD::Camera::UPLOAD_LOCAL)
    emit newImage(thejob, m_State->imageData());


    // Check if we need to execute post capture script first
    if (runCaptureScript(SCRIPT_POST_CAPTURE) == IPS_BUSY)
        return;

    resumeSequence();
}

IPState CaptureProcess::processPreCaptureCalibrationStage()
{
    // in some rare cases it might happen that activeJob() has been cleared by a concurrent thread
    if (activeJob() == nullptr)
    {
        qCWarning(KSTARS_EKOS_CAPTURE) << "Processing pre capture calibration without active job, state = " <<
                                       getCaptureStatusString(m_State->getCaptureState());
        return IPS_ALERT;
    }

    // If we are currently guide and the frame is NOT a light frame, then we shopld suspend.
    // N.B. The guide camera could be on its own scope unaffected but it doesn't hurt to stop
    // guiding since it is no longer used anyway.
    if (activeJob()->getFrameType() != FRAME_LIGHT
            && m_State->getGuideState() == GUIDE_GUIDING)
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

void CaptureProcess::updatePreCaptureCalibrationStatus()
{
    // If process was aborted or stopped by the user
    if (m_State->isBusy() == false)
    {
        emit newLog(i18n("Warning: Calibration process was prematurely terminated."));
        return;
    }

    IPState rc = processPreCaptureCalibrationStage();

    if (rc == IPS_ALERT)
        return;
    else if (rc == IPS_BUSY)
    {
        QTimer::singleShot(1000, this, &CaptureProcess::updatePreCaptureCalibrationStatus);
        return;
    }

    captureImage();
}

void CaptureProcess::processJobCompletion1()
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

void CaptureProcess::processJobCompletion2()
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
            int index = m_State->allJobs().indexOf(activeJob());
            QJsonArray seqArray = m_State->getSequence();
            QJsonObject oneSequence = seqArray[index].toObject();
            oneSequence["Status"] = "Complete";
            seqArray.replace(index, oneSequence);
            m_State->setSequence(seqArray);
            emit sequenceChanged(seqArray);
            emit updateJobTable(activeJob());
        }
    }
    // stopping clears the planned state, therefore skip if pause planned
    if (m_State->getCaptureState() != CAPTURE_PAUSE_PLANNED)
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
        if (m_State->getGuideState() == GUIDE_SUSPENDED && m_State->suspendGuidingOnDownload())
            emit resumeGuiding();
    }

}

IPState CaptureProcess::startNextJob()
{
    SequenceJob * next_job = nullptr;

    for (auto &oneJob : m_State->allJobs())
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
        if (m_State->getGuideState() == GUIDE_SUSPENDED && m_State->suspendGuidingOnDownload() &&
                m_State->getMeridianFlipState()->checkMeridianFlipActive() == false)
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

void CaptureProcess::captureImage()
{
    if (activeJob() == nullptr)
        return;

    // Bail out if we have no CCD anymore
    if (activeCamera()->isConnected() == false)
    {
        emit newLog(i18n("Error: Lost connection to CCD."));
        emit stopCapture(CAPTURE_ABORTED);
        return;
    }

    m_State->getCaptureTimeout().stop();
    m_State->getSeqDelayTimer().stop();
    m_State->getCaptureDelayTimer().stop();
    if (activeCamera()->isFastExposureEnabled())
    {
        int remaining = m_State->isLooping() ? 100000 : (activeJob()->getCoreProperty(
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
        m_State->checkSeqBoundary(m_State->sequenceURL());
        activeCamera()->setNextSequenceID(m_State->nextSequenceID());
    }

    // Re-enable fast exposure if it was disabled before due to pending tasks
    if (m_State->isRememberFastExposure())
    {
        m_State->setRememberFastExposure(false);
        activeCamera()->setFastExposureEnabled(true);
    }

    if (m_State->frameSettings().contains(m_DeviceAdaptor->getActiveChip()))
    {
        const auto roi = activeJob()->getCoreProperty(SequenceJob::SJ_ROI).toRect();
        QVariantMap settings;
        settings["x"]    = roi.x();
        settings["y"]    = roi.y();
        settings["w"]    = roi.width();
        settings["h"]    = roi.height();
        settings["binx"] = activeJob()->getCoreProperty(SequenceJob::SJ_Binning).toPoint().x();
        settings["biny"] = activeJob()->getCoreProperty(SequenceJob::SJ_Binning).toPoint().y();

        m_State->frameSettings()[m_DeviceAdaptor->getActiveChip()] = settings;
    }

    // If using DSLR, make sure it is set to correct transfer format
    activeCamera()->setEncodingFormat(activeJob()->getCoreProperty(
                                          SequenceJob::SJ_Encoding).toString());

    m_State->setStartingCapture(true);
    auto placeholderPath = PlaceholderPath(m_State->sequenceURL().toLocalFile());
    placeholderPath.setGenerateFilenameSettings(*activeJob(), m_State->targetName());
    activeCamera()->setPlaceholderPath(placeholderPath);
    // now hand over the control of capturing to the sequence job. As soon as capturing
    // has started, the sequence job will report the result with the captureStarted() event
    // that will trigger Capture::captureStarted()
    activeJob()->startCapturing(m_State->getRefocusState()->isAutoFocusReady(),
                                activeJob()->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION ? FITS_CALIBRATE :
                                FITS_NORMAL);

    // Re-enable fast exposure if it was disabled before due to pending tasks
    if (m_State->isRememberFastExposure())
    {
        m_State->setRememberFastExposure(false);
        activeCamera()->setFastExposureEnabled(true);
    }

    emit captureImageStarted();
}

void CaptureProcess::resetFrame()
{
    m_DeviceAdaptor->setActiveChip(m_State->useGuideHead() ?
                                   m_DeviceAdaptor->getActiveCamera()->getChip(
                                       ISD::CameraChip::GUIDE_CCD) :
                                   m_DeviceAdaptor->getActiveCamera()->getChip(ISD::CameraChip::PRIMARY_CCD));
    m_DeviceAdaptor->getActiveChip()->resetFrame();
    emit updateFrameProperties(1);
}

void CaptureProcess::setExposureProgress(ISD::CameraChip *tChip, double value, IPState ipstate)
{
    // ignore values if not capturing
    if (m_State->checkCapturing() == false)
        return;

    if (m_DeviceAdaptor->getActiveChip() != tChip ||
            m_DeviceAdaptor->getActiveChip()->getCaptureMode() != FITS_NORMAL
            || m_State->getMeridianFlipState()->getMeridianFlipStage() >= MeridianFlipState::MF_ALIGNING)
        return;

    double deltaMS = std::ceil(1000.0 * value - m_State->lastRemainingFrameTimeMS());
    emit updateCaptureCountDown(int(deltaMS));
    m_State->setLastRemainingFrameTimeMS(m_State->lastRemainingFrameTimeMS() + deltaMS);

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

        if (retries == 3)
        {
            abort();
            return;
        }

        emit newLog((i18n("Restarting capture attempt #%1", retries)));

        m_State->setNextSequenceID(1);

        captureImage();
        return;
    }

    if (activeJob() != nullptr && ipstate == IPS_OK)
    {
        activeJob()->setCaptureRetires(0);
        activeJob()->setExposeLeft(0);

        if (m_DeviceAdaptor->getActiveCamera()
                && m_DeviceAdaptor->getActiveCamera()->getUploadMode() == ISD::Camera::UPLOAD_LOCAL)
        {
            if (activeJob()->getStatus() == JOB_BUSY)
            {
                emit processingFITSfinished(false);
                return;
            }
        }

        if (m_State->getGuideState() == GUIDE_GUIDING && Options::guiderType() == 0
                && m_State->suspendGuidingOnDownload())
        {
            qCDebug(KSTARS_EKOS_CAPTURE) << "Autoguiding suspended until primary CCD chip completes downloading...";
            emit suspendGuiding();
        }

        emit downloadingFrame();

        //This will start the clock to see how long the download takes.
        m_State->downloadTimer().start();
        m_State->downloadProgressTimer().start();
    }
}

void CaptureProcess::setDownloadProgress()
{
    if (activeJob())
    {
        double downloadTimeLeft = m_State->averageDownloadTime() - m_State->downloadTimer().elapsed() /
                                  1000.0;
        if(downloadTimeLeft >= 0)
        {
            m_State->imageCountDown().setHMS(0, 0, 0);
            m_State->imageCountDownAddMSecs(int(std::ceil(downloadTimeLeft * 1000)));
            emit newDownloadProgress(downloadTimeLeft);
        }
    }

}

IPState CaptureProcess::continueFramingAction(const QSharedPointer<FITSData> &imageData)
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
                activeJob()->startCapturing(m_State->getRefocusState()->isAutoFocusReady(),
                                            FITS_NORMAL);
            });
        }
        else
            activeJob()->startCapturing(m_State->getRefocusState()->isAutoFocusReady(),
                                        FITS_NORMAL);
    }
    return IPS_OK;

}

IPState CaptureProcess::updateDownloadTimesAction()
{
    // Do not calculate download time for images stored on server.
    // Only calculate for longer exposures.
    if (activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL
            && m_State->downloadTimer().isValid())
    {
        //This determines the time since the image started downloading
        double currentDownloadTime = m_State->downloadTimer().elapsed() / 1000.0;
        m_State->addDownloadTime(currentDownloadTime);
        // Always invalidate timer as it must be explicitly started.
        m_State->downloadTimer().invalidate();

        QString dLTimeString = QString::number(currentDownloadTime, 'd', 2);
        QString estimatedTimeString = QString::number(m_State->averageDownloadTime(), 'd', 2);
        emit newLog(i18n("Download Time: %1 s, New Download Time Estimate: %2 s.", dLTimeString, estimatedTimeString));
    }
    return IPS_OK;
}

IPState CaptureProcess::previewImageCompletedAction(QSharedPointer<FITSData> imageData)
{
    if (activeJob()->jobType() == SequenceJob::JOBTYPE_PREVIEW)
    {
        //sendNewImage(blobFilename, blobChip);
        emit newImage(activeJob(), imageData);
        // Reset upload mode if it was changed by preview
        activeCamera()->setUploadMode(activeJob()->getUploadMode());
        // Reset active job pointer
        m_State->setActiveJob(nullptr);
        emit stopCapture(CAPTURE_COMPLETE);
        if (m_State->getGuideState() == GUIDE_SUSPENDED && m_State->suspendGuidingOnDownload())
            emit resumeGuiding();
        return IPS_OK;
    }
    else
        return IPS_IDLE;

}

IPState CaptureProcess::updateCompletedCaptureCountersAction()
{
    // update counters if not in preview mode or calibrating
    if (activeJob()->jobType() != SequenceJob::JOBTYPE_PREVIEW
            && activeJob()->getCalibrationStage() != SequenceJobState::CAL_CALIBRATION)
    {
        /* Increase the sequence's current capture count */
        updatedCaptureCompleted(activeJob()->getCompleted() + 1);
        /* Decrease the counter for in-sequence focusing */
        m_State->getRefocusState()->decreaseInSequenceFocusCounter();
        /* Reset adaptive focus flag */
        m_State->getRefocusState()->setAdaptiveFocusDone(false);
    }

    /* Decrease the dithering counter except for directly after meridian flip                                              */
    /* Hint: this isonly relevant when a meridian flip happened during a paused sequence when pressing "Start" afterwards. */
    if (m_State->getMeridianFlipState()->getMeridianFlipStage() < MeridianFlipState::MF_FLIPPING)
        m_State->decreaseDitherCounter();

    /* If we were assigned a captured frame map, also increase the relevant counter for prepareJob */
    m_State->addCapturedFrame(activeJob()->getSignature());

    // report that the image has been received
    emit newLog(i18n("Received image %1 out of %2.", activeJob()->getCompleted(),
                     activeJob()->getCoreProperty(SequenceJob::SJ_Count).toInt()));

    return IPS_OK;
}

IPState CaptureProcess::updateImageMetadataAction(QSharedPointer<FITSData> imageData)
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
        metadata["hfr"] = hfr;
        metadata["starCount"] = numStars;
        metadata["median"] = median;
        metadata["eccentricity"] = eccentricity;
        emit captureComplete(metadata);
    }
    return IPS_OK;
}

IPState CaptureProcess::runCaptureScript(ScriptTypes scriptType, bool precond)
{
    if (activeJob())
    {
        const QString captureScript = activeJob()->getScript(scriptType);
        if (captureScript.isEmpty() == false && precond)
        {
            m_State->setCaptureScriptType(scriptType);
            m_CaptureScript.start(captureScript, generateScriptArguments());
            emit newLog(i18n("Executing capture script %1", captureScript));
            return IPS_BUSY;
        }
    }
    // no script execution started
    return IPS_OK;
}

void CaptureProcess::scriptFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status)

    switch (m_State->captureScriptType())
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
            else if (m_State->checkMeridianFlipReady())
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

void CaptureProcess::selectCamera(QString name)
{
    if (activeCamera() && activeCamera()->getDeviceName() == name)
        checkCamera();

    emit refreshCamera();
}

void CaptureProcess::checkCamera()
{
    // Do not update any camera settings while capture is in progress.
    if (m_State->getCaptureState() == CAPTURE_CAPTURING || !activeCamera())
        return;

    m_DeviceAdaptor->setActiveChip(nullptr);

    // FIXME TODO fix guide head detection
    if (activeCamera()->getDeviceName().contains("Guider"))
    {
        m_State->setUseGuideHead(true);
        m_DeviceAdaptor->setActiveChip(activeCamera()->getChip(ISD::CameraChip::GUIDE_CCD));
    }

    if (m_DeviceAdaptor->getActiveChip() == nullptr)
    {
        m_State->setUseGuideHead(false);
        m_DeviceAdaptor->setActiveChip(activeCamera()->getChip(ISD::CameraChip::PRIMARY_CCD));
    }

    emit refreshCameraSettings();
}

void CaptureProcess::syncDSLRToTargetChip(const QString &model)
{
    auto pos = std::find_if(m_State->DSLRInfos().begin(),
                            m_State->DSLRInfos().end(), [model](const QMap<QString, QVariant> &oneDSLRInfo)
    {
        return (oneDSLRInfo["Model"] == model);
    });

    // Sync Pixel Size
    if (pos != m_State->DSLRInfos().end())
    {
        auto camera = *pos;
        m_DeviceAdaptor->getActiveChip()->setImageInfo(camera["Width"].toInt(),
                camera["Height"].toInt(),
                camera["PixelW"].toDouble(),
                camera["PixelH"].toDouble(),
                8);
    }
}

void CaptureProcess::reconnectCameraDriver(const QString &camera, const QString &filterWheel)
{
    if (activeCamera() && activeCamera()->getDeviceName() == camera)
    {
        // Set camera again to the one we restarted
        CaptureState rememberState = m_State->getCaptureState();
        m_State->setCaptureState(CAPTURE_IDLE);
        checkCamera();
        m_State->setCaptureState(rememberState);

        // restart capture
        m_State->setCaptureTimeoutCounter(0);

        if (activeJob())
        {
            m_DeviceAdaptor->setActiveChip(m_DeviceAdaptor->getActiveChip());
            captureImage();
        }
        return;
    }

    QTimer::singleShot(5000, this, [ &, camera, filterWheel]()
    {
        reconnectCameraDriver(camera, filterWheel);
    });
}

void CaptureProcess::removeDevice(const QSharedPointer<ISD::GenericDevice> &device)
{
    auto name = device->getDeviceName();
    device->disconnect(this);

    // Mounts
    if (m_DeviceAdaptor->mount() && m_DeviceAdaptor->mount()->getDeviceName() == device->getDeviceName())
    {
        m_DeviceAdaptor->mount()->disconnect(this);
        m_DeviceAdaptor->setMount(nullptr);
        if (activeJob() != nullptr)
            activeJob()->addMount(nullptr);
    }

    // Domes
    if (m_DeviceAdaptor->dome() && m_DeviceAdaptor->dome()->getDeviceName() == device->getDeviceName())
    {
        m_DeviceAdaptor->dome()->disconnect(this);
        m_DeviceAdaptor->setDome(nullptr);
    }

    // Rotators
    if (m_DeviceAdaptor->rotator() && m_DeviceAdaptor->rotator()->getDeviceName() == device->getDeviceName())
    {
        m_DeviceAdaptor->rotator()->disconnect(this);
        m_DeviceAdaptor->setRotator(nullptr);
    }

    // Dust Caps
    if (m_DeviceAdaptor->dustCap() && m_DeviceAdaptor->dustCap()->getDeviceName() == device->getDeviceName())
    {
        m_DeviceAdaptor->dustCap()->disconnect(this);
        m_DeviceAdaptor->setDustCap(nullptr);
        m_State->hasDustCap = false;
        m_State->setDustCapState(CaptureModuleState::CAP_UNKNOWN);
    }

    // Light Boxes
    if (m_DeviceAdaptor->lightBox() && m_DeviceAdaptor->lightBox()->getDeviceName() == device->getDeviceName())
    {
        m_DeviceAdaptor->lightBox()->disconnect(this);
        m_DeviceAdaptor->setLightBox(nullptr);
        m_State->hasLightBox = false;
        m_State->setLightBoxLightState(CaptureModuleState::CAP_LIGHT_UNKNOWN);
    }

    // Cameras
    if (activeCamera() && activeCamera()->getDeviceName() == name)
    {
        activeCamera()->disconnect(this);
        m_DeviceAdaptor->setActiveCamera(nullptr);

        QSharedPointer<ISD::GenericDevice> generic;
        if (INDIListener::findDevice(name, generic))
            DarkLibrary::Instance()->removeDevice(generic);

        QTimer::singleShot(1000, this, [this]()
        {
            emit refreshCameraSettings();
        });
    }

    // Filter Wheels
    if (m_DeviceAdaptor->filterWheel() && m_DeviceAdaptor->filterWheel()->getDeviceName() == name)
    {
        m_DeviceAdaptor->filterWheel()->disconnect(this);
        m_DeviceAdaptor->setFilterWheel(nullptr);

        QTimer::singleShot(1000, this, [this]()
        {
            emit refreshFilterSettings();
        });
    }
}

void CaptureProcess::processCaptureTimeout()
{
    m_State->setCaptureTimeoutCounter(m_State->captureTimeoutCounter() + 1);

    if (m_State->deviceRestartCounter() >= 3)
    {
        m_State->setCaptureTimeoutCounter(0);
        m_State->setDeviceRestartCounter(0);
        emit newLog(i18n("Exposure timeout. Aborting..."));
        emit stopCapture(CAPTURE_ABORTED);
        return;
    }

    if (m_State->captureTimeoutCounter() > 1 && activeCamera())
    {
        QString camera = activeCamera()->getDeviceName();
        QString fw = (m_DeviceAdaptor->filterWheel() != nullptr) ?
                     m_DeviceAdaptor->filterWheel()->getDeviceName() : "";
        emit driverTimedout(camera);
        QTimer::singleShot(5000, this, [ &, camera, fw]()
        {
            m_State->setDeviceRestartCounter(m_State->deviceRestartCounter() + 1);
            reconnectCameraDriver(camera, fw);
        });
        return;
    }
    else
    {
        // Double check that m_Camera is valid in case it was reset due to driver restart.
        if (activeCamera() && activeJob())
        {
            emit newLog(i18n("Exposure timeout. Restarting exposure..."));
            activeCamera()->setEncodingFormat("FITS");
            ISD::CameraChip *targetChip = activeCamera()->getChip(m_State->useGuideHead() ?
                                          ISD::CameraChip::GUIDE_CCD :
                                          ISD::CameraChip::PRIMARY_CCD);
            targetChip->abortExposure();
            const double exptime = activeJob()->getCoreProperty(SequenceJob::SJ_Exposure).toDouble();
            targetChip->capture(exptime);
            m_State->getCaptureTimeout().start(static_cast<int>((exptime) * 1000 + CAPTURE_TIMEOUT_THRESHOLD));
        }
        else
        {
            qCDebug(KSTARS_EKOS_CAPTURE) << "Unable to restart exposure as camera is missing, trying again in 5 seconds...";
            QTimer::singleShot(5000, this, &CaptureProcess::processCaptureTimeout);
        }
    }

}

void CaptureProcess::processCaptureError(ISD::Camera::ErrorType type)
{
    if (!activeJob())
        return;

    if (type == ISD::Camera::ERROR_CAPTURE)
    {
        int retries = activeJob()->getCaptureRetires() + 1;

        activeJob()->setCaptureRetires(retries);

        emit newLog(i18n("Capture failed. Check INDI Control Panel for details."));

        if (retries == 3)
        {
            emit stopCapture(CAPTURE_ABORTED);
            return;
        }

        emit newLog(i18n("Restarting capture attempt #%1", retries));

        m_State->setNextSequenceID(1);

        captureImage();
        return;
    }
    else
    {
        emit stopCapture(CAPTURE_ABORTED);
    }
}

bool CaptureProcess::checkFlatCalibration(QSharedPointer<FITSData> imageData, double exp_min, double exp_max)
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
    if (ADUDiff <= m_State->targetADUTolerance())
    {
        if (activeJob()->getCalibrationStage() == SequenceJobState::CAL_CALIBRATION)
        {
            emit newLog(
                i18n("Current ADU %1 within target ADU tolerance range.", QString::number(currentADU, 'f', 0)));
            activeCamera()->setUploadMode(activeJob()->getUploadMode());
            auto placeholderPath = PlaceholderPath();
            // Make sure to update Full Prefix as exposure value was changed
            placeholderPath.processJobInfo(activeJob(), m_State->targetName());
            // Mark calibration as complete
            activeJob()->setCalibrationStage(SequenceJobState::CAL_CALIBRATION_COMPLETE);

            // Must update sequence prefix as this step is only done in prepareJob
            // but since the duration has now been updated, we must take care to update signature
            // since it may include a placeholder for duration which would affect it.
            if (activeCamera()
                    && activeCamera()->getUploadMode() != ISD::Camera::UPLOAD_LOCAL)
                m_State->setNextSequenceID(1);

            startNextExposure();
            return false;
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

double CaptureProcess::calculateFlatExpTime(double currentADU)
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

void CaptureProcess::clearFlatCache()
{
    ADURaw.clear();
    ExpRaw.clear();
}

void CaptureProcess::updateTelescopeInfo()
{
    if (m_DeviceAdaptor->mount() && activeCamera() && m_DeviceAdaptor->mount()->isConnected())
    {
        // Camera to current telescope
        auto activeDevices = activeCamera()->getText("ACTIVE_DEVICES");
        if (activeDevices)
        {
            auto activeTelescope = activeDevices->findWidgetByName("ACTIVE_TELESCOPE");
            if (activeTelescope)
            {
                activeTelescope->setText(m_DeviceAdaptor->mount()->getDeviceName().toLatin1().constData());
                activeCamera()->sendNewProperty(activeDevices);
            }
        }
    }

}

void CaptureProcess::updateFilterInfo()
{
    QList<ISD::ConcreteDevice *> all_devices;
    if (activeCamera())
        all_devices.append(activeCamera());
    if (m_DeviceAdaptor->dustCap())
        all_devices.append(m_DeviceAdaptor->dustCap());

    for (auto &oneDevice : all_devices)
    {
        auto activeDevices = oneDevice->getText("ACTIVE_DEVICES");
        if (activeDevices)
        {
            auto activeFilter = activeDevices->findWidgetByName("ACTIVE_FILTER");
            if (activeFilter)
            {
                if (m_DeviceAdaptor->filterWheel())
                {
                    if (activeFilter->getText() != m_DeviceAdaptor->filterWheel()->getDeviceName())
                    {
                        activeFilter->setText(m_DeviceAdaptor->filterWheel()->getDeviceName().toLatin1().constData());
                        oneDevice->sendNewProperty(activeDevices);
                    }
                }
                // Reset filter name in CCD driver
                else if (QString(activeFilter->getText()).isEmpty())
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

void CaptureProcess::setCamera(bool connection)
{
    if (connection)
    {
        // TODO: do not simply forward the newExposureValue
        connect(activeCamera(), &ISD::Camera::newExposureValue, this,
                &CaptureProcess::setExposureProgress, Qt::UniqueConnection);
        connect(activeCamera(), &ISD::Camera::newImage, this, &CaptureProcess::processFITSData, Qt::UniqueConnection);
        connect(activeCamera(), &ISD::Camera::newRemoteFile, this, &CaptureProcess::processNewRemoteFile, Qt::UniqueConnection);
        //connect(m_Camera, &ISD::Camera::previewFITSGenerated, this, &Capture::setGeneratedPreviewFITS, Qt::UniqueConnection);
        connect(activeCamera(), &ISD::Camera::ready, this, &CaptureProcess::cameraReady);
    }
    else
    {
        // TODO: do not simply forward the newExposureValue
        disconnect(activeCamera(), &ISD::Camera::newExposureValue, this, &CaptureProcess::setExposureProgress);
        disconnect(activeCamera(), &ISD::Camera::newImage, this, &CaptureProcess::processFITSData);
        disconnect(activeCamera(), &ISD::Camera::newRemoteFile, this, &CaptureProcess::processNewRemoteFile);
        //    disconnect(m_Camera, &ISD::Camera::previewFITSGenerated, this, &Capture::setGeneratedPreviewFITS);
        disconnect(activeCamera(), &ISD::Camera::ready, this, &CaptureProcess::cameraReady);
    }

}

bool CaptureProcess::setFilterWheel(ISD::FilterWheel * device)
{
    if (m_DeviceAdaptor->filterWheel() && m_DeviceAdaptor->filterWheel() == device)
        return false;

    if (m_DeviceAdaptor->filterWheel())
        m_DeviceAdaptor->filterWheel()->disconnect(this);

    m_DeviceAdaptor->setFilterWheel(device);

    return (device != nullptr);
}

bool CaptureProcess::checkPausing(CaptureModuleState::ContinueAction continueAction)
{
    if (m_State->getCaptureState() == CAPTURE_PAUSE_PLANNED)
    {
        emit newLog(i18n("Sequence paused."));
        m_State->setCaptureState(CAPTURE_PAUSED);
        // disconnect camera device
        setCamera(false);
        // handle a requested meridian flip
        if (m_State->getMeridianFlipState()->getMeridianFlipStage() != MeridianFlipState::MF_NONE)
            emit updateMeridianFlipStage(MeridianFlipState::MF_READY);
        // save continue action
        m_State->setContinueAction(continueAction);
        // pause
        return true;
    }
    // no pause
    return false;
}

SequenceJob *CaptureProcess::findNextPendingJob()
{
    SequenceJob * first_job = nullptr;

    // search for idle or aborted jobs
    for (auto &job : m_State->allJobs())
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
        for (auto &job : m_State->allJobs())
        {
            if (job->getStatus() != JOB_DONE)
            {
                // If we arrived here with a zero-delay timer, raise the interval before returning to avoid a cpu peak
                if (m_State->getCaptureDelayTimer().isActive())
                {
                    if (m_State->getCaptureDelayTimer().interval() <= 0)
                        m_State->getCaptureDelayTimer().setInterval(1000);
                }
                return nullptr;
            }
        }

        // If we only have completed jobs and we don't ignore job progress, ask the end-user what to do
        if (!m_State->ignoreJobProgress())
            if(KMessageBox::warningContinueCancel(
                        nullptr,
                        i18n("All jobs are complete. Do you want to reset the status of all jobs and restart capturing?"),
                        i18n("Reset job status"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
                        "reset_job_complete_status_warning") != KMessageBox::Continue)
                return nullptr;

        // If the end-user accepted to reset, reset all jobs and restart
        resetAllJobs();

        first_job = m_State->allJobs().first();
    }
    // If we need to ignore job progress, systematically reset all jobs and restart
    // Scheduler will never ignore job progress and doesn't use this path
    else if (m_State->ignoreJobProgress())
    {
        emit newLog(i18n("Warning: option \"Always Reset Sequence When Starting\" is enabled and resets the sequence counts."));
        resetAllJobs();
    }

    return first_job;
}

void Ekos::CaptureProcess::resetJobStatus(JOBStatus newStatus)
{
    if (activeJob() != nullptr)
    {
        activeJob()->resetStatus(newStatus);
        emit updateJobTable(activeJob());
    }
}

void Ekos::CaptureProcess::resetAllJobs()
{
    for (auto &job : m_State->allJobs())
    {
        job->resetStatus();
    }
    // clear existing job counts
    m_State->clearCapturedFramesMap();
    // update the entire job table
    emit updateJobTable(nullptr);
}

void Ekos::CaptureProcess::updatedCaptureCompleted(int count)
{
    activeJob()->setCompleted(count);
    emit updateJobTable(activeJob());
}

void CaptureProcess::llsq(QVector<double> x, QVector<double> y, double &a, double &b)
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

QStringList CaptureProcess::generateScriptArguments() const
{
    // TODO based on user feedback on what paramters are most useful to pass
    return QStringList();
}

bool CaptureProcess::hasCoolerControl()
{
    if (m_DeviceAdaptor->getActiveCamera() && m_DeviceAdaptor->getActiveCamera()->hasCoolerControl())
        return true;

    return false;
}

bool CaptureProcess::setCoolerControl(bool enable)
{
    if (m_DeviceAdaptor->getActiveCamera() && m_DeviceAdaptor->getActiveCamera()->hasCoolerControl())
        return m_DeviceAdaptor->getActiveCamera()->setCoolerControl(enable);

    return false;
}

void CaptureProcess::restartCamera(const QString &name)
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
} // Ekos namespace
