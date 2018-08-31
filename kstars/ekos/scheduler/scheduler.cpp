/*  Ekos Scheduler Module
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    DBus calls from GSoC 2015 Ekos Scheduler project by Daniel Leu <daniel_mihai.leu@cti.pub.ro>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "scheduler.h"

#include "ksalmanac.h"
#include "ksnotification.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "mosaic.h"
#include "Options.h"
#include "scheduleradaptor.h"
#include "schedulerjob.h"
#include "skymapcomposite.h"
#include "auxiliary/QProgressIndicator.h"
#include "dialogs/finddialog.h"
#include "ekos/ekosmanager.h"
#include "ekos/capture/sequencejob.h"
#include "skyobjects/starobject.h"

#include <KNotifications/KNotification>

#include <ekos_scheduler_debug.h>

#define BAD_SCORE               -1000
#define MAX_FAILURE_ATTEMPTS    5
#define UPDATE_PERIOD_MS        1000
#define SETTING_ALTITUDE_CUTOFF 3

#define DEFAULT_CULMINATION_TIME    -60
#define DEFAULT_MIN_ALTITUDE        15
#define DEFAULT_MIN_MOON_SEPARATION 0

namespace Ekos
{
Scheduler::Scheduler()
{
    setupUi(this);

    new SchedulerAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Scheduler", this);

    dirPath = QUrl::fromLocalFile(QDir::homePath());

    // Get current KStars time and set seconds to zero
    QDateTime currentDateTime = KStarsData::Instance()->lt();
    QTime currentTime         = currentDateTime.time();
    currentTime.setHMS(currentTime.hour(), currentTime.minute(), 0);
    currentDateTime.setTime(currentTime);

    // Set initial time for startup and completion times
    startupTimeEdit->setDateTime(currentDateTime);
    completionTimeEdit->setDateTime(currentDateTime);

    // Set up DBus interfaces
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Scheduler", this);
    ekosInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos", "org.kde.kstars.Ekos",
                                       QDBusConnection::sessionBus(), this);

    focusInterface   = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Focus", "org.kde.kstars.Ekos.Focus",
                                        QDBusConnection::sessionBus(), this);
    captureInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Capture", "org.kde.kstars.Ekos.Capture",
                                          QDBusConnection::sessionBus(), this);
    mountInterface   = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Mount", "org.kde.kstars.Ekos.Mount",
                                        QDBusConnection::sessionBus(), this);
    alignInterface   = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Align", "org.kde.kstars.Ekos.Align",
                                        QDBusConnection::sessionBus(), this);
    guideInterface   = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Guide", "org.kde.kstars.Ekos.Guide",
                                        QDBusConnection::sessionBus(), this);
    domeInterface    = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Dome", "org.kde.kstars.Ekos.Dome",
                                       QDBusConnection::sessionBus(), this);
    weatherInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Weather", "org.kde.kstars.Ekos.Weather",
                                          QDBusConnection::sessionBus(), this);
    capInterface     = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/DustCap", "org.kde.kstars.Ekos.DustCap",
                                      QDBusConnection::sessionBus(), this);

    moon = dynamic_cast<KSMoon *>(KStarsData::Instance()->skyComposite()->findByName("Moon"));

    sleepLabel->setPixmap(
        QIcon::fromTheme("chronometer").pixmap(QSize(32, 32)));
    sleepLabel->hide();

    connect(&sleepTimer, &QTimer::timeout, this, &Scheduler::wakeUpScheduler);
    schedulerTimer.setInterval(UPDATE_PERIOD_MS);
    jobTimer.setInterval(UPDATE_PERIOD_MS);

    connect(&schedulerTimer, &QTimer::timeout, this, &Scheduler::checkStatus);
    connect(&jobTimer, &QTimer::timeout, this, &Scheduler::checkJobStage);

    pi = new QProgressIndicator(this);
    bottomLayout->addWidget(pi, 0, nullptr);

    geo = KStarsData::Instance()->geo();

    raBox->setDegType(false); //RA box should be HMS-style

    /* FIXME: Find a way to have multi-line tooltips in the .ui file, then move the widget configuration there - what about i18n? */

    queueTable->setToolTip(i18n("Job scheduler list.\nClick to select a job in the list.\nDouble click to edit a job with the left-hand fields."));

    /* Set first button mode to add observation job from left-hand fields */
    setJobAddApply(true);

    removeFromQueueB->setIcon(QIcon::fromTheme("list-remove"));
    removeFromQueueB->setToolTip(i18n("Remove selected job from the observation list.\nJob properties are copied in the edition fields before removal."));
    removeFromQueueB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    queueUpB->setIcon(QIcon::fromTheme("go-up"));
    queueUpB->setToolTip(i18n("Move selected job one line up in the list.\nOrder only affect observation jobs that are scheduled to start at the same time."));
    queueUpB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueDownB->setIcon(QIcon::fromTheme("go-down"));
    queueDownB->setToolTip(i18n("Move selected job one line down in the list.\nOrder only affect observation jobs that are scheduled to start at the same time."));
    queueDownB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    evaluateOnlyB->setIcon(QIcon::fromTheme("system-reboot"));
    evaluateOnlyB->setToolTip(i18n("Reset state and force reevaluation of all observation jobs."));
    evaluateOnlyB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    mosaicB->setIcon(QIcon::fromTheme("zoom-draw"));
    mosaicB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    queueSaveAsB->setIcon(QIcon::fromTheme("document-save-as"));
    queueSaveAsB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueSaveB->setIcon(QIcon::fromTheme("document-save"));
    queueSaveB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueLoadB->setIcon(QIcon::fromTheme("document-open"));
    queueLoadB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    loadSequenceB->setIcon(QIcon::fromTheme("document-open"));
    loadSequenceB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    selectStartupScriptB->setIcon(QIcon::fromTheme("document-open"));
    selectStartupScriptB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    selectShutdownScriptB->setIcon(
        QIcon::fromTheme("document-open"));
    selectShutdownScriptB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    selectFITSB->setIcon(QIcon::fromTheme("document-open"));
    selectFITSB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    startupB->setIcon(
        QIcon::fromTheme("media-playback-start"));
    startupB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    shutdownB->setIcon(
        QIcon::fromTheme("media-playback-start"));
    shutdownB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    connect(startupB, &QPushButton::clicked, this, &Scheduler::runStartupProcedure);
    connect(shutdownB, &QPushButton::clicked, this, &Scheduler::runShutdownProcedure);

    selectObjectB->setIcon(QIcon::fromTheme("edit-find"));
    connect(selectObjectB, &QPushButton::clicked, this, &Scheduler::selectObject);
    connect(selectFITSB, &QPushButton::clicked, this, &Scheduler::selectFITS);
    connect(loadSequenceB, &QPushButton::clicked, this, &Scheduler::selectSequence);
    connect(selectStartupScriptB, &QPushButton::clicked, this, &Scheduler::selectStartupScript);
    connect(selectShutdownScriptB, &QPushButton::clicked, this, &Scheduler::selectShutdownScript);

    connect(mosaicB, &QPushButton::clicked, this, &Scheduler::startMosaicTool);
    connect(addToQueueB, &QPushButton::clicked, this, &Scheduler::addJob);
    connect(removeFromQueueB, &QPushButton::clicked, this, &Scheduler::removeJob);
    connect(queueUpB, &QPushButton::clicked, this, &Scheduler::moveJobUp);
    connect(queueDownB, &QPushButton::clicked, this, &Scheduler::moveJobDown);
    connect(evaluateOnlyB, &QPushButton::clicked, this, &Scheduler::startJobEvaluation);
    connect(queueTable, &QAbstractItemView::clicked, this, &Scheduler::clickQueueTable);
    connect(queueTable, &QAbstractItemView::doubleClicked, this, &Scheduler::loadJob);

    startB->setIcon(QIcon::fromTheme("media-playback-start"));
    startB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    pauseB->setIcon(QIcon::fromTheme("media-playback-pause"));
    pauseB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    connect(startB, &QPushButton::clicked, this, &Scheduler::toggleScheduler);
    connect(pauseB, &QPushButton::clicked, this, &Scheduler::pause);

    connect(queueSaveAsB, &QPushButton::clicked, this, &Scheduler::saveAs);
    connect(queueSaveB, &QPushButton::clicked, this, &Scheduler::save);
    connect(queueLoadB, &QPushButton::clicked, this, &Scheduler::load);

    connect(twilightCheck, &QCheckBox::toggled, this, &Scheduler::checkTwilightWarning);

    loadProfiles();

    watchJobChanges(true);
}

QString Scheduler::getCurrentJobName()
{
    return (currentJob != nullptr ? currentJob->getName() : "");
}

void Scheduler::watchJobChanges(bool enable)
{
    /* Don't double watch, this will cause multiple signals to be connected */
    if (enable == jobChangesAreWatched)
        return;

    /* These are the widgets we want to connect, per signal function, to listen for modifications */
    QLineEdit * const lineEdits[] = {
        nameEdit,
        raBox,
        decBox,
        fitsEdit,
        sequenceEdit,
        startupScript,
        shutdownScript
    };

    QDateTimeEdit * const dateEdits[] = {
        startupTimeEdit,
        completionTimeEdit
    };

    QComboBox * const comboBoxes[] = {
        schedulerProfileCombo
    };

    QButtonGroup * const buttonGroups[] = {
        stepsButtonGroup,
        startupButtonGroup,
        constraintButtonGroup,
        completionButtonGroup,
        startupProcedureButtonGroup,
        shutdownProcedureGroup
    };

    QSpinBox * const spinBoxes[] = {
        culminationOffset,
        repeatsSpin,
        prioritySpin
    };

    QDoubleSpinBox * const dspinBoxes[] = {
       minMoonSeparation,
       minAltitude
    };

    if (enable)
    {
        /* Connect the relevant signal to setDirty. Note that we are not keeping the connection object: we will
         * only use that signal once, and there will be no leaks. If we were connecting multiple receiver functions
         * to the same signal, we would have to be selective when disconnecting. We also use a lambda to absorb the
         * excess arguments which cannot be passed to setDirty, and limit captured arguments to 'this'.
         * The main problem with this implementation compared to the macro method is that it is now possible to
         * stack signal connections. That is, multiple calls to WatchJobChanges will cause multiple signal-to-slot
         * instances to be registered. As a result, one click will produce N signals, with N*=2 for each call to
         * WatchJobChanges(true) missing its WatchJobChanges(false) counterpart.
         */
        for (auto * const control: lineEdits)
            connect(control, &QLineEdit::editingFinished, this, [this](){setDirty();});
        for (auto * const control: dateEdits)
            connect(control, &QDateTimeEdit::editingFinished, this, [this](){setDirty();});
        for (auto * const control: comboBoxes)
            connect(control, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int){setDirty();});
        for (auto * const control: buttonGroups)
            connect(control, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::buttonToggled), this, [this](int, bool){setDirty();});
        for (auto * const control: spinBoxes)
            connect(control, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int){setDirty();});
        for (auto * const control: dspinBoxes)
            connect(control, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [this](double){setDirty();});
    }
    else
    {
        /* Disconnect the relevant signal from each widget. Actually, this method removes all signals from the widgets,
         * because we did not take care to keep the connection object when connecting. No problem in our case, we do not
         * expect other signals to be connected. Because we used a lambda, we cannot use the same function object to
         * disconnect selectively.
         */
        for (auto * const control: lineEdits)
            disconnect(control, &QLineEdit::editingFinished, this, nullptr);
        for (auto * const control: dateEdits)
            disconnect(control, &QDateTimeEdit::editingFinished, this, nullptr);
        for (auto * const control: comboBoxes)
            disconnect(control, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, nullptr);
        for (auto * const control: buttonGroups)
            disconnect(control, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::buttonToggled), this, nullptr);
        for (auto * const control: spinBoxes)
            disconnect(control, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, nullptr);
        for (auto * const control: dspinBoxes)
            disconnect(control, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, nullptr);
    }

    jobChangesAreWatched = enable;
}

void Scheduler::appendLogText(const QString &text)
{
    /* FIXME: user settings for log length */
    int const max_log_count = 2000;
    if (logText.size() > max_log_count)
        logText.removeLast();

    logText.prepend(i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                          QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS_SCHEDULER) << text;

    emit newLog();
}

void Scheduler::clearLog()
{
    logText.clear();
    emit newLog();
}

void Scheduler::selectObject()
{
    QPointer<FindDialog> fd = new FindDialog(this);
    if (fd->exec() == QDialog::Accepted)
    {
        SkyObject *object = fd->targetObject();
        addObject(object);
    }

    delete fd;
}

void Scheduler::addObject(SkyObject *object)
{
    if (object != nullptr)
    {
        QString finalObjectName(object->name());

        if (object->name() == "star")
        {
            StarObject *s = dynamic_cast<StarObject *>(object);

            if (s->getHDIndex() != 0)
                finalObjectName = QString("HD %1").arg(QString::number(s->getHDIndex()));
        }

        nameEdit->setText(finalObjectName);
        raBox->setText(object->ra0().toHMSString(false, true));
        decBox->setText(object->dec0().toDMSString(false, false, true));

        addToQueueB->setEnabled(sequenceEdit->text().isEmpty() == false);
        mosaicB->setEnabled(sequenceEdit->text().isEmpty() == false);

        setDirty();
    }
}

void Scheduler::selectFITS()
{
    fitsURL = QFileDialog::getOpenFileUrl(this, i18n("Select FITS Image"), dirPath, "FITS (*.fits *.fit)");
    if (fitsURL.isEmpty())
        return;

    dirPath = QUrl(fitsURL.url(QUrl::RemoveFilename));

    fitsEdit->setText(fitsURL.toLocalFile());

    if (nameEdit->text().isEmpty())
        nameEdit->setText(fitsURL.fileName());

    addToQueueB->setEnabled(sequenceEdit->text().isEmpty() == false);
    mosaicB->setEnabled(sequenceEdit->text().isEmpty() == false);

    setDirty();
}

void Scheduler::selectSequence()
{
    sequenceURL =
        QFileDialog::getOpenFileUrl(this, i18n("Select Sequence Queue"), dirPath, i18n("Ekos Sequence Queue (*.esq)"));
    if (sequenceURL.isEmpty())
        return;

    dirPath = QUrl(sequenceURL.url(QUrl::RemoveFilename));

    sequenceEdit->setText(sequenceURL.toLocalFile());

    // For object selection, all fields must be filled
    if ((raBox->isEmpty() == false && decBox->isEmpty() == false && nameEdit->text().isEmpty() == false)
        // For FITS selection, only the name and fits URL should be filled.
        || (nameEdit->text().isEmpty() == false && fitsURL.isEmpty() == false))
    {
        addToQueueB->setEnabled(true);
        mosaicB->setEnabled(true);
    }

    setDirty();
}

void Scheduler::selectStartupScript()
{
    startupScriptURL = QFileDialog::getOpenFileUrl(this, i18n("Select Startup Script"), dirPath, i18n("Script (*)"));
    if (startupScriptURL.isEmpty())
        return;

    dirPath = QUrl(startupScriptURL.url(QUrl::RemoveFilename));

    mDirty = true;
    startupScript->setText(startupScriptURL.toLocalFile());
}

void Scheduler::selectShutdownScript()
{
    shutdownScriptURL = QFileDialog::getOpenFileUrl(this, i18n("Select Shutdown Script"), dirPath, i18n("Script (*)"));
    if (shutdownScriptURL.isEmpty())
        return;

    dirPath = QUrl(shutdownScriptURL.url(QUrl::RemoveFilename));

    mDirty = true;
    shutdownScript->setText(shutdownScriptURL.toLocalFile());
}

void Scheduler::addJob()
{
    if (0 <= jobUnderEdit)
    {
        /* If a job is being edited, reset edition mode as all fields are already transferred to the job */
        resetJobEdit();
    }
    else
    {
        /* If a job is being added, save fields into a new job */
        saveJob();
        /* There is now an evaluation for each change, so don't duplicate the evaluation now */
        // jobEvaluationOnly = true;
        // evaluateJobs();
    }
}

void Scheduler::saveJob()
{
    if (state == SCHEDULER_RUNNIG)
    {
        appendLogText(i18n("Warning: You cannot add or modify a job while the scheduler is running."));
        return;
    }

    if (nameEdit->text().isEmpty())
    {
        appendLogText(i18n("Warning: Target name is required."));
        return;
    }

    if (sequenceEdit->text().isEmpty())
    {
        appendLogText(i18n("Warning: Sequence file is required."));
        return;
    }

    // Coordinates are required unless it is a FITS file
    if ((raBox->isEmpty() || decBox->isEmpty()) && fitsURL.isEmpty())
    {
        appendLogText(i18n("Warning: Target coordinates are required."));
        return;
    }

    bool raOk = false, decOk = false;
    dms /*const*/ ra(raBox->createDms(false, &raOk)); //false means expressed in hours
    dms /*const*/ dec(decBox->createDms(true, &decOk));

    if (raOk == false)
    {
        appendLogText(i18n("Warning: RA value %1 is invalid.", raBox->text()));
        return;
    }

    if (decOk == false)
    {
        appendLogText(i18n("Warning: DEC value %1 is invalid.", decBox->text()));
        return;
    }

    watchJobChanges(false);

    /* Warn if appending a job after infinite repeat */
    /* FIXME: alter looping job priorities so that they are rescheduled later */
    foreach(SchedulerJob * job, jobs)
        if(SchedulerJob::FINISH_LOOP == job->getCompletionCondition())
            appendLogText(i18n("Warning: Job '%1' has completion condition set to infinite repeat, other jobs may not execute.",job->getName()));

    /* Create or Update a scheduler job */
    int currentRow = queueTable->currentRow();
    SchedulerJob * job = nullptr;

    /* If no row is selected for insertion, append at end of list. */
    if (currentRow < 0)
        currentRow = queueTable->rowCount();

    /* Add job to queue only if it is new, else reuse current row.
     * Make sure job is added at the right index, now that queueTable may have a line selected without being edited.
     */
    if (0 <= jobUnderEdit)
    {
        /* FIXME: jobUnderEdit is a parallel variable that may cause issues if it desyncs from queueTable->currentRow(). */
        if (jobUnderEdit != currentRow)
            qCWarning(KSTARS_EKOS_SCHEDULER) << "BUG: the observation job under edit does not match the selected row in the job table.";

        /* Use the job in the row currently edited */
        job = jobs.at(currentRow);
    }
    else
    {
        /* Instantiate a new job, insert it in the job list and add a row in the table for it just after the row currently selected. */
        job = new SchedulerJob();
        jobs.insert(currentRow, job);
        queueTable->insertRow(currentRow);
    }

    /* Configure or reconfigure the observation job */

    job->setName(nameEdit->text());
    job->setPriority(prioritySpin->value());
    job->setTargetCoords(ra, dec);
    job->setDateTimeDisplayFormat(startupTimeEdit->displayFormat());

    /* Consider sequence file is new, and clear captured frames map */
    job->setCapturedFramesMap(SchedulerJob::CapturedFramesMap());
    job->setSequenceFile(sequenceURL);

    fitsURL = QUrl::fromLocalFile(fitsEdit->text());
    job->setFITSFile(fitsURL);

    // #1 Startup conditions

    if (asapConditionR->isChecked())
    {
        job->setStartupCondition(SchedulerJob::START_ASAP);
    }
    else if (culminationConditionR->isChecked())
    {
        job->setStartupCondition(SchedulerJob::START_CULMINATION);
        job->setCulminationOffset(culminationOffset->value());
    }
    else
    {
        job->setStartupCondition(SchedulerJob::START_AT);
        job->setStartupTime(startupTimeEdit->dateTime());
    }

    /* Store the original startup condition */
    job->setFileStartupCondition(job->getStartupCondition());
    job->setFileStartupTime(job->getStartupTime());


    // #2 Constraints

    // Do we have minimum altitude constraint?
    if (altConstraintCheck->isChecked())
        job->setMinAltitude(minAltitude->value());
    else
        job->setMinAltitude(-1);
    // Do we have minimum moon separation constraint?
    if (moonSeparationCheck->isChecked())
        job->setMinMoonSeparation(minMoonSeparation->value());
    else
        job->setMinMoonSeparation(-1);

    // Check enforce weather constraints
    job->setEnforceWeather(weatherCheck->isChecked());
    // twilight constraints
    job->setEnforceTwilight(twilightCheck->isChecked());

    /* Verifications */
    /* FIXME: perhaps use a method more visible to the end-user */
    if (SchedulerJob::START_AT == job->getFileStartupCondition())
    {
        /* Warn if appending a job which startup time doesn't allow proper score */
        if (calculateJobScore(job, job->getStartupTime()) < 0)
            appendLogText(i18n("Warning: job '%1' has startup time %2 resulting in a negative score, and will be marked invalid when processed.",
                               job->getName(), job->getStartupTime().toString(job->getDateTimeDisplayFormat())));

        /* Warn if appending a job with a startup time that is in the past */
        if (job->getStartupTime() < KStarsData::Instance()->lt())
            appendLogText(i18n("Warning: job '%1' has fixed startup time %2 set in the past, and will be marked invalid when evaluated.",
                               job->getName(), job->getStartupTime().toString(job->getDateTimeDisplayFormat())));
    }

    // #3 Completion conditions
    if (sequenceCompletionR->isChecked())
    {
        job->setCompletionCondition(SchedulerJob::FINISH_SEQUENCE);
    }
    else if (repeatCompletionR->isChecked())
    {
        job->setCompletionCondition(SchedulerJob::FINISH_REPEAT);
        job->setRepeatsRequired(repeatsSpin->value());
        job->setRepeatsRemaining(repeatsSpin->value());
    }
    else if (loopCompletionR->isChecked())
    {
        job->setCompletionCondition(SchedulerJob::FINISH_LOOP);
    }
    else
    {
        job->setCompletionCondition(SchedulerJob::FINISH_AT);
        job->setCompletionTime(completionTimeEdit->dateTime());
    }

    // Job steps
    job->setStepPipeline(SchedulerJob::USE_NONE);
    if (trackStepCheck->isChecked())
        job->setStepPipeline(static_cast<SchedulerJob::StepPipeline>(job->getStepPipeline() | SchedulerJob::USE_TRACK));
    if (focusStepCheck->isChecked())
        job->setStepPipeline(static_cast<SchedulerJob::StepPipeline>(job->getStepPipeline() | SchedulerJob::USE_FOCUS));
    if (alignStepCheck->isChecked())
        job->setStepPipeline(static_cast<SchedulerJob::StepPipeline>(job->getStepPipeline() | SchedulerJob::USE_ALIGN));
    if (guideStepCheck->isChecked())
        job->setStepPipeline(static_cast<SchedulerJob::StepPipeline>(job->getStepPipeline() | SchedulerJob::USE_GUIDE));

    /* Reset job state to evaluate the changes */
    job->reset();

    // Warn user if a duplicated job is in the list - same target, same sequence
    // FIXME: Those duplicated jobs are not necessarily processed in the order they appear in the list!
    foreach (SchedulerJob *a_job, jobs)
    {
        if (a_job == job)
        {
            break;
        }
        else if (a_job->getName() == job->getName())
        {
            int const a_job_row = a_job->getNameCell()? a_job->getNameCell()->row()+1 : 0;

            /* FIXME: Warning about duplicate jobs only checks the target name, doing it properly would require checking storage for each sequence job of each scheduler job. */
            appendLogText(i18n("Warning: job '%1' at row %2 has a duplicate target at row %3, "
                               "the scheduler may consider the same storage for captures.",
                               job->getName(), currentRow, a_job_row));

            /* Warn the user in case the two jobs are really identical */
            if (a_job->getSequenceFile() == job->getSequenceFile())
            {
                if (a_job->getRepeatsRequired() == job->getRepeatsRequired() && Options::rememberJobProgress())
                    appendLogText(i18n("Warning: jobs '%1' at row %2 and %3 probably require a different repeat count "
                                       "as currently they will complete simultaneously after %4 batches (or disable option 'Remember job progress')",
                                       job->getName(), currentRow, a_job_row, job->getRepeatsRequired()));

                if (a_job->getStartupTime() == a_job->getStartupTime() && a_job->getPriority() == job->getPriority())
                    appendLogText(i18n("Warning: job '%1' at row %2 might require a specific startup time or a different priority, "
                                       "as currently they will start in order of insertion in the table",
                                       job->getName(), currentRow));
            }
        }
    }

    /* FIXME: Move part of the new job cell-wiring to setJobStatusCells */

    QTableWidgetItem *nameCell = (jobUnderEdit >= 0) ? queueTable->item(currentRow, (int)SCHEDCOL_NAME) : new QTableWidgetItem();
    if (jobUnderEdit == -1) queueTable->setItem(currentRow, (int)SCHEDCOL_NAME, nameCell);
    nameCell->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    nameCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    job->setNameCell(nameCell);

    QTableWidgetItem *statusCell = (jobUnderEdit >= 0) ? queueTable->item(currentRow, (int)SCHEDCOL_STATUS) : new QTableWidgetItem();
    if (jobUnderEdit == -1) queueTable->setItem(currentRow, (int)SCHEDCOL_STATUS, statusCell);
    statusCell->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    statusCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    job->setStatusCell(statusCell);

    QTableWidgetItem *captureCount = (jobUnderEdit >= 0) ? queueTable->item(currentRow, (int)SCHEDCOL_CAPTURES) : new QTableWidgetItem();
    if (jobUnderEdit == -1) queueTable->setItem(currentRow, (int)SCHEDCOL_CAPTURES, captureCount);
    captureCount->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    captureCount->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    job->setCaptureCountCell(captureCount);

    QTableWidgetItem *scoreValue = (jobUnderEdit >= 0) ? queueTable->item(currentRow, (int)SCHEDCOL_SCORE) : new QTableWidgetItem();
    if (jobUnderEdit == -1) queueTable->setItem(currentRow, (int)SCHEDCOL_SCORE, scoreValue);
    scoreValue->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    scoreValue->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    job->setScoreCell(scoreValue);

    QTableWidgetItem *startupCell = (jobUnderEdit >= 0) ? queueTable->item(currentRow, (int)SCHEDCOL_STARTTIME) : new QTableWidgetItem();
    if (jobUnderEdit == -1) queueTable->setItem(currentRow, (int)SCHEDCOL_STARTTIME, startupCell);
    startupCell->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    startupCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    job->setStartupCell(startupCell);

    QTableWidgetItem *completionCell = (jobUnderEdit >= 0) ? queueTable->item(currentRow, (int)SCHEDCOL_ENDTIME) : new QTableWidgetItem();
    if (jobUnderEdit == -1) queueTable->setItem(currentRow, (int)SCHEDCOL_ENDTIME, completionCell);
    completionCell->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    completionCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    job->setCompletionCell(completionCell);

    QTableWidgetItem *estimatedTimeCell = (jobUnderEdit >= 0) ? queueTable->item(currentRow, (int)SCHEDCOL_DURATION) : new QTableWidgetItem();
    if (jobUnderEdit == -1) queueTable->setItem(currentRow, (int)SCHEDCOL_DURATION, estimatedTimeCell);
    estimatedTimeCell->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    estimatedTimeCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    job->setEstimatedTimeCell(estimatedTimeCell);

    /* We just added or saved a job, so we have a job in the list - enable relevant buttons */
    queueSaveAsB->setEnabled(true);
    queueSaveB->setEnabled(true);
    startB->setEnabled(true);
    evaluateOnlyB->setEnabled(true);
    setJobManipulation(true);

    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' at row #%2 was saved.").arg(job->getName()).arg(currentRow+1);

    watchJobChanges(true);
    jobEvaluationOnly = true;
    evaluateJobs();
}

