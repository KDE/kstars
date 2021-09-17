/*  Ekos Scheduler Job
    SPDX-FileCopyrightText: Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "schedulerjob.h"

#include "dms.h"
#include "artificialhorizoncomponent.h"
#include "kstarsdata.h"
#include "skymapcomposite.h"
#include "Options.h"
#include "scheduler.h"
#include "ksalmanac.h"

#include <knotification.h>

#include <QTableWidgetItem>

#include <ekos_scheduler_debug.h>

#define BAD_SCORE -1000
#define MIN_ALTITUDE 15.0


GeoLocation *SchedulerJob::storedGeo = nullptr;
KStarsDateTime *SchedulerJob::storedLocalTime = nullptr;
ArtificialHorizon *SchedulerJob::storedHorizon = nullptr;

SchedulerJob::SchedulerJob()
{
    moon = dynamic_cast<KSMoon *>(KStarsData::Instance()->skyComposite()->findByName(i18n("Moon")));
}

// Private constructor for unit testing.
SchedulerJob::SchedulerJob(KSMoon *moonPtr)
{
    moon = moonPtr;
}

void SchedulerJob::setName(const QString &value)
{
    name = value;
    updateJobCells();
}

KStarsDateTime SchedulerJob::getLocalTime()
{
    if (hasLocalTime())
        return *storedLocalTime;
    return getGeo()->UTtoLT(KStarsData::Instance()->clock()->utc());
}

GeoLocation const *SchedulerJob::getGeo()
{
    if (hasGeo())
        return storedGeo;
    return KStarsData::Instance()->geo();
}

ArtificialHorizon const *SchedulerJob::getHorizon()
{
    if (hasHorizon())
        return storedHorizon;
    if (KStarsData::Instance() == nullptr || KStarsData::Instance()->skyComposite() == nullptr
            || KStarsData::Instance()->skyComposite()->artificialHorizon() == nullptr)
        return nullptr;
    return &KStarsData::Instance()->skyComposite()->artificialHorizon()->getHorizon();
}

void SchedulerJob::setStartupCondition(const StartupCondition &value)
{
    startupCondition = value;

    /* Keep startup time and condition valid */
    if (value == START_ASAP)
        startupTime = QDateTime();

    /* Refresh estimated time - which update job cells */
    setEstimatedTime(estimatedTime);

    /* Refresh dawn and dusk for startup date */
    calculateDawnDusk(startupTime, nextDawn, nextDusk);
}

void SchedulerJob::setStartupTime(const QDateTime &value)
{
    startupTime = value;

    /* Keep startup time and condition valid */
    if (value.isValid())
        startupCondition = START_AT;
    else
        startupCondition = fileStartupCondition;

    // Refresh altitude - invalid date/time is taken care of when rendering
    altitudeAtStartup = findAltitude(targetCoords, startupTime, &isSettingAtStartup);

    /* Refresh estimated time - which update job cells */
    setEstimatedTime(estimatedTime);

    /* Refresh dawn and dusk for startup date */
    calculateDawnDusk(startupTime, nextDawn, nextDusk);
}

void SchedulerJob::setSequenceFile(const QUrl &value)
{
    sequenceFile = value;
}

void SchedulerJob::setFITSFile(const QUrl &value)
{
    fitsFile = value;
}

void SchedulerJob::setMinAltitude(const double &value)
{
    minAltitude = value;
}

bool SchedulerJob::hasAltitudeConstraint() const
{
    return hasMinAltitude() ||
           (enforceArtificialHorizon && (getHorizon() != nullptr) && getHorizon()->altitudeConstraintsExist());
}

void SchedulerJob::setMinMoonSeparation(const double &value)
{
    minMoonSeparation = value;
}

void SchedulerJob::setEnforceWeather(bool value)
{
    enforceWeather = value;
}

void SchedulerJob::setCompletionTime(const QDateTime &value)
{
    /* If completion time is valid, automatically switch condition to FINISH_AT */
    if (value.isValid())
    {
        setCompletionCondition(FINISH_AT);
        completionTime = value;
        altitudeAtCompletion = findAltitude(targetCoords, completionTime, &isSettingAtCompletion);
        setEstimatedTime(-1);
    }
    /* If completion time is invalid, and job is looping, keep completion time undefined */
    else if (FINISH_LOOP == completionCondition)
    {
        completionTime = QDateTime();
        altitudeAtCompletion = findAltitude(targetCoords, completionTime, &isSettingAtCompletion);
        setEstimatedTime(-1);
    }
    /* If completion time is invalid, deduce completion from startup and duration */
    else if (startupTime.isValid())
    {
        completionTime = startupTime.addSecs(estimatedTime);
        altitudeAtCompletion = findAltitude(targetCoords, completionTime, &isSettingAtCompletion);
        updateJobCells();
    }
    /* Else just refresh estimated time - which update job cells */
    else setEstimatedTime(estimatedTime);


    /* Invariants */
    Q_ASSERT_X(completionTime.isValid() ?
               (FINISH_AT == completionCondition || FINISH_REPEAT == completionCondition || FINISH_SEQUENCE == completionCondition) :
               FINISH_LOOP == completionCondition,
               __FUNCTION__, "Valid completion time implies job is FINISH_AT/REPEAT/SEQUENCE, else job is FINISH_LOOP.");
}

