/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "schedulerutils.h"
#include "schedulerjob.h"
#include "schedulermodulestate.h"
#include "ekos/capture/sequencejob.h"
#include "Options.h"
#include "skypoint.h"
#include "kstarsdata.h"
#include <ekos_scheduler_debug.h>

namespace Ekos
{

SchedulerUtils::SchedulerUtils()
{

}

SchedulerJob *SchedulerUtils::createJob(XMLEle *root, SchedulerJob *leadJob)
{
    SchedulerJob *job = new SchedulerJob();
    QString name, group, train;
    bool isLead = true;
    dms ra, dec;
    double rotation = 0.0, minimumAltitude = 0.0, minimumMoonSeparation = -1, maxMoonAltitude = 90.0;
    QUrl sequenceURL, fitsURL;
    StartupCondition startup = START_ASAP;
    CompletionCondition completion = FINISH_SEQUENCE;
    QDateTime startupTime, completionTime;
    int completionRepeats = 0;
    bool enforceTwilight = false, enforceArtificialHorizon = false,
         track = false, focus = false, align = false, guide = false;


    XMLEle *ep, *subEP;
    // We expect all data read from the XML to be in the C locale - QLocale::c()
    QLocale cLocale = QLocale::c();

    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep), "Name"))
            name = pcdataXMLEle(ep);
        else if (!strcmp(tagXMLEle(ep), "JobType"))
        {
            isLead = (QString(findXMLAttValu(ep, "lead")) == "true");
            if (isLead == false)
                job->setLeadJob(leadJob);
        }
        else if (!strcmp(tagXMLEle(ep), "Group"))
            group = pcdataXMLEle(ep);
        else if (!strcmp(tagXMLEle(ep), "OpticalTrain"))
            train = pcdataXMLEle(ep);
        else if (!strcmp(tagXMLEle(ep), "Coordinates"))
        {
            subEP = findXMLEle(ep, "J2000RA");
            if (subEP)
                ra.setH(cLocale.toDouble(pcdataXMLEle(subEP)));

            subEP = findXMLEle(ep, "J2000DE");
            if (subEP)
                dec.setD(cLocale.toDouble(pcdataXMLEle(subEP)));
        }
        else if (!strcmp(tagXMLEle(ep), "Sequence"))
        {
            sequenceURL = QUrl::fromUserInput(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "FITS"))
        {
            fitsURL = QUrl::fromUserInput(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "PositionAngle"))
        {
            rotation = cLocale.toDouble(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "StartupCondition"))
        {
            for (subEP = nextXMLEle(ep, 1); subEP != nullptr; subEP = nextXMLEle(ep, 0))
            {
                if (!strcmp("ASAP", pcdataXMLEle(subEP)))
                    startup = START_ASAP;
                else if (!strcmp("At", pcdataXMLEle(subEP)))
                {
                    startup = START_AT;
                    startupTime = QDateTime::fromString(findXMLAttValu(subEP, "value"), Qt::ISODate);
                    // Todo sterne-jaeger 2024-01-01: setting time spec from KStars necessary?
                }
            }
        }
        else if (!strcmp(tagXMLEle(ep), "Constraints"))
        {
            for (subEP = nextXMLEle(ep, 1); subEP != nullptr; subEP = nextXMLEle(ep, 0))
            {
                if (!strcmp("MinimumAltitude", pcdataXMLEle(subEP)))
                {
                    Options::setEnableAltitudeLimits(true);
                    minimumAltitude = cLocale.toDouble(findXMLAttValu(subEP, "value"));
                }
                else if (!strcmp("MoonSeparation", pcdataXMLEle(subEP)))
                {
                    Options::setSchedulerMoonSeparation(true);
                    minimumMoonSeparation = cLocale.toDouble(findXMLAttValu(subEP, "value"));
                }
                else if (!strcmp("MoonMaxAltitude", pcdataXMLEle(subEP)))
                {
                    Options::setSchedulerMoonAltitude(true);
                    maxMoonAltitude = cLocale.toDouble(findXMLAttValu(subEP, "value"));
                }
                else if (!strcmp("EnforceTwilight", pcdataXMLEle(subEP)))
                    enforceTwilight = true;
                else if (!strcmp("EnforceArtificialHorizon", pcdataXMLEle(subEP)))
                    enforceArtificialHorizon = true;
            }
        }
        else if (!strcmp(tagXMLEle(ep), "CompletionCondition"))
        {
            for (subEP = nextXMLEle(ep, 1); subEP != nullptr; subEP = nextXMLEle(ep, 0))
            {
                if (!strcmp("Sequence", pcdataXMLEle(subEP)))
                    completion = FINISH_SEQUENCE;
                else if (!strcmp("Repeat", pcdataXMLEle(subEP)))
                {
                    completion = FINISH_REPEAT;
                    completionRepeats = cLocale.toInt(findXMLAttValu(subEP, "value"));
                }
                else if (!strcmp("Loop", pcdataXMLEle(subEP)))
                    completion = FINISH_LOOP;
                else if (!strcmp("At", pcdataXMLEle(subEP)))
                {
                    completion = FINISH_AT;
                    completionTime = QDateTime::fromString(findXMLAttValu(subEP, "value"), Qt::ISODate);
                }
            }
        }
        else if (!strcmp(tagXMLEle(ep), "Steps"))
        {
            XMLEle *module;

            for (module = nextXMLEle(ep, 1); module != nullptr; module = nextXMLEle(ep, 0))
            {
                const char *proc = pcdataXMLEle(module);

                if (!strcmp(proc, "Track"))
                    track = true;
                else if (!strcmp(proc, "Focus"))
                    focus = true;
                else if (!strcmp(proc, "Align"))
                    align = true;
                else if (!strcmp(proc, "Guide"))
                    guide = true;
            }
        }
    }
    SchedulerUtils::setupJob(*job, name, isLead, group, train, ra, dec,
                             KStarsData::Instance()->ut().djd(),
                             rotation, sequenceURL, fitsURL,

                             startup, startupTime,
                             completion, completionTime, completionRepeats,

                             minimumAltitude,
                             minimumMoonSeparation,
                             maxMoonAltitude,
                             enforceTwilight,
                             enforceArtificialHorizon,

                             track,
                             focus,
                             align,
                             guide);

    return job;
}