void Scheduler::loadJob(QModelIndex i)
{
    if (jobUnderEdit == i.row())
        return;

    if (state == SCHEDULER_RUNNIG)
    {
        appendLogText(i18n("Warning: you cannot add or modify a job while the scheduler is running."));
        return;
    }

    SchedulerJob * const job = jobs.at(i.row());

    if (job == nullptr)
        return;

    watchJobChanges(false);

    //job->setState(SchedulerJob::JOB_IDLE);
    //job->setStage(SchedulerJob::STAGE_IDLE);

    nameEdit->setText(job->getName());

    prioritySpin->setValue(job->getPriority());

    raBox->setText(job->getTargetCoords().ra0().toHMSString());
    decBox->setText(job->getTargetCoords().dec0().toDMSString());

    if (job->getFITSFile().isEmpty() == false)
    {
        fitsEdit->setText(job->getFITSFile().toLocalFile());
        fitsURL = job->getFITSFile();
    }
    else
    {
        fitsEdit->clear();
        fitsURL = QUrl();
    }

    sequenceEdit->setText(job->getSequenceFile().toLocalFile());
    sequenceURL = job->getSequenceFile();

    trackStepCheck->setChecked(job->getStepPipeline() & SchedulerJob::USE_TRACK);
    focusStepCheck->setChecked(job->getStepPipeline() & SchedulerJob::USE_FOCUS);
    alignStepCheck->setChecked(job->getStepPipeline() & SchedulerJob::USE_ALIGN);
    guideStepCheck->setChecked(job->getStepPipeline() & SchedulerJob::USE_GUIDE);

    switch (job->getFileStartupCondition())
    {
        case SchedulerJob::START_ASAP:
            asapConditionR->setChecked(true);
            culminationOffset->setValue(DEFAULT_CULMINATION_TIME);
            break;

        case SchedulerJob::START_CULMINATION:
            culminationConditionR->setChecked(true);
            culminationOffset->setValue(job->getCulminationOffset());
            break;

        case SchedulerJob::START_AT:
            startupTimeConditionR->setChecked(true);
            startupTimeEdit->setDateTime(job->getStartupTime());
            culminationOffset->setValue(DEFAULT_CULMINATION_TIME);
            break;
    }

    if (job->getMinAltitude() >= 0)
    {
        altConstraintCheck->setChecked(true);
        minAltitude->setValue(job->getMinAltitude());
    }
    else
    {
        altConstraintCheck->setChecked(false);
        minAltitude->setValue(DEFAULT_MIN_ALTITUDE);
    }

    if (job->getMinMoonSeparation() >= 0)
    {
        moonSeparationCheck->setChecked(true);
        minMoonSeparation->setValue(job->getMinMoonSeparation());
    }
    else
    {
        moonSeparationCheck->setChecked(false);
        minMoonSeparation->setValue(DEFAULT_MIN_MOON_SEPARATION);
    }

    weatherCheck->setChecked(job->getEnforceWeather());

    twilightCheck->blockSignals(true);
    twilightCheck->setChecked(job->getEnforceTwilight());
    twilightCheck->blockSignals(false);

    switch (job->getCompletionCondition())
    {
        case SchedulerJob::FINISH_SEQUENCE:
            sequenceCompletionR->setChecked(true);
            break;

        case SchedulerJob::FINISH_REPEAT:
            repeatCompletionR->setChecked(true);
            repeatsSpin->setValue(job->getRepeatsRequired());
            break;

        case SchedulerJob::FINISH_LOOP:
            loopCompletionR->setChecked(true);
            break;

        case SchedulerJob::FINISH_AT:
            timeCompletionR->setChecked(true);
            completionTimeEdit->setDateTime(job->getCompletionTime());
            break;
    }

    /* Turn the add button into an apply button */
    setJobAddApply(false);

    /* Disable scheduler start/evaluate buttons */
    startB->setEnabled(false);
    evaluateOnlyB->setEnabled(false);

    /* Don't let the end-user remove a job being edited */
    setJobManipulation(false);

    jobUnderEdit = i.row();
    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' at row #%2 is currently edited.").arg(job->getName()).arg(jobUnderEdit+1);

    watchJobChanges(true);
}

void Scheduler::clickQueueTable(QModelIndex index)
{
    setJobManipulation(index.isValid());
}

void Scheduler::setJobAddApply(bool add_mode)
{
    if (add_mode)
    {
        addToQueueB->setIcon(QIcon::fromTheme("list-add"));
        addToQueueB->setToolTip(i18n("Use edition fields to create a new job in the observation list."));
        //addToQueueB->setStyleSheet(QString());
        addToQueueB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    }
    else
    {
        addToQueueB->setIcon(QIcon::fromTheme("dialog-ok-apply"));
        addToQueueB->setToolTip(i18n("Apply job changes."));
        //addToQueueB->setStyleSheet("background-color:orange;}");
        addToQueueB->setEnabled(true);
    }
}

void Scheduler::setJobManipulation(bool can_manipulate)
{
    if (can_manipulate)
    {
        int const currentRow = queueTable->currentRow();
        queueUpB->setEnabled(0 < currentRow);
        queueDownB->setEnabled(currentRow < queueTable->rowCount() - 1);
        removeFromQueueB->setEnabled(true);
    }
    else
    {
        queueUpB->setEnabled(false);
        queueDownB->setEnabled(false);
        removeFromQueueB->setEnabled(false);
    }
}

void Scheduler::moveJobUp()
{
    int const rowCount = queueTable->rowCount();
    int const currentRow = queueTable->currentRow();
    int const destinationRow = currentRow - 1;

    /* No move if no job selected, if table has one line or less or if destination is out of table */
    if (currentRow < 0 || rowCount <= 1 || destinationRow < 0)
        return;

    /* Swap jobs in the list */
    jobs.swap(currentRow, destinationRow);

    /* Reassign status cells */
    setJobStatusCells(currentRow);
    setJobStatusCells(destinationRow);

    /* Move selection to destination row */
    queueTable->selectRow(destinationRow);
    setJobManipulation(true);

    /* Make list modified */
    setDirty();

    /* Reset all jobs starting from the one moved */
    for (int i = currentRow; i < jobs.size(); i++)
        jobs.at(i)->reset();

    /* Run evaluation as jobs that can run now changed order - saveJob will only evaluate if a job is edited */
    jobEvaluationOnly = true;
    evaluateJobs();
}

void Scheduler::moveJobDown()
{
    int const rowCount = queueTable->rowCount();
    int const currentRow = queueTable->currentRow();
    int const destinationRow = currentRow + 1;

    /* No move if no job selected, if table has one line or less or if destination is out of table */
    if (currentRow < 0 || rowCount <= 1 || destinationRow == rowCount)
        return;

    /* Swap jobs in the list */
    jobs.swap(currentRow, destinationRow);

    /* Reassign status cells */
    setJobStatusCells(currentRow);
    setJobStatusCells(destinationRow);

    /* Move selection to destination row */
    queueTable->selectRow(destinationRow);
    setJobManipulation(true);

    /* Make list modified */
    setDirty();

    /* Reset all jobs starting from the one moved */
    for (int i = currentRow; i < jobs.size(); i++)
        jobs.at(i)->reset();

    /* Run evaluation as jobs that can run now changed order - saveJob will only evaluate if a job is edited */
    jobEvaluationOnly = true;
    evaluateJobs();
}

void Scheduler::setJobStatusCells(int row)
{
    if (row < 0 || jobs.size() <= row)
        return;

    SchedulerJob * const job = jobs.at(row);

    job->setNameCell(queueTable->item(row, (int)SCHEDCOL_NAME));
    job->setStatusCell(queueTable->item(row, (int)SCHEDCOL_STATUS));
    job->setCaptureCountCell(queueTable->item(row, (int)SCHEDCOL_CAPTURES));
    job->setScoreCell(queueTable->item(row, (int)SCHEDCOL_SCORE));
    job->setStartupCell(queueTable->item(row, (int)SCHEDCOL_STARTTIME));
    job->setCompletionCell(queueTable->item(row, (int)SCHEDCOL_ENDTIME));
    job->setEstimatedTimeCell(queueTable->item(row, (int)SCHEDCOL_DURATION));
}

void Scheduler::resetJobEdit()
{
    if (jobUnderEdit < 0)
        return;

    SchedulerJob * const job = jobs.at(jobUnderEdit);
    Q_ASSERT_X(job != nullptr,__FUNCTION__,"Edited job must be valid");

    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' at row #%2 is not longer edited.").arg(job->getName()).arg(jobUnderEdit+1);

    jobUnderEdit = -1;

    watchJobChanges(false);

    /* Revert apply button to add */
    setJobAddApply(true);

    /* Refresh state of job manipulation buttons */
    setJobManipulation(true);

    /* Restore scheduler operation buttons */
    evaluateOnlyB->setEnabled(true);
    startB->setEnabled(true);

    Q_ASSERT_X(jobUnderEdit == -1,__FUNCTION__,"No more edited/selected job after exiting edit mode");
}

void Scheduler::removeJob()
{
    int currentRow = queueTable->currentRow();

    /* Don't remove a row that is not selected */
    if (currentRow < 0)
        return;

    /* Grab the job currently selected */
    SchedulerJob * const job = jobs.at(currentRow);
    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' at row #%2 is being deleted.").arg(job->getName()).arg(currentRow+1);

    /* Remove the job from the table */
    queueTable->removeRow(currentRow);

    /* If there are no job rows left, update UI buttons */
    if (queueTable->rowCount() == 0)
    {
        setJobManipulation(false);
        evaluateOnlyB->setEnabled(false);
        queueSaveAsB->setEnabled(false);
        queueSaveB->setEnabled(false);
        startB->setEnabled(false);
        pauseB->setEnabled(false);
    }
    /* Else load the settings of the job that was just deleted */
    else loadJob(queueTable->currentIndex());

    /* If needed, reset edit mode to clean up UI */
    if (jobUnderEdit >= 0)
        resetJobEdit();

    /* And remove the job object */
    jobs.removeOne(job);
    delete (job);

    mDirty = true;
    jobEvaluationOnly = true;
    evaluateJobs();
}

void Scheduler::toggleScheduler()
{
    if (state == SCHEDULER_RUNNIG)
    {
        preemptiveShutdown = false;
        stop();
    }
    else
        start();
}

void Scheduler::stop()
{
    if (state != SCHEDULER_RUNNIG)
        return;

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Scheduler is stopping...";

    // Stop running job and abort all others
    // in case of soft shutdown we skip this
    if (preemptiveShutdown == false)
    {
        bool wasAborted = false;
        foreach (SchedulerJob *job, jobs)
        {
            if (job == currentJob)
            {
                stopCurrentJobAction();
                stopGuiding();
            }

            if (job->getState() <= SchedulerJob::JOB_BUSY)
            {
                appendLogText(i18n("Job '%1' has not been processed upon scheduler stop, marking aborted.", job->getName()));
                job->setState(SchedulerJob::JOB_ABORTED);
                wasAborted = true;
            }
        }

        if (wasAborted)
            KNotification::event(QLatin1String("SchedulerAborted"), i18n("Scheduler aborted."));
    }

    schedulerTimer.stop();
    jobTimer.stop();

    state     = SCHEDULER_IDLE;
    ekosState = EKOS_IDLE;
    indiState = INDI_IDLE;

    parkWaitState = PARKWAIT_IDLE;

    // Only reset startup state to idle if the startup procedure was interrupted before it had the chance to complete.
    // Or if we're doing a soft shutdown
    if (startupState != STARTUP_COMPLETE || preemptiveShutdown)
    {
        if (startupState == STARTUP_SCRIPT)
        {
            scriptProcess.disconnect();
            scriptProcess.terminate();
        }

        startupState = STARTUP_IDLE;
    }
    // Reset startup state to unparking phase (dome -> mount -> cap)
    // We do not want to run the startup script again but unparking should be checked
    // whenever the scheduler is running again.
    else if (startupState == STARTUP_COMPLETE)
    {
        if (unparkDomeCheck->isChecked())
            startupState = STARTUP_UNPARKING_DOME;
        else if (unparkMountCheck->isChecked())
            startupState = STARTUP_UNPARKING_MOUNT;
        else if (uncapCheck->isChecked())
            startupState = STARTUP_UNPARKING_CAP;
    }

    shutdownState = SHUTDOWN_IDLE;

    setCurrentJob(nullptr);
    captureBatch            = 0;
    indiConnectFailureCount = 0;
    ekosConnectFailureCount = 0;
    focusFailureCount       = 0;
    guideFailureCount       = 0;
    alignFailureCount       = 0;
    captureFailureCount     = 0;
    jobEvaluationOnly       = false;
    loadAndSlewProgress     = false;
    autofocusCompleted      = false;

    startupB->setEnabled(true);
    shutdownB->setEnabled(true);

    // If soft shutdown, we return for now
    if (preemptiveShutdown)
    {
        sleepLabel->setToolTip(i18n("Scheduler is in shutdown until next job is ready"));
        sleepLabel->show();
        return;
    }

    // Clear target name in capture interface upon stopping
    captureInterface->call(QDBus::AutoDetect, "setTargetName", QString());

    if (scriptProcess.state() == QProcess::Running)
        scriptProcess.terminate();

    sleepTimer.stop();
    //sleepTimer.disconnect();
    sleepLabel->hide();
    pi->stopAnimation();

    startB->setIcon(QIcon::fromTheme("media-playback-start"));
    startB->setToolTip(i18n("Start Scheduler"));
    pauseB->setEnabled(false);
    //startB->setText("Start Scheduler");

    queueLoadB->setEnabled(true);
    addToQueueB->setEnabled(true);
    setJobManipulation(false);
    mosaicB->setEnabled(true);
    evaluateOnlyB->setEnabled(true);
}

void Scheduler::start()
{
    switch (state)
    {
        case SCHEDULER_IDLE:
            /* FIXME: Manage the non-validity of the startup script earlier, and make it a warning only when the scheduler starts */
            startupScriptURL = QUrl::fromUserInput(startupScript->text());
            if (!startupScript->text().isEmpty() && !startupScriptURL.isValid())
            {
                appendLogText(i18n("Warning: startup script URL %1 is not valid.", startupScript->text()));
                return;
            }

            /* FIXME: Manage the non-validity of the shutdown script earlier, and make it a warning only when the scheduler starts */
            shutdownScriptURL = QUrl::fromUserInput(shutdownScript->text());
            if (!shutdownScript->text().isEmpty() && !shutdownScriptURL.isValid())
            {
                appendLogText(i18n("Warning: shutdown script URL %1 is not valid.", shutdownScript->text()));
                return;
            }

            qCInfo(KSTARS_EKOS_SCHEDULER) << "Scheduler is starting...";

            /* Update UI to reflect startup */
            pi->startAnimation();
            sleepLabel->hide();
            startB->setIcon(QIcon::fromTheme("media-playback-stop"));
            startB->setToolTip(i18n("Stop Scheduler"));
            pauseB->setEnabled(true);

            /* Disable edit-related buttons */
            queueLoadB->setEnabled(false);
            addToQueueB->setEnabled(false);
            setJobManipulation(false);
            mosaicB->setEnabled(false);
            evaluateOnlyB->setEnabled(false);
            startupB->setEnabled(false);
            shutdownB->setEnabled(false);

            /* Reset and re-evaluate all scheduler jobs, then start the Scheduler */
            startJobEvaluation();
            state = SCHEDULER_RUNNIG;
            schedulerTimer.start();

            qCDebug(KSTARS_EKOS_SCHEDULER) << "Scheduler started.";
            break;

        case SCHEDULER_PAUSED:
            /* Update UI to reflect resume */
            startB->setIcon(QIcon::fromTheme("media-playback-stop"));
            startB->setToolTip(i18n("Stop Scheduler"));

            /* Edit-related buttons are still disabled */

            /* The end-user cannot update the schedule, don't re-evaluate jobs. Timer schedulerTimer is already running. */
            state = SCHEDULER_RUNNIG;

            qCDebug(KSTARS_EKOS_SCHEDULER) << "Scheduler paused.";
            break;

        default: break;
    }
}

void Scheduler::pause()
{
    state = SCHEDULER_PAUSED;
    appendLogText(i18n("Scheduler paused."));
    pauseB->setEnabled(false);

    startB->setIcon(QIcon::fromTheme("media-playback-start"));
    startB->setToolTip(i18n("Resume Scheduler"));
}

void Scheduler::setCurrentJob(SchedulerJob *job)
{
    /* Reset job widgets */
    if (currentJob)
    {
        currentJob->setStageLabel(nullptr);
    }

    /* Set current job */
    currentJob = job;

    /* Reassign job widgets, or reset to defaults */
    if (currentJob)
    {
        currentJob->setStageLabel(jobStatus);
        queueTable->selectRow(currentJob->getStartupCell()->row());
    }
    else
    {
        jobStatus->setText(i18n("No job running"));
        queueTable->clearSelection();
    }
}