void SchedulerJob::setCompletionCondition(const CompletionCondition &value)
{
    completionCondition = value;

    // Update repeats requirement, looping jobs have none
    switch (completionCondition)
    {
        case FINISH_LOOP:
            setCompletionTime(QDateTime());
        /* Fall through */
        case FINISH_AT:
            if (0 < getRepeatsRequired())
                setRepeatsRequired(0);
            break;

        case FINISH_SEQUENCE:
            if (1 != getRepeatsRequired())
                setRepeatsRequired(1);
            break;

        case FINISH_REPEAT:
            if (0 == getRepeatsRequired())
                setRepeatsRequired(1);
            break;

        default:
            break;
    }

    updateJobCells();
}

void SchedulerJob::setStepPipeline(const StepPipeline &value)
{
    stepPipeline = value;
}

void SchedulerJob::setState(const JOBStatus &value)
{
    state = value;

    /* FIXME: move this to Scheduler, SchedulerJob is mostly a model */
    if (JOB_ERROR == state)
        KNotification::event(QLatin1String("EkosSchedulerJobFail"), i18n("Ekos job failed (%1)", getName()));

    /* If job becomes invalid or idle, automatically reset its startup characteristics, and force its duration to be reestimated */
    if (JOB_INVALID == value || JOB_IDLE == value)
    {
        setStartupCondition(fileStartupCondition);
        setStartupTime(fileStartupTime);
        setEstimatedTime(-1);
    }

    /* If job is aborted, automatically reset its startup characteristics */
    if (JOB_ABORTED == value)
    {
        setStartupCondition(fileStartupCondition);
        /* setStartupTime(fileStartupTime); */
    }

    updateJobCells();
}

void SchedulerJob::setLeadTime(const int64_t &value)
{
    leadTime = value;
    updateJobCells();
}

void SchedulerJob::setScore(int value)
{
    score = value;
    updateJobCells();
}

void SchedulerJob::setCulminationOffset(const int16_t &value)
{
    culminationOffset = value;
}

void SchedulerJob::setSequenceCount(const int count)
{
    sequenceCount = count;
    updateJobCells();
}

void SchedulerJob::setNameCell(QTableWidgetItem *value)
{
    nameCell = value;
}

void SchedulerJob::setCompletedCount(const int count)
{
    completedCount = count;
    updateJobCells();
}

void SchedulerJob::setStatusCell(QTableWidgetItem *value)
{
    statusCell = value;
    if (nullptr != statusCell)
        statusCell->setToolTip(i18n("Current status of job '%1', managed by the Scheduler.\n"
                                    "If invalid, the Scheduler was not able to find a proper observation time for the target.\n"
                                    "If aborted, the Scheduler missed the scheduled time or encountered transitory issues and will reschedule the job.\n"
                                    "If complete, the Scheduler verified that all sequence captures requested were stored, including repeats.",
                                    name));
}

void SchedulerJob::setAltitudeCell(QTableWidgetItem *value)
{
    altitudeCell = value;
    if (nullptr != altitudeCell)
        altitudeCell->setToolTip(i18n("Current altitude of the target of job '%1'.\n"
                                      "A rising target is indicated with an arrow going up.\n"
                                      "A setting target is indicated with an arrow going down.",
                                      name));
}

void SchedulerJob::setStartupCell(QTableWidgetItem *value)
{
    startupCell = value;
    if (nullptr != startupCell)
        startupCell->setToolTip(i18n("Startup time for job '%1', as estimated by the Scheduler.\n"
                                     "The altitude at startup, if available, is displayed too.\n"
                                     "Fixed time from user or culmination time is marked with a chronometer symbol. ",
                                     name));
}

void SchedulerJob::setCompletionCell(QTableWidgetItem *value)
{
    completionCell = value;
    if (nullptr != completionCell)
        completionCell->setToolTip(i18n("Completion time for job '%1', as estimated by the Scheduler.\n"
                                        "You may specify a fixed time to limit duration of looping jobs. "
                                        "A warning symbol indicates the altitude at completion may cause the job to abort before completion.\n",
                                        name));
}

void SchedulerJob::setCaptureCountCell(QTableWidgetItem *value)
{
    captureCountCell = value;
    if (nullptr != captureCountCell)
        captureCountCell->setToolTip(i18n("Count of captures stored for job '%1', based on its sequence job.\n"
                                          "This is a summary, additional specific frame types may be required to complete the job.",
                                          name));
}

void SchedulerJob::setScoreCell(QTableWidgetItem *value)
{
    scoreCell = value;
    if (nullptr != scoreCell)
        scoreCell->setToolTip(i18n("Current score for job '%1', from its altitude, moon separation and sky darkness.\n"
                                   "Negative if adequate altitude is not achieved yet or if there is no proper observation time today.\n"
                                   "The Scheduler will refresh scores when picking a new candidate job.",
                                   name));
}

void SchedulerJob::setLeadTimeCell(QTableWidgetItem *value)
{
    leadTimeCell = value;
    if (nullptr != leadTimeCell)
        leadTimeCell->setToolTip(i18n("Time interval from the job which precedes job '%1'.\n"
                                      "Adjust the Lead Time in Ekos options to increase that duration and leave time for jobs to complete.\n"
                                      "Rearrange jobs to minimize that duration and optimize your imaging time.",
                                      name));
}