void SchedulerUtils::setupJob(SchedulerJob &job, const QString &name, bool isLead, const QString &group,
                              const QString &train, const dms &ra, const dms &dec, double djd, double rotation, const QUrl &sequenceUrl,
                              const QUrl &fitsUrl, StartupCondition startup, const QDateTime &startupTime, CompletionCondition completion,
                              const QDateTime &completionTime, int completionRepeats, double minimumAltitude, double minimumMoonSeparation,
                              double maxMoonAltitude, bool enforceTwilight, bool enforceArtificialHorizon, bool track, bool focus,
                              bool align, bool guide)
{
    /* Configure or reconfigure the observation job */

    job.setIsLead(isLead);
    job.setOpticalTrain(train);
    job.setPositionAngle(rotation);

    if (isLead)
    {
        job.setName(name);
        job.setGroup(group);
        job.setLeadJob(nullptr);
        // djd should be ut.djd
        job.setTargetCoords(ra, dec, djd);
        job.setFITSFile(fitsUrl);

        // #1 Startup conditions
        job.setStartupCondition(startup);
        if (startup == START_AT)
        {
            job.setStartupTime(startupTime);
        }
        /* Store the original startup condition */
        job.setFileStartupCondition(job.getStartupCondition());
        job.setStartAtTime(job.getStartupTime());

        // #2 Constraints
        job.setMinAltitude(minimumAltitude);
        job.setMinMoonSeparation(minimumMoonSeparation);
        job.setMaxMoonAltitude(maxMoonAltitude);

        // Check enforce weather constraints
        // twilight constraints
        job.setEnforceTwilight(enforceTwilight);
        job.setEnforceArtificialHorizon(enforceArtificialHorizon);

        // Job steps
        job.setStepPipeline(SchedulerJob::USE_NONE);
        if (track)
            job.setStepPipeline(static_cast<SchedulerJob::StepPipeline>(job.getStepPipeline() | SchedulerJob::USE_TRACK));
        if (focus)
            job.setStepPipeline(static_cast<SchedulerJob::StepPipeline>(job.getStepPipeline() | SchedulerJob::USE_FOCUS));
        if (align)
            job.setStepPipeline(static_cast<SchedulerJob::StepPipeline>(job.getStepPipeline() | SchedulerJob::USE_ALIGN));
        if (guide)
            job.setStepPipeline(static_cast<SchedulerJob::StepPipeline>(job.getStepPipeline() | SchedulerJob::USE_GUIDE));

        /* Store the original startup condition */
        job.setFileStartupCondition(job.getStartupCondition());
        job.setStartAtTime(job.getStartupTime());
    }

    /* Consider sequence file is new, and clear captured frames map */
    job.setCapturedFramesMap(CapturedFramesMap());
    job.setSequenceFile(sequenceUrl);
    job.setCompletionCondition(completion);
    if (completion == FINISH_AT)
        job.setFinishAtTime(completionTime);
    else if (completion == FINISH_REPEAT)
    {
        job.setRepeatsRequired(completionRepeats);
        job.setRepeatsRemaining(completionRepeats);
    }

    /* Reset job state to evaluate the changes */
    job.reset();
}