void Scheduler::evaluateJobs()
{
    /* FIXME: it is possible to evaluate jobs while KStars has a time offset, so warn the user about this */
    QDateTime const now = KStarsData::Instance()->lt();

    /* Start by refreshing the number of captures already present */
    updateCompletedJobsCount();

    /* Update dawn and dusk astronomical times - unconditionally in case date changed */
    calculateDawnDusk();

    /* First, filter out non-schedulable jobs */
    /* FIXME: jobs in state JOB_ERROR should not be in the list, reorder states */
    QList<SchedulerJob *> sortedJobs = jobs;
    sortedJobs.erase(std::remove_if(sortedJobs.begin(), sortedJobs.end(),[](SchedulerJob* job)
    { return SchedulerJob::JOB_ABORTED < job->getState(); }), sortedJobs.end());

    /* Then reorder jobs by priority */
    /* FIXME: refactor so all sorts are using the same predicates */
    /* FIXME: use std::stable_sort as qStableSort is deprecated */
    if (Options::sortSchedulerJobs())
        qStableSort(sortedJobs.begin(), sortedJobs.end(), SchedulerJob::increasingPriorityOrder);

    /* Then enumerate SchedulerJobs, scheduling only what is required */
    foreach (SchedulerJob *job, sortedJobs)
    {
        /* Let aborted jobs be rescheduled later instead of forgetting them */
        /* FIXME: minimum altitude and altitude cutoff may cause loops here */
        switch (job->getState())
        {
            /* If job is idle, set it for evaluation */
            case SchedulerJob::JOB_IDLE:
                job->setState(SchedulerJob::JOB_EVALUATION);
                job->setEstimatedTime(-1);
                break;

            /* If job is aborted, reset it for evaluation */
            case SchedulerJob::JOB_ABORTED:
                job->setState(SchedulerJob::JOB_EVALUATION);
                break;

            /* If job is scheduled, quick-check startup and bypass evaluation if in future */
            case SchedulerJob::JOB_SCHEDULED:
                if (job->getStartupTime() < now)
                    break;
                continue;

            /* If job is in error, invalid or complete, bypass evaluation */
            case SchedulerJob::JOB_ERROR:
            case SchedulerJob::JOB_INVALID:
            case SchedulerJob::JOB_COMPLETE:
                continue;

            /* If job is busy, edge case, bypass evaluation */
            case SchedulerJob::JOB_BUSY:
                continue;

            /* Else evaluate */
            case SchedulerJob::JOB_EVALUATION:
            default:
                break;
        }

        // In case of a repeating jobs, let's make sure we have more runs left to go
        if (job->getCompletionCondition() == SchedulerJob::FINISH_REPEAT)
        {
            if (job->getRepeatsRemaining() == 0)
            {
                appendLogText(i18n("Job '%1' has no more batches remaining.", job->getName()));
                job->setState(SchedulerJob::JOB_EVALUATION);
            }
        }

        // -1 = Job is not estimated yet
        // -2 = Job is estimated but time is unknown
        // > 0  Job is estimated and time is known
        if (job->getEstimatedTime() == -1)
        {
            if (estimateJobTime(job) == false)
            {
                job->setState(SchedulerJob::JOB_INVALID);
                continue;
            }
        }

        if (job->getEstimatedTime() == 0)
        {
            job->setRepeatsRemaining(0);
            job->setState(SchedulerJob::JOB_COMPLETE);
            continue;
        }

        // #1 Check startup conditions
        switch (job->getStartupCondition())
        {
            // #1.1 ASAP?
            case SchedulerJob::START_ASAP:
            {
                /* Job is to be started as soon as possible, so check its current score */
                int16_t const score = calculateJobScore(job, now);

                /* If it's not possible to run the job now, find proper altitude time */
                if (score < 0)
                {
                    // If Altitude or Dark score are negative, we try to schedule a better time for altitude and dark sky period.
                    if (calculateAltitudeTime(job, job->getMinAltitude() > 0 ? job->getMinAltitude() : 0,
                                              job->getMinMoonSeparation()))
                    {
                        //appendLogText(i18n("%1 observation job is scheduled at %2", job->getName(), job->getStartupTime().toString()));
                        job->setState(SchedulerJob::JOB_SCHEDULED);
                        // Since it's scheduled, we need to skip it now and re-check it later since its startup condition changed to START_AT
                        /*job->setScore(BAD_SCORE);
                        continue;*/
                    }
                    else
                    {
                        job->setState(SchedulerJob::JOB_INVALID);
                        qCWarning(KSTARS_EKOS_SCHEDULER) << QString("Ekos failed to schedule %1.").arg(job->getName());
                    }

                    /* Keep the job score for current time, score will refresh as scheduler progresses */
                    /* score = calculateJobScore(job, job->getStartupTime()); */
                    job->setScore(score);
                }
                /* If it's possible to run the job now, check weather */
                else if (isWeatherOK(job) == false)
                {
                    appendLogText(i18n("Job '%1' cannot run now because of bad weather.", job->getName()));
                    job->setState(SchedulerJob::JOB_ABORTED);
                    job->setScore(BAD_SCORE);
                }
                /* If weather is ok, schedule the job to run now */
                else
                {
                    appendLogText(i18n("Job '%1' is due to run as soon as possible.", job->getName()));
                    /* Give a proper start time, so that job can be rescheduled if others also start asap */
                    job->setStartupTime(now);
                    job->setState(SchedulerJob::JOB_SCHEDULED);
                    job->setScore(score);
                }
            }
            break;

                // #1.2 Culmination?
            case SchedulerJob::START_CULMINATION:
            {
                if (calculateCulmination(job))
                {
                    appendLogText(i18n("Job '%1' is scheduled at %2 for culmination.", job->getName(),
                                       job->getStartupTime().toString()));
                    job->setState(SchedulerJob::JOB_SCHEDULED);
                    // Since it's scheduled, we need to skip it now and re-check it later since its startup condition changed to START_AT
                    /*job->setScore(BAD_SCORE);
                    continue;*/
                }
                else
                {
                    appendLogText(i18n("Job '%1' culmination cannot be scheduled, marking invalid.", job->getName()));
                    job->setState(SchedulerJob::JOB_INVALID);
                }
            }
            break;

                // #1.3 Start at?
            case SchedulerJob::START_AT:
            {
                if (job->getCompletionCondition() == SchedulerJob::FINISH_AT)
                {
                    if (job->getCompletionTime() <= job->getStartupTime())
                    {
                        appendLogText(i18n("Job '%1' completion time (%2) could not be achieved before start up time (%3), marking invalid", job->getName(),
                                           job->getCompletionTime().toString(), job->getStartupTime().toString()));
                        job->setState(SchedulerJob::JOB_INVALID);
                        continue;
                    }
                }

                int const timeUntil = now.secsTo(job->getStartupTime());

                // If starting time already passed by 5 minutes (default), we mark the job as invalid or aborted
                if (timeUntil < (-1 * Options::leadTime() * 60))
                {
                    dms const passedUp(-timeUntil * 15.0  / 3600.0);

                    /* Mark the job invalid only if its startup time was a user request, else just abort it for later reschedule */
                    if (job->getFileStartupCondition() == SchedulerJob::START_AT)
                    {
                        appendLogText(i18n("Job '%1' startup time was fixed at %2, and is already passed by %3, marking invalid.",
                                           job->getName(), job->getStartupTime().toString(job->getDateTimeDisplayFormat()), passedUp.toHMSString()));
                        job->setState(SchedulerJob::JOB_INVALID);
                    }
                    else
                    {
                        appendLogText(i18n("Job '%1' startup time was %2, and is already passed by %3, marking aborted.",
                                           job->getName(), job->getStartupTime().toString(job->getDateTimeDisplayFormat()), passedUp.toHMSString()));
                        job->setState(SchedulerJob::JOB_ABORTED);
                    }
                }
                // Start scoring once we reach startup time
                else if (timeUntil <= 0)
                {
                    /* Consolidate altitude, moon separation and sky darkness scores */
                    int16_t const score = calculateJobScore(job, now);

                    if (score < 0)
                    {
                        /* If job score is already negative, silently abort the job to avoid spamming the user */
                        if (0 < job->getScore())
                        {
                            if (job->getState() == SchedulerJob::JOB_EVALUATION)
                                appendLogText(i18n("Job '%1' evaluation failed with a score of %2, marking aborted.",
                                                   job->getName(), score));
                            else if (timeUntil == 0)
                                appendLogText(i18n("Job '%1' updated score is %2 at startup time, marking aborted.",
                                                   job->getName(), score));
                            else
                                appendLogText(i18n("Job '%1' updated score is %2 %3 seconds after startup time, marking aborted.",
                                                   job->getName(), score, abs(timeUntil)));
                        }

                        job->setState(SchedulerJob::JOB_ABORTED);
                        job->setScore(score);
                    }
                    /* Positive score means job is already scheduled, so we check the weather, and if it is not OK, we set bad score until weather improves. */
                    else if (isWeatherOK(job) == false)
                    {
                        appendLogText(i18n("Job '%1' cannot run now because of bad weather.", job->getName()));
                        job->setState(SchedulerJob::JOB_ABORTED);
                        job->setScore(BAD_SCORE);
                    }
                    /* Else record current score */
                    else
                    {
                        appendLogText(i18n("Job '%1' will be run at %2.", job->getName(), job->getStartupTime().toString(job->getDateTimeDisplayFormat())));
                        job->setState(SchedulerJob::JOB_SCHEDULED);
                        job->setScore(score);
                    }
                }
#if 0
                // If it is in the future and originally was designated as ASAP job
                // Job must be less than 12 hours away to be considered for re-evaluation
                else if (timeUntil > (Options::leadTime() * 60) && (timeUntil < 12 * 3600) &&
                         job->getFileStartupCondition() == SchedulerJob::START_ASAP)
                {
                    QDateTime nextJobTime = now.addSecs(Options::leadTime() * 60);
                    if (job->getEnforceTwilight() == false || (now > duskDateTime && now < preDawnDateTime))
                    {
                        appendLogText(i18n("Job '%1' can be scheduled under 12 hours, but will be re-evaluated at %2.",
                                           job->getName(), nextJobTime.toString(job->getDateTimeDisplayFormat())));
                        job->setStartupTime(nextJobTime);
                    }
                    job->setScore(BAD_SCORE);
                }
                // If time is far in the future, we make the score negative
                else
                {
                    if (job->getState() == SchedulerJob::JOB_EVALUATION &&
                        calculateJobScore(job, job->getStartupTime()) < 0)
                    {
                        appendLogText(i18n("Job '%1' can only be scheduled in more than 12 hours, marking aborted.",
                                           job->getName()));
                        job->setState(SchedulerJob::JOB_ABORTED);
                        continue;
                    }

                    /*score += BAD_SCORE;*/
                }
#endif
                /* Else simply refresh job score */
                else
                {
                    appendLogText(i18n("Job '%1' unmodified, will be run at %2.", job->getName(), job->getStartupTime().toString(job->getDateTimeDisplayFormat())));
                    job->setState(SchedulerJob::JOB_SCHEDULED);
                    job->setScore(calculateJobScore(job, now));
                }

            }
            break;
        }


        if (job->getState() == SchedulerJob::JOB_EVALUATION)
        {
            qCDebug(KSTARS_EKOS_SCHEDULER) << "BUGBUG! Job '" << job->getName() << "' was unexpectedly not scheduled by evaluation.";
        }
    }

    /*
     * At this step, we scheduled all jobs that had to be scheduled because they could not start as soon as possible.
     * Now we check the amount of jobs we have to run.
     */

    int invalidJobs = 0, completedJobs = 0, abortedJobs = 0, upcomingJobs = 0;

    /* Partition jobs into invalid/aborted/completed/upcoming jobs */
    foreach (SchedulerJob *job, jobs)
    {
        switch (job->getState())
        {
            case SchedulerJob::JOB_INVALID:
                invalidJobs++;
                break;

            case SchedulerJob::JOB_ERROR:
            case SchedulerJob::JOB_ABORTED:
                abortedJobs++;
                break;

            case SchedulerJob::JOB_COMPLETE:
                completedJobs++;
                break;

            case SchedulerJob::JOB_SCHEDULED:
            case SchedulerJob::JOB_BUSY:
                upcomingJobs++;
                break;

            default:
                break;
        }
    }

    /* And render some statistics */
    if (upcomingJobs == 0 && jobEvaluationOnly == false)
    {
        if (invalidJobs > 0)
            appendLogText(i18np("%1 job is invalid.", "%1 jobs are invalid.", invalidJobs));

        if (abortedJobs > 0)
            appendLogText(i18np("%1 job aborted.", "%1 jobs aborted", abortedJobs));

        if (completedJobs > 0)
            appendLogText(i18np("%1 job completed.", "%1 jobs completed.", completedJobs));
    }

    /*
     * At this step, we still have jobs to run.
     * We filter out jobs that won't run now, and make sure jobs are not all starting at the same time.
     */

    updatePreDawn();

    /* Remove complete and invalid jobs that could have appeared during the last evaluation */
    sortedJobs.erase(std::remove_if(sortedJobs.begin(), sortedJobs.end(),[](SchedulerJob* job)
    { return SchedulerJob::JOB_ABORTED < job->getState(); }), sortedJobs.end());

    /* If there are no jobs left to run in the filtered list, shutdown scheduler and stop evaluation now */
    if (sortedJobs.isEmpty())
    {
        if (!jobEvaluationOnly)
        {
            if (startupState == STARTUP_COMPLETE)
            {
                appendLogText(i18n("No jobs left in the scheduler queue, starting shutdown procedure..."));
                // Let's start shutdown procedure
                checkShutdownState();
            }
            else
            {
                appendLogText(i18n("No jobs left in the scheduler queue, scheduler is stopping."));
                stop();
            }
        }
        else jobEvaluationOnly = false;

        return;
    }

    /* Now that jobs are scheduled, possibly at the same time, reorder by altitude and priority again */
    if (Options::sortSchedulerJobs())
    {
        qStableSort(sortedJobs.begin(), sortedJobs.end(), SchedulerJob::decreasingAltitudeOrder);
        qStableSort(sortedJobs.begin(), sortedJobs.end(), SchedulerJob::increasingPriorityOrder);
    }

    /* Reorder jobs by schedule time */
    qStableSort(sortedJobs.begin(), sortedJobs.end(), SchedulerJob::increasingStartupTimeOrder);

    // Our first job now takes priority over ALL others.
    // So if any other jobs conflicts with ours, we re-schedule that job to another time.
    SchedulerJob *firstJob      = sortedJobs.first();
    QDateTime firstStartTime    = firstJob->getStartupTime();
    QDateTime lastStartTime     = firstJob->getStartupTime();
    double lastJobEstimatedTime = firstJob->getEstimatedTime();
    int daysCount               = 0;

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Option to sort jobs based on priority and altitude is" << Options::sortSchedulerJobs();
    qCDebug(KSTARS_EKOS_SCHEDULER) << "First job after sort is" << firstJob->getName() << "starting at" << firstJob->getStartupTime().toString(firstJob->getDateTimeDisplayFormat());

    // Make sure no two jobs have the same scheduled time or overlap with other jobs
    foreach (SchedulerJob *job, sortedJobs)
    {
        // If this job is not scheduled, continue
        // If this job startup conditon is not to start at a specific time, continue
        if (job == firstJob || job->getState() != SchedulerJob::JOB_SCHEDULED ||
            job->getStartupCondition() != SchedulerJob::START_AT)
            continue;

        qCDebug(KSTARS_EKOS_SCHEDULER) << "Examining job" << job->getName() << "starting at" << job->getStartupTime().toString(job->getDateTimeDisplayFormat());

        double timeBetweenJobs = (double)std::abs(firstStartTime.secsTo(job->getStartupTime()));

        qCDebug(KSTARS_EKOS_SCHEDULER) << "Job starts in" << timeBetweenJobs << "seconds (lead time" << Options::leadTime()*60 << ")";

        // If there are within 5 minutes of each other, try to advance scheduling time of the lower altitude one
        if (timeBetweenJobs < (Options::leadTime()) * 60)
        {
            double delayJob = timeBetweenJobs + lastJobEstimatedTime;

            if (delayJob < (Options::leadTime() * 60))
                delayJob = Options::leadTime() * 60;

            QDateTime otherjob_time = lastStartTime.addSecs(delayJob);
            QDateTime nextPreDawnTime = preDawnDateTime.addDays(daysCount);
            // If other jobs starts after pre-dawn limit, then we schedule it to the next day.
            // But we only take this action IF the job we are checking against starts _before_ dawn and our
            // job therefore carry us after down, then there is an actual need to schedule it next day.
            // FIXME: After changing time we are not evaluating job again when we should.
            if (job->getEnforceTwilight() && lastStartTime < nextPreDawnTime && otherjob_time >= nextPreDawnTime)
            {
                QDateTime date;

                daysCount++;

                lastStartTime = job->getStartupTime().addDays(daysCount);
                job->setStartupTime(lastStartTime);
                date = lastStartTime.addSecs(delayJob);
            }
            else
            {
                lastStartTime = lastStartTime.addSecs(delayJob);
                job->setStartupTime(lastStartTime);
            }

            job->setState(SchedulerJob::JOB_SCHEDULED);

            /* Kept the informative log now that aborted jobs are rescheduled */
            appendLogText(i18n("Jobs '%1' and '%2' have close start up times, job '%2' is rescheduled to %3.",
                               firstJob->getName(), job->getName(), job->getStartupTime().toString(job->getDateTimeDisplayFormat())));
        }

        lastJobEstimatedTime = job->getEstimatedTime();
    }

    if (jobEvaluationOnly || state != SCHEDULER_RUNNIG)
    {
        qCInfo(KSTARS_EKOS_SCHEDULER) << "Ekos finished evaluating jobs, no job selection required.";
        jobEvaluationOnly = false;
        return;
    }

    /*
     * At this step, we finished evaluating jobs.
     * We select the first job that has to be run, per schedule.
     */

    // Sort again by schedule, sooner first, as some jobs may have shifted during the last step
    qStableSort(sortedJobs.begin(), sortedJobs.end(), SchedulerJob::increasingStartupTimeOrder);

    SchedulerJob * const job_to_execute = sortedJobs.first();

    /* Check if job can be processed right now */
    if (job_to_execute->getFileStartupCondition() == SchedulerJob::START_ASAP)
        if( 0 < calculateJobScore(job_to_execute, now))
            job_to_execute->setStartupTime(now);

    appendLogText(i18n("Job '%1' is selected for next observation with priority #%2 and score %3.",
                       job_to_execute->getName(), job_to_execute->getPriority(), job_to_execute->getScore()));

    // Set the current job, and let the status timer execute it when ready
    setCurrentJob(job_to_execute);
}

void Scheduler::wakeUpScheduler()
{
    sleepLabel->hide();
    sleepTimer.stop();

    if (preemptiveShutdown)
    {
        preemptiveShutdown = false;
        appendLogText(i18n("Scheduler is awake."));
        start();
    }
    else
    {
        if (state == SCHEDULER_RUNNIG)
            appendLogText(i18n("Scheduler is awake. Jobs shall be started when ready..."));
        else
            appendLogText(i18n("Scheduler is awake. Jobs shall be started when scheduler is resumed."));

        schedulerTimer.start();
    }
}

double Scheduler::findAltitude(const SkyPoint &target, const QDateTime &when)
{
    // Make a copy
    /*SkyPoint p = target;
    QDateTime lt(when.date(), QTime());
    KStarsDateTime ut = KStarsData::Instance()->geo()->LTtoUT(KStarsDateTime(lt));

    KStarsDateTime myUT = ut.addSecs(when.time().msecsSinceStartOfDay() / 1000);

    CachingDms LST = KStarsData::Instance()->geo()->GSTtoLST(myUT.gst());
    p.EquatorialToHorizontal(&LST, KStarsData::Instance()->geo()->lat());

    return p.alt().Degrees();*/

    SkyPoint p = target;
    KStarsDateTime lt(when);
    CachingDms LST = KStarsData::Instance()->geo()->GSTtoLST(lt.gst());
    p.EquatorialToHorizontal(&LST, KStarsData::Instance()->geo()->lat());

    return p.alt().Degrees();
}

bool Scheduler::calculateAltitudeTime(SchedulerJob *job, double minAltitude, double minMoonAngle)
{
    // We wouldn't stat observation 30 mins (default) before dawn.
    double const earlyDawn = Dawn - Options::preDawnTime() / (60.0 * 24.0);

    /* Compute UTC for beginning of today */
    QDateTime const lt(KStarsData::Instance()->lt().date(), QTime());
    KStarsDateTime const ut = geo->LTtoUT(KStarsDateTime(lt));

    /* Retrieve target coordinates to be converted to horizontal to determine altitude */
    SkyPoint target = job->getTargetCoords();

    /* Retrieve the current fraction of the day */
    QTime const now       = KStarsData::Instance()->lt().time();
    double const fraction = now.hour() + now.minute() / 60.0 + now.second() / 3600;

    /* This attempts to locate the first minute of the next 24 hours when the job target matches the altitude and moon constraints */
    for (double hour = fraction; hour < (fraction + 24); hour += 1.0 / 60.0)
    {
        double const rawFrac = (hour > 24 ? (hour - 24) : hour) / 24.0;

        /* Test twilight enforcement, and if enforced, bail out if start time is during day */
        /* FIXME: rework day fraction loop to shift to dusk directly */
        if (job->getEnforceTwilight() && Dawn <= rawFrac && rawFrac <= Dusk)
            continue;

        /* Compute altitude of target for the current fraction of the day */
        KStarsDateTime const myUT = ut.addSecs(hour * 3600.0);
        CachingDms const LST = geo->GSTtoLST(myUT.gst());
        target.EquatorialToHorizontal(&LST, geo->lat());
        double const altitude = target.alt().Degrees();

        if (altitude > minAltitude)
        {
            QDateTime const startTime = geo->UTtoLT(myUT);

            /* Test twilight enforcement, and if enforced, bail out if start time is too close to dawn */
            if (job->getEnforceTwilight() && earlyDawn < rawFrac && rawFrac < Dawn)
            {
                appendLogText(i18n("Warning: job '%1' reaches an altitude of %2 degrees at %3 but will not be scheduled due to "
                            "close proximity to astronomical twilight rise.",
                            job->getName(), QString::number(minAltitude, 'g', 3), startTime.toString(job->getDateTimeDisplayFormat())));
                return false;
            }

            /* Continue searching if Moon separation is not good enough */
            if (minMoonAngle > 0 && getMoonSeparationScore(job, startTime) < 0)
                continue;

            /* FIXME: the name of the function doesn't suggest the job can be modified */
            job->setStartupTime(startTime);
            /* Kept the informative log because of the reschedule of aborted jobs */
            appendLogText(i18n("Job '%1' is scheduled to start at %2 where its altitude is %3 degrees.", job->getName(),
                        startTime.toString(job->getDateTimeDisplayFormat()), QString::number(altitude, 'g', 3)));
            return true;
        }
    }

    /* FIXME: move this to the caller too to comment the decision to reject the job */
    if (minMoonAngle == -1)
    {
        if (job->getEnforceTwilight())
        {
            appendLogText(i18n("Warning: job '%1' has no night time with an altitude above %2 degrees during the next 24 hours, marking invalid.",
                               job->getName(), QString::number(minAltitude, 'g', 3)));
        }
        else appendLogText(i18n("Warning: job '%1' cannot rise to an altitude above %2 degrees in the next 24 hours, marking invalid.",
                                job->getName(), QString::number(minAltitude, 'g', 3)));
    }
    else appendLogText(i18n("Warning: job '%1' cannot be scheduled with an altitude above %2 degrees with minimum moon "
                            "separation of %3 degrees in the next 24 hours, marking invalid.",
                            job->getName(), QString::number(minAltitude, 'g', 3),
                            QString::number(minMoonAngle, 'g', 3)));
    return false;
}

bool Scheduler::calculateCulmination(SchedulerJob *job)
{
    SkyPoint target = job->getTargetCoords();

    SkyObject o;

    o.setRA0(target.ra0());
    o.setDec0(target.dec0());

    o.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

    QDateTime midnight(KStarsData::Instance()->lt().date(), QTime());
    KStarsDateTime dt = geo->LTtoUT(KStarsDateTime(midnight));

    QTime transitTime = o.transitTime(dt, geo);

    appendLogText(i18n("%1 Transit time is %2", job->getName(), transitTime.toString(job->getDateTimeDisplayFormat())));

    int dayOffset = 0;
    if (KStarsData::Instance()->lt().time() > transitTime)
        dayOffset = 1;

    QDateTime observationDateTime(QDate::currentDate().addDays(dayOffset),
                                  transitTime.addSecs(job->getCulminationOffset() * 60));

    appendLogText(i18np("%1 Observation time is %2 adjusted for %3 minute.",
                        "%1 Observation time is %2 adjusted for %3 minutes.", job->getName(),
                        observationDateTime.toString(job->getDateTimeDisplayFormat()), job->getCulminationOffset()));

    if (job->getEnforceTwilight() && getDarkSkyScore(observationDateTime) < 0)
    {
        appendLogText(i18n("%1 culminates during the day and cannot be scheduled for observation.", job->getName()));
        return false;
    }

    if (observationDateTime < (static_cast<QDateTime>(KStarsData::Instance()->lt())))
    {
        appendLogText(i18n("Observation time for %1 already passed.", job->getName()));
        return false;
    }

    job->setStartupTime(observationDateTime);
    return true;
}

void Scheduler::checkWeather()
{
    if (weatherCheck->isEnabled() == false || weatherCheck->isChecked() == false)
        return;

    IPState newStatus;
    QString statusString;

    QDBusReply<int> weatherReply = weatherInterface->call(QDBus::AutoDetect, "getWeatherStatus");
    if (weatherReply.error().type() == QDBusError::NoError)
    {
        newStatus = (IPState)weatherReply.value();

        switch (newStatus)
        {
            case IPS_OK:
                statusString = i18n("Weather conditions are OK.");
                break;

            case IPS_BUSY:
                statusString = i18n("Warning: weather conditions are in the WARNING zone.");
                break;

            case IPS_ALERT:
                statusString = i18n("Caution: weather conditions are in the DANGER zone!");
                break;

            default:
                if (noWeatherCounter++ >= MAX_FAILURE_ATTEMPTS)
                {
                    noWeatherCounter = 0;
                    appendLogText(i18n("Warning: Ekos did not receive any weather updates for the last %1 minutes.",
                                       weatherTimer.interval() / (60000.0)));
                }
                break;
        }

        if (newStatus != weatherStatus)
        {
            weatherStatus = newStatus;

            qCDebug(KSTARS_EKOS_SCHEDULER) << statusString;

            if (weatherStatus == IPS_OK)
                weatherLabel->setPixmap(
                    QIcon::fromTheme("security-high")
                        .pixmap(QSize(32, 32)));
            else if (weatherStatus == IPS_BUSY)
            {
                weatherLabel->setPixmap(
                    QIcon::fromTheme("security-medium")
                        .pixmap(QSize(32, 32)));
                KNotification::event(QLatin1String("WeatherWarning"), i18n("Weather conditions in warning zone"));
            }
            else if (weatherStatus == IPS_ALERT)
            {
                weatherLabel->setPixmap(
                    QIcon::fromTheme("security-low")
                        .pixmap(QSize(32, 32)));
                KNotification::event(QLatin1String("WeatherAlert"),
                                     i18n("Weather conditions are critical. Observatory shutdown is imminent"));
            }
            else
                weatherLabel->setPixmap(QIcon::fromTheme("chronometer")
                                            .pixmap(QSize(32, 32)));

            weatherLabel->show();
            weatherLabel->setToolTip(statusString);

            appendLogText(statusString);

            emit weatherChanged(weatherStatus);
        }

        if (weatherStatus == IPS_ALERT)
        {
            appendLogText(i18n("Starting shutdown procedure due to severe weather."));
            if (currentJob)
            {
                currentJob->setState(SchedulerJob::JOB_ABORTED);
                stopCurrentJobAction();
                stopGuiding();
                jobTimer.stop();
            }
            checkShutdownState();
            //connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkStatus()), &Scheduler::Qt::UniqueConnection);
        }
    }
}

int16_t Scheduler::getWeatherScore()
{
    if (weatherCheck->isEnabled() == false || weatherCheck->isChecked() == false)
        return 0;

    if (weatherStatus == IPS_BUSY)
        return BAD_SCORE / 2;
    else if (weatherStatus == IPS_ALERT)
        return BAD_SCORE;

    return 0;
}

int16_t Scheduler::getDarkSkyScore(const QDateTime &observationDateTime)
{
    //  if (job->getStartingCondition() == SchedulerJob::START_CULMINATION)
    //    return -1000;

    int16_t score      = 0;
    double dayFraction = 0;

    // Anything half an hour before dawn shouldn't be a good candidate
    double earlyDawn = Dawn - Options::preDawnTime() / (60.0 * 24.0);

    dayFraction = observationDateTime.time().msecsSinceStartOfDay() / (24.0 * 60.0 * 60.0 * 1000.0);

    // The farther the target from dawn, the better.
    if (dayFraction > earlyDawn && dayFraction < Dawn)
        score = BAD_SCORE / 50;
    else if (dayFraction < Dawn)
        score = (Dawn - dayFraction) * 100;
    else if (dayFraction > Dusk)
    {
        score = (dayFraction - Dusk) * 100;
    }
    else
        score = BAD_SCORE;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Dark sky score is" << score << "for time" << observationDateTime.toString();

    return score;
}