void SchedulerJob::setDateTimeDisplayFormat(const QString &value)
{
    dateTimeDisplayFormat = value;
    updateJobCells();
}

void SchedulerJob::setStage(const JOBStage &value)
{
    stage = value;
    updateJobCells();
}

void SchedulerJob::setStageCell(QTableWidgetItem *cell)
{
    stageCell = cell;
    // FIXME: Add a tool tip if cell is used
}

void SchedulerJob::setStageLabel(QLabel *label)
{
    stageLabel = label;
}

void SchedulerJob::setFileStartupCondition(const StartupCondition &value)
{
    fileStartupCondition = value;
}

void SchedulerJob::setFileStartupTime(const QDateTime &value)
{
    fileStartupTime = value;
}

void SchedulerJob::setEstimatedTime(const int64_t &value)
{
    /* Estimated time is generally the difference between startup and completion times:
     * - It is fixed when startup and completion times are fixed, that is, we disregard the argument
     * - Else mostly it pushes completion time from startup time
     *
     * However it cannot advance startup time when completion time is fixed because of the way jobs are scheduled.
     * This situation requires a warning in the user interface when there is not enough time for the job to process.
     */

    /* If startup and completion times are fixed, estimated time cannot change - disregard the argument */
    if (START_ASAP != fileStartupCondition && FINISH_AT == completionCondition)
    {
        estimatedTime = startupTime.secsTo(completionTime);
    }
    /* If completion time isn't fixed, estimated time adjusts completion time */
    else if (FINISH_AT != completionCondition && FINISH_LOOP != completionCondition)
    {
        estimatedTime = value;
        completionTime = startupTime.addSecs(value);
        altitudeAtCompletion = findAltitude(targetCoords, completionTime, &isSettingAtCompletion);
    }
    /* Else estimated time is simply stored as is - covers FINISH_LOOP from setCompletionTime */
    else estimatedTime = value;

    updateJobCells();
}

void SchedulerJob::setInSequenceFocus(bool value)
{
    inSequenceFocus = value;
}

void SchedulerJob::setPriority(const uint8_t &value)
{
    priority = value;
}

void SchedulerJob::setEnforceTwilight(bool value)
{
    enforceTwilight = value;
    calculateDawnDusk(startupTime, nextDawn, nextDusk);
}

void SchedulerJob::setEnforceArtificialHorizon(bool value)
{
    enforceArtificialHorizon = value;
}

void SchedulerJob::setEstimatedTimeCell(QTableWidgetItem *value)
{
    estimatedTimeCell = value;
    if (estimatedTimeCell)
        estimatedTimeCell->setToolTip(i18n("Duration job '%1' will take to complete when started, as estimated by the Scheduler.\n"
                                           "Depends on the actions to be run, and the sequence job to be processed.",
                                           name));
}

void SchedulerJob::setLightFramesRequired(bool value)
{
    lightFramesRequired = value;
}

void SchedulerJob::setRepeatsRequired(const uint16_t &value)
{
    repeatsRequired = value;

    // Update completion condition to be compatible
    if (1 < repeatsRequired)
    {
        if (FINISH_REPEAT != completionCondition)
            setCompletionCondition(FINISH_REPEAT);
    }
    else if (0 < repeatsRequired)
    {
        if (FINISH_SEQUENCE != completionCondition)
            setCompletionCondition(FINISH_SEQUENCE);
    }
    else
    {
        if (FINISH_LOOP != completionCondition)
            setCompletionCondition(FINISH_LOOP);
    }

    updateJobCells();
}

void SchedulerJob::setRepeatsRemaining(const uint16_t &value)
{
    repeatsRemaining = value;
    updateJobCells();
}

void SchedulerJob::setCapturedFramesMap(const CapturedFramesMap &value)
{
    capturedFramesMap = value;
}

void SchedulerJob::setTargetCoords(const dms &ra, const dms &dec, double djd)
{
    targetCoords.setRA0(ra);
    targetCoords.setDec0(dec);

    targetCoords.apparentCoord(static_cast<long double>(J2000), djd);
}

void SchedulerJob::setRotation(double value)
{
    rotation = value;
}