uint16_t SchedulerUtils::fillCapturedFramesMap(const CapturedFramesMap &expected,
        const CapturedFramesMap &capturedFramesCount, SchedulerJob &schedJob, CapturedFramesMap &capture_map,
        int &completedIterations)
{
    uint16_t totalCompletedCount = 0;

    // Figure out which repeat this is for the key with the least progress.
    int minIterationsCompleted = -1, currentIteration = 0;
    if (Options::rememberJobProgress())
    {
        completedIterations = 0;
        for (const QString &key : expected.keys())
        {
            const int iterationsCompleted = capturedFramesCount[key] / expected[key];
            if (minIterationsCompleted == -1 || iterationsCompleted < minIterationsCompleted)
                minIterationsCompleted = iterationsCompleted;
        }
        // If this condition is FINISH_REPEAT, and we've already completed enough iterations
        // Then set the currentIteratiion as 1 more than required. No need to go higher.
        if (schedJob.getCompletionCondition() == FINISH_REPEAT
                && minIterationsCompleted >= schedJob.getRepeatsRequired())
            currentIteration  = schedJob.getRepeatsRequired() + 1;
        else
            // Otherwise set it to one more than the number completed (i.e. the one it'll be working on).
            currentIteration = minIterationsCompleted + 1;
        completedIterations = std::max(0, currentIteration - 1);
    }
    else
        // If we are not remembering progress, we'll only know the iterations completed
        // by the current job's run.
        completedIterations = schedJob.getCompletedIterations();

    for (const QString &key : expected.keys())
    {
        if (Options::rememberJobProgress())
        {
            // If we're remembering progress, then figure out how many captures have not yet been captured.
            const int diff = expected[key] * currentIteration - capturedFramesCount[key];

            // Already captured more than required? Then don't capture any this round.
            if (diff <= 0)
                capture_map[key] = expected[key];
            // Need more captures than one cycle could capture? If so, capture the full amount.
            else if (diff >= expected[key])
                capture_map[key] = 0;
            // Otherwise we know that 0 < diff < expected[key]. Capture just the number needed.
            else
                capture_map[key] = expected[key] - diff;
        }
        else
            // If we are not remembering progress, then the capture module, which reads this
            // Will capture all requirements in the .esq file.
            capture_map[key] = 0;

        // collect all captured frames counts
        if (schedJob.getCompletionCondition() == FINISH_LOOP)
            totalCompletedCount += capturedFramesCount[key];
        else
            totalCompletedCount += std::min(capturedFramesCount[key],
                                            static_cast<uint16_t>(expected[key] * schedJob.getRepeatsRequired()));
    }
    return totalCompletedCount;
}

void SchedulerUtils::updateLightFramesRequired(SchedulerJob * oneJob, const QList<QSharedPointer<SequenceJob> > &seqjobs,
        const CapturedFramesMap &framesCount)
{
    bool lightFramesRequired = false;
    CapturedFramesMap expected;
    switch (oneJob->getCompletionCondition())
    {
        case FINISH_SEQUENCE:
        case FINISH_REPEAT:
            // Step 1: determine expected frames
            SchedulerUtils::calculateExpectedCapturesMap(seqjobs, expected);
            // Step 2: compare with already captured frames
            for (auto oneSeqJob : seqjobs)
            {
                QString const signature = oneSeqJob->getSignature();
                /* If frame is LIGHT, how many do we have left? */
                if (oneSeqJob->getFrameType() == FRAME_LIGHT && expected[signature] * oneJob->getRepeatsRequired() > framesCount[signature])
                {
                    lightFramesRequired = true;
                    // exit the loop, one found is sufficient
                    break;
                }
            }
            break;
        default:
            // in all other cases it does not depend on the number of captured frames
            lightFramesRequired = true;
    }
    oneJob->setLightFramesRequired(lightFramesRequired);
}