int16_t Scheduler::calculateJobScore(SchedulerJob *job, QDateTime when)
{
    /* Only consolidate the score if light frames are required, calibration frames can run whenever needed */
    if (!job->getLightFramesRequired())
        return 1000;

    int16_t total = 0;

    /* As soon as one score is negative, it's a no-go and other scores are unneeded */

    if (job->getEnforceTwilight())
        total += getDarkSkyScore(when);

    /* We still enforce altitude if the job is neither required to track nor guide, because this is too confusing for the end-user.
     * If we bypass calculation here, it must also be bypassed when checking job constraints in checkJobStage.
     */
    if (0 <= total /*&& ((job->getStepPipeline() & SchedulerJob::USE_TRACK) || (job->getStepPipeline() & SchedulerJob::USE_GUIDE))*/)
        total += getAltitudeScore(job, when);

    if (0 <= total)
        total += getMoonSeparationScore(job, when);

    qCInfo(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' has a total score of %2").arg(job->getName()).arg(total);
    return total;
}

int16_t Scheduler::getAltitudeScore(SchedulerJob *job, QDateTime when)
{
    int16_t score     = 0;
    double currentAlt = findAltitude(job->getTargetCoords(), when);

    if (currentAlt < 0)
    {
        score = BAD_SCORE;
    }
    // If minimum altitude is specified
    else if (job->getMinAltitude() > 0)
    {
        // if current altitude is lower that's not good
        if (currentAlt < job->getMinAltitude())
            score = BAD_SCORE;
        else
        {
            // Get HA of actual object, and not of the mount as was done below
            double HA = KStars::Instance()->data()->lst()->Hours() - job->getTargetCoords().ra().Hours();

#if 0
            if (indiState == INDI_READY)
            {
                QDBusReply<double> haReply = mountInterface->call(QDBus::AutoDetect, "getHourAngle");
                if (haReply.error().type() == QDBusError::NoError)
                    HA = haReply.value();
            }
#endif

            // If already passed the merdian and setting we check if it is within setting alttidue cut off value (3 degrees default)
            // If it is within that value then it is useless to start the job which will end very soon so we better look for a better job.
            /* FIXME: don't use BAD_SCORE/2, a negative result implies the job has to be aborted - we'd be annoyed if that score became positive again */
            /* FIXME: bug here, raising target will get a negative score if under cutoff, issue mitigated by aborted jobs getting rescheduled */
            if (HA > 0 && (currentAlt - SETTING_ALTITUDE_CUTOFF) < job->getMinAltitude())
                score = BAD_SCORE / 2.0;
            else
                // Otherwise, adjust score and add current altitude to score weight
                score = (1.5 * pow(1.06, currentAlt)) - (minAltitude->minimum() / 10.0);
        }
    }
    // If it's below minimum hard altitude (15 degrees now), set score to 10% of altitude value
    else if (currentAlt < minAltitude->minimum())
    {
        score = currentAlt / 10.0;
    }
    // If no minimum altitude, then adjust altitude score to account for current target altitude
    else
    {
        score = (1.5 * pow(1.06, currentAlt)) - (minAltitude->minimum() / 10.0);
    }

    /* Kept the informative log now that scores are displayed */
    appendLogText(i18n("Job '%1' target altitude is %3 degrees at %2 (score %4).", job->getName(), when.toString(job->getDateTimeDisplayFormat()),
                       QString::number(currentAlt, 'g', 3), QString::asprintf("%+d", score)));

    return score;
}

double Scheduler::getCurrentMoonSeparation(SchedulerJob *job)
{
    // Get target altitude given the time
    SkyPoint p = job->getTargetCoords();
    QDateTime midnight(KStarsData::Instance()->lt().date(), QTime());
    KStarsDateTime ut   = geo->LTtoUT(KStarsDateTime(midnight));
    KStarsDateTime myUT = ut.addSecs(KStarsData::Instance()->lt().time().msecsSinceStartOfDay() / 1000);
    CachingDms LST      = geo->GSTtoLST(myUT.gst());
    p.EquatorialToHorizontal(&LST, geo->lat());

    // Update moon
    ut = geo->LTtoUT(KStarsData::Instance()->lt());
    KSNumbers ksnum(ut.djd());
    LST = geo->GSTtoLST(ut.gst());
    moon->updateCoords(&ksnum, true, geo->lat(), &LST, true);

    // Moon/Sky separation p
    return moon->angularDistanceTo(&p).Degrees();
}

int16_t Scheduler::getMoonSeparationScore(SchedulerJob *job, QDateTime when)
{
    int16_t score = 0;

    // Get target altitude given the time
    SkyPoint p = job->getTargetCoords();
    QDateTime midnight(when.date(), QTime());
    KStarsDateTime ut   = geo->LTtoUT(KStarsDateTime(midnight));
    KStarsDateTime myUT = ut.addSecs(when.time().msecsSinceStartOfDay() / 1000);
    CachingDms LST      = geo->GSTtoLST(myUT.gst());
    p.EquatorialToHorizontal(&LST, geo->lat());
    double currentAlt = p.alt().Degrees();

    // Update moon
    ut = geo->LTtoUT(KStarsDateTime(when));
    KSNumbers ksnum(ut.djd());
    LST = geo->GSTtoLST(ut.gst());
    moon->updateCoords(&ksnum, true, geo->lat(), &LST, true);

    double moonAltitude = moon->alt().Degrees();

    // Lunar illumination %
    double illum = moon->illum() * 100.0;

    // Moon/Sky separation p
    double separation = moon->angularDistanceTo(&p).Degrees();

    // Zenith distance of the moon
    double zMoon = (90 - moonAltitude);
    // Zenith distance of target
    double zTarget = (90 - currentAlt);

    // If target = Moon, or no illuminiation, or moon below horizon, return static score.
    if (zMoon == zTarget || illum == 0 || zMoon >= 90)
        score = 100;
    else
    {
        // JM: Some magic voodoo formula I came up with!
        double moonEffect = (pow(separation, 1.7) * pow(zMoon, 0.5)) / (pow(zTarget, 1.1) * pow(illum, 0.5));

        // Limit to 0 to 100 range.
        moonEffect = KSUtils::clamp(moonEffect, 0.0, 100.0);

        if (job->getMinMoonSeparation() > 0)
        {
            if (separation < job->getMinMoonSeparation())
                score = BAD_SCORE * 5;
            else
                score = moonEffect;
        }
        else
            score = moonEffect;
    }

    // Limit to 0 to 20
    score /= 5.0;

    /* Kept the informative log now that score is displayed */
    appendLogText(i18n("Job '%1' target is %3 degrees from Moon (score %2).", job->getName(), QString::asprintf("%+d", score), separation));

    return score;
}

void Scheduler::calculateDawnDusk()
{
    KSAlmanac ksal;
    Dawn = ksal.getDawnAstronomicalTwilight();
    Dusk = ksal.getDuskAstronomicalTwilight();

    QTime now  = KStarsData::Instance()->lt().time();
    QTime dawn = QTime(0, 0, 0).addSecs(Dawn * 24 * 3600);
    QTime dusk = QTime(0, 0, 0).addSecs(Dusk * 24 * 3600);

    duskDateTime.setDate(KStars::Instance()->data()->lt().date());
    duskDateTime.setTime(dusk);

    appendLogText(i18n("Astronomical twilight rise is at %1, set is at %2, and current time is %3", dawn.toString(),
                       dusk.toString(), now.toString()));
}

void Scheduler::executeJob(SchedulerJob *job)
{
    // Don't execute the current job if it is already busy
    if (currentJob == job && SchedulerJob::JOB_BUSY == currentJob->getState())
        return;

    setCurrentJob(job);

    QDateTime const now = KStarsData::Instance()->lt();

    // If we already started, we check when the next object is scheduled at.
    // If it is more than 30 minutes in the future, we park the mount if that is supported
    // and we unpark when it is due to start.
    int const nextObservationTime = now.secsTo(currentJob->getStartupTime());

    // If start up procedure is complete and the user selected pre-emptive shutdown, let us check if the next observation time exceed
    // the pre-emptive shutdown time in hours (default 2). If it exceeds that, we perform complete shutdown until next job is ready
    if (startupState == STARTUP_COMPLETE &&
            Options::preemptiveShutdown() &&
            nextObservationTime > (Options::preemptiveShutdownTime() * 3600))
    {
        appendLogText(i18n(
                    "Job '%1' scheduled for execution at %2. "
                    "Observatory scheduled for shutdown until next job is ready.",
                    currentJob->getName(), currentJob->getStartupTime().toString()));
        preemptiveShutdown = true;
        weatherCheck->setEnabled(false);
        weatherLabel->hide();
        checkShutdownState();

        schedulerTimer.stop();

        // Wake up when job is due.
        // FIXME: Implement waking up periodically before job is due for weather check.
        // int const nextWakeup = nextObservationTime < 60 ? nextObservationTime : 60;
        sleepTimer.setInterval( (nextObservationTime+1) * 1000);
        sleepTimer.start();

        return;
    }
    // Otherise, sleep until job is ready
    /* FIXME: if not parking, stop tracking maybe? this would prevent crashes or scheduler stops from leaving the mount to track and bump the pier */
    // If start up procedure is already complete, and we didn't issue any parking commands before and parking is checked and enabled
    // Then we park the mount until next job is ready. But only if the job uses TRACK as its first step, otherwise we cannot get into position again.
    // This is also only performed if next job is due more than the default lead time (5 minutes).
    // If job is due sooner than that is not worth parking and we simply go into sleep or wait modes.
    else if (nextObservationTime > Options::leadTime() * 60 &&
            startupState == STARTUP_COMPLETE &&
            parkWaitState == PARKWAIT_IDLE &&
            (currentJob->getStepPipeline() & SchedulerJob::USE_TRACK) &&
            parkMountCheck->isEnabled() &&
            parkMountCheck->isChecked())
    {
        appendLogText(i18n(
                    "Job '%1' scheduled for execution at %2. "
                    "Parking the mount until the job is ready.",
                    currentJob->getName(), currentJob->getStartupTime().toString()));

        parkWaitState = PARKWAIT_PARK;

        return;
    }
    // If the time to wait is greater than the lead time (5 minutes by default)
    // then we sleep, otherwise we wait. It's the same thing, just different labels.
    else if (nextObservationTime > Options::leadTime() * 60)
    {
        appendLogText(i18n("Sleeping until observation job %1 is ready at %2...", currentJob->getName(),
                    now.addSecs(nextObservationTime+1).toString()));
        sleepLabel->setToolTip(i18n("Scheduler is in sleep mode"));
        sleepLabel->show();

        // Warn the user if the next job is really far away - 60/5 = 12 times the lead time
        if (nextObservationTime > Options::leadTime() * 60 * 12)
        {
            dms delay((double) nextObservationTime * 15.0 / 3600.0);
            appendLogText(i18n(
                        "Warning: Job '%1' is %2 away from now, you may want to enable Preemptive Shutdown.",
                        currentJob->getName(), delay.toHMSString()));
        }

        /* FIXME: stop tracking now */

        schedulerTimer.stop();

        // Wake up when job is due.
        // FIXME: Implement waking up periodically before job is due for weather check.
        // int const nextWakeup = nextObservationTime < 60 ? nextObservationTime : 60;
        sleepTimer.setInterval(( (nextObservationTime+1) * 1000));
        sleepTimer.start();

        return;
    }
    // If job schedule isn't now, wait - continuing to execute would cancel a parking attempt
    else if (0 < KStarsData::Instance()->lt().secsTo(currentJob->getStartupTime()))
        return;

    // From this point job can be executed now

    if (job->getCompletionCondition() == SchedulerJob::FINISH_SEQUENCE && Options::rememberJobProgress())
    {
        QString targetName = job->getName().replace(' ', "");
        QList<QVariant> targetArgs;

        targetArgs.append(targetName);
        captureInterface->callWithArgumentList(QDBus::AutoDetect, "setTargetName", targetArgs);
    }

    updatePreDawn();

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Executing Job " << currentJob->getName();

    currentJob->setState(SchedulerJob::JOB_BUSY);

    KNotification::event(QLatin1String("EkosSchedulerJobStart"),
            i18n("Ekos job started (%1)", currentJob->getName()));

    // No need to continue evaluating jobs as we already have one.

    schedulerTimer.stop();
    jobTimer.start();
}

bool Scheduler::checkEkosState()
{
    if (state == SCHEDULER_PAUSED)
        return false;

    switch (ekosState)
    {
        case EKOS_IDLE:
        {
            // Even if state is IDLE, check if Ekos is already started. If not, start it.
            QDBusReply<int> isEkosStarted;
            isEkosStarted = ekosInterface->call(QDBus::AutoDetect, "getEkosStartingStatus");
            if (isEkosStarted.value() == EkosManager::EKOS_STATUS_SUCCESS)
            {
                ekosState = EKOS_READY;
                return true;
            }
            else
            {
                ekosInterface->call(QDBus::AutoDetect, "start");
                ekosState = EKOS_STARTING;

                currentOperationTime.start();

                return false;
            }
        }
        break;

        case EKOS_STARTING:
        {
            QDBusReply<int> isEkosStarted;
            isEkosStarted = ekosInterface->call(QDBus::AutoDetect, "getEkosStartingStatus");
            if (isEkosStarted.value() == EkosManager::EKOS_STATUS_SUCCESS)
            {
                appendLogText(i18n("Ekos started."));
                ekosState = EKOS_READY;
                return true;
            }
            else if (isEkosStarted.value() == EkosManager::EKOS_STATUS_ERROR)
            {
                if (ekosConnectFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Ekos failed to connect. Retrying..."));
                    ekosInterface->call(QDBus::AutoDetect, "start");
                    return false;
                }

                appendLogText(i18n("Ekos failed to start."));
                stop();
                return false;
            }
            // If a minute passed, give up
            else if (currentOperationTime.elapsed() > (60 * 1000))
            {
                if (ekosConnectFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Ekos failed to connect. Retrying..."));
                    ekosInterface->call(QDBus::AutoDetect, "start");
                    return false;
                }

                appendLogText(i18n("Ekos timed out."));
                stop();
                return false;
            }
        }
        break;

        case EKOS_STOPPING:
        {
            QDBusReply<int> isEkosStarted;
            isEkosStarted = ekosInterface->call(QDBus::AutoDetect, "getEkosStartingStatus");
            if (isEkosStarted.value() == EkosManager::EKOS_STATUS_IDLE)
            {
                appendLogText(i18n("Ekos stopped."));
                ekosState = EKOS_IDLE;
                return true;
            }
        }
        break;

        case EKOS_READY:
            return true;
            break;
    }

    return false;
}

bool Scheduler::isINDIConnected()
{
    QDBusReply<int> isINDIConnected = ekosInterface->call(QDBus::AutoDetect, "getINDIConnectionStatus");

    if (isINDIConnected.error().type() != QDBusError::NoError)
        return false;
    else if (isINDIConnected.value() == EkosManager::EKOS_STATUS_SUCCESS)
        return true;
    else // ERROR or IDLE
        return false;
}

bool Scheduler::checkINDIState()
{
    if (state == SCHEDULER_PAUSED)
        return false;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking INDI State...";

    switch (indiState)
    {
        case INDI_IDLE:
        {
            // Even in idle state, we make sure that INDI is not already connected.
            if (isINDIConnected())
            {
                indiState = INDI_PROPERTY_CHECK;

                qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking INDI Properties...";

                return false;
            }
            else
            {
                ekosInterface->call(QDBus::AutoDetect, "connectDevices");
                indiState = INDI_CONNECTING;

                currentOperationTime.start();

                qCDebug(KSTARS_EKOS_SCHEDULER) << "Connecting INDI Devices";

                return false;
            }
        }
        break;

        case INDI_CONNECTING:
        {
            QDBusReply<int> isINDIConnected = ekosInterface->call(QDBus::AutoDetect, "getINDIConnectionStatus");
            if (isINDIConnected.value() == EkosManager::EKOS_STATUS_SUCCESS)
            {
                appendLogText(i18n("INDI devices connected."));
                indiState = INDI_PROPERTY_CHECK;
                return false;
            }
            else if (isINDIConnected.value() == EkosManager::EKOS_STATUS_ERROR)
            {
                if (indiConnectFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("One or more INDI devices failed to connect. Retrying..."));
                    ekosInterface->call(QDBus::AutoDetect, "connectDevices");
                    return false;
                }

                appendLogText(i18n("INDI devices failed to connect. Check INDI control panel for details."));
                stop();
                return false;
            }
            // If 30 seconds passed, we retry
            else if (currentOperationTime.elapsed() > (30 * 1000))
            {
                if (indiConnectFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("One or more INDI devices failed to connect. Retrying..."));
                    ekosInterface->call(QDBus::AutoDetect, "connectDevices");
                    return false;
                }

                appendLogText(i18n("INDI devices connection timed out. Check INDI control panel for details."));
                stop();
                return false;
            }
            else
                return false;
        }
        break;

        case INDI_DISCONNECTING:
        {
            QDBusReply<int> isINDIConnected = ekosInterface->call(QDBus::AutoDetect, "getINDIConnectionStatus");
            if (isINDIConnected.value() == EkosManager::EKOS_STATUS_IDLE)
            {
                appendLogText(i18n("INDI devices disconnected."));
                indiState = INDI_IDLE;
                return true;
            }
        }
        break;

        case INDI_PROPERTY_CHECK:
        {
            // Check if mount and dome support parking or not.
            QDBusReply<bool> boolReply = mountInterface->call(QDBus::AutoDetect, "canPark");
            unparkMountCheck->setEnabled(boolReply.value());
            parkMountCheck->setEnabled(boolReply.value());

            //qDebug() << "Mount can park " << boolReply.value();

            boolReply = domeInterface->call(QDBus::AutoDetect, "canPark");
            unparkDomeCheck->setEnabled(boolReply.value());
            parkDomeCheck->setEnabled(boolReply.value());

            boolReply = captureInterface->call(QDBus::AutoDetect, "hasCoolerControl");
            warmCCDCheck->setEnabled(boolReply.value());

            QDBusReply<int> updateReply = weatherInterface->call(QDBus::AutoDetect, "getUpdatePeriod");
            if (updateReply.error().type() == QDBusError::NoError)
            {
                weatherCheck->setEnabled(true);
                if (updateReply.value() > 0)
                {
                    weatherTimer.setInterval(updateReply.value() * 1000);
                    connect(&weatherTimer, &QTimer::timeout, this, &Scheduler::checkWeather);
                    weatherTimer.start();

                    // Check weather initially
                    checkWeather();
                }
            }
            else
                weatherCheck->setEnabled(false);

            QDBusReply<bool> capReply = capInterface->call(QDBus::AutoDetect, "canPark");
            if (capReply.error().type() == QDBusError::NoError)
            {
                capCheck->setEnabled(capReply.value());
                uncapCheck->setEnabled(capReply.value());
            }
            else
            {
                capCheck->setEnabled(false);
                uncapCheck->setEnabled(false);
            }

            indiState = INDI_READY;
            return true;
        }
        break;

        case INDI_READY:
            return true;
    }

    return false;
}

bool Scheduler::checkStartupState()
{
    if (state == SCHEDULER_PAUSED)
        return false;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking Startup State...";

    switch (startupState)
    {
        case STARTUP_IDLE:
        {
            KNotification::event(QLatin1String("ObservatoryStartup"), i18n("Observatory is in the startup process"));

            qCDebug(KSTARS_EKOS_SCHEDULER) << "Startup Idle. Starting startup process...";

            // If Ekos is already started, we skip the script and move on to dome unpark step
            // unless we do not have light frames, then we skip all
            QDBusReply<int> isEkosStarted;
            isEkosStarted = ekosInterface->call(QDBus::AutoDetect, "getEkosStartingStatus");
            if (isEkosStarted.value() == EkosManager::EKOS_STATUS_SUCCESS)
            {
                if (startupScriptURL.isEmpty() == false)
                    appendLogText(i18n("Ekos is already started, skipping startup script..."));

                if (currentJob->getLightFramesRequired())
                    startupState = STARTUP_UNPARK_DOME;
                else
                    startupState = STARTUP_COMPLETE;
                return true;
            }

            if (schedulerProfileCombo->currentText() != i18n("Default"))
            {
                QList<QVariant> profile;
                profile.append(schedulerProfileCombo->currentText());
                ekosInterface->callWithArgumentList(QDBus::AutoDetect, "setProfile", profile);
            }

            if (startupScriptURL.isEmpty() == false)
            {
                startupState = STARTUP_SCRIPT;
                executeScript(startupScriptURL.toString(QUrl::PreferLocalFile));
                return false;
            }

            startupState = STARTUP_UNPARK_DOME;
            return false;
        }
        break;

        case STARTUP_SCRIPT:
            return false;
            break;

        case STARTUP_UNPARK_DOME:
            // If there is no job in case of manual startup procedure,
            // or if the job requires light frames, let's proceed with
            // unparking the dome, otherwise startup process is complete.
            if (currentJob == nullptr || currentJob->getLightFramesRequired())
            {
                if (unparkDomeCheck->isEnabled() && unparkDomeCheck->isChecked())
                    unParkDome();
                else
                    startupState = STARTUP_UNPARK_MOUNT;
            }
            else
            {
                startupState = STARTUP_COMPLETE;
                return true;
            }

            break;

        case STARTUP_UNPARKING_DOME:
            checkDomeParkingStatus();
            break;

        case STARTUP_UNPARK_MOUNT:
            if (unparkMountCheck->isEnabled() && unparkMountCheck->isChecked())
                unParkMount();
            else
                startupState = STARTUP_UNPARK_CAP;
            break;

        case STARTUP_UNPARKING_MOUNT:
            checkMountParkingStatus();
            break;

        case STARTUP_UNPARK_CAP:
            if (uncapCheck->isEnabled() && uncapCheck->isChecked())
                unParkCap();
            else
                startupState = STARTUP_COMPLETE;
            break;

        case STARTUP_UNPARKING_CAP:
            checkCapParkingStatus();
            break;

        case STARTUP_COMPLETE:
            return true;

        case STARTUP_ERROR:
            stop();
            return true;
            break;
    }

    return false;
}

bool Scheduler::checkShutdownState()
{
    if (state == SCHEDULER_PAUSED)
        return false;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking shutown state...";

    switch (shutdownState)
    {
        case SHUTDOWN_IDLE:
            KNotification::event(QLatin1String("ObservatoryShutdown"), i18n("Observatory is in the shutdown process"));

            qCInfo(KSTARS_EKOS_SCHEDULER) << "Starting shutdown process...";

            weatherTimer.stop();
            weatherTimer.disconnect();
            weatherLabel->hide();

            jobTimer.stop();

            setCurrentJob(nullptr);

            if (state == SCHEDULER_RUNNIG)
                schedulerTimer.start();

            if (preemptiveShutdown == false)
            {
                sleepTimer.stop();
                //sleepTimer.disconnect();
            }

            if (warmCCDCheck->isEnabled() && warmCCDCheck->isChecked())
            {
                appendLogText(i18n("Warming up CCD..."));

                // Turn it off
                QVariant arg(false);
                captureInterface->call(QDBus::AutoDetect, "setCoolerControl", arg);
            }

            // The following steps require a connection to the INDI server
            if (isINDIConnected())
            {
                if (capCheck->isEnabled() && capCheck->isChecked())
                {
                    shutdownState = SHUTDOWN_PARK_CAP;
                    return false;
                }

                if (parkMountCheck->isEnabled() && parkMountCheck->isChecked())
                {
                    shutdownState = SHUTDOWN_PARK_MOUNT;
                    return false;
                }

                if (parkDomeCheck->isEnabled() && parkDomeCheck->isChecked())
                {
                    shutdownState = SHUTDOWN_PARK_DOME;
                    return false;
                }
            }
            else appendLogText(i18n("Warning: Bypassing parking procedures, no INDI connection."));

            if (shutdownScriptURL.isEmpty() == false)
            {
                shutdownState = SHUTDOWN_SCRIPT;
                return false;
            }

            shutdownState = SHUTDOWN_COMPLETE;
            return true;
            break;

        case SHUTDOWN_PARK_CAP:
            if (!isINDIConnected())
            {
                qCInfo(KSTARS_EKOS_SCHEDULER) << "Bypassing shutdown step 'park cap', no INDI connection.";
                shutdownState = SHUTDOWN_SCRIPT;
            }
            else if (capCheck->isEnabled() && capCheck->isChecked())
                parkCap();
            else
                shutdownState = SHUTDOWN_PARK_MOUNT;
            break;

        case SHUTDOWN_PARKING_CAP:
            checkCapParkingStatus();
            break;

        case SHUTDOWN_PARK_MOUNT:
            if (!isINDIConnected())
            {
                qCInfo(KSTARS_EKOS_SCHEDULER) << "Bypassing shutdown step 'park cap', no INDI connection.";
                shutdownState = SHUTDOWN_SCRIPT;
            }
            else if (parkMountCheck->isEnabled() && parkMountCheck->isChecked())
                parkMount();
            else
                shutdownState = SHUTDOWN_PARK_DOME;
            break;

        case SHUTDOWN_PARKING_MOUNT:
            checkMountParkingStatus();
            break;

        case SHUTDOWN_PARK_DOME:
            if (!isINDIConnected())
            {
                qCInfo(KSTARS_EKOS_SCHEDULER) << "Bypassing shutdown step 'park cap', no INDI connection.";
                shutdownState = SHUTDOWN_SCRIPT;
            }
            else if (parkDomeCheck->isEnabled() && parkDomeCheck->isChecked())
                parkDome();
            else
                shutdownState = SHUTDOWN_SCRIPT;
            break;

        case SHUTDOWN_PARKING_DOME:
            checkDomeParkingStatus();
            break;

        case SHUTDOWN_SCRIPT:
            if (shutdownScriptURL.isEmpty() == false)
            {
                // Need to stop Ekos now before executing script if it happens to stop INDI
                if (ekosState != EKOS_IDLE && Options::shutdownScriptTerminatesINDI())
                {
                    stopEkos();
                    return false;
                }

                shutdownState = SHUTDOWN_SCRIPT_RUNNING;
                executeScript(shutdownScriptURL.toString(QUrl::PreferLocalFile));
            }
            else
                shutdownState = SHUTDOWN_COMPLETE;
            break;

        case SHUTDOWN_SCRIPT_RUNNING:
            return false;

        case SHUTDOWN_COMPLETE:
            return true;

        case SHUTDOWN_ERROR:
            stop();
            return true;
            break;
    }

    return false;
}

bool Scheduler::checkParkWaitState()
{
    if (state == SCHEDULER_PAUSED)
        return false;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking Park Wait State...";

    switch (parkWaitState)
    {
        case PARKWAIT_IDLE:
            return true;

        case PARKWAIT_PARK:
            parkMount();
            break;

        case PARKWAIT_PARKING:
            checkMountParkingStatus();
            break;

        case PARKWAIT_PARKED:
            return true;

        case PARKWAIT_UNPARK:
            unParkMount();
            break;

        case PARKWAIT_UNPARKING:
            checkMountParkingStatus();
            break;

        case PARKWAIT_UNPARKED:
            return true;

        case PARKWAIT_ERROR:
            appendLogText(i18n("park/unpark wait procedure failed, aborting..."));
            stop();
            return true;
            break;
    }

    return false;
}

void Scheduler::executeScript(const QString &filename)
{
    appendLogText(i18n("Executing script %1...", filename));

    connect(&scriptProcess, &QProcess::readyReadStandardOutput, this, &Scheduler::readProcessOutput);

    connect(&scriptProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus){checkProcessExit(exitCode);});

    scriptProcess.start(filename);
}

void Scheduler::readProcessOutput()
{
    appendLogText(scriptProcess.readAllStandardOutput().simplified());
}

void Scheduler::checkProcessExit(int exitCode)
{
    scriptProcess.disconnect();

    if (exitCode == 0)
    {
        if (startupState == STARTUP_SCRIPT)
            startupState = STARTUP_UNPARK_DOME;
        else if (shutdownState == SHUTDOWN_SCRIPT_RUNNING)
            shutdownState = SHUTDOWN_COMPLETE;

        return;
    }

    if (startupState == STARTUP_SCRIPT)
    {
        appendLogText(i18n("Startup script failed, aborting..."));
        startupState = STARTUP_ERROR;
    }
    else if (shutdownState == SHUTDOWN_SCRIPT_RUNNING)
    {
        appendLogText(i18n("Shutdown script failed, aborting..."));
        shutdownState = SHUTDOWN_ERROR;
    }
}

bool Scheduler::checkStatus()
{
    if (state == SCHEDULER_PAUSED)
        return true;

    // #1 If no current job selected, let's check if we need to shutdown or evaluate jobs
    if (currentJob == nullptr)
    {
        // #2.1 If shutdown is already complete or in error, we need to stop
        if (shutdownState == SHUTDOWN_COMPLETE || shutdownState == SHUTDOWN_ERROR)
        {
            // If INDI is not done disconnecting, try again later
            if (indiState == INDI_DISCONNECTING && checkINDIState() == false)
                return false;

            // Disconnect INDI if required first
            if (indiState != INDI_IDLE && Options::stopEkosAfterShutdown())
            {
                disconnectINDI();
                return false;
            }

            // If Ekos is not done stopping, try again later
            if (ekosState == EKOS_STOPPING && checkEkosState() == false)
                return false;

            // Stop Ekos if required.
            if (ekosState != EKOS_IDLE && Options::stopEkosAfterShutdown())
            {
                stopEkos();
                return false;
            }

            if (shutdownState == SHUTDOWN_COMPLETE)
                appendLogText(i18n("Shutdown complete."));
            else
                appendLogText(i18n("Shutdown procedure failed, aborting..."));

            // Stop Scheduler
            stop();

            return true;
        }

        // #2.2  Check if shutdown is in progress
        if (shutdownState > SHUTDOWN_IDLE)
        {
            // If Ekos is not done stopping, try again later
            if (ekosState == EKOS_STOPPING && checkEkosState() == false)
                return false;

            checkShutdownState();
            return false;
        }

        // #2.3 Check if park wait procedure is in progress
        if (checkParkWaitState() == false)
            return false;

        // #2.4 If not in shutdown state, evaluate the jobs
        evaluateJobs();
    }
    else
    {
        // #3 Check if startup procedure has failed.
        if (startupState == STARTUP_ERROR)
        {
            // Stop Scheduler
            stop();
            return true;
        }

        // #4 Check if startup procedure Phase #1 is complete (Startup script)
        if ((startupState == STARTUP_IDLE && checkStartupState() == false) || startupState == STARTUP_SCRIPT)
            return false;

        // #5 Check if Ekos is started
        if (checkEkosState() == false)
            return false;

        // #6 Check if INDI devices are connected.
        if (checkINDIState() == false)
            return false;

        // #6.1 Check if park wait procedure is in progress - in the case we're waiting for a distant job
        if (checkParkWaitState() == false)
            return false;

        // #7 Check if startup procedure Phase #2 is complete (Unparking phase)
        if (startupState > STARTUP_SCRIPT && startupState < STARTUP_ERROR && checkStartupState() == false)
            return false;

        // #8 Execute the job
        executeJob(currentJob);
    }

    return true;
}