void SchedulerJob::updateJobCells()
{
    if (nullptr != nameCell)
    {
        nameCell->setText(name);
        if (nullptr != nameCell)
            nameCell->tableWidget()->resizeColumnToContents(nameCell->column());
    }

    if (nullptr != nameLabel)
    {
        nameLabel->setText(name + QString(":"));
    }

    if (nullptr != statusCell)
    {
        static QMap<JOBStatus, QString> stateStrings;
        static QString stateStringUnknown;
        if (stateStrings.isEmpty())
        {
            stateStrings[JOB_IDLE] = i18n("Idle");
            stateStrings[JOB_EVALUATION] = i18n("Evaluating");
            stateStrings[JOB_SCHEDULED] = i18n("Scheduled");
            stateStrings[JOB_BUSY] = i18n("Running");
            stateStrings[JOB_INVALID] = i18n("Invalid");
            stateStrings[JOB_COMPLETE] = i18n("Complete");
            stateStrings[JOB_ABORTED] = i18n("Aborted");
            stateStrings[JOB_ERROR] =  i18n("Error");
            stateStringUnknown = i18n("Unknown");
        }
        statusCell->setText(stateStrings.value(state, stateStringUnknown));

        if (nullptr != statusCell->tableWidget())
            statusCell->tableWidget()->resizeColumnToContents(statusCell->column());
    }

    if (nullptr != stageCell || nullptr != stageLabel)
    {
        /* Translated string cache - overkill, probably, and doesn't warn about missing enums like switch/case should ; also, not thread-safe */
        /* FIXME: this should work with a static initializer in C++11, but QT versions are touchy on this, and perhaps i18n can't be used? */
        static QMap<JOBStage, QString> stageStrings;
        static QString stageStringUnknown;
        if (stageStrings.isEmpty())
        {
            stageStrings[STAGE_IDLE] = i18n("Idle");
            stageStrings[STAGE_SLEWING] = i18n("Slewing");
            stageStrings[STAGE_SLEW_COMPLETE] = i18n("Slew complete");
            stageStrings[STAGE_FOCUSING] =
                stageStrings[STAGE_POSTALIGN_FOCUSING] = i18n("Focusing");
            stageStrings[STAGE_FOCUS_COMPLETE] =
                stageStrings[STAGE_POSTALIGN_FOCUSING_COMPLETE ] = i18n("Focus complete");
            stageStrings[STAGE_ALIGNING] = i18n("Aligning");
            stageStrings[STAGE_ALIGN_COMPLETE] = i18n("Align complete");
            stageStrings[STAGE_RESLEWING] = i18n("Repositioning");
            stageStrings[STAGE_RESLEWING_COMPLETE] = i18n("Repositioning complete");
            /*stageStrings[STAGE_CALIBRATING] = i18n("Calibrating");*/
            stageStrings[STAGE_GUIDING] = i18n("Guiding");
            stageStrings[STAGE_GUIDING_COMPLETE] = i18n("Guiding complete");
            stageStrings[STAGE_CAPTURING] = i18n("Capturing");
            stageStringUnknown = i18n("Unknown");
        }
        if (nullptr != stageCell)
        {
            stageCell->setText(stageStrings.value(stage, stageStringUnknown));
            if (nullptr != stageCell->tableWidget())
                stageCell->tableWidget()->resizeColumnToContents(stageCell->column());
        }
        if (nullptr != stageLabel)
        {
            stageLabel->setText(QString("%1: %2").arg(name, stageStrings.value(stage, stageStringUnknown)));
        }
    }

    if (nullptr != startupCell)
    {
        /* Display startup time if it is valid */
        if (startupTime.isValid())
        {
            startupCell->setText(QString("%1%2%L3° %4")
                                 .arg(altitudeAtStartup < minAltitude ? QString(QChar(0x26A0)) : "")
                                 .arg(QChar(isSettingAtStartup ? 0x2193 : 0x2191))
                                 .arg(altitudeAtStartup, 0, 'f', 1)
                                 .arg(startupTime.toString(dateTimeDisplayFormat)));

            switch (fileStartupCondition)
            {
                /* If the original condition is START_AT/START_CULMINATION, startup time is fixed */
                case START_AT:
                case START_CULMINATION:
                    startupCell->setIcon(QIcon::fromTheme("chronometer"));
                    break;

                /* If the original condition is START_ASAP, startup time is informational */
                case START_ASAP:
                    startupCell->setIcon(QIcon());
                    break;

                default:
                    break;
            }
        }
        /* Else do not display any startup time */
        else
        {
            startupCell->setText("-");
            startupCell->setIcon(QIcon());
        }

        if (nullptr != startupCell->tableWidget())
            startupCell->tableWidget()->resizeColumnToContents(startupCell->column());
    }

    if (nullptr != altitudeCell)
    {
        // FIXME: Cache altitude calculations
        bool is_setting = false;
        double const alt = findAltitude(targetCoords, QDateTime(), &is_setting);

        altitudeCell->setText(QString("%1%L2°")
                              .arg(QChar(is_setting ? 0x2193 : 0x2191))
                              .arg(alt, 0, 'f', 1));

        if (nullptr != altitudeCell->tableWidget())
            altitudeCell->tableWidget()->resizeColumnToContents(altitudeCell->column());
    }

    if (nullptr != completionCell)
    {
        /* Display completion time if it is valid and job is not looping */
        if (FINISH_LOOP != completionCondition && completionTime.isValid())
        {
            completionCell->setText(QString("%1%2%L3° %4")
                                    .arg(altitudeAtCompletion < minAltitude ? QString(QChar(0x26A0)) : "")
                                    .arg(QChar(isSettingAtCompletion ? 0x2193 : 0x2191))
                                    .arg(altitudeAtCompletion, 0, 'f', 1)
                                    .arg(completionTime.toString(dateTimeDisplayFormat)));

            switch (completionCondition)
            {
                case FINISH_AT:
                    completionCell->setIcon(QIcon::fromTheme("chronometer"));
                    break;

                case FINISH_SEQUENCE:
                case FINISH_REPEAT:
                default:
                    completionCell->setIcon(QIcon());
                    break;
            }
        }
        /* Else do not display any completion time */
        else
        {
            completionCell->setText("-");
            completionCell->setIcon(QIcon());
        }

        if (nullptr != completionCell->tableWidget())
            completionCell->tableWidget()->resizeColumnToContents(completionCell->column());
    }

    if (nullptr != estimatedTimeCell)
    {
        if (0 < estimatedTime)
            /* Seconds to ms - this doesn't follow dateTimeDisplayFormat, which renders YMD too */
            estimatedTimeCell->setText(QTime::fromMSecsSinceStartOfDay(estimatedTime * 1000).toString("HH:mm:ss"));
#if 0
        else if(0 == estimatedTime)
            /* FIXME: this special case could be merged with the previous, kept for future to indicate actual duration */
            estimatedTimeCell->setText("00:00:00");
#endif
        else
            /* Invalid marker */
            estimatedTimeCell->setText("-");

        /* Warn the end-user if estimated time doesn't fit in the startup/completion interval */
        if (estimatedTime < startupTime.secsTo(completionTime))
            estimatedTimeCell->setIcon(QIcon::fromTheme("document-find"));
        else
            estimatedTimeCell->setIcon(QIcon());

        if (nullptr != estimatedTimeCell->tableWidget())
            estimatedTimeCell->tableWidget()->resizeColumnToContents(estimatedTimeCell->column());
    }

    if (nullptr != captureCountCell)
    {
        switch (completionCondition)
        {
            case FINISH_AT:
            // FIXME: Attempt to calculate the number of frames until end - requires detailed imaging time

            case FINISH_LOOP:
                // If looping, display the count of completed frames
                captureCountCell->setText(QString("%L1/-").arg(completedCount));
                break;

            case FINISH_SEQUENCE:
            case FINISH_REPEAT:
            default:
                // If repeating, display the count of completed frames to the count of requested frames
                captureCountCell->setText(QString("%L1/%L2").arg(completedCount).arg(sequenceCount));
                break;
        }

        if (nullptr != captureCountCell->tableWidget())
            captureCountCell->tableWidget()->resizeColumnToContents(captureCountCell->column());
    }

    if (nullptr != scoreCell)
    {
        if (0 <= score)
            scoreCell->setText(QString("%L1").arg(score));
        else
            /* FIXME: negative scores are just weird for the end-user */
            scoreCell->setText("<0");

        if (nullptr != scoreCell->tableWidget())
            scoreCell->tableWidget()->resizeColumnToContents(scoreCell->column());
    }

    if (nullptr != leadTimeCell)
    {
        // Display lead time, plus a warning if lead time is more than twice the lead time of the Ekos options
        switch (state)
        {
            case JOB_INVALID:
            case JOB_ERROR:
            case JOB_COMPLETE:
                leadTimeCell->setText("-");
                break;

            default:
                leadTimeCell->setText(QString("%1%2")
                                      .arg(Options::leadTime() * 60 * 2 < leadTime ? QString(QChar(0x26A0)) : "")
                                      .arg(QTime::fromMSecsSinceStartOfDay(leadTime * 1000).toString("HH:mm:ss")));
                break;
        }

        if (nullptr != leadTimeCell->tableWidget())
            leadTimeCell->tableWidget()->resizeColumnToContents(leadTimeCell->column());
    }
}