QSharedPointer<SequenceJob> SchedulerUtils::processSequenceJobInfo(XMLEle * root, SchedulerJob * schedJob)
{
    QSharedPointer<SequenceJob> job = QSharedPointer<SequenceJob>(new SequenceJob(root, schedJob->getName()));
    if (schedJob)
    {
        if (FRAME_LIGHT == job->getFrameType())
            schedJob->setLightFramesRequired(true);
        if (job->getCalibrationPreAction() & CAPTURE_PREACTION_PARK_MOUNT)
            schedJob->setCalibrationMountPark(true);
    }

    auto placeholderPath = Ekos::PlaceholderPath();
    placeholderPath.processJobInfo(job.get());

    return job;
}

bool SchedulerUtils::loadSequenceQueue(const QString &fileURL, SchedulerJob *schedJob,
                                       QList<QSharedPointer<SequenceJob>> &jobs, bool &hasAutoFocus, ModuleLogger * logger)
{
    QFile sFile;
    sFile.setFileName(fileURL);

    if (!sFile.open(QIODevice::ReadOnly))
    {
        if (logger != nullptr) logger->appendLogText(i18n("Unable to open sequence queue file '%1'", fileURL));
        return false;
    }

    LilXML *xmlParser = newLilXML();
    char errmsg[MAXRBUF];
    XMLEle *root = nullptr;
    XMLEle *ep   = nullptr;
    char c;

    while (sFile.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
        {
            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                if (!strcmp(tagXMLEle(ep), "Autofocus"))
                    hasAutoFocus = (!strcmp(findXMLAttValu(ep, "enabled"), "true"));
                else if (!strcmp(tagXMLEle(ep), "Job"))
                {
                    QSharedPointer<SequenceJob> thisJob = processSequenceJobInfo(ep, schedJob);
                    jobs.append(thisJob);
                    if (jobs.count() == 1)
                    {
                        auto &firstJob = jobs.first();
                        if (FRAME_LIGHT == firstJob->getFrameType() && nullptr != schedJob)
                        {
                            schedJob->setInitialFilter(firstJob->getCoreProperty(SequenceJob::SJ_Filter).toString());
                        }

                    }
                }
            }
            delXMLEle(root);
        }
        else if (errmsg[0])
        {
            if (logger != nullptr) logger->appendLogText(QString(errmsg));
            delLilXML(xmlParser);
            return false;
        }
    }

    return true;
}