void Scheduler::checkJobStage()
{
    if (state == SCHEDULER_PAUSED)
        return;

    Q_ASSERT_X(currentJob, __FUNCTION__, "Actual current job is required to check job stage");
    if (!currentJob)
        return;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Checking job stage for" << currentJob->getName() << "startup" << currentJob->getStartupCondition() << currentJob->getStartupTime().toString(currentJob->getDateTimeDisplayFormat()) << "state" << currentJob->getState();

    QDateTime const now = KStarsData::Instance()->lt();

    /* Refresh the score of the current job */
    /* currentJob->setScore(calculateJobScore(currentJob, now)); */

    /* If current job is scheduled and has not started yet, wait */
    if (SchedulerJob::JOB_SCHEDULED == currentJob->getState())
        if (now < currentJob->getStartupTime())
            return;

    // #1 Check if we need to stop at some point
    if (currentJob->getCompletionCondition() == SchedulerJob::FINISH_AT &&
        currentJob->getState() == SchedulerJob::JOB_BUSY)
    {
        // If the job reached it COMPLETION time, we stop it.
        if (now.secsTo(currentJob->getCompletionTime()) <= 0)
        {
            appendLogText(i18n("Job '%1' reached completion time %2, stopping.", currentJob->getName(),
                               currentJob->getCompletionTime().toString(currentJob->getDateTimeDisplayFormat())));
            currentJob->setState(SchedulerJob::JOB_ABORTED);
            stopCurrentJobAction();
            stopGuiding();
            findNextJob();
            return;
        }
    }

    // #2 Check if altitude restriction still holds true
    if (currentJob->getMinAltitude() > 0)
    {
        SkyPoint p = currentJob->getTargetCoords();

        p.EquatorialToHorizontal(KStarsData::Instance()->lst(), geo->lat());

        /* FIXME: find a way to use altitude cutoff here, because the job can be scheduled when evaluating, then aborted when running */
        if (p.alt().Degrees() < currentJob->getMinAltitude())
        {
            // Only terminate job due to altitude limitation if mount is NOT parked.
            if (isMountParked() == false)
            {
                appendLogText(i18n("Job '%1' current altitude (%2 degrees) crossed minimum constraint altitude (%3 degrees), "
                                   "marking aborted.",
                                   currentJob->getName(), p.alt().Degrees(), currentJob->getMinAltitude()));

                currentJob->setState(SchedulerJob::JOB_ABORTED);
                stopCurrentJobAction();
                stopGuiding();
                findNextJob();
                return;
            }
        }
    }

    // #3 Check if moon separation is still valid
    if (currentJob->getMinMoonSeparation() > 0)
    {
        SkyPoint p = currentJob->getTargetCoords();
        p.EquatorialToHorizontal(KStarsData::Instance()->lst(), geo->lat());

        double moonSeparation = getCurrentMoonSeparation(currentJob);

        if (moonSeparation < currentJob->getMinMoonSeparation())
        {
            // Only terminate job due to moon separation limitation if mount is NOT parked.
            if (isMountParked() == false)
            {
                appendLogText(i18n("Job '%2' current moon separation (%1 degrees) is lower than minimum constraint (%3 "
                                   "degrees), marking aborted.",
                                   moonSeparation, currentJob->getName(), currentJob->getMinMoonSeparation()));

                currentJob->setState(SchedulerJob::JOB_ABORTED);
                stopCurrentJobAction();
                stopGuiding();
                findNextJob();
                return;
            }
        }
    }

    // #4 Check if we're not at dawn
    if (currentJob->getEnforceTwilight() && now > KStarsDateTime(preDawnDateTime))
    {
        // If either mount or dome are not parked, we shutdown if we approach dawn
        if (isMountParked() == false || (parkDomeCheck->isEnabled() && isDomeParked() == false))
        {
            // Minute is a DOUBLE value, do not use i18np
            appendLogText(i18n(
                "Job '%3' is now approaching astronomical twilight rise limit at %1 (%2 minutes safety margin), marking aborted.",
                preDawnDateTime.toString(), Options::preDawnTime(), currentJob->getName()));
            currentJob->setState(SchedulerJob::JOB_ABORTED);
            return;
        }
    }

    // #5 Check system status to improve robustness
    // This handles external events such as disconnections or end-user manipulating INDI panel
    if (!checkStatus())
        return;

    switch (currentJob->getStage())
    {
        case SchedulerJob::STAGE_IDLE:
            getNextAction();
            break;

        case SchedulerJob::STAGE_SLEWING:
        {
            QDBusReply<int> slewStatus = mountInterface->call(QDBus::AutoDetect, "getSlewStatus");
            bool isDomeMoving          = false;

            if (parkDomeCheck->isEnabled())
            {
                QDBusReply<bool> domeReply = domeInterface->call(QDBus::AutoDetect, "isMoving");
                if (domeReply.error().type() == QDBusError::NoError && domeReply.value() == true)
                    isDomeMoving = true;
            }

            if (slewStatus.error().type() == QDBusError::UnknownObject)
            {
                appendLogText(i18n("Warning: job '%1' lost connection to INDI while slewing, attempting to reconnect.", currentJob->getName()));
                if (!manageConnectionLoss())
                    currentJob->setState(SchedulerJob::JOB_ERROR);
                return;
            }

            qCDebug(KSTARS_EKOS_SCHEDULER)
                << "Slewing Stage... Slew Status is " << pstateStr(static_cast<IPState>(slewStatus.value()));

            if (slewStatus.value() == IPS_OK && isDomeMoving == false)
            {
                appendLogText(i18n("Job '%1' slew is complete.", currentJob->getName()));
                currentJob->setStage(SchedulerJob::STAGE_SLEW_COMPLETE);
                getNextAction();
            }
            else if (slewStatus.value() == IPS_ALERT)
            {
                appendLogText(i18n("Warning: job '%1' slew failed, marking terminated due to errors.", currentJob->getName()));
                currentJob->setState(SchedulerJob::JOB_ERROR);

                findNextJob();
            }
            else if (slewStatus.value() == IPS_IDLE)
            {
                appendLogText(i18n("Warning: job '%1' found not slewing, restarting.", currentJob->getName()));
                currentJob->setStage(SchedulerJob::STAGE_IDLE);

                getNextAction();
            }
        }
        break;

        case SchedulerJob::STAGE_FOCUSING:        
        {
            QDBusReply<int> focusReply = focusInterface->call(QDBus::AutoDetect, "getStatus");

            if (focusReply.error().type() == QDBusError::UnknownObject)
            {
                appendLogText(i18n("Warning: job '%1' lost connection to INDI server while focusing, attempting to reconnect.", currentJob->getName()));
                if (!manageConnectionLoss())
                    currentJob->setState(SchedulerJob::JOB_ERROR);
                return;
            }

            qCDebug(KSTARS_EKOS_SCHEDULER) << "Focus stage...";

            Ekos::FocusState focusStatus = static_cast<Ekos::FocusState>(focusReply.value());

            // Is focus complete?
            if (focusStatus == Ekos::FOCUS_COMPLETE)
            {
                appendLogText(i18n("Job '%1' focusing is complete.", currentJob->getName()));

                autofocusCompleted = true;

                currentJob->setStage(SchedulerJob::STAGE_FOCUS_COMPLETE);

                getNextAction();
            }
            else if (focusStatus == Ekos::FOCUS_FAILED || focusStatus == Ekos::FOCUS_ABORTED)
            {
                appendLogText(i18n("Warning: job '%1' focusing failed.", currentJob->getName()));

                if (focusFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Job '%1' is restarting its focusing procedure.", currentJob->getName()));
                    // Reset frame to original size.
                    focusInterface->call(QDBus::AutoDetect, "resetFrame");
                    // Restart focusing
                    startFocusing();
                }
                else
                {
                    appendLogText(i18n("Warning: job '%1' focusing procedure failed, marking terminated due to errors.", currentJob->getName()));
                    currentJob->setState(SchedulerJob::JOB_ERROR);

                    findNextJob();
                }
            }
        }
        break;

        /*case SchedulerJob::STAGE_POSTALIGN_FOCUSING:
        focusInterface->call(QDBus::AutoDetect,"resetFrame");
        currentJob->setStage(SchedulerJob::STAGE_POSTALIGN_FOCUSING_COMPLETE);
        getNextAction();
        break;*/

        case SchedulerJob::STAGE_ALIGNING:
        {
            QDBusReply<int> alignReply;

            qCDebug(KSTARS_EKOS_SCHEDULER) << "Alignment stage...";

            alignReply = alignInterface->call(QDBus::AutoDetect, "getStatus");

            if (alignReply.error().type() == QDBusError::UnknownObject)
            {
                appendLogText(i18n("Warning: job '%1' lost connection to INDI server while aligning, attempting to reconnect.", currentJob->getName()));
                if (!manageConnectionLoss())
                    currentJob->setState(SchedulerJob::JOB_ERROR);
                return;
            }

            Ekos::AlignState alignStatus = static_cast<Ekos::AlignState>(alignReply.value());

            // Is solver complete?
            if (alignStatus == Ekos::ALIGN_COMPLETE)
            {
                appendLogText(i18n("Job '%1' alignment is complete.", currentJob->getName()));
                alignFailureCount = 0;

                currentJob->setStage(SchedulerJob::STAGE_ALIGN_COMPLETE);
                getNextAction();
            }
            else if (alignStatus == Ekos::ALIGN_FAILED || alignStatus == Ekos::ALIGN_ABORTED)
            {
                appendLogText(i18n("Warning: job '%1' alignment failed.", currentJob->getName()));

                if (alignFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    if (Options::resetMountModelOnAlignFail() && MAX_FAILURE_ATTEMPTS-1 < alignFailureCount)
                    {
                        appendLogText(i18n("Warning: job '%1' forcing mount model reset after failing alignment #%2.", currentJob->getName(), alignFailureCount));
                        mountInterface->call(QDBus::AutoDetect, "resetModel");
                    }
                    appendLogText(i18n("Restarting %1 alignment procedure...", currentJob->getName()));
                    startAstrometry();
                }
                else
                {
                    appendLogText(i18n("Warning: job '%1' alignment procedure failed, aborting job.", currentJob->getName()));
                    currentJob->setState(SchedulerJob::JOB_ABORTED);

                    findNextJob();
                }
            }
        }
        break;

        case SchedulerJob::STAGE_RESLEWING:
        {
            QDBusReply<int> slewStatus = mountInterface->call(QDBus::AutoDetect, "getSlewStatus");
            bool isDomeMoving          = false;

            qCDebug(KSTARS_EKOS_SCHEDULER) << "Re-slewing stage...";

            if (parkDomeCheck->isEnabled())
            {
                QDBusReply<bool> domeReply = domeInterface->call(QDBus::AutoDetect, "isMoving");
                if (domeReply.error().type() == QDBusError::NoError && domeReply.value() == true)
                    isDomeMoving = true;
            }

            if (slewStatus.error().type() == QDBusError::UnknownObject)
            {
                appendLogText(i18n("Warning: job '%1' lost connection to INDI server while reslewing, attempting to reconnect.",currentJob->getName()));
                if (!manageConnectionLoss())
                    currentJob->setState(SchedulerJob::JOB_ERROR);
            }
            else if (slewStatus.value() == IPS_OK && isDomeMoving == false)
            {
                appendLogText(i18n("Job '%1' repositioning is complete.", currentJob->getName()));
                currentJob->setStage(SchedulerJob::STAGE_RESLEWING_COMPLETE);
                getNextAction();
            }
            else if (slewStatus.value() == IPS_ALERT)
            {
                appendLogText(i18n("Warning: job '%1' repositioning failed, marking terminated due to errors.", currentJob->getName()));
                currentJob->setState(SchedulerJob::JOB_ERROR);

                findNextJob();
            }
            else if (slewStatus.value() == IPS_IDLE)
            {
                appendLogText(i18n("Warning: job '%1' found not repositioning, restarting.", currentJob->getName()));
                currentJob->setStage(SchedulerJob::STAGE_IDLE);

                getNextAction();
            }
        }
        break;

        case SchedulerJob::STAGE_GUIDING:
        {
            QDBusReply<int> guideReply = guideInterface->call(QDBus::AutoDetect, "getStatus");

            qCDebug(KSTARS_EKOS_SCHEDULER) << "Calibration & Guide stage...";

            if (guideReply.error().type() == QDBusError::UnknownObject)
            {
                appendLogText(i18n("Warning: job '%1' lost connection to INDI server while guiding, attempting to reconnect.",currentJob->getName()));
                if (!manageConnectionLoss())
                    currentJob->setState(SchedulerJob::JOB_ERROR);
                return;
            }

            Ekos::GuideState guideStatus = static_cast<Ekos::GuideState>(guideReply.value());

            // If calibration stage complete?
            if (guideStatus == Ekos::GUIDE_GUIDING)
            {
                appendLogText(i18n("Job '%1' guiding is in progress.", currentJob->getName()));
                guideFailureCount = 0;

                currentJob->setStage(SchedulerJob::STAGE_GUIDING_COMPLETE);
                getNextAction();
            }
            // JM 2018-07-30: GUIDE_IDLE is also a failure
            else if (guideStatus == Ekos::GUIDE_CALIBRATION_ERROR ||
                     guideStatus == Ekos::GUIDE_ABORTED)
            {
                if (guideStatus == Ekos::GUIDE_ABORTED)
                    appendLogText(i18n("Warning: job '%1' guiding failed.", currentJob->getName()));
                else
                    appendLogText(i18n("Warning: job '%1' calibration failed.", currentJob->getName()));

                if (guideFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    if (guideStatus == Ekos::GUIDE_CALIBRATION_ERROR &&
                        Options::realignAfterCalibrationFailure())
                    {
                        appendLogText(i18n("Restarting %1 alignment procedure...", currentJob->getName()));
                        // JM: We have to go back to startSlew() since if we just call startAstrometry()
                        // It would captureAndSolve at the _current_ coords which could be way off center if the calibration
                        // process took a wild ride search for a suitable guide star and then failed. So startSlew() would ensure
                        // we're back on our target and then it proceed to alignment (focus is skipped since it is done if it was checked anyway).
                        startSlew();
                    }
                    else
                    {
                        appendLogText(i18n("Job '%1' is guiding, and is restarting its guiding procedure.", currentJob->getName()));
                        startGuiding(true);
                    }
                }
                else
                {
                    appendLogText(i18n("Warning: job '%1' guiding procedure failed, marking terminated due to errors.", currentJob->getName()));
                    currentJob->setState(SchedulerJob::JOB_ERROR);

                    findNextJob();
                }
            }
        }
        break;

        case SchedulerJob::STAGE_CAPTURING:
        {
            QDBusReply<QString> captureReply = captureInterface->call(QDBus::AutoDetect, "getSequenceQueueStatus");

            if (captureReply.error().type() == QDBusError::UnknownObject)
            {
                appendLogText(i18n("Warning: job '%1' lost connection to INDI server while capturing, attempting to reconnect.",currentJob->getName()));
                if (!manageConnectionLoss())
                    currentJob->setState(SchedulerJob::JOB_ERROR);
            }
            else if (captureReply.value().toStdString() == "Aborted" || captureReply.value().toStdString() == "Error")
            {
                appendLogText(i18n("Warning: job '%1' failed to capture target (%2).", currentJob->getName(), captureReply.value()));

                if (captureFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    // If capture failed due to guiding error, let's try to restart that
                    if (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE)
                    {
                        // Check if it is guiding related.
                        QDBusReply<int> guideReply = guideInterface->call(QDBus::AutoDetect, "getStatus");
                        if (guideReply.value() == Ekos::GUIDE_ABORTED ||
                                guideReply.value() == Ekos::GUIDE_CALIBRATION_ERROR ||
                                guideReply.value() == GUIDE_DITHERING_ERROR)
                            // If guiding failed, let's restart it
                            //if(guideReply.value() == false)
                        {
                            appendLogText(i18n("Job '%1' is capturing, and is restarting its guiding procedure.", currentJob->getName()));
                            //currentJob->setStage(SchedulerJob::STAGE_GUIDING);
                            startGuiding(true);
                            return;
                        }
                    }

                    /* FIXME: it's not clear whether it is actually possible to continue capturing when capture fails this way */
                    appendLogText(i18n("Warning: job '%1' failed its capture procedure, restarting capture.", currentJob->getName()));
                    startCapture();
                }
                else
                {
                    /* FIXME: it's not clear whether this situation can be recovered at all */
                    appendLogText(i18n("Warning: job '%1' failed its capture procedure, marking aborted.", currentJob->getName()));
                    currentJob->setState(SchedulerJob::JOB_ABORTED);

                    findNextJob();
                }
            }
            else if (captureReply.value().toStdString() == "Complete")
            {
                KNotification::event(QLatin1String("EkosScheduledImagingFinished"),
                                     i18n("Ekos job (%1) - Capture finished", currentJob->getName()));


                captureInterface->call(QDBus::AutoDetect, "clearSequenceQueue");

                currentJob->setState(SchedulerJob::JOB_COMPLETE);
                findNextJob();
            }
            else
            {
                captureFailureCount = 0;
                /* currentJob->setCompletedCount(currentJob->getCompletedCount() + 1); */
            }
        }
        break;

        default:
            break;
    }
}

void Scheduler::getNextAction()
{
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Get next action...";

    switch (currentJob->getStage())
    {
        case SchedulerJob::STAGE_IDLE:
            if (currentJob->getLightFramesRequired())
            {
                if (currentJob->getStepPipeline() & SchedulerJob::USE_TRACK)
                    startSlew();
                else if (currentJob->getStepPipeline() & SchedulerJob::USE_FOCUS && autofocusCompleted == false)
                    startFocusing();
                else if (currentJob->getStepPipeline() & SchedulerJob::USE_ALIGN)
                    startAstrometry();
                else if (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE)
                    startGuiding();
                else
                    startCapture();
            }
            else
            {
                if (currentJob->getStepPipeline())
                    appendLogText(
                        i18n("Job '%1' is proceeding directly to capture stage because only calibration frames are pending.", currentJob->getName()));
                startCapture();
            }

            break;

        case SchedulerJob::STAGE_SLEW_COMPLETE:
            if (currentJob->getStepPipeline() & SchedulerJob::USE_FOCUS && autofocusCompleted == false)
                startFocusing();
            else if (currentJob->getStepPipeline() & SchedulerJob::USE_ALIGN)
                startAstrometry();
            else if (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE)
                startGuiding();
            else
                startCapture();
            break;

        case SchedulerJob::STAGE_FOCUS_COMPLETE:
            if (currentJob->getStepPipeline() & SchedulerJob::USE_ALIGN)
                startAstrometry();
            else if (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE)
                startGuiding();
            else
                startCapture();
            break;

        case SchedulerJob::STAGE_ALIGN_COMPLETE:
            currentJob->setStage(SchedulerJob::STAGE_RESLEWING);
            break;

        case SchedulerJob::STAGE_RESLEWING_COMPLETE:
            // If we have in-sequence-focus in the sequence file then we perform post alignment focusing so that the focus
            // frame is ready for the capture module in-sequence-focus procedure.
            if ((currentJob->getStepPipeline() & SchedulerJob::USE_FOCUS) && currentJob->getInSequenceFocus())
                // Post alignment re-focusing
                startFocusing();
            else if (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE)
                startGuiding();
            else
                startCapture();
            break;

        case SchedulerJob::STAGE_POSTALIGN_FOCUSING_COMPLETE:
            if (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE)
                startGuiding();
            else
                startCapture();
            break;

        case SchedulerJob::STAGE_GUIDING_COMPLETE:
            startCapture();
            break;

        default:
            break;
    }
}

void Scheduler::stopCurrentJobAction()
{
    if (nullptr != currentJob)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Job '" << currentJob->getName() << "' is stopping current action..." << currentJob->getStage();

        switch (currentJob->getStage())
        {
            case SchedulerJob::STAGE_IDLE:
                break;

            case SchedulerJob::STAGE_SLEWING:
                mountInterface->call(QDBus::AutoDetect, "abort");
                break;

            case SchedulerJob::STAGE_FOCUSING:
                focusInterface->call(QDBus::AutoDetect, "abort");
                break;

            case SchedulerJob::STAGE_ALIGNING:
                alignInterface->call(QDBus::AutoDetect, "abort");
                break;

                //case SchedulerJob::STAGE_CALIBRATING:
                //        guideInterface->call(QDBus::AutoDetect,"stopCalibration");
                //    break;

            case SchedulerJob::STAGE_GUIDING:
                stopGuiding();
                break;

            case SchedulerJob::STAGE_CAPTURING:
                captureInterface->call(QDBus::AutoDetect, "abort");
                //stopGuiding();
                break;

            default:
                break;
        }

        /* Reset interrupted job stage */
        currentJob->setStage(SchedulerJob::STAGE_IDLE);
    }
}

bool Scheduler::manageConnectionLoss()
{
    if (SCHEDULER_RUNNIG != state)
        return false;

    // Don't manage loss if Ekos is actually down in the state machine
    switch (ekosState)
    {
        case EKOS_IDLE:
        case EKOS_STOPPING:
            return false;

        default: break;
    }

    // Don't manage loss if INDI is actually down in the state machine
    switch (indiState)
    {
        case INDI_IDLE:
        case INDI_DISCONNECTING:
            return false;

        default: break;
    }

    // If Ekos is assumed to be up, check its state
    QDBusReply<int> const isEkosStarted = ekosInterface->call(QDBus::AutoDetect, "getEkosStartingStatus");
    if (isEkosStarted.value() == EkosManager::EKOS_STATUS_SUCCESS)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Ekos is currently connected, checking INDI before mitigating connection loss.");

        // If INDI is assumed to be up, check its state
        if (isINDIConnected())
        {
            // If both Ekos and INDI are assumed up, and are actually up, no mitigation needed, this is a DBus interface error
            qCDebug(KSTARS_EKOS_SCHEDULER) << QString("INDI is currently connected, no connection loss mitigation needed.");
            return false;
        }
    }

    // Stop actions of the current job
    stopCurrentJobAction();

    // Stop guiding, in case we are using it
    stopGuiding();

    // Acknowledge INDI and Ekos disconnections
    disconnectINDI();
    stopEkos();

    // Let the Scheduler attempt to connect INDI again
    return true;
}

void Scheduler::load()
{
    QUrl fileURL =
        QFileDialog::getOpenFileUrl(this, i18n("Open Ekos Scheduler List"), dirPath, "Ekos Scheduler List (*.esl)");
    if (fileURL.isEmpty())
        return;

    if (fileURL.isValid() == false)
    {
        QString message = i18n("Invalid URL: %1", fileURL.toLocalFile());
        KMessageBox::sorry(0, message, i18n("Invalid URL"));
        return;
    }

    dirPath = QUrl(fileURL.url(QUrl::RemoveFilename));

    /* Run a job idle evaluation after a successful load */
    if (loadScheduler(fileURL.toLocalFile()))
        startJobEvaluation();
}

bool Scheduler::loadScheduler(const QString &fileURL)
{
    QFile sFile;
    sFile.setFileName(fileURL);

    if (!sFile.open(QIODevice::ReadOnly))
    {
        QString message = i18n("Unable to open file %1", fileURL);
        KMessageBox::sorry(0, message, i18n("Could Not Open File"));
        return false;
    }

    if (jobUnderEdit >= 0)
        resetJobEdit();

    qDeleteAll(jobs);
    jobs.clear();
    while (queueTable->rowCount() > 0)
        queueTable->removeRow(0);

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
                const char *tag = tagXMLEle(ep);
                if (!strcmp(tag, "Job"))
                    processJobInfo(ep);
                else if (!strcmp(tag, "Profile"))
                {
                    schedulerProfileCombo->setCurrentText(pcdataXMLEle(ep));
                }
                else if (!strcmp(tag, "StartupProcedure"))
                {
                    XMLEle *procedure;
                    startupScript->clear();
                    unparkDomeCheck->setChecked(false);
                    unparkMountCheck->setChecked(false);
                    uncapCheck->setChecked(false);

                    for (procedure = nextXMLEle(ep, 1); procedure != nullptr; procedure = nextXMLEle(ep, 0))
                    {
                        const char *proc = pcdataXMLEle(procedure);

                        if (!strcmp(proc, "StartupScript"))
                        {
                            startupScript->setText(findXMLAttValu(procedure, "value"));
                            startupScriptURL = QUrl::fromUserInput(startupScript->text());
                        }
                        else if (!strcmp(proc, "UnparkDome"))
                            unparkDomeCheck->setChecked(true);
                        else if (!strcmp(proc, "UnparkMount"))
                            unparkMountCheck->setChecked(true);
                        else if (!strcmp(proc, "UnparkCap"))
                            uncapCheck->setChecked(true);
                    }
                }
                else if (!strcmp(tag, "ShutdownProcedure"))
                {
                    XMLEle *procedure;
                    shutdownScript->clear();
                    warmCCDCheck->setChecked(false);
                    parkDomeCheck->setChecked(false);
                    parkMountCheck->setChecked(false);
                    capCheck->setChecked(false);

                    for (procedure = nextXMLEle(ep, 1); procedure != nullptr; procedure = nextXMLEle(ep, 0))
                    {
                        const char *proc = pcdataXMLEle(procedure);

                        if (!strcmp(proc, "ShutdownScript"))
                        {
                            shutdownScript->setText(findXMLAttValu(procedure, "value"));
                            shutdownScriptURL = QUrl::fromUserInput(shutdownScript->text());
                        }
                        else if (!strcmp(proc, "ParkDome"))
                            parkDomeCheck->setChecked(true);
                        else if (!strcmp(proc, "ParkMount"))
                            parkMountCheck->setChecked(true);
                        else if (!strcmp(proc, "ParkCap"))
                            capCheck->setChecked(true);
                        else if (!strcmp(proc, "WarmCCD"))
                            warmCCDCheck->setChecked(true);
                    }
                }
            }
            delXMLEle(root);
        }
        else if (errmsg[0])
        {
            appendLogText(QString(errmsg));
            delLilXML(xmlParser);
            return false;
        }
    }

    schedulerURL = QUrl::fromLocalFile(fileURL);
    mosaicB->setEnabled(true);
    mDirty = false;
    delLilXML(xmlParser);
    return true;
}