void SchedulerJob::reset()
{
    state = JOB_IDLE;
    stage = STAGE_IDLE;
    estimatedTime = -1;
    leadTime = 0;
    startupCondition = fileStartupCondition;
    startupTime = fileStartupCondition == START_AT ? fileStartupTime : QDateTime();

    /* Refresh dawn and dusk for startup date */
    calculateDawnDusk(startupTime, nextDawn, nextDusk);

    /* No change to culmination offset */
    repeatsRemaining = repeatsRequired;
    updateJobCells();
}

bool SchedulerJob::decreasingScoreOrder(SchedulerJob const *job1, SchedulerJob const *job2)
{
    return job1->getScore() > job2->getScore();
}

bool SchedulerJob::increasingPriorityOrder(SchedulerJob const *job1, SchedulerJob const *job2)
{
    return job1->getPriority() < job2->getPriority();
}

bool SchedulerJob::decreasingAltitudeOrder(SchedulerJob const *job1, SchedulerJob const *job2, QDateTime const &when)
{
    bool A_is_setting = job1->isSettingAtStartup;
    double const altA = when.isValid() ?
                        findAltitude(job1->getTargetCoords(), when, &A_is_setting) :
                        job1->altitudeAtStartup;

    bool B_is_setting = job2->isSettingAtStartup;
    double const altB = when.isValid() ?
                        findAltitude(job2->getTargetCoords(), when, &B_is_setting) :
                        job2->altitudeAtStartup;

    // Sort with the setting target first
    if (A_is_setting && !B_is_setting)
        return true;
    else if (!A_is_setting && B_is_setting)
        return false;

    // If both targets rise or set, sort by decreasing altitude, considering a setting target is prioritary
    return (A_is_setting && B_is_setting) ? altA < altB : altB < altA;
}