bool SchedulerUtils::estimateJobTime(SchedulerJob * schedJob, const CapturedFramesMap &capturedFramesCount,
                                     ModuleLogger * logger)
{
    static SchedulerJob *jobWarned = nullptr;

    // Load the sequence job associated with the argument scheduler job.
    QList<QSharedPointer<SequenceJob>> seqJobs;
    bool hasAutoFocus = false;
    bool result = loadSequenceQueue(schedJob->getSequenceFile().toLocalFile(), schedJob, seqJobs, hasAutoFocus, logger);
    if (result == false)
    {
        qCWarning(KSTARS_EKOS_SCHEDULER) <<
                                         QString("Warning: Failed estimating the duration of job '%1', its sequence file is invalid.").arg(
                                             schedJob->getSequenceFile().toLocalFile());
        return result;
    }

    // FIXME: setting in-sequence focus should be done in XML processing.
    schedJob->setInSequenceFocus(hasAutoFocus);

    // Stop spam of log on re-evaluation. If we display the warning once, then that's it.
    if (schedJob != jobWarned && hasAutoFocus && !(schedJob->getStepPipeline() & SchedulerJob::USE_FOCUS))
    {
        logger->appendLogText(
            i18n("Warning: Job '%1' has its focus step disabled, periodic and/or HFR procedures currently set in its sequence will not occur.",
                 schedJob->getName()));
        jobWarned = schedJob;
    }

    /* This is the map of captured frames for this scheduler job, keyed per storage signature.
     * It will be forwarded to the Capture module in order to capture only what frames are required.
     * If option "Remember Job Progress" is disabled, this map will be empty, and the Capture module will process all requested captures unconditionally.
     */
    CapturedFramesMap capture_map;
    bool const rememberJobProgress = Options::rememberJobProgress();

    double totalImagingTime  = 0;
    double imagingTimePerRepeat = 0, imagingTimeLeftThisRepeat = 0;

    // Determine number of captures in the scheduler job
    CapturedFramesMap expected;
    uint16_t allCapturesPerRepeat = calculateExpectedCapturesMap(seqJobs, expected);

    // fill the captured frames map
    int completedIterations;
    uint16_t totalCompletedCount = fillCapturedFramesMap(expected, capturedFramesCount, *schedJob, capture_map,
                                   completedIterations);
    schedJob->setCompletedIterations(completedIterations);
    // Loop through sequence jobs to calculate the number of required frames and estimate duration.
    foreach (auto seqJob, seqJobs)
    {
        // FIXME: find a way to actually display the filter name.
        QString seqName = i18n("Job '%1' %2x%3\" %4", schedJob->getName(), seqJob->getCoreProperty(SequenceJob::SJ_Count).toInt(),
                               seqJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble(),
                               seqJob->getCoreProperty(SequenceJob::SJ_Filter).toString());

        if (seqJob->getUploadMode() == ISD::Camera::UPLOAD_REMOTE)
        {
            qCInfo(KSTARS_EKOS_SCHEDULER) <<
                                          QString("%1 duration cannot be estimated time since the sequence saves the files remotely.").arg(seqName);
            schedJob->setEstimatedTime(-2);
            return true;
        }

        // Note that looping jobs will have zero repeats required.
        QString const signature      = seqJob->getSignature();
        QString const signature_path = QFileInfo(signature).path();
        int captures_required        = seqJob->getCoreProperty(SequenceJob::SJ_Count).toInt() * schedJob->getRepeatsRequired();
        int captures_completed       = capturedFramesCount[signature];
        const int capturesRequiredPerRepeat = std::max(1, seqJob->getCoreProperty(SequenceJob::SJ_Count).toInt());
        int capturesLeftThisRepeat   = std::max(0, capturesRequiredPerRepeat - (captures_completed % capturesRequiredPerRepeat));
        if (captures_completed >= (1 + completedIterations) * capturesRequiredPerRepeat)
        {
            // Something else is causing this iteration to be incomplete. Nothing left to do for this seqJob.
            capturesLeftThisRepeat = 0;
        }

        if (rememberJobProgress && schedJob->getCompletionCondition() != FINISH_LOOP)
        {
            /* Enumerate sequence jobs associated to this scheduler job, and assign them a completed count.
             *
             * The objective of this block is to fill the storage map of the scheduler job with completed counts for each capture storage.
             *
             * Sequence jobs capture to a storage folder, and are given a count of captures to store at that location.
             * The tricky part is to make sure the repeat count of the scheduler job is properly transferred to each sequence job.
             *
             * For instance, a scheduler job repeated three times must execute the full list of sequence jobs three times, thus
             * has to tell each sequence job it misses all captures, three times. It cannot tell the sequence job three captures are
             * missing, first because that's not how the sequence job is designed (completed count, not required count), and second
             * because this would make the single sequence job repeat three times, instead of repeating the full list of sequence
             * jobs three times.
             *
             * The consolidated storage map will be assigned to each sequence job based on their signature when the scheduler job executes them.
             *
             * For instance, consider a RGBL sequence of single captures. The map will store completed captures for R, G, B and L storages.
             * If R and G have 1 file each, and B and L have no files, map[storage(R)] = map[storage(G)] = 1 and map[storage(B)] = map[storage(L)] = 0.
             * When that scheduler job executes, only B and L captures will be processed.
             *
             * In the case of a RGBLRGB sequence of single captures, the second R, G and B map items will count one less capture than what is really in storage.
             * If R and G have 1 file each, and B and L have no files, map[storage(R1)] = map[storage(B1)] = 1, and all others will be 0.
             * When that scheduler job executes, B1, L, R2, G2 and B2 will be processed.
             *
             * This doesn't handle the case of duplicated scheduler jobs, that is, scheduler jobs with the same storage for capture sets.
             * Those scheduler jobs will all change state to completion at the same moment as they all target the same storage.
             * This is why it is important to manage the repeat count of the scheduler job, as stated earlier.
             */

            captures_required = expected[seqJob->getSignature()] * schedJob->getRepeatsRequired();

            qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 sees %2 captures in output folder '%3'.").arg(seqName).arg(
                                              captures_completed).arg(QFileInfo(signature).path());

            // Enumerate sequence jobs to check how many captures are completed overall in the same storage as the current one
            foreach (auto prevSeqJob, seqJobs)
            {
                // Enumerate seqJobs up to the current one
                if (seqJob == prevSeqJob)
                    break;

                // If the previous sequence signature matches the current, skip counting to take duplicates into account
                if (!signature.compare(prevSeqJob->getSignature()))
                    captures_required = 0;

                // And break if no captures remain, this job does not need to be executed
                if (captures_required == 0)
                    break;
            }

            qCDebug(KSTARS_EKOS_SCHEDULER) << QString("%1 has completed %2/%3 of its required captures in output folder '%4'.").arg(
                                               seqName).arg(captures_completed).arg(captures_required).arg(signature_path);

        }
        // Else rely on the captures done during this session
        else if (0 < allCapturesPerRepeat)
        {
            captures_completed = schedJob->getCompletedCount() / allCapturesPerRepeat * seqJob->getCoreProperty(
                                     SequenceJob::SJ_Count).toInt();
        }
        else
        {
            captures_completed = 0;
        }

        // Check if we still need any light frames. Because light frames changes the flow of the observatory startup
        // Without light frames, there is no need to do focusing, alignment, guiding...etc
        // We check if the frame type is LIGHT and if either the number of captures_completed frames is less than required
        // OR if the completion condition is set to LOOP so it is never complete due to looping.
        // Note that looping jobs will have zero repeats required.
        // FIXME: As it is implemented now, FINISH_LOOP may loop over a capture-complete, therefore inoperant, scheduler job.
        bool const areJobCapturesComplete = (0 == captures_required || captures_completed >= captures_required);
        if (seqJob->getFrameType() == FRAME_LIGHT)
        {
            if(areJobCapturesComplete)
            {
                qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 completed its sequence of %2 light frames.").arg(seqName).arg(
                                                  captures_required);
            }
        }
        else
        {
            qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 captures calibration frames.").arg(seqName);
        }

        /* If captures are not complete, we have imaging time left */
        if (!areJobCapturesComplete || schedJob->getCompletionCondition() == FINISH_LOOP)
        {
            unsigned int const captures_to_go = captures_required - captures_completed;
            const double secsPerCapture = (seqJob->getCoreProperty(SequenceJob::SJ_Exposure).toDouble() +
                                           (seqJob->getCoreProperty(SequenceJob::SJ_Delay).toInt() / 1000.0));
            totalImagingTime += fabs(secsPerCapture * captures_to_go);
            imagingTimePerRepeat += fabs(secsPerCapture * seqJob->getCoreProperty(SequenceJob::SJ_Count).toInt());
            imagingTimeLeftThisRepeat += fabs(secsPerCapture * capturesLeftThisRepeat);
            /* If we have light frames to process, add focus/dithering delay */
            if (seqJob->getFrameType() == FRAME_LIGHT)
            {
                // If inSequenceFocus is true
                if (hasAutoFocus)
                {
                    // Wild guess, 10s of autofocus for each capture required. Can vary a lot, but this is just a completion estimate.
                    constexpr int afSecsPerCapture = 10;
                    qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 requires a focus procedure.").arg(seqName);
                    totalImagingTime += captures_to_go * afSecsPerCapture;
                    imagingTimePerRepeat += capturesRequiredPerRepeat * afSecsPerCapture;
                    imagingTimeLeftThisRepeat += capturesLeftThisRepeat * afSecsPerCapture;
                }
                // If we're dithering after each exposure, that's another 10-20 seconds
                if (schedJob->getStepPipeline() & SchedulerJob::USE_GUIDE && Options::ditherEnabled())
                {
                    constexpr int ditherSecs = 15;
                    qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 requires a dither procedure.").arg(seqName);
                    totalImagingTime += (captures_to_go * ditherSecs) / Options::ditherFrames();
                    imagingTimePerRepeat += (capturesRequiredPerRepeat * ditherSecs) / Options::ditherFrames();
                    imagingTimeLeftThisRepeat += (capturesLeftThisRepeat * ditherSecs) / Options::ditherFrames();
                }
            }
        }
    }

    schedJob->setCapturedFramesMap(capture_map);
    schedJob->setSequenceCount(allCapturesPerRepeat * schedJob->getRepeatsRequired());

    // only in case we remember the job progress, we change the completion count
    if (rememberJobProgress)
        schedJob->setCompletedCount(totalCompletedCount);

    schedJob->setEstimatedTimePerRepeat(imagingTimePerRepeat);
    schedJob->setEstimatedTimeLeftThisRepeat(imagingTimeLeftThisRepeat);
    if (schedJob->getLightFramesRequired())
        schedJob->setEstimatedStartupTime(timeHeuristics(schedJob));

    // FIXME: Move those ifs away to the caller in order to avoid estimating in those situations!

    // We can't estimate times that do not finish when sequence is done
    if (schedJob->getCompletionCondition() == FINISH_LOOP)
    {
        // We can't know estimated time if it is looping indefinitely
        schedJob->setEstimatedTime(-2);
        qCDebug(KSTARS_EKOS_SCHEDULER) <<
                                       QString("Job '%1' is configured to loop until Scheduler is stopped manually, has undefined imaging time.")
                                       .arg(schedJob->getName());
    }
    // If we know startup and finish times, we can estimate time right away
    else if (schedJob->getStartupCondition() == START_AT &&
             schedJob->getCompletionCondition() == FINISH_AT)
    {
        // FIXME: SchedulerJob is probably doing this already
        qint64 const diff = schedJob->getStartupTime().secsTo(schedJob->getFinishAtTime());
        schedJob->setEstimatedTime(diff);

        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' has a startup time and fixed completion time, will run for %2.")
                                       .arg(schedJob->getName())
                                       .arg(dms(diff * 15.0 / 3600.0f).toHMSString());
    }
    // If we know finish time only, we can roughly estimate the time considering the job starts now
    else if (schedJob->getStartupCondition() != START_AT &&
             schedJob->getCompletionCondition() == FINISH_AT)
    {
        qint64 const diff = SchedulerModuleState::getLocalTime().secsTo(schedJob->getFinishAtTime());
        schedJob->setEstimatedTime(diff);
        qCDebug(KSTARS_EKOS_SCHEDULER) <<
                                       QString("Job '%1' has no startup time but fixed completion time, will run for %2 if started now.")
                                       .arg(schedJob->getName())
                                       .arg(dms(diff * 15.0 / 3600.0f).toHMSString());
    }
    // Rely on the estimated imaging time to determine whether this job is complete or not - this makes the estimated time null
    else if (totalImagingTime <= 0)
    {
        schedJob->setEstimatedTime(0);
        schedJob->setEstimatedTimePerRepeat(1);
        schedJob->setEstimatedTimeLeftThisRepeat(0);

        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' will not run, complete with %2/%3 captures.")
                                       .arg(schedJob->getName()).arg(schedJob->getCompletedCount()).arg(schedJob->getSequenceCount());
    }
    // Else consolidate with step durations
    else
    {
        if (schedJob->getLightFramesRequired())
        {
            totalImagingTime += timeHeuristics(schedJob);
            schedJob->setEstimatedStartupTime(timeHeuristics(schedJob));
        }
        dms const estimatedTime(totalImagingTime * 15.0 / 3600.0);
        schedJob->setEstimatedTime(std::ceil(totalImagingTime));

        qCInfo(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' estimated to take %2 to complete.").arg(schedJob->getName(),
                                      estimatedTime.toHMSString());
    }

    return true;
}