bool Scheduler::processJobInfo(XMLEle *root)
{
    XMLEle *ep;
    XMLEle *subEP;

    altConstraintCheck->setChecked(false);
    moonSeparationCheck->setChecked(false);
    weatherCheck->setChecked(false);

    twilightCheck->blockSignals(true);
    twilightCheck->setChecked(false);
    twilightCheck->blockSignals(false);

    minAltitude->setValue(minAltitude->minimum());
    minMoonSeparation->setValue(minMoonSeparation->minimum());

    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep), "Name"))
            nameEdit->setText(pcdataXMLEle(ep));
        else if (!strcmp(tagXMLEle(ep), "Priority"))
            prioritySpin->setValue(atoi(pcdataXMLEle(ep)));
        else if (!strcmp(tagXMLEle(ep), "Coordinates"))
        {
            subEP = findXMLEle(ep, "J2000RA");
            if (subEP)
            {
                dms ra;
                ra.setH(atof(pcdataXMLEle(subEP)));
                raBox->showInHours(ra);
            }
            subEP = findXMLEle(ep, "J2000DE");
            if (subEP)
            {
                dms de;
                de.setD(atof(pcdataXMLEle(subEP)));
                decBox->showInDegrees(de);
            }
        }
        else if (!strcmp(tagXMLEle(ep), "Sequence"))
        {
            sequenceEdit->setText(pcdataXMLEle(ep));
            sequenceURL = QUrl::fromUserInput(sequenceEdit->text());
        }
        else if (!strcmp(tagXMLEle(ep), "FITS"))
        {
            fitsEdit->setText(pcdataXMLEle(ep));
            fitsURL.setPath(fitsEdit->text());
        }
        else if (!strcmp(tagXMLEle(ep), "StartupCondition"))
        {
            for (subEP = nextXMLEle(ep, 1); subEP != nullptr; subEP = nextXMLEle(ep, 0))
            {
                if (!strcmp("ASAP", pcdataXMLEle(subEP)))
                    asapConditionR->setChecked(true);
                else if (!strcmp("Culmination", pcdataXMLEle(subEP)))
                {
                    culminationConditionR->setChecked(true);
                    culminationOffset->setValue(atof(findXMLAttValu(subEP, "value")));
                }
                else if (!strcmp("At", pcdataXMLEle(subEP)))
                {
                    startupTimeConditionR->setChecked(true);
                    startupTimeEdit->setDateTime(QDateTime::fromString(findXMLAttValu(subEP, "value"), Qt::ISODate));
                }
            }
        }
        else if (!strcmp(tagXMLEle(ep), "Constraints"))
        {
            for (subEP = nextXMLEle(ep, 1); subEP != nullptr; subEP = nextXMLEle(ep, 0))
            {
                if (!strcmp("MinimumAltitude", pcdataXMLEle(subEP)))
                {
                    altConstraintCheck->setChecked(true);
                    minAltitude->setValue(atof(findXMLAttValu(subEP, "value")));
                }
                else if (!strcmp("MoonSeparation", pcdataXMLEle(subEP)))
                {
                    moonSeparationCheck->setChecked(true);
                    minMoonSeparation->setValue(atof(findXMLAttValu(subEP, "value")));
                }
                else if (!strcmp("EnforceWeather", pcdataXMLEle(subEP)))
                    weatherCheck->setChecked(true);
                else if (!strcmp("EnforceTwilight", pcdataXMLEle(subEP)))
                    twilightCheck->setChecked(true);
            }
        }
        else if (!strcmp(tagXMLEle(ep), "CompletionCondition"))
        {
            for (subEP = nextXMLEle(ep, 1); subEP != nullptr; subEP = nextXMLEle(ep, 0))
            {
                if (!strcmp("Sequence", pcdataXMLEle(subEP)))
                    sequenceCompletionR->setChecked(true);
                else if (!strcmp("Repeat", pcdataXMLEle(subEP)))
                {
                    repeatCompletionR->setChecked(true);
                    repeatsSpin->setValue(atoi(findXMLAttValu(subEP, "value")));
                }
                else if (!strcmp("Loop", pcdataXMLEle(subEP)))
                    loopCompletionR->setChecked(true);
                else if (!strcmp("At", pcdataXMLEle(subEP)))
                {
                    timeCompletionR->setChecked(true);
                    completionTimeEdit->setDateTime(QDateTime::fromString(findXMLAttValu(subEP, "value"), Qt::ISODate));
                }
            }
        }
        else if (!strcmp(tagXMLEle(ep), "Steps"))
        {
            XMLEle *module;
            trackStepCheck->setChecked(false);
            focusStepCheck->setChecked(false);
            alignStepCheck->setChecked(false);
            guideStepCheck->setChecked(false);

            for (module = nextXMLEle(ep, 1); module != nullptr; module = nextXMLEle(ep, 0))
            {
                const char *proc = pcdataXMLEle(module);

                if (!strcmp(proc, "Track"))
                    trackStepCheck->setChecked(true);
                else if (!strcmp(proc, "Focus"))
                    focusStepCheck->setChecked(true);
                else if (!strcmp(proc, "Align"))
                    alignStepCheck->setChecked(true);
                else if (!strcmp(proc, "Guide"))
                    guideStepCheck->setChecked(true);
            }
        }
    }

    addToQueueB->setEnabled(true);
    saveJob();

    return true;
}

void Scheduler::saveAs()
{
    schedulerURL.clear();
    save();
}

void Scheduler::save()
{
    QUrl backupCurrent = schedulerURL;

    if (schedulerURL.toLocalFile().startsWith(QLatin1String("/tmp/")) || schedulerURL.toLocalFile().contains("/Temp"))
        schedulerURL.clear();

    // If no changes made, return.
    if (mDirty == false && !schedulerURL.isEmpty())
        return;

    if (schedulerURL.isEmpty())
    {
        schedulerURL =
            QFileDialog::getSaveFileUrl(this, i18n("Save Ekos Scheduler List"), dirPath, "Ekos Scheduler List (*.esl)");
        // if user presses cancel
        if (schedulerURL.isEmpty())
        {
            schedulerURL = backupCurrent;
            return;
        }

        dirPath = QUrl(schedulerURL.url(QUrl::RemoveFilename));

        if (schedulerURL.toLocalFile().contains('.') == 0)
            schedulerURL.setPath(schedulerURL.toLocalFile() + ".esl");
    }

    if (schedulerURL.isValid())
    {
        if ((saveScheduler(schedulerURL)) == false)
        {
            KMessageBox::error(KStars::Instance(), i18n("Failed to save scheduler list"), i18n("Save"));
            return;
        }

        mDirty = false;
    }
    else
    {
        QString message = i18n("Invalid URL: %1", schedulerURL.url());
        KMessageBox::sorry(KStars::Instance(), message, i18n("Invalid URL"));
    }
}

bool Scheduler::saveScheduler(const QUrl &fileURL)
{
    QFile file;
    file.setFileName(fileURL.toLocalFile());

    if (!file.open(QIODevice::WriteOnly))
    {
        QString message = i18n("Unable to write to file %1", fileURL.toLocalFile());
        KMessageBox::sorry(0, message, i18n("Could Not Open File"));
        return false;
    }

    QTextStream outstream(&file);

    outstream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
    outstream << "<SchedulerList version='1.4'>" << endl;
    outstream << "<Profile>" << schedulerProfileCombo->currentText() << "</Profile>" << endl;

    foreach (SchedulerJob *job, jobs)
    {
        outstream << "<Job>" << endl;

        outstream << "<Name>" << job->getName() << "</Name>" << endl;
        outstream << "<Priority>" << job->getPriority() << "</Priority>" << endl;
        outstream << "<Coordinates>" << endl;
        outstream << "<J2000RA>" << job->getTargetCoords().ra0().Hours() << "</J2000RA>" << endl;
        outstream << "<J2000DE>" << job->getTargetCoords().dec0().Degrees() << "</J2000DE>" << endl;
        outstream << "</Coordinates>" << endl;

        if (job->getFITSFile().isValid() && job->getFITSFile().isEmpty() == false)
            outstream << "<FITS>" << job->getFITSFile().toLocalFile() << "</FITS>" << endl;

        outstream << "<Sequence>" << job->getSequenceFile().toLocalFile() << "</Sequence>" << endl;

        outstream << "<StartupCondition>" << endl;
        if (job->getFileStartupCondition() == SchedulerJob::START_ASAP)
            outstream << "<Condition>ASAP</Condition>" << endl;
        else if (job->getFileStartupCondition() == SchedulerJob::START_CULMINATION)
            outstream << "<Condition value='" << job->getCulminationOffset() << "'>Culmination</Condition>" << endl;
        else if (job->getFileStartupCondition() == SchedulerJob::START_AT)
            outstream << "<Condition value='" << job->getFileStartupTime().toString(Qt::ISODate) << "'>At</Condition>"
                      << endl;
        outstream << "</StartupCondition>" << endl;

        outstream << "<Constraints>" << endl;
        if (job->getMinAltitude() > 0)
            outstream << "<Constraint value='" << job->getMinAltitude() << "'>MinimumAltitude</Constraint>" << endl;
        if (job->getMinMoonSeparation() > 0)
            outstream << "<Constraint value='" << job->getMinMoonSeparation() << "'>MoonSeparation</Constraint>"
                      << endl;
        if (job->getEnforceWeather())
            outstream << "<Constraint>EnforceWeather</Constraint>" << endl;
        if (job->getEnforceTwilight())
            outstream << "<Constraint>EnforceTwilight</Constraint>" << endl;
        outstream << "</Constraints>" << endl;

        outstream << "<CompletionCondition>" << endl;
        if (job->getCompletionCondition() == SchedulerJob::FINISH_SEQUENCE)
            outstream << "<Condition>Sequence</Condition>" << endl;
        else if (job->getCompletionCondition() == SchedulerJob::FINISH_REPEAT)
            outstream << "<Condition value='" << job->getRepeatsRequired() << "'>Repeat</Condition>" << endl;
        else if (job->getCompletionCondition() == SchedulerJob::FINISH_LOOP)
            outstream << "<Condition>Loop</Condition>" << endl;
        else if (job->getCompletionCondition() == SchedulerJob::FINISH_AT)
            outstream << "<Condition value='" << job->getCompletionTime().toString(Qt::ISODate) << "'>At</Condition>"
                      << endl;
        outstream << "</CompletionCondition>" << endl;

        outstream << "<Steps>" << endl;
        if (job->getStepPipeline() & SchedulerJob::USE_TRACK)
            outstream << "<Step>Track</Step>" << endl;
        if (job->getStepPipeline() & SchedulerJob::USE_FOCUS)
            outstream << "<Step>Focus</Step>" << endl;
        if (job->getStepPipeline() & SchedulerJob::USE_ALIGN)
            outstream << "<Step>Align</Step>" << endl;
        if (job->getStepPipeline() & SchedulerJob::USE_GUIDE)
            outstream << "<Step>Guide</Step>" << endl;
        outstream << "</Steps>" << endl;

        outstream << "</Job>" << endl;
    }

    outstream << "<StartupProcedure>" << endl;
    if (startupScript->text().isEmpty() == false)
        outstream << "<Procedure value='" << startupScript->text() << "'>StartupScript</Procedure>" << endl;
    if (unparkDomeCheck->isChecked())
        outstream << "<Procedure>UnparkDome</Procedure>" << endl;
    if (unparkMountCheck->isChecked())
        outstream << "<Procedure>UnparkMount</Procedure>" << endl;
    if (uncapCheck->isChecked())
        outstream << "<Procedure>UnparkCap</Procedure>" << endl;
    outstream << "</StartupProcedure>" << endl;

    outstream << "<ShutdownProcedure>" << endl;
    if (warmCCDCheck->isChecked())
        outstream << "<Procedure>WarmCCD</Procedure>" << endl;
    if (capCheck->isChecked())
        outstream << "<Procedure>ParkCap</Procedure>" << endl;
    if (parkMountCheck->isChecked())
        outstream << "<Procedure>ParkMount</Procedure>" << endl;
    if (parkDomeCheck->isChecked())
        outstream << "<Procedure>ParkDome</Procedure>" << endl;
    if (shutdownScript->text().isEmpty() == false)
        outstream << "<Procedure value='" << shutdownScript->text() << "'>ShutdownScript</Procedure>" << endl;
    outstream << "</ShutdownProcedure>" << endl;

    outstream << "</SchedulerList>" << endl;

    appendLogText(i18n("Scheduler list saved to %1", fileURL.toLocalFile()));
    file.close();
    return true;
}

void Scheduler::startSlew()
{
    Q_ASSERT(currentJob != nullptr);

    // If the mount was parked by a pause or the end-user, unpark
    if (isMountParked())
    {
        parkWaitState = PARKWAIT_UNPARK;
        return;
    }

    if (Options::resetMountModelBeforeJob())
        mountInterface->call(QDBus::AutoDetect, "resetModel");

    SkyPoint target = currentJob->getTargetCoords();
    QList<QVariant> telescopeSlew;
    telescopeSlew.append(target.ra().Hours());
    telescopeSlew.append(target.dec().Degrees());

    QDBusReply<bool> const slewModeReply = mountInterface->callWithArgumentList(QDBus::AutoDetect, "slew", telescopeSlew);
    if (slewModeReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' slew request received DBUS error: %2").arg(currentJob->getName(), QDBusError::errorString(slewModeReply.error().type()));
        if (!manageConnectionLoss())
            currentJob->setState(SchedulerJob::JOB_ERROR);
    }
    else
    {
       currentJob->setStage(SchedulerJob::STAGE_SLEWING);
       appendLogText(i18n("Job '%1' is slewing to target.", currentJob->getName()));
    }
}

void Scheduler::startFocusing()
{
    // 2017-09-30 Jasem: We're skipping post align focusing now as it can be performed
    // when first focus request is made in capture module
    if (currentJob->getStage() == SchedulerJob::STAGE_RESLEWING_COMPLETE ||
        currentJob->getStage() == SchedulerJob::STAGE_POSTALIGN_FOCUSING)
    {
        // Clear the HFR limit value set in the capture module
        captureInterface->call(QDBus::AutoDetect, "clearAutoFocusHFR");
        // Reset Focus frame so that next frame take a full-resolution capture first.
        focusInterface->call(QDBus::AutoDetect,"resetFrame");
        currentJob->setStage(SchedulerJob::STAGE_POSTALIGN_FOCUSING_COMPLETE);
        getNextAction();
        return;
    }

    // Check if autofocus is supported
    QDBusReply<bool> focusModeReply;
    focusModeReply = focusInterface->call(QDBus::AutoDetect, "canAutoFocus");

    if (focusModeReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' canAutoFocus request received DBUS error: %2").arg(currentJob->getName(), QDBusError::errorString(focusModeReply.error().type()));
        if (!manageConnectionLoss())
            currentJob->setState(SchedulerJob::JOB_ERROR);
        return;
    }

    if (focusModeReply.value() == false)
    {
        appendLogText(i18n("Warning: job '%1' is unable to proceed with autofocus, not supported.", currentJob->getName()));
        currentJob->setStepPipeline(
            static_cast<SchedulerJob::StepPipeline>(currentJob->getStepPipeline() & ~SchedulerJob::USE_FOCUS));
        currentJob->setStage(SchedulerJob::STAGE_FOCUS_COMPLETE);
        getNextAction();
        return;
    }

    // Clear the HFR limit value set in the capture module
    captureInterface->call(QDBus::AutoDetect, "clearAutoFocusHFR");

    QDBusMessage reply;

    // We always need to reset frame first
    if ((reply = focusInterface->call(QDBus::AutoDetect, "resetFrame")).type() == QDBusMessage::ErrorMessage)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' resetFrame request received DBUS error: %2").arg(currentJob->getName(), reply.errorMessage());
        if (!manageConnectionLoss())
            currentJob->setState(SchedulerJob::JOB_ERROR);
        return;
    }

    // Set autostar if full field option is false
    if (Options::focusUseFullField() == false)
    {
        QList<QVariant> autoStar;
        autoStar.append(true);
        if ((reply = focusInterface->callWithArgumentList(QDBus::AutoDetect, "setAutoStarEnabled", autoStar)).type() ==
            QDBusMessage::ErrorMessage)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' setAutoFocusStar request received DBUS error: %1").arg(currentJob->getName(), reply.errorMessage());
            if (!manageConnectionLoss())
                currentJob->setState(SchedulerJob::JOB_ERROR);
            return;
        }
    }

    // Start auto-focus
    if ((reply = focusInterface->call(QDBus::AutoDetect, "start")).type() == QDBusMessage::ErrorMessage)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' startFocus request received DBUS error: %2").arg(currentJob->getName(), reply.errorMessage());
        if (!manageConnectionLoss())
            currentJob->setState(SchedulerJob::JOB_ERROR);
        return;
    }    

    /*if (currentJob->getStage() == SchedulerJob::STAGE_RESLEWING_COMPLETE ||
        currentJob->getStage() == SchedulerJob::STAGE_POSTALIGN_FOCUSING)
    {
        currentJob->setStage(SchedulerJob::STAGE_POSTALIGN_FOCUSING);
        appendLogText(i18n("Post-alignment focusing for %1 ...", currentJob->getName()));
    }
    else
    {
        currentJob->setStage(SchedulerJob::STAGE_FOCUSING);
        appendLogText(i18n("Focusing %1 ...", currentJob->getName()));
    }*/

    currentJob->setStage(SchedulerJob::STAGE_FOCUSING);
    appendLogText(i18n("Job '%1' is focusing.", currentJob->getName()));
}

void Scheduler::findNextJob()
{
    Q_ASSERT_X(currentJob->getState() == SchedulerJob::JOB_ERROR ||
               currentJob->getState() == SchedulerJob::JOB_ABORTED ||
               currentJob->getState() == SchedulerJob::JOB_COMPLETE,
               __FUNCTION__, "Finding next job requires current to be in error, aborted or complete");

    jobTimer.stop();

    // Reset failed count
    alignFailureCount = guideFailureCount = 0;

    /* FIXME: Other debug logs in that function probably */
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Find next job...";

    if (currentJob->getState() == SchedulerJob::JOB_ERROR)
    {
        captureBatch = 0;
        // Stop Guiding if it was used
        stopGuiding();

        appendLogText(i18n("Job '%1' is terminated due to errors.", currentJob->getName()));
        setCurrentJob(nullptr);
        schedulerTimer.start();
    }
    else if (currentJob->getState() == SchedulerJob::JOB_ABORTED)
    {
        // Stop Guiding if it was used
        stopGuiding();

        appendLogText(i18n("Job '%1' is aborted.", currentJob->getName()));
        setCurrentJob(nullptr);
        schedulerTimer.start();
    }
    // Job is complete, so check completion criteria to optimize processing
    // In any case, we're done whether the job completed successfully or not.
    else if (currentJob->getCompletionCondition() == SchedulerJob::FINISH_SEQUENCE)
    {
        /* Mark the job idle as well as all its duplicates for re-evaluation */
        foreach(SchedulerJob *a_job, jobs)
            if (a_job == currentJob || a_job->isDuplicateOf(currentJob))
                a_job->setState(SchedulerJob::JOB_IDLE);

        captureBatch = 0;
        // Stop Guiding if it was used
        stopGuiding();

        appendLogText(i18n("Job '%1' is complete.", currentJob->getName()));
        setCurrentJob(nullptr);
        schedulerTimer.start();
    }
    else if (currentJob->getCompletionCondition() == SchedulerJob::FINISH_REPEAT)
    {
        if (0 < currentJob->getRepeatsRemaining())
            currentJob->setRepeatsRemaining(currentJob->getRepeatsRemaining() - 1);

        /* Mark the job idle as well as all its duplicates for re-evaluation */
        foreach(SchedulerJob *a_job, jobs)
            if (a_job == currentJob || a_job->isDuplicateOf(currentJob))
                a_job->setState(SchedulerJob::JOB_IDLE);

        /* Re-evaluate all jobs, without selecting a new job */
        jobEvaluationOnly = true;
        evaluateJobs();

        /* If current job is actually complete because of previous duplicates, prepare for next job */
        if (currentJob->getRepeatsRemaining() == 0)
        {
            stopCurrentJobAction();
            stopGuiding();

            appendLogText(i18np("Job '%1' is complete after #%2 batch.",
                                "Job '%1' is complete after #%2 batches.",
                                currentJob->getName(), currentJob->getRepeatsRequired()));
            setCurrentJob(nullptr);
            schedulerTimer.start();
        }
        /* If job requires more work, continue current observation */
        else
        {
            /* FIXME: raise priority to allow other jobs to schedule in-between */
            executeJob(currentJob);

            /* If we are guiding, continue capturing */
            if (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE)
            {
                currentJob->setStage(SchedulerJob::STAGE_CAPTURING);
                startCapture();
            }
            /* If we are not guiding, but using alignment, realign */
            else if (currentJob->getStepPipeline() & SchedulerJob::USE_ALIGN)
            {
                currentJob->setStage(SchedulerJob::STAGE_ALIGNING);
                startAstrometry();
            }
            /* Else if we are neither guiding nor using alignment, slew back to target */
            else if (currentJob->getStepPipeline() & SchedulerJob::USE_TRACK)
            {
                currentJob->setStage(SchedulerJob::STAGE_SLEWING);
                startSlew();
            }
            /* Else just start capturing */
            else
            {
                currentJob->setStage(SchedulerJob::STAGE_CAPTURING);
                startCapture();
            }

            appendLogText(i18np("Job '%1' is repeating, #%2 batch remaining.",
                                "Job '%1' is repeating, #%2 batches remaining.",
                                currentJob->getName(), currentJob->getRepeatsRemaining()));
            /* currentJob remains the same */
            jobTimer.start();
        }
    }
    else if (currentJob->getCompletionCondition() == SchedulerJob::FINISH_LOOP)
    {
        executeJob(currentJob);

        currentJob->setStage(SchedulerJob::STAGE_CAPTURING);
        captureBatch++;
        startCapture();

        appendLogText(i18n("Job '%1' is repeating, looping indefinitely.", currentJob->getName()));
        /* currentJob remains the same */
        jobTimer.start();
    }
    else if (currentJob->getCompletionCondition() == SchedulerJob::FINISH_AT)
    {
        if (KStarsData::Instance()->lt().secsTo(currentJob->getCompletionTime()) <= 0)
        {
            /* Mark the job idle as well as all its duplicates for re-evaluation */
            foreach(SchedulerJob *a_job, jobs)
                if (a_job == currentJob || a_job->isDuplicateOf(currentJob))
                    a_job->setState(SchedulerJob::JOB_IDLE);
            stopCurrentJobAction();
            stopGuiding();
            captureBatch = 0;

            appendLogText(i18np("Job '%1' stopping, reached completion time with #%2 batch done.",
                                "Job '%1' stopping, reached completion time with #%2 batches done.",
                                currentJob->getName(), captureBatch + 1));
            setCurrentJob(nullptr);
            schedulerTimer.start();
        }
        else
        {
            executeJob(currentJob);

            currentJob->setStage(SchedulerJob::STAGE_CAPTURING);
            captureBatch++;
            startCapture();

            appendLogText(i18np("Job '%1' completed #%2 batch before completion time, restarted.",
                                "Job '%1' completed #%2 batches before completion time, restarted.",
                                currentJob->getName(), captureBatch));
            /* currentJob remains the same */
            jobTimer.start();
        }
    }
    else
    {
        /* Unexpected situation, mitigate by resetting the job and restarting the scheduler timer */
        qCDebug(KSTARS_EKOS_SCHEDULER) << "BUGBUG! Job '" << currentJob->getName() << "' timer elapsed, but no action to be taken.";
        setCurrentJob(nullptr);
        schedulerTimer.start();
    }
}

void Scheduler::startAstrometry()
{
    QDBusMessage reply;
    setSolverAction(Align::GOTO_SLEW);

    // Always turn update coords on
    QVariant arg(true);
    alignInterface->call(QDBus::AutoDetect, "setUpdateCoords", arg);

    // If FITS file is specified, then we use load and slew
    if (currentJob->getFITSFile().isEmpty() == false)
    {
        QList<QVariant> solveArgs;
        solveArgs.append(currentJob->getFITSFile().toString(QUrl::PreferLocalFile));

        if ((reply = alignInterface->callWithArgumentList(QDBus::AutoDetect, "loadAndSlew", solveArgs)).type() ==
            QDBusMessage::ErrorMessage)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' loadAndSlew request received DBUS error: %2").arg(currentJob->getName(), reply.errorMessage());
            if (!manageConnectionLoss())
                currentJob->setState(SchedulerJob::JOB_ERROR);
            return;
        }

        loadAndSlewProgress = true;
        appendLogText(i18n("Job '%1' is plate solving capture %2.", currentJob->getName(), currentJob->getFITSFile().fileName()));
    }
    else
    {
        if ((reply = alignInterface->call(QDBus::AutoDetect, "captureAndSolve")).type() == QDBusMessage::ErrorMessage)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' captureAndSolve request received DBUS error: %2").arg(currentJob->getName(), reply.errorMessage());
            if (!manageConnectionLoss())
                currentJob->setState(SchedulerJob::JOB_ERROR);
            return;
        }

        appendLogText(i18n("Job '%1' is capturing and plate solving.", currentJob->getName()));
    }

    /* FIXME: not supposed to modify the job */
    currentJob->setStage(SchedulerJob::STAGE_ALIGNING);
}

void Scheduler::startGuiding(bool resetCalibration)
{
    // Set Auto Star to true
    QVariant arg(true);
    guideInterface->call(QDBus::AutoDetect,"setCalibrationAutoStar", arg);

    // Only reset calibration on trouble
    // and if we are allowed to reset calibration (true by default)
    if (resetCalibration && Options::resetGuideCalibration())
        guideInterface->call(QDBus::AutoDetect, "clearCalibration");

    guideInterface->call(QDBus::AutoDetect, "guide");

    currentJob->setStage(SchedulerJob::STAGE_GUIDING);

    appendLogText(i18n("Starting guiding procedure for %1 ...", currentJob->getName()));
}

void Scheduler::startCapture()
{
    captureInterface->call(QDBus::AutoDetect, "clearSequenceQueue");

    QString targetName = currentJob->getName().replace(' ', "");
    QList<QVariant> targetArgs;
    targetArgs.append(targetName);
    captureInterface->callWithArgumentList(QDBus::AutoDetect, "setTargetName", targetArgs);

    QString url = currentJob->getSequenceFile().toLocalFile();

    QList<QVariant> dbusargs;
    dbusargs.append(url);
    captureInterface->callWithArgumentList(QDBus::AutoDetect, "loadSequenceQueue", dbusargs);

    SchedulerJob::CapturedFramesMap fMap = currentJob->getCapturedFramesMap();

    for (auto &e : fMap.keys())
    {
        QList<QVariant> dbusargs;
        QDBusMessage reply;

        dbusargs.append(e);
        dbusargs.append(fMap.value(e));
        if ((reply = captureInterface->callWithArgumentList(QDBus::AutoDetect, "setCapturedFramesMap", dbusargs)).type() ==
            QDBusMessage::ErrorMessage)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: job '%1' setCapturedFramesCount request received DBUS error: %1").arg(currentJob->getName()).arg(reply.errorMessage());
            if (!manageConnectionLoss())
                currentJob->setState(SchedulerJob::JOB_ERROR);
            return;
        }
    }

    // If sequence is a loop, ignore sequence history
    if (currentJob->getCompletionCondition() != SchedulerJob::FINISH_SEQUENCE)
        captureInterface->call(QDBus::AutoDetect, "ignoreSequenceHistory");

    // Start capture process
    captureInterface->call(QDBus::AutoDetect, "start");

    currentJob->setStage(SchedulerJob::STAGE_CAPTURING);

    KNotification::event(QLatin1String("EkosScheduledImagingStart"),
                         i18n("Ekos job (%1) - Capture started", currentJob->getName()));

    if (captureBatch > 0)
        appendLogText(i18n("Job '%1' capture is in progress (batch #%2)...", currentJob->getName(), captureBatch + 1));
    else
        appendLogText(i18n("Job '%1' capture is in progress...", currentJob->getName()));
}