bool SchedulerJob::increasingStartupTimeOrder(SchedulerJob const *job1, SchedulerJob const *job2)
{
    return job1->getStartupTime() < job2->getStartupTime();
}

// This uses both the user-setting minAltitude, as well as any artificial horizon
// constraints the user might have setup.
double SchedulerJob::getMinAltitudeConstraint(double azimuth) const
{
    double constraint = getMinAltitude();
    if (getHorizon() != nullptr && enforceArtificialHorizon)
        constraint = std::max(constraint, getHorizon()->altitudeConstraint(azimuth));
    return constraint;
}

int16_t SchedulerJob::getAltitudeScore(QDateTime const &when, double *altPtr) const
{
    // FIXME: block calculating target coordinates at a particular time is duplicated in several places

    // Retrieve the argument date/time, or fall back to current time - don't use QDateTime's timezone!
    KStarsDateTime ltWhen(when.isValid() ?
                          Qt::UTC == when.timeSpec() ? getGeo()->UTtoLT(KStarsDateTime(when)) : when :
                          getLocalTime());

    // Create a sky object with the target catalog coordinates
    SkyPoint const target = getTargetCoords();
    SkyObject o;
    o.setRA0(target.ra0());
    o.setDec0(target.dec0());

    // Update RA/DEC of the target for the current fraction of the day
    KSNumbers numbers(ltWhen.djd());
    o.updateCoordsNow(&numbers);

    // Compute local sidereal time for the current fraction of the day, calculate altitude
    CachingDms const LST = getGeo()->GSTtoLST(getGeo()->LTtoUT(ltWhen).gst());
    o.EquatorialToHorizontal(&LST, getGeo()->lat());
    double const altitude = o.alt().Degrees();
    double const azimuth = o.az().Degrees();
    if (altPtr != nullptr)
        *altPtr = altitude;

    double const SETTING_ALTITUDE_CUTOFF = Options::settingAltitudeCutoff();
    int16_t score = BAD_SCORE - 1;

    const double minAlt = getMinAltitudeConstraint(azimuth);

    // If altitude is negative, bad score
    // FIXME: some locations may allow negative altitudes
    if (altitude < 0)
    {
        score = BAD_SCORE;
    }
    else if (hasAltitudeConstraint())
    {
        // If under altitude constraint, bad score
        if (altitude < minAlt)
            score = BAD_SCORE;
        // Else if setting and under altitude cutoff, job would end soon after starting, bad score
        // FIXME: half bad score when under altitude cutoff risk getting positive again
        else
        {
            double offset = LST.Hours() - o.ra().Hours();
            if (24.0 <= offset)
                offset -= 24.0;
            else if (offset < 0.0)
                offset += 24.0;
            if (0.0 <= offset && offset < 12.0)
                if (altitude - SETTING_ALTITUDE_CUTOFF < minAlt)
                    score = BAD_SCORE / 2;
        }
    }
    // If not constrained but below minimum hard altitude, set score to 10% of altitude value
    else if (altitude < MIN_ALTITUDE)
    {
        score = static_cast <int16_t> (altitude / 10.0);
    }

    // Else default score calculation without altitude constraint
    if (score < BAD_SCORE)
        score = static_cast <int16_t> ((1.5 * pow(1.06, altitude)) - (MIN_ALTITUDE / 10.0));

    //qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' target altitude is %3 degrees at %2 (score %4).")
    //    .arg(getName())
    //    .arg(when.toString(getDateTimeDisplayFormat()))
    //    .arg(currentAlt, 0, 'f', minAltitude->decimals())
    //    .arg(QString::asprintf("%+d", score));

    return score;
}

int16_t SchedulerJob::getMoonSeparationScore(QDateTime const &when) const
{
    if (moon == nullptr) return 100;

    // FIXME: block calculating target coordinates at a particular time is duplicated in several places

    // Retrieve the argument date/time, or fall back to current time - don't use QDateTime's timezone!
    KStarsDateTime ltWhen(when.isValid() ?
                          Qt::UTC == when.timeSpec() ? getGeo()->UTtoLT(KStarsDateTime(when)) : when :
                          getLocalTime());

    // Create a sky object with the target catalog coordinates
    SkyPoint const target = getTargetCoords();
    SkyObject o;
    o.setRA0(target.ra0());
    o.setDec0(target.dec0());

    // Update RA/DEC of the target for the current fraction of the day
    KSNumbers numbers(ltWhen.djd());
    o.updateCoordsNow(&numbers);

    // Update moon
    //ut = getGeo()->LTtoUT(ltWhen);
    //KSNumbers ksnum(ut.djd()); // BUG: possibly LT.djd() != UT.djd() because of translation
    //LST = getGeo()->GSTtoLST(ut.gst());
    CachingDms LST = getGeo()->GSTtoLST(getGeo()->LTtoUT(ltWhen).gst());
    moon->updateCoords(&numbers, true, getGeo()->lat(), &LST, true);

    double const moonAltitude = moon->alt().Degrees();

    // Lunar illumination %
    double const illum = moon->illum() * 100.0;

    // Moon/Sky separation p
    double const separation = moon->angularDistanceTo(&o).Degrees();

    // Zenith distance of the moon
    double const zMoon = (90 - moonAltitude);
    // Zenith distance of target
    double const zTarget = (90 - o.alt().Degrees());

    int16_t score = 0;

    // If target = Moon, or no illuminiation, or moon below horizon, return static score.
    if (zMoon == zTarget || illum == 0 || zMoon >= 90)
        score = 100;
    else
    {
        // JM: Some magic voodoo formula I came up with!
        double moonEffect = (pow(separation, 1.7) * pow(zMoon, 0.5)) / (pow(zTarget, 1.1) * pow(illum, 0.5));

        // Limit to 0 to 100 range.
        moonEffect = KSUtils::clamp(moonEffect, 0.0, 100.0);

        if (getMinMoonSeparation() > 0)
        {
            if (separation < getMinMoonSeparation())
                score = BAD_SCORE * 5;
            else
                score = moonEffect;
        }
        else
            score = moonEffect;
    }

    // Limit to 0 to 20
    score /= 5.0;

    //qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' target is %L3 degrees from Moon (score %2).")
    //    .arg(getName())
    //    .arg(separation, 0, 'f', 3)
    //    .arg(QString::asprintf("%+d", score));

    return score;
}