int SchedulerUtils::timeHeuristics(const SchedulerJob * schedJob)
{
    double imagingTime = 0;
    /* FIXME: estimation should base on actual measure of each step, eventually with preliminary data as what it used now */
    // Are we doing tracking? It takes about 30 seconds
    if (schedJob->getStepPipeline() & SchedulerJob::USE_TRACK)
        imagingTime += 30;
    // Are we doing initial focusing? That can take about 2 minutes
    if (schedJob->getStepPipeline() & SchedulerJob::USE_FOCUS)
        imagingTime += 120;
    // Are we doing astrometry? That can take about 60 seconds
    if (schedJob->getStepPipeline() & SchedulerJob::USE_ALIGN)
    {
        imagingTime += 60;
    }
    // Are we doing guiding?
    if (schedJob->getStepPipeline() & SchedulerJob::USE_GUIDE)
    {
        // Looping, finding guide star, settling takes 15 sec
        imagingTime += 15;

        // Add guiding settle time from dither setting (used by phd2::guide())
        imagingTime += Options::ditherSettle();
        // Add guiding settle time from ekos sccheduler setting
        imagingTime += Options::guidingSettle();

        // If calibration always cleared
        // then calibration process can take about 2 mins
        if(Options::resetGuideCalibration())
            imagingTime += 120;
    }
    return imagingTime;

}