void Scheduler::stopGuiding()
{
    if (nullptr != currentJob && (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE))
    {
        switch (currentJob->getStage())
        {
            case SchedulerJob::STAGE_GUIDING_COMPLETE:
            case SchedulerJob::STAGE_CAPTURING:
                qCInfo(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' is stopping guiding...").arg(currentJob->getName());
                guideInterface->call(QDBus::AutoDetect, "abort");
                guideFailureCount = 0;
                break;

            default: break;
        }
    }
}

void Scheduler::setSolverAction(Align::GotoMode mode)
{
    QVariant gotoMode(static_cast<int>(mode));
    alignInterface->call(QDBus::AutoDetect, "setSolverAction", gotoMode);
}

void Scheduler::disconnectINDI()
{
    qCInfo(KSTARS_EKOS_SCHEDULER) << "Disconnecting INDI...";
    indiState               = INDI_DISCONNECTING;
    indiConnectFailureCount = 0;
    ekosInterface->call(QDBus::AutoDetect, "disconnectDevices");
}

void Scheduler::stopEkos()
{
    qCInfo(KSTARS_EKOS_SCHEDULER) << "Stopping Ekos...";
    ekosState               = EKOS_STOPPING;
    ekosConnectFailureCount = 0;
    ekosInterface->call(QDBus::AutoDetect, "stop");
}

void Scheduler::setDirty()
{
    mDirty = true;

    if (sender() == startupProcedureButtonGroup || sender() == shutdownProcedureGroup)
        return;

    if (0 <= jobUnderEdit && state != SCHEDULER_RUNNIG && !queueTable->selectedItems().isEmpty())
        saveJob();

    // For object selection, all fields must be filled
    bool const nameSelectionOK = !raBox->isEmpty()  && !decBox->isEmpty() && !nameEdit->text().isEmpty();

    // For FITS selection, only the name and fits URL should be filled.
    bool const fitsSelectionOK = !nameEdit->text().isEmpty() && !fitsURL.isEmpty();

    // Sequence selection is required
    bool const seqSelectionOK = !sequenceEdit->text().isEmpty();

    // Finally, adding is allowed upon object/FITS and sequence selection
    bool const addingOK = (nameSelectionOK || fitsSelectionOK) && seqSelectionOK;

    addToQueueB->setEnabled(addingOK);
    mosaicB->setEnabled(addingOK);
}

void Scheduler::updateCompletedJobsCount(bool forced)
{
    /* Use a temporary map in order to limit the number of file searches */
    SchedulerJob::CapturedFramesMap newFramesCount;

    /* If update is forced, clear the frame map */
    if (forced)
        capturedFramesCount.clear();

    /* Enumerate SchedulerJobs to count captures that are already stored */
    for (SchedulerJob *oneJob : jobs)
    {
        QList<SequenceJob*> seqjobs;
        bool hasAutoFocus = false;

        /* Look into the sequence requirements, bypass if invalid */
        if (loadSequenceQueue(oneJob->getSequenceFile().toLocalFile(), oneJob, seqjobs, hasAutoFocus) == false)
        {
            appendLogText(i18n("Warning: job '%1' has inaccessible sequence '%2', marking invalid.", oneJob->getName(), oneJob->getSequenceFile().toLocalFile()));
            oneJob->setState(SchedulerJob::JOB_INVALID);
            continue;
        }

        /* Enumerate the SchedulerJob's SequenceJobs to count captures stored for each */
        foreach (SequenceJob *oneSeqJob, seqjobs)
        {
            /* Only consider captures stored on client (Ekos) side */
            /* FIXME: ask the remote for the file count */
            if (oneSeqJob->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
                continue;

            /* FIXME: this signature path is incoherent when there is no filter wheel on the setup - bugfix should be elsewhere though */
            QString const signature = oneSeqJob->getSignature();

            /* Bypass this SchedulerJob if we already checked its signature */
            switch(oneJob->getState())
            {
            case SchedulerJob::JOB_IDLE:
            case SchedulerJob::JOB_EVALUATION:
                /* We recount idle/evaluated jobs systematically */
                break;

            default:
                /* We recount other jobs if somehow we don't have any count for their signature, else we reuse the previous count */
                QMap<QString, uint16_t>::iterator const sigCount = capturedFramesCount.find(signature);
                if (capturedFramesCount.end() != sigCount)
                {
                    newFramesCount[signature] = sigCount.value();
                    continue;
                }
            }

            /* Count captures already stored */
            newFramesCount[signature] = getCompletedFiles(signature, oneSeqJob->getFullPrefix());
        }
    }

    capturedFramesCount = newFramesCount;

    //if (forced)
    {
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Frame map summary:";
        QMap<QString, uint16_t>::const_iterator it = capturedFramesCount.constBegin();
        for (; it != capturedFramesCount.constEnd(); it++)
            qCDebug(KSTARS_EKOS_SCHEDULER) << " " << it.key() << ':' << it.value();
    }
}

bool Scheduler::estimateJobTime(SchedulerJob *schedJob)
{
    /* updateCompletedJobsCount(); */

    // Load the sequence job associated with the argument scheduler job.
    QList<SequenceJob *> seqJobs;
    bool hasAutoFocus = false;
    if (loadSequenceQueue(schedJob->getSequenceFile().toLocalFile(), schedJob, seqJobs, hasAutoFocus) == false)
    {
        qCWarning(KSTARS_EKOS_SCHEDULER) << QString("Warning: Failed estimating the duration of job '%1', its sequence file is invalid.").arg(schedJob->getSequenceFile().toLocalFile());
        return false;
    }

    // FIXME: setting in-sequence focus should be done in XML processing.
    schedJob->setInSequenceFocus(hasAutoFocus);

    /* This is the map of captured frames for this scheduler job, keyed per storage signature.
     * It will be forwarded to the Capture module in order to capture only what frames are required.
     * If option "Remember Job Progress" is disabled, this map will be empty, and the Capture module will process all requested captures unconditionally.
     */
    SchedulerJob::CapturedFramesMap capture_map;
    bool const rememberJobProgress = Options::rememberJobProgress();

    int totalSequenceCount = 0, totalCompletedCount = 0;
    double totalImagingTime  = 0;

    // Loop through sequence jobs to calculate the number of required frames and estimate duration.
    foreach (SequenceJob *seqJob, seqJobs)
    {
        // FIXME: find a way to actually display the filter name.
        QString seqName = i18n("Job '%1' %2x%3\" %4", schedJob->getName(), seqJob->getCount(), seqJob->getExposure(), seqJob->getFilterName());

        if (seqJob->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
        {
            qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 duration cannot be estimated time since the sequence saves the files remotely.").arg(seqName);
            schedJob->setEstimatedTime(-2);
            qDeleteAll(seqJobs);
            return true;
        }

        int const captures_required = seqJob->getCount()*schedJob->getRepeatsRequired();

        int captures_completed = 0;
        if (rememberJobProgress)
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

            // Retrieve cached count of completed captures for the output folder of this seqJob
            QString const signature = seqJob->getSignature();
            QString const signature_path = QFileInfo(signature).path();
            captures_completed = capturedFramesCount[signature];

            qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 sees %2 captures in output folder '%3'.").arg(seqName).arg(captures_completed).arg(signature_path);

            // Enumerate sequence jobs to check how many captures are completed overall in the same storage as the current one
            foreach (SequenceJob *prevSeqJob, seqJobs)
            {
                // Enumerate seqJobs up to the current one
                if (seqJob == prevSeqJob)
                    break;

                // If the previous sequence signature matches the current, reduce completion count to take duplicates into account
                if (!signature.compare(prevSeqJob->getLocalDir() + prevSeqJob->getDirectoryPostfix()))
                {
                    int const previous_captures_required = prevSeqJob->getCount()*schedJob->getRepeatsRequired();
                    qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 has a previous duplicate sequence job requiring %2 captures.").arg(seqName).arg(previous_captures_required);
                    captures_completed -= previous_captures_required;
                }

                // Now completed count can be needlessly negative for this job, so clamp to zero
                if (captures_completed < 0)
                    captures_completed = 0;

                // And break if no captures remain, this job has to execute
                if (captures_completed == 0)
                    break;
            }

            // Finally we're only interested in the number of captures required for this sequence item
            if (captures_required < captures_completed)
                captures_completed = captures_required;

            qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 has completed %2/%3 of its required captures in output folder '%4'.").arg(seqName).arg(captures_completed).arg(captures_required).arg(signature_path);

            // Update the completion count for this signature in the frame map if we still have captures to take.
            // That frame map will be transferred to the Capture module, for which the sequence is a single batch of the scheduler job.
            // For instance, consider a scheduler job repeated 3 times and using a 3xLum sequence, so we want 9xLum in the end.
            // - If no captures are already processed, the frame map contains Lum=0
            // - If 1xLum are already processed, the frame map contains Lum=0 when the batch executes, so that 3xLum may be taken.
            // - If 3xLum are already processed, the frame map contains Lum=0 when the batch executes, as we still need more than what the sequence provides.
            // - If 7xLum are already processed, the frame map contains Lum=1 when the batch executes, because we now only need 2xLum to finish the job.
            // Therefore we need to specify a number of existing captures only for the last batch of the scheduler job.
            // In the last batch, we only need the remainder of frames to get to the required total.
            if (captures_completed < captures_required)
            {
                if (captures_required - captures_completed < seqJob->getCount())
                    capture_map[signature] = captures_completed % seqJob->getCount();
                else
                    capture_map[signature] = 0;
            }
            else capture_map[signature] = captures_required;

            // From now on, 'captures_completed' is the number of frames completed for the *current* sequence job
        }


        // Check if we still need any light frames. Because light frames changes the flow of the observatory startup
        // Without light frames, there is no need to do focusing, alignment, guiding...etc
        // We check if the frame type is LIGHT and if either the number of captures_completed frames is less than required
        // OR if the completion condition is set to LOOP so it is never complete due to looping.
        // FIXME: As it is implemented now, FINISH_LOOP may loop over a capture-complete, therefore inoperant, scheduler job.
        bool const areJobCapturesComplete = !(captures_completed < captures_required || schedJob->getCompletionCondition() == SchedulerJob::FINISH_LOOP);
        if (seqJob->getFrameType() == FRAME_LIGHT)
        {
            if(areJobCapturesComplete)
            {
                qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 completed its sequence of %2 light frames.").arg(seqName).arg(captures_required);
            }
        }
        else
        {
            qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 captures calibration frames.").arg(seqName);
        }

        totalSequenceCount += captures_required;
        totalCompletedCount += rememberJobProgress ? captures_completed : 0;

        /* If captures are not complete, we have imaging time left */
        if (!areJobCapturesComplete)
        {
            /* if looping, consider we always have one capture left - currently this is discarded afterwards as -2 */
            if (schedJob->getCompletionCondition() == SchedulerJob::FINISH_LOOP)
                totalImagingTime += fabs((seqJob->getExposure() + seqJob->getDelay()) * 1);
            else
                totalImagingTime += fabs((seqJob->getExposure() + seqJob->getDelay()) * (captures_required - captures_completed));

            /* If we have light frames to process, add focus/dithering delay */
            if (seqJob->getFrameType() == FRAME_LIGHT)
            {
                // If inSequenceFocus is true
                if (hasAutoFocus)
                {
                    // Wild guess that each in sequence auto focus takes an average of 30 seconds. It can take any where from 2 seconds to 2+ minutes.
                    qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 requires a focus procedure.").arg(seqName);
                    totalImagingTime += (captures_required - captures_completed) * 30;
                }
                // If we're dithering after each exposure, that's another 10-20 seconds
                if (schedJob->getStepPipeline() & SchedulerJob::USE_GUIDE && Options::ditherEnabled())
                {
                    qCInfo(KSTARS_EKOS_SCHEDULER) << QString("%1 requires a dither procedure.").arg(seqName);
                    totalImagingTime += ((captures_required - captures_completed) * 15) / Options::ditherFrames();
                }
            }
        }
    }

    schedJob->setCapturedFramesMap(capture_map);
    schedJob->setSequenceCount(totalSequenceCount);
    schedJob->setCompletedCount(totalCompletedCount);

    qDeleteAll(seqJobs);

    // We can't estimate times that do not finish when sequence is done
    if (schedJob->getCompletionCondition() == SchedulerJob::FINISH_LOOP)
    {
        // We can't know estimated time if it is looping indefinitely
        appendLogText(i18n("Warning: job '%1' will be looping until Scheduler is stopped manually.", schedJob->getName()));
        schedJob->setEstimatedTime(-2);
    }
    // If we know startup and finish times, we can estimate time right away
    else if (schedJob->getStartupCondition() == SchedulerJob::START_AT &&
        schedJob->getCompletionCondition() == SchedulerJob::FINISH_AT)
    {
        qint64 const diff = schedJob->getStartupTime().secsTo(schedJob->getCompletionTime());
        appendLogText(i18n("Job '%1' will run for %2.", schedJob->getName(), dms(diff * 15.0 / 3600.0f).toHMSString()));
        schedJob->setEstimatedTime(diff);
    }
    // If we know finish time only, we can roughly estimate the time considering the job starts now
    else if (schedJob->getStartupCondition() != SchedulerJob::START_AT &&
        schedJob->getCompletionCondition() == SchedulerJob::FINISH_AT)
    {
        qint64 const diff = KStarsData::Instance()->lt().secsTo(schedJob->getCompletionTime());
        appendLogText(i18n("Job '%1' will run for %2 if started now.", schedJob->getName(), dms(diff * 15.0 / 3600.0f).toHMSString()));
        schedJob->setEstimatedTime(diff);
    }
    // Rely on the estimated imaging time to determine whether this job is complete or not - this makes the estimated time null
    else if (totalImagingTime <= 0)
    {
        appendLogText(i18n("Job '%1' will not run, complete with %2/%3 captures.", schedJob->getName(), totalCompletedCount, totalSequenceCount));
        schedJob->setEstimatedTime(0);
    }
    else
    {
        if (schedJob->getLightFramesRequired())
        {
            /* FIXME: estimation doesn't need to consider repeats, those will be optimized away by findNextJob (this is a regression) */
            /* FIXME: estimation should base on actual measure of each step, eventually with preliminary data as what it used now */
            // Are we doing tracking? It takes about 30 seconds
            if (schedJob->getStepPipeline() & SchedulerJob::USE_TRACK)
                totalImagingTime += 30*schedJob->getRepeatsRequired();
            // Are we doing initial focusing? That can take about 2 minutes
            if (schedJob->getStepPipeline() & SchedulerJob::USE_FOCUS)
                totalImagingTime += 120*schedJob->getRepeatsRequired();
            // Are we doing astrometry? That can take about 30 seconds
            if (schedJob->getStepPipeline() & SchedulerJob::USE_ALIGN)
                totalImagingTime += 30*schedJob->getRepeatsRequired();
            // Are we doing guiding? Calibration process can take about 2 mins
            if (schedJob->getStepPipeline() & SchedulerJob::USE_GUIDE)
                totalImagingTime += 120*schedJob->getRepeatsRequired();
        }

        dms const estimatedTime(totalImagingTime * 15.0 / 3600.0);
        qCInfo(KSTARS_EKOS_SCHEDULER) << QString("Job '%1' estimated to take %2 to complete.").arg(schedJob->getName(), estimatedTime.toHMSString());

        schedJob->setEstimatedTime(totalImagingTime);
    }

    return true;
}

void Scheduler::parkMount()
{
    QDBusReply<int> MountReply  = mountInterface->call(QDBus::AutoDetect, "getParkingStatus");
    Mount::ParkingStatus status = (Mount::ParkingStatus)MountReply.value();

    if (status != Mount::PARKING_OK)
    {
        if (status == Mount::PARKING_BUSY)
        {
            appendLogText(i18n("Parking mount in progress..."));
        }
        else
        {
            mountInterface->call(QDBus::AutoDetect, "park");
            appendLogText(i18n("Parking mount..."));

            currentOperationTime.start();
        }

        if (shutdownState == SHUTDOWN_PARK_MOUNT)
            shutdownState = SHUTDOWN_PARKING_MOUNT;
        else if (parkWaitState == PARKWAIT_PARK)
            parkWaitState = PARKWAIT_PARKING;
    }
    else
    {
        appendLogText(i18n("Mount already parked."));

        if (shutdownState == SHUTDOWN_PARK_MOUNT)
            shutdownState = SHUTDOWN_PARK_DOME;
        else if (parkWaitState == PARKWAIT_PARK)
            parkWaitState = PARKWAIT_PARKED;
    }
}

void Scheduler::unParkMount()
{
    QDBusReply<int> const mountReply = mountInterface->call(QDBus::AutoDetect, "getParkingStatus");

    if (mountReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount getParkingStatus request received DBUS error: %1").arg(QDBusError::errorString(mountReply.error().type()));
        if (!manageConnectionLoss())
            parkWaitState = PARKWAIT_ERROR;
    }
    else switch ((Mount::ParkingStatus) mountReply.value())
    {
        case Mount::UNPARKING_OK:
            if (startupState == STARTUP_UNPARK_MOUNT)
                startupState = STARTUP_UNPARK_CAP;
            else if (parkWaitState == PARKWAIT_UNPARK)
                parkWaitState = PARKWAIT_UNPARKED;

            appendLogText(i18n("Mount already unparked."));
            break;

        case Mount::PARKING_BUSY:
            /* FIXME: Handle the situation where we request unparking but a parking procedure is running. */

        case Mount::PARKING_IDLE:
        case Mount::PARKING_OK:
        case Mount::PARKING_ERROR:
            {
                QDBusReply<bool> const mountReply = mountInterface->call(QDBus::AutoDetect, "unpark");

                if (mountReply.error().type() != QDBusError::NoError)
                {
                    qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount unpark request received DBUS error: %1").arg(QDBusError::errorString(mountReply.error().type()));
                    if (!manageConnectionLoss())
                        parkWaitState = PARKWAIT_ERROR;
                }
                else currentOperationTime.start();
            }

            // Fall through
        case Mount::UNPARKING_BUSY:
            if (startupState == STARTUP_UNPARK_MOUNT)
                startupState = STARTUP_UNPARKING_MOUNT;
            else if (parkWaitState == PARKWAIT_UNPARK)
                parkWaitState = PARKWAIT_UNPARKING;

            qCInfo(KSTARS_EKOS_SCHEDULER) << "Unparking mount in progress...";
            break;

        default:
            qCWarning(KSTARS_EKOS_SCHEDULER) << QString("BUG: Parking state %1 not managed while unparking mount.").arg(mountReply.value());
    }
}

void Scheduler::checkMountParkingStatus()
{
    static int parkingFailureCount = 0;
    QDBusReply<int> const mountReply = mountInterface->call(QDBus::AutoDetect, "getParkingStatus");

    if (mountReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount getParkingStatus request received DBUS error: %1").arg(QDBusError::errorString(mountReply.error().type()));
        if (!manageConnectionLoss())
            parkWaitState = PARKWAIT_ERROR;
    }
    else switch ((Mount::ParkingStatus)mountReply.value())
    {
        case Mount::PARKING_OK:
            // If we are starting up, we will unpark the mount in checkParkWaitState soon
            // If we are shutting down and mount is parked, proceed to next step
            if (shutdownState == SHUTDOWN_PARKING_MOUNT)
                shutdownState = SHUTDOWN_PARK_DOME;

            // Update parking engine state
            if (parkWaitState == PARKWAIT_PARKING)
                parkWaitState = PARKWAIT_PARKED;

            appendLogText(i18n("Mount parked."));
            parkingFailureCount = 0;
            break;

        case Mount::UNPARKING_OK:
            // If we are starting up and mount is unparked, proceed to next step
            // If we are shutting down, we will park the mount in checkParkWaitState soon
            if (startupState == STARTUP_UNPARKING_MOUNT)
                startupState = STARTUP_UNPARK_CAP;

            // Update parking engine state
            if (parkWaitState == PARKWAIT_UNPARKING)
                parkWaitState = PARKWAIT_UNPARKED;

            appendLogText(i18n("Mount unparked."));
            parkingFailureCount = 0;
            break;

        // FIXME: Create an option for the parking/unparking timeout.

        case Mount::UNPARKING_BUSY:
            if (currentOperationTime.elapsed() > (60 * 1000))
            {
                if (++parkingFailureCount < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Warning: mount unpark operation timed out on attempt %1/%2. Restarting operation...", parkingFailureCount, MAX_FAILURE_ATTEMPTS));
                    unParkMount();
                }
                else
                {
                    appendLogText(i18n("Warning: mount unpark operation timed out on last attempt."));
                    parkWaitState = PARKWAIT_ERROR;
                }
            }
            else qCInfo(KSTARS_EKOS_SCHEDULER) << "Unparking mount in progress...";

            break;

        case Mount::PARKING_BUSY:
            if (currentOperationTime.elapsed() > (60 * 1000))
            {
                if (++parkingFailureCount < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Warning: mount park operation timed out on attempt %1/%2. Restarting operation...", parkingFailureCount, MAX_FAILURE_ATTEMPTS));
                    parkMount();
                }
                else
                {
                    appendLogText(i18n("Warning: mount park operation timed out on last attempt."));
                    parkWaitState = PARKWAIT_ERROR;
                }
            }
            else qCInfo(KSTARS_EKOS_SCHEDULER) << "Parking mount in progress...";

            break;

        case Mount::PARKING_ERROR:
            if (startupState == STARTUP_UNPARKING_MOUNT)
            {
                appendLogText(i18n("Mount unparking error."));
                startupState = STARTUP_ERROR;
            }
            else if (shutdownState == SHUTDOWN_PARKING_MOUNT)
            {
                appendLogText(i18n("Mount parking error."));
                shutdownState = SHUTDOWN_ERROR;
            }
            else if (parkWaitState == PARKWAIT_PARKING)
            {
                appendLogText(i18n("Mount parking error."));
                parkWaitState = PARKWAIT_ERROR;
            }
            else if (parkWaitState == PARKWAIT_UNPARK)
            {
                appendLogText(i18n("Mount unparking error."));
                parkWaitState = PARKWAIT_ERROR;
            }

            parkingFailureCount = 0;
            break;

        case Mount::PARKING_IDLE:
            // Last parking action did not result in an action, so proceed to next step
            if (shutdownState == SHUTDOWN_PARKING_MOUNT)
                shutdownState = SHUTDOWN_PARK_DOME;

            // Last unparking action did not result in an action, so proceed to next step
            if (startupState == STARTUP_UNPARKING_MOUNT)
                startupState = STARTUP_UNPARK_CAP;

            // Update parking engine state
            if (parkWaitState == PARKWAIT_PARKING)
                parkWaitState = PARKWAIT_PARKED;
            else if (parkWaitState == PARKWAIT_UNPARKING)
                parkWaitState = PARKWAIT_UNPARKED;

            parkingFailureCount = 0;
            break;

        default:
            qCWarning(KSTARS_EKOS_SCHEDULER) << QString("BUG: Parking state %1 not managed while checking progress.").arg(mountReply.value());
    }
}

bool Scheduler::isMountParked()
{
    // First check if the mount is able to park - if it isn't, getParkingStatus will reply PARKING_ERROR and status won't be clear
    QDBusReply<bool> const parkCapableReply = mountInterface->call(QDBus::AutoDetect, "canPark");
    if (parkCapableReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount canPark request received DBUS error: %1").arg(QDBusError::errorString(parkCapableReply.error().type()));
        manageConnectionLoss();
        return false;
    }
    else if (true == parkCapableReply.value())
    {
        // If it is able to park, obtain its current status
        QDBusReply<int> const mountReply  = mountInterface->call(QDBus::AutoDetect, "getParkingStatus");
        if (mountReply.error().type() != QDBusError::NoError)
        {
            qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: mount getParkingStatus request received DBUS error: %1").arg(QDBusError::errorString(mountReply.error().type()));
            manageConnectionLoss();
            return false;
        }

        // Deduce state of mount - see getParkingStatus in mount.cpp
        switch ((Mount::ParkingStatus)mountReply.value())
        {
            case Mount::PARKING_OK:     // INDI switch ok, and parked
            case Mount::PARKING_IDLE:   // INDI switch idle, and parked
                return true;

            case Mount::UNPARKING_OK:   // INDI switch idle or ok, and unparked
            case Mount::PARKING_ERROR:  // INDI switch error
            case Mount::PARKING_BUSY:   // INDI switch busy
            case Mount::UNPARKING_BUSY: // INDI switch busy
            default:
                return false;
        }
    }
    // If the mount is not able to park, consider it not parked
    else return false;
}

void Scheduler::parkDome()
{
    QDBusReply<int> const domeReply = domeInterface->call(QDBus::AutoDetect, "getParkingStatus");
    Dome::ParkingStatus status = (Dome::ParkingStatus)domeReply.value();

    if (domeReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: dome getParkingStatus request received DBUS error: %1").arg(QDBusError::errorString(domeReply.error().type()));
        if (!manageConnectionLoss())
            status = Dome::PARKING_ERROR;
    }

    if (status != Dome::PARKING_OK)
    {
        shutdownState = SHUTDOWN_PARKING_DOME;
        domeInterface->call(QDBus::AutoDetect, "park");
        appendLogText(i18n("Parking dome..."));

        currentOperationTime.start();
    }
    else
    {
        appendLogText(i18n("Dome already parked."));
        shutdownState = SHUTDOWN_SCRIPT;
    }
}

void Scheduler::unParkDome()
{
    QDBusReply<int> const domeReply  = domeInterface->call(QDBus::AutoDetect, "getParkingStatus");
    Dome::ParkingStatus status = (Dome::ParkingStatus)domeReply.value();

    if (domeReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: dome getParkingStatus request received DBUS error: %1").arg(QDBusError::errorString(domeReply.error().type()));
        if (!manageConnectionLoss())
            status = Dome::PARKING_ERROR;
    }

    if (status != Dome::UNPARKING_OK)
    {
        startupState = STARTUP_UNPARKING_DOME;
        domeInterface->call(QDBus::AutoDetect, "unpark");
        appendLogText(i18n("Unparking dome..."));

        currentOperationTime.start();
    }
    else
    {
        appendLogText(i18n("Dome already unparked."));
        startupState = STARTUP_UNPARK_MOUNT;
    }
}

void Scheduler::checkDomeParkingStatus()
{
    /* FIXME: move this elsewhere */
    static int parkingFailureCount = 0;

    QDBusReply<int> const domeReply = domeInterface->call(QDBus::AutoDetect, "getParkingStatus");
    Dome::ParkingStatus status = (Dome::ParkingStatus)domeReply.value();

    if (domeReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: dome getParkingStatus request received DBUS error: %1").arg(QDBusError::errorString(domeReply.error().type()));
        if (!manageConnectionLoss())
            status = Dome::PARKING_ERROR;
    }

    switch (status)
    {
        case Dome::PARKING_OK:
            if (shutdownState == SHUTDOWN_PARKING_DOME)
            {
                appendLogText(i18n("Dome parked."));

                shutdownState = SHUTDOWN_SCRIPT;
            }
            parkingFailureCount = 0;
            break;

        case Dome::UNPARKING_OK:
            if (startupState == STARTUP_UNPARKING_DOME)
            {
                startupState = STARTUP_UNPARK_MOUNT;
                appendLogText(i18n("Dome unparked."));
            }
            parkingFailureCount = 0;
            break;

        case Dome::PARKING_BUSY:
        case Dome::UNPARKING_BUSY:
            // TODO make the timeouts configurable by the user
            if (currentOperationTime.elapsed() > (120 * 1000))
            {
                if (parkingFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Operation timeout. Restarting operation..."));
                    if (status == Dome::PARKING_BUSY)
                        parkDome();
                    else
                        unParkDome();
                    break;
                }
            }
            break;

        case Dome::PARKING_ERROR:
            if (shutdownState == SHUTDOWN_PARKING_DOME)
            {
                appendLogText(i18n("Dome parking error."));
                shutdownState = SHUTDOWN_ERROR;
            }
            else if (startupState == STARTUP_UNPARKING_DOME)
            {
                appendLogText(i18n("Dome unparking error."));
                startupState = STARTUP_ERROR;
            }
            parkingFailureCount = 0;
            break;

        default:
            break;
    }
}

bool Scheduler::isDomeParked()
{
    QDBusReply<int> const domeReply = domeInterface->call(QDBus::AutoDetect, "getParkingStatus");
    Dome::ParkingStatus status = (Dome::ParkingStatus)domeReply.value();

    if (domeReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: dome getParkingStatus request received DBUS error: %1").arg(QDBusError::errorString(domeReply.error().type()));
        if (!manageConnectionLoss())
            status = Dome::PARKING_ERROR;
    }

    return status == Dome::PARKING_OK || status == Dome::PARKING_IDLE;
}

void Scheduler::parkCap()
{
    QDBusReply<int> const capReply = capInterface->call(QDBus::AutoDetect, "getParkingStatus");
    DustCap::ParkingStatus status = (DustCap::ParkingStatus)capReply.value();

    if (capReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: cap getParkingStatus request received DBUS error: %1").arg(QDBusError::errorString(capReply.error().type()));
        if (!manageConnectionLoss())
            status = DustCap::PARKING_ERROR;
    }

    if (status != DustCap::PARKING_OK)
    {
        shutdownState = SHUTDOWN_PARKING_CAP;
        capInterface->call(QDBus::AutoDetect, "park");
        appendLogText(i18n("Parking Cap..."));

        currentOperationTime.start();
    }
    else
    {
        appendLogText(i18n("Cap already parked."));
        shutdownState = SHUTDOWN_PARK_MOUNT;
    }
}

void Scheduler::unParkCap()
{
    QDBusReply<int> const capReply = capInterface->call(QDBus::AutoDetect, "getParkingStatus");
    DustCap::ParkingStatus status = (DustCap::ParkingStatus)capReply.value();

    if (capReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: cap getParkingStatus request received DBUS error: %1").arg(QDBusError::errorString(capReply.error().type()));
        if (!manageConnectionLoss())
            status = DustCap::PARKING_ERROR;
    }

    if (status != DustCap::UNPARKING_OK)
    {
        startupState = STARTUP_UNPARKING_CAP;
        capInterface->call(QDBus::AutoDetect, "unpark");
        appendLogText(i18n("Unparking cap..."));

        currentOperationTime.start();
    }
    else
    {
        appendLogText(i18n("Cap already unparked."));
        startupState = STARTUP_COMPLETE;
    }
}

void Scheduler::checkCapParkingStatus()
{
    /* FIXME: move this elsewhere */
    static int parkingFailureCount = 0;

    QDBusReply<int> const capReply = capInterface->call(QDBus::AutoDetect, "getParkingStatus");
    DustCap::ParkingStatus status  = (DustCap::ParkingStatus)capReply.value();

    if (capReply.error().type() != QDBusError::NoError)
    {
        qCCritical(KSTARS_EKOS_SCHEDULER) << QString("Warning: cap getParkingStatus request received DBUS error: %1").arg(QDBusError::errorString(capReply.error().type()));
        if (!manageConnectionLoss())
            status = DustCap::PARKING_ERROR;
    }

    switch (status)
    {
        case DustCap::PARKING_OK:
            if (shutdownState == SHUTDOWN_PARKING_CAP)
            {
                appendLogText(i18n("Cap parked."));
                shutdownState = SHUTDOWN_PARK_MOUNT;
            }
            parkingFailureCount = 0;
            break;

        case DustCap::UNPARKING_OK:
            if (startupState == STARTUP_UNPARKING_CAP)
            {
                startupState = STARTUP_COMPLETE;
                appendLogText(i18n("Cap unparked."));
            }
            parkingFailureCount = 0;
            break;

        case DustCap::PARKING_BUSY:
        case DustCap::UNPARKING_BUSY:
            // TODO make the timeouts configurable by the user
            if (currentOperationTime.elapsed() > (60 * 1000))
            {
                if (parkingFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Operation timeout. Restarting operation..."));
                    if (status == DustCap::PARKING_BUSY)
                        parkCap();
                    else
                        unParkCap();
                    break;
                }
            }
            break;

        case DustCap::PARKING_ERROR:
            if (shutdownState == SHUTDOWN_PARKING_CAP)
            {
                appendLogText(i18n("Cap parking error."));
                shutdownState = SHUTDOWN_ERROR;
            }
            else if (startupState == STARTUP_UNPARKING_CAP)
            {
                appendLogText(i18n("Cap unparking error."));
                startupState = STARTUP_ERROR;
            }
            parkingFailureCount = 0;
            break;

        default:
            break;
    }
}

void Scheduler::startJobEvaluation()
{
    // Reset current job
    setCurrentJob(nullptr);

    // Unconditionally update the capture storage
    updateCompletedJobsCount(true);

    // Reset ALL scheduler jobs to IDLE and re-evaluate them all again
    for (SchedulerJob * job: jobs)
        job->reset();

    // And evaluate all pending jobs per the conditions set in each
    jobEvaluationOnly = true;
    evaluateJobs();
}

void Scheduler::updatePreDawn()
{
    double earlyDawn = Dawn - Options::preDawnTime() / (60.0 * 24.0);
    int dayOffset    = 0;
    QTime dawn       = QTime(0, 0, 0).addSecs(Dawn * 24 * 3600);
    if (KStarsData::Instance()->lt().time() >= dawn)
        dayOffset = 1;
    preDawnDateTime.setDate(KStarsData::Instance()->lt().date().addDays(dayOffset));
    preDawnDateTime.setTime(QTime::fromMSecsSinceStartOfDay(earlyDawn * 24 * 3600 * 1000));
}

bool Scheduler::isWeatherOK(SchedulerJob *job)
{
    if (weatherStatus == IPS_OK || weatherCheck->isChecked() == false)
        return true;
    else if (weatherStatus == IPS_IDLE)
    {
        if (indiState == INDI_READY)
            appendLogText(i18n("Weather information is pending..."));
        return true;
    }

    // Temporary BUSY is ALSO accepted for now
    // TODO Figure out how to exactly handle this
    if (weatherStatus == IPS_BUSY)
        return true;

    if (weatherStatus == IPS_ALERT)
    {
        job->setState(SchedulerJob::JOB_ABORTED);
        appendLogText(i18n("Job '%1' suffers from bad weather, marking aborted.", job->getName()));
    }
    /*else if (weatherStatus == IPS_BUSY)
    {
        appendLogText(i18n("%1 observation job delayed due to bad weather.", job->getName()));
        schedulerTimer.stop();
        connect(this, &Scheduler::weatherChanged, this, &Scheduler::resumeCheckStatus);
    }*/

    return false;
}

void Scheduler::resumeCheckStatus()
{
    disconnect(this, &Scheduler::weatherChanged, this, &Scheduler::resumeCheckStatus);
    schedulerTimer.start();
}

void Scheduler::startMosaicTool()
{
    bool raOk = false, decOk = false;
    dms ra(raBox->createDms(false, &raOk)); //false means expressed in hours
    dms dec(decBox->createDms(true, &decOk));

    if (raOk == false)
    {
        appendLogText(i18n("Warning: RA value %1 is invalid.", raBox->text()));
        return;
    }

    if (decOk == false)
    {
        appendLogText(i18n("Warning: DEC value %1 is invalid.", decBox->text()));
        return;
    }

    Mosaic mosaicTool;

    SkyPoint center;
    center.setRA0(ra);
    center.setDec0(dec);

    mosaicTool.setCenter(center);
    mosaicTool.calculateFOV();
    mosaicTool.adjustSize();

    if (mosaicTool.exec() == QDialog::Accepted)
    {
        // #1 Edit Sequence File ---> Not needed as of 2016-09-12 since Scheduler can send Target Name to Capture module it will append it to root dir
        // #1.1 Set prefix to Target-Part#
        // #1.2 Set directory to output/Target-Part#

        // #2 Save all sequence files in Jobs dir
        // #3 Set as currnet Sequence file
        // #4 Change Target name to Target-Part#
        // #5 Update J2000 coords
        // #6 Repeat and save Ekos Scheduler List in the output directory
        qCDebug(KSTARS_EKOS_SCHEDULER) << "Job accepted with # " << mosaicTool.getJobs().size() << " jobs and fits dir "
                                       << mosaicTool.getJobsDir();

        QString outputDir  = mosaicTool.getJobsDir();
        QString targetName = nameEdit->text().simplified().remove(' ');
        int batchCount     = 1;

        XMLEle *root = getSequenceJobRoot();
        if (root == nullptr)
            return;

        int currentJobsCount = jobs.count();

        foreach (OneTile *oneJob, mosaicTool.getJobs())
        {
            QString prefix = QString("%1-Part%2").arg(targetName).arg(batchCount++);

            prefix.replace(' ', '-');
            nameEdit->setText(prefix);

            if (createJobSequence(root, prefix, outputDir) == false)
                return;

            QString filename = QString("%1/%2.esq").arg(outputDir, prefix);
            sequenceEdit->setText(filename);
            sequenceURL = QUrl::fromLocalFile(filename);

            raBox->setText(oneJob->skyCenter.ra0().toHMSString());
            decBox->setText(oneJob->skyCenter.dec0().toDMSString());

            saveJob();
        }

        delXMLEle(root);

        // Delete any prior jobs before saving
        if (0 < currentJobsCount)
        {
            if (KMessageBox::questionYesNo(nullptr, i18n("Do you want to keep the existing jobs in the mosaic schedule?")) == KMessageBox::No)
            {
                for (int i = 0; i < currentJobsCount; i++)
                {
                    delete (jobs.takeFirst());
                    queueTable->removeRow(0);
                }
            }
        }

        QUrl mosaicURL = QUrl::fromLocalFile((QString("%1/%2_mosaic.esl").arg(outputDir, targetName)));

        if (saveScheduler(mosaicURL))
        {
            appendLogText(i18n("Mosaic file %1 saved successfully.", mosaicURL.toLocalFile()));
        }
        else
        {
            appendLogText(i18n("Error saving mosaic file %1. Please reload job.", mosaicURL.toLocalFile()));
        }
    }
}

XMLEle *Scheduler::getSequenceJobRoot()
{
    QFile sFile;
    sFile.setFileName(sequenceURL.toLocalFile());

    if (!sFile.open(QIODevice::ReadOnly))
    {
        KMessageBox::sorry(KStars::Instance(), i18n("Unable to open file %1", sFile.fileName()),
                           i18n("Could Not Open File"));
        return nullptr;
    }

    LilXML *xmlParser = newLilXML();
    char errmsg[MAXRBUF];
    XMLEle *root = nullptr;
    char c;

    while (sFile.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
            break;
    }

    delLilXML(xmlParser);
    sFile.close();
    return root;
}

bool Scheduler::createJobSequence(XMLEle *root, const QString &prefix, const QString &outputDir)
{
    QFile sFile;
    sFile.setFileName(sequenceURL.toLocalFile());

    if (!sFile.open(QIODevice::ReadOnly))
    {
        KMessageBox::sorry(KStars::Instance(), i18n("Unable to open file %1", sFile.fileName()),
                           i18n("Could Not Open File"));
        return false;
    }

    XMLEle *ep    = nullptr;
    XMLEle *subEP = nullptr;

    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep), "Job"))
        {
            for (subEP = nextXMLEle(ep, 1); subEP != nullptr; subEP = nextXMLEle(ep, 0))
            {
                if (!strcmp(tagXMLEle(subEP), "Prefix"))
                {
                    XMLEle *rawPrefix = findXMLEle(subEP, "RawPrefix");
                    if (rawPrefix)
                    {
                        editXMLEle(rawPrefix, prefix.toLatin1().constData());
                    }
                }
                else if (!strcmp(tagXMLEle(subEP), "FITSDirectory"))
                {
                    editXMLEle(subEP, QString("%1/%2").arg(outputDir, prefix).toLatin1().constData());
                }
            }
        }
    }

    QDir().mkpath(outputDir);

    QString filename = QString("%1/%2.esq").arg(outputDir, prefix);
    FILE *outputFile = fopen(filename.toLatin1().constData(), "w");

    if (outputFile == nullptr)
    {
        QString message = i18n("Unable to write to file %1", filename);
        KMessageBox::sorry(0, message, i18n("Could Not Open File"));
        return false;
    }

    fprintf(outputFile, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    prXMLEle(outputFile, root, 0);

    fclose(outputFile);

    return true;
}