double SchedulerJob::getCurrentMoonSeparation() const
{
    if (moon == nullptr) return 180.0;

    // FIXME: block calculating target coordinates at a particular time is duplicated in several places

    // Retrieve the argument date/time, or fall back to current time - don't use QDateTime's timezone!
    KStarsDateTime ltWhen(getLocalTime());

    // Create a sky object with the target catalog coordinates
    SkyPoint const target = getTargetCoords();
    SkyObject o;
    o.setRA0(target.ra0());
    o.setDec0(target.dec0());

    // Update RA/DEC of the target for the current fraction of the day
    KSNumbers numbers(ltWhen.djd());
    o.updateCoordsNow(&numbers);

    // Update moon
    //ut = getGeo()->LTtoUT(ltWhen);
    //KSNumbers ksnum(ut.djd()); // BUG: possibly LT.djd() != UT.djd() because of translation
    //LST = getGeo()->GSTtoLST(ut.gst());
    CachingDms LST = getGeo()->GSTtoLST(getGeo()->LTtoUT(ltWhen).gst());
    moon->updateCoords(&numbers, true, getGeo()->lat(), &LST, true);

    // Moon/Sky separation p
    return moon->angularDistanceTo(&o).Degrees();
}

QDateTime SchedulerJob::calculateAltitudeTime(QDateTime const &when) const
{
    // FIXME: block calculating target coordinates at a particular time is duplicated in several places

    // Retrieve the argument date/time, or fall back to current time - don't use QDateTime's timezone!
    KStarsDateTime ltWhen(when.isValid() ?
                          Qt::UTC == when.timeSpec() ? getGeo()->UTtoLT(KStarsDateTime(when)) : when :
                          getLocalTime());

    // Create a sky object with the target catalog coordinates
    SkyPoint const target = getTargetCoords();
    SkyObject o;
    o.setRA0(target.ra0());
    o.setDec0(target.dec0());

    // Calculate the UT at the argument time
    KStarsDateTime const ut = getGeo()->LTtoUT(ltWhen);

    double const SETTING_ALTITUDE_CUTOFF = Options::settingAltitudeCutoff();

    // Within the next 24 hours, search when the job target matches the altitude and moon constraints
    for (unsigned int minute = 0; minute < 24 * 60; minute++)
    {
        KStarsDateTime const ltOffset(ltWhen.addSecs(minute * 60));

        // Update RA/DEC of the target for the current fraction of the day
        KSNumbers numbers(ltOffset.djd());
        o.updateCoordsNow(&numbers);

        // Compute local sidereal time for the current fraction of the day, calculate altitude
        CachingDms const LST = getGeo()->GSTtoLST(getGeo()->LTtoUT(ltOffset).gst());
        o.EquatorialToHorizontal(&LST, getGeo()->lat());
        double const altitude = o.alt().Degrees();
        double const azimuth = o.az().Degrees();
        double const minAlt = getMinAltitudeConstraint(azimuth);

        if (minAlt <= altitude)
        {
            // Don't test proximity to dawn in this situation, we only cater for altitude here

            // Continue searching if Moon separation is not good enough
            if (0 < getMinMoonSeparation() && getMoonSeparationScore(ltOffset) < 0)
                continue;

            // Continue searching if target is setting and under the cutoff
            double offset = LST.Hours() - o.ra().Hours();
            if (24.0 <= offset)
                offset -= 24.0;
            else if (offset < 0.0)
                offset += 24.0;
            if (0.0 <= offset && offset < 12.0)
                if (altitude - SETTING_ALTITUDE_CUTOFF < minAlt)
                    continue;

            return ltOffset;
        }
    }

    return QDateTime();
}