uint16_t SchedulerUtils::calculateExpectedCapturesMap(const QList<QSharedPointer<SequenceJob>> &seqJobs,
        CapturedFramesMap &expected)
{
    uint16_t capturesPerRepeat = 0;
    for (auto &seqJob : seqJobs)
    {
        capturesPerRepeat += seqJob->getCoreProperty(SequenceJob::SJ_Count).toInt();
        QString signature = seqJob->getCoreProperty(SequenceJob::SJ_Signature).toString();
        expected[signature] = static_cast<uint16_t>(seqJob->getCoreProperty(SequenceJob::SJ_Count).toInt()) + (expected.contains(
                                  signature) ? expected[signature] : 0);
    }
    return capturesPerRepeat;
}

double SchedulerUtils::findAltitude(const SkyPoint &target, const QDateTime &when, bool * is_setting,
                                    GeoLocation *geo, bool debug)
{
    // FIXME: block calculating target coordinates at a particular time is duplicated in several places

    auto geoLocation = (geo == nullptr) ? SchedulerModuleState::getGeo() : geo;

    // Retrieve the argument date/time, or fall back to current time - don't use QDateTime's timezone!
    KStarsDateTime ltWhen(when.isValid() ?
                          Qt::UTC == when.timeSpec() ? geoLocation->UTtoLT(KStarsDateTime(when)) : when :
                          SchedulerModuleState::getLocalTime());

    // Create a sky object with the target catalog coordinates
    SkyObject o;
    o.setRA0(target.ra0());
    o.setDec0(target.dec0());

    // Update RA/DEC of the target for the current fraction of the day
    KSNumbers numbers(ltWhen.djd());
    o.updateCoordsNow(&numbers);

    // Calculate alt/az coordinates using KStars instance's geolocation
    CachingDms const LST = geoLocation->GSTtoLST(geoLocation->LTtoUT(ltWhen).gst());
    o.EquatorialToHorizontal(&LST, geoLocation->lat());

    // Hours are reduced to [0,24[, meridian being at 0
    double offset = LST.Hours() - o.ra().Hours();
    if (24.0 <= offset)
        offset -= 24.0;
    else if (offset < 0.0)
        offset += 24.0;
    bool const passed_meridian = 0.0 <= offset && offset < 12.0;

    if (debug)
        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("When:%9 LST:%8 RA:%1 RA0:%2 DEC:%3 DEC0:%4 alt:%5 setting:%6 HA:%7")
                                       .arg(o.ra().toHMSString())
                                       .arg(o.ra0().toHMSString())
                                       .arg(o.dec().toHMSString())
                                       .arg(o.dec0().toHMSString())
                                       .arg(o.alt().Degrees())
                                       .arg(passed_meridian ? "yes" : "no")
                                       .arg(o.ra().Hours())
                                       .arg(LST.toHMSString())
                                       .arg(ltWhen.toString("HH:mm:ss"));

    if (is_setting)
        *is_setting = passed_meridian;

    return o.alt().Degrees();
}

QList<SchedulerJob *> SchedulerUtils::filterLeadJobs(const QList<SchedulerJob *> &jobs)
{
    QList<SchedulerJob *> result;
    for (auto job : jobs)
        if (job->isLead())
            result.append(job);

    return result;
}

} // namespace