void Scheduler::resetAllJobs()
{
    if (state == SCHEDULER_RUNNIG)
        return;

    // Evaluate all jobs, this refreshes storage and resets job states
    startJobEvaluation();
}

void Scheduler::checkTwilightWarning(bool enabled)
{
    if (enabled)
        return;

    if (KMessageBox::warningContinueCancel(
            NULL,
            i18n("Turning off astronomial twilight check may cause the observatory "
                 "to run during daylight. This can cause irreversible damage to your equipment!"),
            i18n("Astronomial Twilight Warning"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(),
            "astronomical_twilight_warning") == KMessageBox::Cancel)
    {
        twilightCheck->setChecked(true);
    }
}

void Scheduler::checkStartupProcedure()
{
    if (checkStartupState() == false)
        QTimer::singleShot(1000, this, SLOT(checkStartupProcedure()));
    else
    {
        if (startupState == STARTUP_COMPLETE)
            appendLogText(i18n("Manual startup procedure completed successfully."));
        else if (startupState == STARTUP_ERROR)
            appendLogText(i18n("Manual startup procedure terminated due to errors."));

        startupB->setIcon(
            QIcon::fromTheme("media-playback-start"));
    }
}

void Scheduler::runStartupProcedure()
{
    if (startupState == STARTUP_IDLE || startupState == STARTUP_ERROR || startupState == STARTUP_COMPLETE)
    {
        /* FIXME: Probably issue a warning only, in case the user wants to run the startup script alone */
        if (indiState == INDI_IDLE)
        {
            KSNotification::sorry(i18n("Cannot run startup procedure while INDI devices are not online."));
            return;
        }

        if (KMessageBox::questionYesNo(
                nullptr, i18n("Are you sure you want to execute the startup procedure manually?")) == KMessageBox::Yes)
        {
            appendLogText(i18n("Warning: executing startup procedure manually..."));
            startupB->setIcon(
                QIcon::fromTheme("media-playback-stop"));
            startupState = STARTUP_IDLE;
            checkStartupState();
            QTimer::singleShot(1000, this, SLOT(checkStartupProcedure()));
        }
    }
    else
    {
        switch (startupState)
        {
            case STARTUP_IDLE:
                break;

            case STARTUP_SCRIPT:
                scriptProcess.terminate();
                break;

            case STARTUP_UNPARK_DOME:
                break;

            case STARTUP_UNPARKING_DOME:
                domeInterface->call(QDBus::AutoDetect, "abort");
                break;

            case STARTUP_UNPARK_MOUNT:
                break;

            case STARTUP_UNPARKING_MOUNT:
                mountInterface->call(QDBus::AutoDetect, "abort");
                break;

            case STARTUP_UNPARK_CAP:
                break;

            case STARTUP_UNPARKING_CAP:
                break;

            case STARTUP_COMPLETE:
                break;

            case STARTUP_ERROR:
                break;
        }

        startupState = STARTUP_IDLE;

        appendLogText(i18n("Startup procedure terminated."));
    }
}

void Scheduler::checkShutdownProcedure()
{
    // If shutdown procedure is not finished yet, let's check again in 1 second.
    if (checkShutdownState() == false)
        QTimer::singleShot(1000, this, SLOT(checkShutdownProcedure()));
    else
    {
        if (shutdownState == SHUTDOWN_COMPLETE)
        {
            appendLogText(i18n("Manual shutdown procedure completed successfully."));
            // Stop Ekos
            if (Options::stopEkosAfterShutdown())
                stopEkos();
        }
        else if (shutdownState == SHUTDOWN_ERROR)
            appendLogText(i18n("Manual shutdown procedure terminated due to errors."));

        shutdownState = SHUTDOWN_IDLE;
        shutdownB->setIcon(
            QIcon::fromTheme("media-playback-start"));
    }
}

void Scheduler::runShutdownProcedure()
{
    if (shutdownState == SHUTDOWN_IDLE || shutdownState == SHUTDOWN_ERROR || shutdownState == SHUTDOWN_COMPLETE)
    {
        if (KMessageBox::questionYesNo(
                nullptr, i18n("Are you sure you want to execute the shutdown procedure manually?")) == KMessageBox::Yes)
        {
            appendLogText(i18n("Warning: executing shutdown procedure manually..."));
            shutdownB->setIcon(
                QIcon::fromTheme("media-playback-stop"));
            shutdownState = SHUTDOWN_IDLE;
            checkShutdownState();
            QTimer::singleShot(1000, this, SLOT(checkShutdownProcedure()));
        }
    }
    else
    {
        switch (shutdownState)
        {
            case SHUTDOWN_IDLE:
                break;

            case SHUTDOWN_SCRIPT:
                break;

            case SHUTDOWN_SCRIPT_RUNNING:
                scriptProcess.terminate();
                break;

            case SHUTDOWN_PARK_DOME:
                break;

            case SHUTDOWN_PARKING_DOME:
                domeInterface->call(QDBus::AutoDetect, "abort");
                break;

            case SHUTDOWN_PARK_MOUNT:
                break;

            case SHUTDOWN_PARKING_MOUNT:
                mountInterface->call(QDBus::AutoDetect, "abort");
                break;

            case SHUTDOWN_PARK_CAP:
                break;

            case SHUTDOWN_PARKING_CAP:
                break;

            case SHUTDOWN_COMPLETE:
                break;

            case SHUTDOWN_ERROR:
                break;
        }

        shutdownState = SHUTDOWN_IDLE;

        appendLogText(i18n("Shutdown procedure terminated."));
    }
}

void Scheduler::loadProfiles()
{
    QString currentProfile = schedulerProfileCombo->currentText();

    QDBusReply<QStringList> profiles = ekosInterface->call(QDBus::AutoDetect, "getProfiles");

    if (profiles.error().type() == QDBusError::NoError)
    {
        schedulerProfileCombo->blockSignals(true);
        schedulerProfileCombo->clear();
        schedulerProfileCombo->addItem(i18n("Default"));
        schedulerProfileCombo->addItems(profiles);
        schedulerProfileCombo->setCurrentText(currentProfile);
        schedulerProfileCombo->blockSignals(false);
    }
}

bool Scheduler::loadSequenceQueue(const QString &fileURL, SchedulerJob *schedJob, QList<SequenceJob *> &jobs,
                                  bool &hasAutoFocus)
{
    QFile sFile;
    sFile.setFileName(fileURL);

    if (!sFile.open(QIODevice::ReadOnly))
    {
        QString message = i18n("Unable to open sequence queue file '%1'", fileURL);
        KMessageBox::sorry(0, message, i18n("Could Not Open File"));
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
                    jobs.append(processJobInfo(ep, schedJob));
            }
            delXMLEle(root);
        }
        else if (errmsg[0])
        {
            appendLogText(QString(errmsg));
            delLilXML(xmlParser);
            qDeleteAll(jobs);
            return false;
        }
    }

    return true;
}

SequenceJob *Scheduler::processJobInfo(XMLEle *root, SchedulerJob *schedJob)
{
    XMLEle *ep    = nullptr;
    XMLEle *subEP = nullptr;

    const QMap<QString, CCDFrameType> frameTypes = {
        { "Light", FRAME_LIGHT }, { "Dark", FRAME_DARK }, { "Bias", FRAME_BIAS }, { "Flat", FRAME_FLAT }
    };

    SequenceJob *job = new SequenceJob();
    QString rawPrefix, frameType, filterType;
    double exposure    = 0;
    bool filterEnabled = false, expEnabled = false, tsEnabled = false;

    /* Reset light frame presence flag before enumerating */
    if (nullptr != schedJob)
        schedJob->setLightFramesRequired(false);

    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep), "Exposure"))
        {
            exposure = atof(pcdataXMLEle(ep));
            job->setExposure(exposure);
        }
        else if (!strcmp(tagXMLEle(ep), "Filter"))
        {
            filterType = QString(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "Type"))
        {
            frameType = QString(pcdataXMLEle(ep));

            /* Record frame type and mark presence of light frames for this sequence */
            CCDFrameType const frameEnum = frameTypes[frameType];
            job->setFrameType(frameEnum);
            if (FRAME_LIGHT == frameEnum && nullptr != schedJob)
                schedJob->setLightFramesRequired(true);
        }
        else if (!strcmp(tagXMLEle(ep), "Prefix"))
        {
            subEP = findXMLEle(ep, "RawPrefix");
            if (subEP)
                rawPrefix = QString(pcdataXMLEle(subEP));

            subEP = findXMLEle(ep, "FilterEnabled");
            if (subEP)
                filterEnabled = !strcmp("1", pcdataXMLEle(subEP));

            subEP = findXMLEle(ep, "ExpEnabled");
            if (subEP)
                expEnabled = (!strcmp("1", pcdataXMLEle(subEP)));

            subEP = findXMLEle(ep, "TimeStampEnabled");
            if (subEP)
                tsEnabled = (!strcmp("1", pcdataXMLEle(subEP)));

            job->setPrefixSettings(rawPrefix, filterEnabled, expEnabled, tsEnabled);
        }
        else if (!strcmp(tagXMLEle(ep), "Count"))
        {
            job->setCount(atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "Delay"))
        {
            job->setDelay(atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "FITSDirectory"))
        {
            job->setLocalDir(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "RemoteDirectory"))
        {
            job->setRemoteDir(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "UploadMode"))
        {
            job->setUploadMode(static_cast<ISD::CCD::UploadMode>(atoi(pcdataXMLEle(ep))));
        }
    }

    // Make full prefix
    QString imagePrefix = rawPrefix;

    if (imagePrefix.isEmpty() == false)
        imagePrefix += '_';

    imagePrefix += frameType;

    if (filterEnabled && filterType.isEmpty() == false &&
        (job->getFrameType() == FRAME_LIGHT || job->getFrameType() == FRAME_FLAT))
    {
        imagePrefix += '_';

        imagePrefix += filterType;
    }

    if (expEnabled)
    {
        imagePrefix += '_';

        imagePrefix += QString::number(exposure, 'd', 0) + QString("_secs");
    }

    job->setFullPrefix(imagePrefix);


    QString targetName = schedJob->getName().remove(' ');

    // Directory postfix
    QString directoryPostfix;

    /* FIXME: Refactor directoryPostfix assignment, whose code is duplicated in capture.cpp */
    if (targetName.isEmpty())
        directoryPostfix = QLatin1Literal("/") + frameType;
    else
        directoryPostfix = QLatin1Literal("/") + targetName + QLatin1Literal("/") + frameType;
    if ((job->getFrameType() == FRAME_LIGHT || job->getFrameType() == FRAME_FLAT) && filterType.isEmpty() == false)
        directoryPostfix += QLatin1Literal("/") + filterType;

    job->setDirectoryPostfix(directoryPostfix);

    return job;
}

int Scheduler::getCompletedFiles(const QString &path, const QString &seqPrefix)
{
    int seqFileCount = 0;
    QFileInfo const path_info(path);
    QString const sig_dir(path_info.dir().path());
    QString const sig_file(path_info.baseName());

    qCDebug(KSTARS_EKOS_SCHEDULER) << QString("Searching in path '%1', files '%2*' for prefix '%3'...").arg(sig_dir, sig_file, seqPrefix);
    QDirIterator it(sig_dir, QDir::Files);

    /* FIXME: this counts all files with prefix in the storage location, not just captures. DSS analysis files are counted in, for instance. */
    while (it.hasNext())
    {
        QString const fileName = QFileInfo(it.next()).baseName();

        if (fileName.startsWith(seqPrefix))
        {
            qCDebug(KSTARS_EKOS_SCHEDULER) << QString("> Found '%1'").arg(fileName);
            seqFileCount++;
        }
    }

    return seqFileCount;
}
}