QDateTime SchedulerJob::calculateCulmination(QDateTime const &when) const
{
    // FIXME: culmination calculation is a min altitude requirement, should be an interval altitude requirement

    // FIXME: block calculating target coordinates at a particular time is duplicated in calculateCulmination

    // Retrieve the argument date/time, or fall back to current time - don't use QDateTime's timezone!
    KStarsDateTime ltWhen(when.isValid() ?
                          Qt::UTC == when.timeSpec() ? getGeo()->UTtoLT(KStarsDateTime(when)) : when :
                          getLocalTime());

    // Create a sky object with the target catalog coordinates
    SkyPoint const target = getTargetCoords();
    SkyObject o;
    o.setRA0(target.ra0());
    o.setDec0(target.dec0());

    // Update RA/DEC for the argument date/time
    KSNumbers numbers(ltWhen.djd());
    o.updateCoordsNow(&numbers);

    // Calculate transit date/time at the argument date - transitTime requires UT and returns LocalTime
    KStarsDateTime transitDateTime(ltWhen.date(), o.transitTime(getGeo()->LTtoUT(ltWhen), getGeo()), Qt::LocalTime);

    // Shift transit date/time by the argument offset
    KStarsDateTime observationDateTime = transitDateTime.addSecs(getCulminationOffset() * 60);

    // Relax observation time, culmination calculation is stable at minute only
    KStarsDateTime relaxedDateTime = observationDateTime.addSecs(Options::leadTime() * 60);

    // Verify resulting observation time is under lead time vs. argument time
    // If sooner, delay by 8 hours to get to the next transit - perhaps in a third call
    if (relaxedDateTime < ltWhen)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' startup %2 is posterior to transit %3, shifting by 8 hours.")
                                       .arg(getName())
                                       .arg(ltWhen.toString(getDateTimeDisplayFormat()))
                                       .arg(relaxedDateTime.toString(getDateTimeDisplayFormat()));

        return calculateCulmination(when.addSecs(8 * 60 * 60));
    }

    // Guarantees - culmination calculation is stable at minute level, so relax by lead time
    Q_ASSERT_X(observationDateTime.isValid(), __FUNCTION__, "Observation time for target culmination is valid.");
    Q_ASSERT_X(ltWhen <= relaxedDateTime, __FUNCTION__,
               "Observation time for target culmination is at or after than argument time");

    // Return consolidated culmination time
    return Qt::UTC == observationDateTime.timeSpec() ? getGeo()->UTtoLT(observationDateTime) : observationDateTime;
}

double SchedulerJob::findAltitude(const SkyPoint &target, const QDateTime &when, bool * is_setting, bool debug)
{
    // FIXME: block calculating target coordinates at a particular time is duplicated in several places

    // Retrieve the argument date/time, or fall back to current time - don't use QDateTime's timezone!
    KStarsDateTime ltWhen(when.isValid() ?
                          Qt::UTC == when.timeSpec() ? getGeo()->UTtoLT(KStarsDateTime(when)) : when :
                          getLocalTime());

    // Create a sky object with the target catalog coordinates
    SkyObject o;
    o.setRA0(target.ra0());
    o.setDec0(target.dec0());

    // Update RA/DEC of the target for the current fraction of the day
    KSNumbers numbers(ltWhen.djd());
    o.updateCoordsNow(&numbers);

    // Calculate alt/az coordinates using KStars instance's geolocation
    CachingDms const LST = getGeo()->GSTtoLST(getGeo()->LTtoUT(ltWhen).gst());
    o.EquatorialToHorizontal(&LST, getGeo()->lat());

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

void SchedulerJob::calculateDawnDusk(QDateTime const &when, QDateTime &nextDawn, QDateTime &nextDusk)
{
    QDateTime startup = when;

    if (!startup.isValid())
        startup = getLocalTime();

    // Our local midnight - the KStarsDateTime date+time constructor is safe for local times
    KStarsDateTime midnight(startup.date(), QTime(0, 0), Qt::LocalTime);

    QDateTime dawn = startup, dusk = startup;

    // Loop dawn and dusk calculation until the events found are the next events
    for ( ; dawn <= startup || dusk <= startup ; midnight = midnight.addDays(1))
    {
        // KSAlmanac computes the closest dawn and dusk events from the local sidereal time corresponding to the midnight argument
        KSAlmanac const ksal(midnight, getGeo());

        // If dawn is in the past compared to this observation, fetch the next dawn
        if (dawn <= startup)
            dawn = getGeo()->UTtoLT(ksal.getDate().addSecs((ksal.getDawnAstronomicalTwilight() * 24.0 + Options::dawnOffset()) *
                                    3600.0));

        // If dusk is in the past compared to this observation, fetch the next dusk
        if (dusk <= startup)
            dusk = getGeo()->UTtoLT(ksal.getDate().addSecs((ksal.getDuskAstronomicalTwilight() * 24.0 + Options::duskOffset()) *
                                    3600.0));
    }

    // Now we have the next events:
    // - if dawn comes first, observation runs during the night
    // - if dusk comes first, observation runs during the day
    nextDawn = dawn;
    nextDusk = dusk;
}

bool SchedulerJob::runsDuringAstronomicalNightTime() const
{
    // Calculate the next astronomical dawn time, adjusted with the Ekos pre-dawn offset
    QDateTime const earlyDawn = nextDawn.addSecs(-60.0 * abs(Options::preDawnTime()));

    // Dawn and dusk are ordered as the immediate next events following the observation time
    // Thus if dawn comes first, the job startup time occurs during the dusk/dawn interval.
    return nextDawn < nextDusk && startupTime <= earlyDawn;
}
