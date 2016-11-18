/*  Ekos Scheduler Module
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    DBus calls from GSoC 2015 Ekos Scheduler project by Daniel Leu <daniel_mihai.leu@cti.pub.ro>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "Options.h"

#include <stdio.h>
#include <stdlib.h>

#include <QtDBus>
#include <QFileDialog>

#include <KMessageBox>
#include <KLocalizedString>
#include <KNotifications/KNotification>

#include "scheduleradaptor.h"
#include "dialogs/finddialog.h"
#include "ekos/capture/sequencejob.h"
#include "ekos/ekosmanager.h"
#include "kstars.h"
#include "scheduler.h"
#include "skymapcomposite.h"
#include "kstarsdata.h"
#include "ksmoon.h"
#include "ksalmanac.h"
#include "ksutils.h"
#include "mosaic.h"
#include "skyobjects/starobject.h"

#define BAD_SCORE                       -1000
#define MAX_FAILURE_ATTEMPTS            3
#define UPDATE_PERIOD_MS                1000
#define SETTING_ALTITUDE_CUTOFF         3

#define DEFAULT_CULMINATION_TIME        -60
#define DEFAULT_MIN_ALTITUDE            15
#define DEFAULT_MIN_MOON_SEPARATION     0

namespace Ekos
{

bool scoreHigherThan(SchedulerJob *job1, SchedulerJob *job2)
{
    return job1->getScore() > job2->getScore();
}

bool priorityHigherThan(SchedulerJob *job1, SchedulerJob *job2)
{
    return job1->getPriority() < job2->getPriority();
}

bool altitudeHigherThan(SchedulerJob *job1, SchedulerJob *job2)
{
    double job1_altitude = Scheduler::findAltitude(job1->getTargetCoords(), job1->getStartupTime());
    double job2_altitude = Scheduler::findAltitude(job2->getTargetCoords(), job2->getStartupTime());

    return job1_altitude > job2_altitude;
}

Scheduler::Scheduler()
{
    setupUi(this);

    new SchedulerAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Scheduler",  this);

    dirPath     = QUrl::fromLocalFile(QDir::homePath());
    state       = SCHEDULER_IDLE;
    ekosState   = EKOS_IDLE;
    indiState   = INDI_IDLE;

    startupState = STARTUP_IDLE;
    shutdownState= SHUTDOWN_IDLE;

    parkWaitState= PARKWAIT_IDLE;

    currentJob   = NULL;
    geo          = NULL;
    captureBatch = 0;
    jobUnderEdit = -1;
    mDirty       = false;
    jobEvaluationOnly=false;
    loadAndSlewProgress=false;
    autofocusCompleted=false;
    preemptiveShutdown=false;

    Dawn         = -1;
    Dusk         = -1;

    indiConnectFailureCount=0;
    focusFailureCount=0;
    guideFailureCount=0;
    alignFailureCount=0;
    captureFailureCount=0;

    noWeatherCounter=0;

    weatherStatus=IPS_IDLE;

    // Get current KStars time and set seconds to zero
    QDateTime currentDateTime = KStarsData::Instance()->lt();
    QTime     currentTime     = currentDateTime.time();
    currentTime.setHMS(currentTime.hour(), currentTime.minute(), 0);
    currentDateTime.setTime(currentTime);

    // Set initial time for startup and completion times
    startupTimeEdit->setDateTime(currentDateTime);
    completionTimeEdit->setDateTime(currentDateTime);

    // Set up DBus interfaces
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Scheduler",  this);
    ekosInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos", "org.kde.kstars.Ekos", QDBusConnection::sessionBus(), this);

    focusInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Focus", "org.kde.kstars.Ekos.Focus", QDBusConnection::sessionBus(),  this);
    captureInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Capture", "org.kde.kstars.Ekos.Capture", QDBusConnection::sessionBus(), this);
    mountInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Mount", "org.kde.kstars.Ekos.Mount", QDBusConnection::sessionBus(), this);
    alignInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Align", "org.kde.kstars.Ekos.Align", QDBusConnection::sessionBus(), this);
    guideInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Guide", "org.kde.kstars.Ekos.Guide", QDBusConnection::sessionBus(), this);
    domeInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Dome", "org.kde.kstars.Ekos.Dome", QDBusConnection::sessionBus(), this);
    weatherInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Weather", "org.kde.kstars.Ekos.Weather", QDBusConnection::sessionBus(), this);
    capInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/DustCap", "org.kde.kstars.Ekos.DustCap", QDBusConnection::sessionBus(), this);

    moon = dynamic_cast<KSMoon*> (KStarsData::Instance()->skyComposite()->findByName("Moon"));

    sleepLabel->setPixmap(QIcon::fromTheme("chronometer", QIcon(":/icons/breeze/default/chronometer.svg")).pixmap(QSize(32,32)));
    sleepLabel->hide();

    schedulerTimer.setInterval(UPDATE_PERIOD_MS);
    jobTimer.setInterval(UPDATE_PERIOD_MS);

    connect(&schedulerTimer, SIGNAL(timeout()), this, SLOT(checkStatus()));
    connect(&jobTimer, SIGNAL(timeout()), this, SLOT(checkJobStage()));

    pi = new QProgressIndicator(this);
    bottomLayout->addWidget(pi,0,0);

    geo = KStarsData::Instance()->geo();

    raBox->setDegType(false); //RA box should be HMS-style

    addToQueueB->setIcon(QIcon::fromTheme("list-add", QIcon(":/icons/breeze/default/list-add.svg")));
    addToQueueB->setToolTip(i18n("Add observation job to list."));
    addToQueueB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    removeFromQueueB->setIcon(QIcon::fromTheme("list-remove", QIcon(":/icons/breeze/default/list-remove.svg")));
    removeFromQueueB->setToolTip(i18n("Remove observation job from list."));
    removeFromQueueB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    evaluateOnlyB->setIcon(QIcon::fromTheme("tools-wizard", QIcon(":/icons/breeze/default/tools-wizard.svg")));
    evaluateOnlyB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    mosaicB->setIcon(QIcon::fromTheme("zoom-draw", QIcon(":/icons/breeze/default/zoom-draw.svg")));
    mosaicB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    queueSaveAsB->setIcon(QIcon::fromTheme("document-save-as", QIcon(":/icons/breeze/default/document-save-as.svg")));
    queueSaveAsB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueSaveB->setIcon(QIcon::fromTheme("document-save", QIcon(":/icons/breeze/default/document-save.svg")));
    queueSaveB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    queueLoadB->setIcon(QIcon::fromTheme("document-open", QIcon(":/icons/breeze/default/document-open.svg")));
    queueLoadB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    loadSequenceB->setIcon(QIcon::fromTheme("document-open", QIcon(":/icons/breeze/default/document-open.svg")));
    loadSequenceB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    selectStartupScriptB->setIcon(QIcon::fromTheme("document-open", QIcon(":/icons/breeze/default/document-open.svg")));
    selectStartupScriptB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    selectShutdownScriptB->setIcon(QIcon::fromTheme("document-open", QIcon(":/icons/breeze/default/document-open.svg")));
    selectShutdownScriptB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    selectFITSB->setIcon(QIcon::fromTheme("document-open", QIcon(":/icons/breeze/default/document-open.svg")));
    selectFITSB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    startupB->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/icons/breeze/default/media-playback-start.svg")));
    startupB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    shutdownB->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/icons/breeze/default/media-playback-start.svg")));
    shutdownB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    connect(startupB, SIGNAL(clicked()), this, SLOT(runStartupProcedure()));
    connect(shutdownB, SIGNAL(clicked()), this, SLOT(runShutdownProcedure()));

    selectObjectB->setIcon(QIcon::fromTheme("edit-find", QIcon(":/icons/breeze/default/edit-find.svg")));
    connect(selectObjectB,SIGNAL(clicked()),this,SLOT(selectObject()));
    connect(selectFITSB,SIGNAL(clicked()),this,SLOT(selectFITS()));
    connect(loadSequenceB,SIGNAL(clicked()),this,SLOT(selectSequence()));
    connect(selectStartupScriptB, SIGNAL(clicked()), this, SLOT(selectStartupScript()));
    connect(selectShutdownScriptB, SIGNAL(clicked()), this, SLOT(selectShutdownScript()));

    connect(mosaicB, SIGNAL(clicked()), this, SLOT(startMosaicTool()));
    connect(addToQueueB,SIGNAL(clicked()),this,SLOT(addJob()));
    connect(removeFromQueueB, SIGNAL(clicked()), this, SLOT(removeJob()));
    connect(evaluateOnlyB, SIGNAL(clicked()), this, SLOT(startJobEvaluation()));
    connect(queueTable, SIGNAL(clicked(QModelIndex)), this, SLOT(loadJob(QModelIndex)));
    connect(queueTable, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(resetJobState(QModelIndex)));

    startB->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/icons/breeze/default/media-playback-start.svg")));
    startB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    pauseB->setIcon(QIcon::fromTheme("media-playback-pause", QIcon(":/icons/breeze/default/media-playback-pause.svg")));
    pauseB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    connect(startB,SIGNAL(clicked()),this,SLOT(toggleScheduler()));
    connect(pauseB,SIGNAL(clicked()),this,SLOT(pause()));

    connect(queueSaveAsB,SIGNAL(clicked()),this,SLOT(saveAs()));
    connect(queueSaveB,SIGNAL(clicked()),this,SLOT(save()));
    connect(queueLoadB,SIGNAL(clicked()),this,SLOT(load()));

    connect(twilightCheck, SIGNAL(toggled(bool)), this, SLOT(checkTwilightWarning(bool)));

    loadProfiles();

}

Scheduler::~Scheduler()
{

}

void Scheduler::watchJobChanges(bool enable)
{
    if (enable)
    {
        connect(fitsEdit, SIGNAL(editingFinished()), this, SLOT(setDirty()));
        connect(startupScript, SIGNAL(editingFinished()), this, SLOT(setDirty()));
        connect(shutdownScript, SIGNAL(editingFinished()), this, SLOT(setDirty()));
        connect(profileCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setDirty()));

        connect(profileCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setDirty()));
        connect(stepsButtonGroup, SIGNAL(buttonToggled(int,bool)), this, SLOT(setDirty()));
        connect(startupButtonGroup, SIGNAL(buttonToggled(int,bool)), this, SLOT(setDirty()));
        connect(constraintButtonGroup, SIGNAL(buttonToggled(int,bool)), this, SLOT(setDirty()));
        connect(completionButtonGroup, SIGNAL(buttonToggled(int,bool)), this, SLOT(setDirty()));

        connect(startupProcedureButtonGroup, SIGNAL(buttonToggled(int,bool)), this, SLOT(setDirty()));
        connect(shutdownProcedureGroup, SIGNAL(buttonToggled(int,bool)), this, SLOT(setDirty()));

        connect(culminationOffset, SIGNAL(editingFinished()), this, SLOT(setDirty()));
        connect(startupTimeEdit, SIGNAL(editingFinished()), this, SLOT(setDirty()));
        connect(minAltitude, SIGNAL(editingFinished()), this, SLOT(setDirty()));
        connect(minMoonSeparation, SIGNAL(editingFinished()), this, SLOT(setDirty()));
        connect(completionTimeEdit, SIGNAL(editingFinished()), this, SLOT(setDirty()));

    }
    else
    {
       //disconnect(this, SLOT(setDirty()));

        disconnect(fitsEdit, SIGNAL(editingFinished()), this, SLOT(setDirty()));
        disconnect(startupScript, SIGNAL(editingFinished()), this, SLOT(setDirty()));
        disconnect(shutdownScript, SIGNAL(editingFinished()), this, SLOT(setDirty()));
        disconnect(profileCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setDirty()));

        disconnect(profileCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setDirty()));
        disconnect(stepsButtonGroup, SIGNAL(buttonToggled(int,bool)), this, SLOT(setDirty()));
        disconnect(startupButtonGroup, SIGNAL(buttonToggled(int,bool)), this, SLOT(setDirty()));
        disconnect(constraintButtonGroup, SIGNAL(buttonToggled(int,bool)), this, SLOT(setDirty()));
        disconnect(completionButtonGroup, SIGNAL(buttonToggled(int,bool)), this, SLOT(setDirty()));

        disconnect(startupProcedureButtonGroup, SIGNAL(buttonToggled(int,bool)), this, SLOT(setDirty()));
        disconnect(shutdownProcedureGroup, SIGNAL(buttonToggled(int,bool)), this, SLOT(setDirty()));

        disconnect(culminationOffset, SIGNAL(editingFinished()), this, SLOT(setDirty()));
        disconnect(startupTimeEdit, SIGNAL(editingFinished()), this, SLOT(setDirty()));
        disconnect(minAltitude, SIGNAL(editingFinished()), this, SLOT(setDirty()));
        disconnect(minMoonSeparation, SIGNAL(editingFinished()), this, SLOT(setDirty()));
        disconnect(completionTimeEdit, SIGNAL(editingFinished()), this, SLOT(setDirty()));
    }
}

void Scheduler::appendLogText(const QString &text)
{

    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    if (Options::verboseLogging())
        qDebug() << "Scheduler: " << text;

    emit newLog();
}

void Scheduler::clearLog()
{
    logText.clear();
    emit newLog();
}

void Scheduler::selectObject()
{

    QPointer<FindDialog> fd = new FindDialog( this );
    if ( fd->exec() == QDialog::Accepted )
    {
        SkyObject *object = fd->targetObject();
        addObject(object);
    }

    delete fd;

}

void Scheduler::addObject(SkyObject *object)
{
    if( object != NULL )
    {
        QString finalObjectName(object->name());

        if( object->name() == "star" )
        {
            StarObject *s = (StarObject *)object;

            if( s->getHDIndex() != 0 )
                    finalObjectName = QString("HD %1" ).arg( QString::number( s->getHDIndex() ) );
        }

        nameEdit->setText(finalObjectName);
        raBox->setText(object->ra0().toHMSString());
        decBox->setText(object->dec0().toDMSString());

        addToQueueB->setEnabled(sequenceEdit->text().isEmpty() == false);
        mosaicB->setEnabled(sequenceEdit->text().isEmpty() == false);
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
    sequenceURL = QFileDialog::getOpenFileUrl(this, i18n("Select Sequence Queue"), dirPath, i18n("Ekos Sequence Queue (*.esq)"));
    if (sequenceURL.isEmpty())
        return;

    dirPath = QUrl(sequenceURL.url(QUrl::RemoveFilename));

    sequenceEdit->setText(sequenceURL.toLocalFile());

    // For object selection, all fields must be filled
    if ( (raBox->isEmpty() == false && decBox->isEmpty() == false && nameEdit->text().isEmpty() == false)
    // For FITS selection, only the name and fits URL should be filled.
        || (nameEdit->text().isEmpty() == false && fitsURL.isEmpty() == false) )
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

    mDirty=true;
    startupScript->setText(startupScriptURL.toLocalFile());
}

void Scheduler::selectShutdownScript()
{
    shutdownScriptURL = QFileDialog::getOpenFileUrl(this, i18n("Select Shutdown Script"), dirPath, i18n("Script (*)"));
    if (shutdownScriptURL.isEmpty())
        return;

    dirPath = QUrl(shutdownScriptURL.url(QUrl::RemoveFilename));

    mDirty=true;
    shutdownScript->setText(shutdownScriptURL.toLocalFile());
}

void Scheduler::addJob()
{
    if (jobUnderEdit >= 0)
    {
        resetJobEdit();
        return;
    }

    //jobUnderEdit = false;
    saveJob();
}

void Scheduler::saveJob()
{

    if (state == SCHEDULER_RUNNIG)
    {
        appendLogText(i18n("You cannot add or modify a job while the scheduler is running."));
        return;
    }

    watchJobChanges(false);

    if(nameEdit->text().isEmpty())
    {
        appendLogText(i18n("Target name is required."));
        return;
    }

    if (sequenceEdit->text().isEmpty())
    {
        appendLogText(i18n("Sequence file is required."));
        return;
    }

    // Coordinates are required unless it is a FITS file
    if ( (raBox->isEmpty() || decBox->isEmpty()) && fitsURL.isEmpty())
    {
        appendLogText(i18n("Target coordinates are required."));
        return;
    }

    // Create or Update a scheduler job
    SchedulerJob *job = NULL;

    if (jobUnderEdit >= 0)
        job = jobs.at(queueTable->currentRow());
    else
        job = new SchedulerJob();

    job->setName(nameEdit->text());

    job->setPriority(prioritySpin->value());

    bool raOk=false, decOk=false;
    dms ra( raBox->createDms( false, &raOk ) ); //false means expressed in hours
    dms dec( decBox->createDms( true, &decOk ) );

    if (raOk == false)
    {
        appendLogText(i18n("RA value %1 is invalid.", raBox->text()));
        return;
    }

    if (decOk == false)
    {
        appendLogText(i18n("DEC value %1 is invalid.", decBox->text()));
        return;
    }

    job->setTargetCoords(ra, dec);

    job->setDateTimeDisplayFormat(startupTimeEdit->displayFormat());
    job->setSequenceFile(sequenceURL);
    job->setProfile(profileCombo->currentText());

    fitsURL = QUrl::fromLocalFile(fitsEdit->text());
    job->setFITSFile(fitsURL);

    // #1 Startup conditions

    if (asapConditionR->isChecked())
        job->setStartupCondition(SchedulerJob::START_ASAP);
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

    job->setFileStartupCondition(job->getStartupCondition());

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

    // #3 Completion conditions
    if (sequenceCompletionR->isChecked())
        job->setCompletionCondition(SchedulerJob::FINISH_SEQUENCE);
    else if (loopCompletionR->isChecked())
        job->setCompletionCondition(SchedulerJob::FINISH_LOOP);
    else
    {
        job->setCompletionCondition(SchedulerJob::FINISH_AT);
        job->setCompletionTime(completionTimeEdit->dateTime());
    }

    // Job steps
    job->setStepPipeline(SchedulerJob::USE_NONE);
    if (trackStepCheck->isChecked())
        job->setStepPipeline(static_cast<SchedulerJob::StepPipeline> (job->getStepPipeline() | SchedulerJob::USE_TRACK));
    if (focusStepCheck->isChecked())
        job->setStepPipeline(static_cast<SchedulerJob::StepPipeline> (job->getStepPipeline() | SchedulerJob::USE_FOCUS));
    if (alignStepCheck->isChecked())
        job->setStepPipeline(static_cast<SchedulerJob::StepPipeline> (job->getStepPipeline() | SchedulerJob::USE_ALIGN));
    if (guideStepCheck->isChecked())
        job->setStepPipeline(static_cast<SchedulerJob::StepPipeline> (job->getStepPipeline() | SchedulerJob::USE_GUIDE));


    // Add job to queue if it is new
    if (jobUnderEdit == -1)
        jobs.append(job);

    int currentRow = 0;
    if (jobUnderEdit == -1)
    {
        currentRow = queueTable->rowCount();
        queueTable->insertRow(currentRow);
    }
    else
        currentRow = queueTable->currentRow();

    QTableWidgetItem *nameCell = (jobUnderEdit >= 0) ? queueTable->item(currentRow, 0) : new QTableWidgetItem();
    nameCell->setText(job->getName());
    nameCell->setTextAlignment(Qt::AlignHCenter);
    nameCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *statusCell = (jobUnderEdit >= 0) ? queueTable->item(currentRow, 1) : new QTableWidgetItem();
    statusCell->setTextAlignment(Qt::AlignHCenter);
    statusCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    job->setStatusCell(statusCell);
    // Refresh state
    job->setState(job->getState());

    QTableWidgetItem *startupCell = (jobUnderEdit >= 0) ? queueTable->item(currentRow, 2) : new QTableWidgetItem();
    if (startupTimeConditionR->isChecked())
        startupCell->setText(startupTimeEdit->text());
    else
        startupCell->setText(QString());
    startupCell->setTextAlignment(Qt::AlignHCenter);
    startupCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    job->setStartupCell(startupCell);

    QTableWidgetItem *completionCell = (jobUnderEdit >= 0) ? queueTable->item(currentRow, 3) : new QTableWidgetItem();
    if (timeCompletionR->isChecked())
        completionCell->setText(completionTimeEdit->text());
    else
        completionCell->setText(QString());
    completionCell->setTextAlignment(Qt::AlignHCenter);
    completionCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *estimatedTimeCell = (jobUnderEdit >= 0) ? queueTable->item(currentRow, 4) : new QTableWidgetItem();
    if (job->getEstimatedTime() > 0)
    {
        QTime estimatedTime = QTime::fromMSecsSinceStartOfDay(job->getEstimatedTime()*3600);
        estimatedTimeCell->setText(estimatedTime.toString("HH:mm:ss"));
    }
    else
        estimatedTimeCell->setText(QString());
    estimatedTimeCell->setTextAlignment(Qt::AlignHCenter);
    estimatedTimeCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    job->setEstimatedTimeCell(estimatedTimeCell);

    if (jobUnderEdit == -1)
    {
        queueTable->setItem(currentRow, 0, nameCell);
        queueTable->setItem(currentRow, 1, statusCell);
        queueTable->setItem(currentRow, 2, startupCell);
        queueTable->setItem(currentRow, 3, completionCell);
        queueTable->setItem(currentRow, 4, estimatedTimeCell);
    }

    if (queueTable->rowCount() > 0)
    {
        queueSaveAsB->setEnabled(true);
        queueSaveB->setEnabled(true);
        startB->setEnabled(true);
        mDirty = true;
    }

    removeFromQueueB->setEnabled(true);

    if (jobUnderEdit == -1)
    {
        startB->setEnabled(true);
        evaluateOnlyB->setEnabled(true);
    }

    watchJobChanges(true);
}

void Scheduler::resetJobState(QModelIndex i)
{
    if (state == SCHEDULER_RUNNIG)
    {
        appendLogText(i18n("You cannot reset a job while the scheduler is running."));
        return;
    }

    SchedulerJob *job = jobs.at(i.row());
    if (job == NULL)
        return;

    job->setState(SchedulerJob::JOB_IDLE);
    job->setStage(SchedulerJob::STAGE_IDLE);

    if (job->getFileStartupCondition() != SchedulerJob::START_AT)
       queueTable->item(i.row(), 2)->setText(QString());

    if (job->getCompletionCondition() != SchedulerJob::FINISH_AT)
       queueTable->item(i.row(), 3)->setText(QString());

    appendLogText(i18n("Job %1 status is reset.", job->getName()));
}

void Scheduler::loadJob(QModelIndex i)
{
    if (jobUnderEdit == i.row())
        return;

    if (state == SCHEDULER_RUNNIG)
    {
        appendLogText(i18n("You cannot add or modify a job while the scheduler is running."));
        return;
    }

    SchedulerJob *job = jobs.at(i.row());

    if (job == NULL)
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

    profileCombo->setCurrentText(job->getProfile());

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

        case SchedulerJob::FINISH_LOOP:
            loopCompletionR->setChecked(true);
            break;

        case SchedulerJob::FINISH_AT:
            timeCompletionR->setChecked(true);
            completionTimeEdit->setDateTime(job->getCompletionTime());
            break;
    }

   appendLogText(i18n("Editing job #%1...", i.row()+1));

   addToQueueB->setIcon(QIcon::fromTheme("edit-undo", QIcon(":/icons/breeze/default/edit-undo.svg")));
   addToQueueB->setStyleSheet("background-color:orange;}");
   addToQueueB->setEnabled(true);
   startB->setEnabled(false);
   evaluateOnlyB->setEnabled(false);
   addToQueueB->setToolTip(i18n("Exit edit mode"));

   jobUnderEdit = i.row();

   watchJobChanges(true);
}

void Scheduler::resetJobEdit()
{
    if (jobUnderEdit == -1)
        return;

    appendLogText(i18n("Edit mode cancelled."));

    jobUnderEdit = -1;

    watchJobChanges(false);

    addToQueueB->setIcon(QIcon::fromTheme("list-add", QIcon(":/icons/breeze/default/list-add.svg")));
    addToQueueB->setStyleSheet(QString());
    addToQueueB->setToolTip(i18n("Add observation job to list."));
    queueTable->clearSelection();

    evaluateOnlyB->setEnabled(true);
    startB->setEnabled(true);

    //removeFromQueueB->setToolTip(i18n("Remove observation job from list."));
}

void Scheduler::removeJob()
{
    /*if (jobUnderEdit)
    {
        resetJobEdit();
        return;
    }*/

    int currentRow = queueTable->currentRow();

    if (currentRow < 0)
    {
        currentRow = queueTable->rowCount()-1;
        if (currentRow < 0)
            return;
    }

    queueTable->removeRow(currentRow);

    SchedulerJob *job = jobs.at(currentRow);
    jobs.removeOne(job);
    delete (job);

    if (queueTable->rowCount() == 0)
    {
        removeFromQueueB->setEnabled(false);
        evaluateOnlyB->setEnabled(false);
    }

    for (int i=0; i < jobs.count(); i++)
    {
        jobs.at(i)->setStatusCell(queueTable->item(i, 1));
        jobs.at(i)->setStartupCell(queueTable->item(i, 2));
    }

    queueTable->selectRow(queueTable->currentRow());

    if (queueTable->rowCount() == 0)
    {
        queueSaveAsB->setEnabled(false);
        queueSaveB->setEnabled(false);
        startB->setEnabled(false);
        pauseB->setEnabled(false);

        if (jobUnderEdit >= 0)
            resetJobEdit();
    }
    else
        loadJob(queueTable->currentIndex());

    mDirty = true;

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
    if(state != SCHEDULER_RUNNIG)
        return;

    if (Options::verboseLogging())
        qDebug() << "Scheduler: Stop";

    // Stop running job and abort all others
    // in case of soft shutdown we skip this
    if (preemptiveShutdown == false)
    {
        bool wasAborted = false;
        foreach(SchedulerJob *job, jobs)
        {
            if (job == currentJob)
            {
                stopCurrentJobAction();
                stopGuiding();
            }

            if (job->getState() <= SchedulerJob::JOB_BUSY)
            {
                job->setState(SchedulerJob::JOB_ABORTED);
                job->setStartupCondition(job->getFileStartupCondition());
                wasAborted = true;
            }
        }

        if (wasAborted)
            KNotification::event( QLatin1String( "SchedulerAborted" ) , i18n("Scheduler aborted."));
    }

    schedulerTimer.stop();
    jobTimer.stop();

    state           = SCHEDULER_IDLE;
    ekosState       = EKOS_IDLE;
    indiState       = INDI_IDLE;

    parkWaitState   = PARKWAIT_IDLE;

    // Only reset startup state to idle if the startup procedure was interrupted before it had the chance to complete.
    // Or if we're doing a soft shutdown
    if (startupState != STARTUP_COMPLETE || preemptiveShutdown)
    {
        if (startupState == STARTUP_SCRIPT)
        {
            scriptProcess.disconnect();
            scriptProcess.terminate();
        }

        startupState    = STARTUP_IDLE;
    }

    shutdownState   = SHUTDOWN_IDLE;

    currentJob = NULL;
    captureBatch =0;
    indiConnectFailureCount=0;
    focusFailureCount=0;
    guideFailureCount=0;
    alignFailureCount=0;
    captureFailureCount=0;
    jobEvaluationOnly=false;
    loadAndSlewProgress=false;
    autofocusCompleted=false;

    startupB->setEnabled(true);
    shutdownB->setEnabled(true);

    // If soft shutdown, we return for now
    if (preemptiveShutdown)
    {
        sleepLabel->setToolTip(i18n("Scheduler is in shutdown until next job is ready"));
        sleepLabel->show();
        return;
    }

    if (scriptProcess.state() == QProcess::Running)
        scriptProcess.terminate();

    sleepTimer.stop();
    sleepTimer.disconnect();
    sleepLabel->hide();
    pi->stopAnimation();

    startB->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/icons/breeze/default/media-playback-start.svg")));
    startB->setToolTip(i18n("Start Scheduler"));
    pauseB->setEnabled(false);
    //startB->setText("Start Scheduler");

    queueLoadB->setEnabled(true);
    addToQueueB->setEnabled(true);
    removeFromQueueB->setEnabled(true);
    mosaicB->setEnabled(true);
    evaluateOnlyB->setEnabled(true);
}

void Scheduler::start()
{
    if(state == SCHEDULER_RUNNIG)
        return;
    else if (state == SCHEDULER_PAUSED)
    {
        state = SCHEDULER_RUNNIG;
        appendLogText(i18n("Scheduler resumed."));

        startB->setIcon(QIcon::fromTheme("media-playback-stop", QIcon(":/icons/breeze/default/media-playback-stop.svg")));
        startB->setToolTip(i18n("Stop Scheduler"));
        return;
    }

    startupScriptURL = QUrl::fromUserInput(startupScript->text());
    if (startupScript->text().isEmpty() == false && startupScriptURL.isValid() == false)
    {
        appendLogText(i18n("Startup script URL %1 is not valid.", startupScript->text()));
        return;
    }

    shutdownScriptURL= QUrl::fromUserInput(shutdownScript->text());
    if (shutdownScript->text().isEmpty() == false && shutdownScriptURL.isValid() == false)
    {
        appendLogText(i18n("Shutdown script URL %1 is not valid.", shutdownScript->text()));
        return;
    }

    if (Options::verboseLogging())
        qDebug() << "Scheduler: Start";

    pi->startAnimation();

    sleepLabel->hide();

    //startB->setText("Stop Scheduler");
    startB->setIcon(QIcon::fromTheme("media-playback-stop", QIcon(":/icons/breeze/default/media-playback-stop.svg")));
    startB->setToolTip(i18n("Stop Scheduler"));
    pauseB->setEnabled(true);

    if (Dawn < 0)
        calculateDawnDusk();

    state = SCHEDULER_RUNNIG;

    currentJob = NULL;
    jobEvaluationOnly=false;

    // Reset all aborted jobs
    foreach(SchedulerJob *job, jobs)
    {
        if (job->getState() == SchedulerJob::JOB_ABORTED)
        {
            job->setState(SchedulerJob::JOB_IDLE);
            job->setStage(SchedulerJob::STAGE_IDLE);
        }
    }

    queueLoadB->setEnabled(false);
    addToQueueB->setEnabled(false);
    removeFromQueueB->setEnabled(false);
    mosaicB->setEnabled(false);
    evaluateOnlyB->setEnabled(false);

    startupB->setEnabled(false);
    shutdownB->setEnabled(false);

    schedulerTimer.start();
}

void Scheduler::pause()
{
    state = SCHEDULER_PAUSED;
    appendLogText(i18n("Scheduler paused."));
    pauseB->setEnabled(false);

    startB->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/icons/breeze/default/media-playback-start.svg")));
    startB->setToolTip(i18n("Resume Scheduler"));
}

void Scheduler::evaluateJobs()
{
    foreach(SchedulerJob *job, jobs)
    {
        if (job->getState() > SchedulerJob::JOB_SCHEDULED)
            continue;

        if (job->getState() == SchedulerJob::JOB_IDLE)
            job->setState(SchedulerJob::JOB_EVALUATION);

        int16_t score = 0;

        QDateTime now = KStarsData::Instance()->lt();

        if (job->getEstimatedTime() < 0)
        {
            if (estimateJobTime(job) == false)
            {
                job->setState(SchedulerJob::JOB_INVALID);
                continue;
            }

            if (job->getEstimatedTime() == 0)
            {
                job->setState(SchedulerJob::JOB_COMPLETE);
                continue;
            }
        }

        // #1 Check startup conditions
        switch (job->getStartupCondition())
        {
                // #1.1 ASAP?
                case SchedulerJob::START_ASAP:           
                    // If not light frames are required, run it now
                    if (job->getLightFramesRequired())
                        score = calculateJobScore(job, now);
                    else
                        score = 1000;

                    job->setScore(score);

                    // If we can't start now, let's schedule it
                    if (score < 0)
                    {
                        // If Altitude or Dark score are negative, we try to schedule a better time for altitude and dark sky period.
                        if (calculateAltitudeTime(job, job->getMinAltitude() > 0 ? job->getMinAltitude() : 0, job->getMinMoonSeparation()))
                        {
                            //appendLogText(i18n("%1 observation job is scheduled at %2", job->getName(), job->getStartupTime().toString()));
                            job->setState(SchedulerJob::JOB_SCHEDULED);
                            // Since it's scheduled, we need to skip it now and re-check it later since its startup condition changed to START_AT
                            job->setScore(BAD_SCORE);
                            continue;
                        }
                        else
                        {
                            job->setState(SchedulerJob::JOB_INVALID);
                            appendLogText(i18n("Ekos failed to schedule %1.", job->getName()));
                        }
                    }
                    else if (isWeatherOK(job) == false)
                        job->setScore(BAD_SCORE);
                    else
                        appendLogText(i18n("%1 observation job is due to run as soon as possible.", job->getName()));
                    break;

                  // #1.2 Culmination?
                  case SchedulerJob::START_CULMINATION:
                        if (calculateCulmination(job))
                        {
                            appendLogText(i18n("%1 observation job is scheduled at %2", job->getName(), job->getStartupTime().toString()));
                            job->setState(SchedulerJob::JOB_SCHEDULED);
                            // Since it's scheduled, we need to skip it now and re-check it later since its startup condition changed to START_AT
                            job->setScore(BAD_SCORE);
                            continue;
                        }
                        else
                            job->setState(SchedulerJob::JOB_INVALID);
                        break;

                 // #1.3 Start at?
                 case SchedulerJob::START_AT:
                 {
                    if (job->getCompletionCondition() == SchedulerJob::FINISH_AT)
                    {
                        if (job->getStartupTime().secsTo(job->getCompletionTime()) <= 0)
                        {
                            appendLogText(i18n("%1 completion time (%2) is earlier than start up time (%3)", job->getName(), job->getCompletionTime().toString(), job->getStartupTime().toString()));
                            job->setState(SchedulerJob::JOB_INVALID);
                            continue;
                        }
                    }

                    QDateTime startupTime = job->getStartupTime();
                    int timeUntil = KStarsData::Instance()->lt().secsTo(startupTime);
                    // If starting time already passed by 5 minutes (default), we mark the job as invalid
                    if (timeUntil < (-1 * Options::leadTime() * 60))
                    {
                        dms passedUp( timeUntil / 3600.0);
                        if (job->getState() == SchedulerJob::JOB_EVALUATION)
                        {
                            appendLogText(i18n("%1 startup time already passed by %2. Job is marked as invalid.", job->getName(), passedUp.toHMSString()));
                            job->setState(SchedulerJob::JOB_INVALID);
                        }
                        else
                        {
                            appendLogText(i18n("%1 startup time already passed by %2. Aborting job...", job->getName(), passedUp.toHMSString()));
                            job->setState(SchedulerJob::JOB_ABORTED);
                        }

                        continue;
                    }
                    // Start scoring once we reach startup time
                    else if (timeUntil <= 0)
                    {
                        /*score += getAltitudeScore(job, now);
                        score += getMoonSeparationScore(job, now);
                        score += getDarkSkyScore(now);*/
                        score = calculateJobScore(job, now);

                        if (score < 0)
                        {
                            if (job->getState() == SchedulerJob::JOB_EVALUATION)
                            {
                                appendLogText(i18n("%1 observation job evaluation failed with a score of %2. Aborting job...", job->getName(), score));
                                job->setState(SchedulerJob::JOB_INVALID);
                            }
                            else
                            {
                                appendLogText(i18n("%1 observation job updated score is %2 %3 seconds after startup time. Aborting job...", job->getName(), abs(timeUntil), score));
                                job->setState(SchedulerJob::JOB_ABORTED);
                            }

                            continue;
                        }
                        // If job is already scheduled, we check the weather, and if it is not OK, we set bad score until weather improves.
                        else if (isWeatherOK(job) == false)
                            score += BAD_SCORE;
                    }
                    // If it is in the future and originally was designated as ASAP job
                    // Job must be less than 12 hours away to be considered for re-evaluation
                    else if (timeUntil > (Options::leadTime() * 60) && (timeUntil < 12 * 3600) && job->getFileStartupCondition() == SchedulerJob::START_ASAP)
                    {
                        QDateTime nextJobTime = now.addSecs(Options::leadTime() * 60);
                        if (job->getEnforceTwilight() == false || (now > duskDateTime && now < preDawnDateTime))
                            job->setStartupTime(nextJobTime);
                        score += BAD_SCORE;
                    }
                    // If time is far in the future, we make the score negative
                    else
                    {
                        if (job->getState() == SchedulerJob::JOB_EVALUATION && calculateJobScore(job, job->getStartupTime()) < 0)
                        {
                            appendLogText(i18n("%1 observation job evaluation failed with a score of %2. Aborting job...", job->getName(), score));
                            job->setState(SchedulerJob::JOB_INVALID);
                            continue;

                        }

                        score += BAD_SCORE;
                    }

                    job->setScore(score);
                  }
                    break;
        }

       // appendLogText(i18n("Job total score is %1", score));

        //if (score > 0 && job->getState() == SchedulerJob::JOB_EVALUATION)
        if (job->getState() == SchedulerJob::JOB_EVALUATION)
            job->setState(SchedulerJob::JOB_SCHEDULED);
    }

    int invalidJobs=0, completedJobs=0, abortedJobs=0, upcomingJobs=0;

    // Find invalid jobs
    foreach(SchedulerJob *job, jobs)
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

    if (upcomingJobs == 0 && jobEvaluationOnly == false)
    {
        if (invalidJobs == jobs.count())
        {
            appendLogText(i18n("No valid jobs found, aborting..."));
            stop();
            return;
        }

        if (invalidJobs > 0)
            appendLogText(i18np("%1 job is invalid.", "%1 jobs are invalid.", invalidJobs));

        if (abortedJobs > 0)
            appendLogText(i18np("%1 job aborted.", "%1 jobs aborted", abortedJobs));

        if (completedJobs > 0)
            appendLogText(i18np("%1 job completed.", "%1 jobs completed.", completedJobs));

        if (startupState == STARTUP_COMPLETE)
        {
            appendLogText(i18n("Scheduler complete. Starting shutdown procedure..."));
            // Let's start shutdown procedure
            checkShutdownState();
        }
        else
            stop();

        return;
    }

    SchedulerJob *bestCandidate = NULL;

    updatePreDawn();

    QList<SchedulerJob*> sortedJobs = jobs;

    // Order by altitude first
    qSort(sortedJobs.begin(), sortedJobs.end(), altitudeHigherThan);
    // Then by priority
    qSort(sortedJobs.begin(), sortedJobs.end(), priorityHigherThan);


    SchedulerJob *firstJob      = sortedJobs.first();
    QDateTime firstStartTime    = firstJob->getStartupTime();
    QDateTime lastStartTime     = firstJob->getStartupTime();
    double lastJobEstimatedTime = firstJob->getEstimatedTime();
    int daysCount               = 0;


    // Make sure no two jobs have the same scheduled time or overlap with other jobs
    foreach(SchedulerJob *job, sortedJobs)
    {
        // If this job is not scheduled, continue
        // If this job startup conditon is not to start at a specific time, continue
        if (job == firstJob || job->getState() != SchedulerJob::JOB_SCHEDULED || job->getStartupCondition() != SchedulerJob::START_AT)
            continue;

        double timeBetweenJobs = fabs(firstStartTime.secsTo(job->getStartupTime()));

        // If there are within 5 minutes of each other, try to advance scheduling time of the lower altitude one
        if (timeBetweenJobs  < (Options::leadTime())*60)
        {
            double delayJob = timeBetweenJobs + lastJobEstimatedTime;

            if (delayJob < (Options::leadTime()*60))
                delayJob = Options::leadTime()*60;

            QDateTime otherjob_time = lastStartTime.addSecs(delayJob);
            // If other jobs starts after pre-dawn limit, then we scheduler it to the next day.
            // FIXME: After changing time we are not evaluating job again when we should.
            if (otherjob_time >= preDawnDateTime.addDays(daysCount))
            {
                daysCount++;

                lastStartTime = job->getStartupTime().addDays(daysCount);
                job->setStartupTime(lastStartTime);
                lastStartTime.addSecs(delayJob);
            }
            else
            {
                lastStartTime = lastStartTime.addSecs(delayJob);
                job->setStartupTime(lastStartTime);
            }

            job->setState(SchedulerJob::JOB_SCHEDULED);

            appendLogText(i18n("Observation jobs %1 and %2 have close start up times. %2 is rescheduled to %3.", firstJob->getName(), job->getName(), job->getStartupTime().toString()));
        }

        lastJobEstimatedTime = job->getEstimatedTime();
    }

    if (jobEvaluationOnly)
    {
        appendLogText(i18n("Job evaluation complete."));
        jobEvaluationOnly = false;
        return;
    }

    // Find best score
    /*foreach(SchedulerJob *job, jobs)
    {
        if (job->getState() != SchedulerJob::JOB_SCHEDULED)
            continue;

        int jobScore    = job->getScore();
        int jobPriority = job->getPriority();

        if (jobPriority <= maxPriority)
        {
            maxPriority = jobPriority;

            if (jobScore > 0 && jobScore > maxScore)
            {
                    maxScore    = jobScore;
                    bestCandidate = job;
            }
        }
    }*/

    // Order by score first
    qSort(sortedJobs.begin(), sortedJobs.end(), scoreHigherThan);
    // Then by priority
    qSort(sortedJobs.begin(), sortedJobs.end(), priorityHigherThan);

    foreach(SchedulerJob *job, sortedJobs)
    {
        if (job->getState() != SchedulerJob::JOB_SCHEDULED || job->getScore() <= 0)
            continue;

         bestCandidate = job;
         break;

    }

    if (bestCandidate != NULL)
    {
        // If mount was previously parked awaiting job activation, we unpark it.
        if (parkWaitState == PARKWAIT_PARKED)
        {
            parkWaitState = PARKWAIT_UNPARK;
            return;
        }

        appendLogText(i18n("Found candidate job %1 (Priority #%2).", bestCandidate->getName(), bestCandidate->getPriority() ));

        currentJob = bestCandidate;
    }
    // If we already started, we check when the next object is scheduled at.
    // If it is more than 30 minutes in the future, we park the mount if that is supported
    // and we unpark when it is due to start.
    else// if (startupState == STARTUP_COMPLETE)
    {
        int nextObservationTime= 1e6;
        SchedulerJob *nextObservationJob = NULL;
        foreach(SchedulerJob *job, jobs)
        {
            if (job->getState() != SchedulerJob::JOB_SCHEDULED || job->getStartupCondition() != SchedulerJob::START_AT)
                continue;

            int timeLeft = KStarsData::Instance()->lt().secsTo(job->getStartupTime());

            if (timeLeft < nextObservationTime)
            {
                nextObservationTime = timeLeft;
                nextObservationJob = job;
            }
        }

        if (nextObservationJob)
        {
            // If start up procedure is complete and the user selected pre-emptive shutdown, let us check if the next observation time exceed
            // the pre-emptive shutdown time in hours (default 2). If it exceeds that, we perform complete shutdown until next job is ready
            if (startupState == STARTUP_COMPLETE && Options::preemptiveShutdown() && nextObservationTime > (Options::preemptiveShutdownTime()*3600))
            {
                appendLogText(i18n("%1 observation job is scheduled for execution at %2. Observatory is scheduled for shutdown until next job is ready.", nextObservationJob->getName(), nextObservationJob->getStartupTime().toString()));
                preemptiveShutdown = true;
                weatherCheck->setEnabled(false);
                weatherLabel->hide();
                checkShutdownState();

                // Wake up 5 minutes before next job is ready
                sleepTimer.setInterval((nextObservationTime*1000 - (1000*Options::leadTime()*60)));
                connect(&sleepTimer, SIGNAL(timeout()), this, SLOT(wakeUpScheduler()));
                sleepTimer.start();
            }
            // Otherise, check if the next observation time exceeds the job lead time (default 5 minutes)
            else if (nextObservationTime > (Options::leadTime()*60))
            {
                // If start up procedure is already complete, and we didn't issue any parking commands before and parking is checked and enabled
                // Then we park the mount until next job is ready. But only if the job uses TRACK as its first step, otherwise we cannot get into position again.
                if (startupState == STARTUP_COMPLETE && parkWaitState == PARKWAIT_IDLE && (nextObservationJob->getStepPipeline() & SchedulerJob::USE_TRACK) && parkMountCheck->isEnabled() && parkMountCheck->isChecked())
                {
                        appendLogText(i18n("%1 observation job is scheduled for execution at %2. Parking the mount until the job is ready.", nextObservationJob->getName(), nextObservationJob->getStartupTime().toString()));
                        parkWaitState = PARKWAIT_PARK;
                }
                // If mount was pre-emptivally parked OR if parking is not supported or if start up procedure is IDLE then go into sleep mode until next job is ready
                else if (parkWaitState == PARKWAIT_PARKED || parkMountCheck->isEnabled() == false || parkMountCheck->isChecked() == false || startupState == STARTUP_IDLE)
                {
                    appendLogText(i18n("Scheduler is going into sleep mode..."));
                    schedulerTimer.stop();

                    sleepLabel->setToolTip(i18n("Scheduler is in sleep mode"));
                    sleepLabel->show();

                    //weatherCheck->setEnabled(false);
                    //weatherLabel->hide();

                    // Wake up 5 minutes (default) before next job is ready
                    sleepTimer.setInterval((nextObservationTime*1000 - (1000*Options::leadTime()*60)));
                    connect(&sleepTimer, SIGNAL(timeout()), this, SLOT(wakeUpScheduler()));
                    sleepTimer.start();
                }
            }
        }
    }
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

double Scheduler::findAltitude(const SkyPoint & target, const QDateTime when)
{
    // Make a copy
    SkyPoint p = target;
    QDateTime lt( when.date(), QTime() );
    KStarsDateTime ut = KStarsData::Instance()->geo()->LTtoUT( lt );

    KStarsDateTime myUT = ut.addSecs(when.time().msecsSinceStartOfDay()/1000);

    CachingDms LST = KStarsData::Instance()->geo()->GSTtoLST( myUT.gst() );
    p.EquatorialToHorizontal( &LST, KStarsData::Instance()->geo()->lat() );

    return p.alt().Degrees();
}

bool Scheduler::calculateAltitudeTime(SchedulerJob *job, double minAltitude, double minMoonAngle)
{
    // We wouldn't stat observation 30 mins (default) before dawn.
    double earlyDawn = Dawn - Options::preDawnTime()/(60.0 * 24.0);
    double altitude=0;
    QDateTime lt( KStarsData::Instance()->lt().date(), QTime() );
    KStarsDateTime ut = geo->LTtoUT( lt );

    SkyPoint target = job->getTargetCoords();

    QTime now = KStarsData::Instance()->lt().time();
    double fraction = now.hour() + now.minute()/60.0 + now.second()/3600;
    double rawFrac  = 0;

    for (double hour=fraction; hour < (fraction+24); hour+= 1.0/60.0)
    {
        KStarsDateTime myUT = ut.addSecs(hour * 3600.0);

        rawFrac = (hour > 24 ? (hour - 24) : hour) / 24.0;

        if (rawFrac < Dawn || rawFrac > Dusk)
        {
            CachingDms LST = geo->GSTtoLST( myUT.gst() );
            target.EquatorialToHorizontal( &LST, geo->lat() );
            altitude =  target.alt().Degrees();

            if (altitude > minAltitude)
            {
                QDateTime startTime = geo->UTtoLT(myUT);

                if (rawFrac > earlyDawn && rawFrac < Dawn)
                {
                    appendLogText(i18n("%1 reaches an altitude of %2 degrees at %3 but will not be scheduled due to close proximity to astronomical twilight rise.", job->getName(), QString::number(minAltitude,'g', 3), startTime.toString()));
                    return false;
                }

                if (minMoonAngle > 0 && getMoonSeparationScore(job, startTime) < 0)
                    continue;

                job->setStartupTime(startTime);
                job->setStartupCondition(SchedulerJob::START_AT);
                appendLogText(i18n("%1 is scheduled to start at %2 where its altitude is %3 degrees.", job->getName(), startTime.toString(), QString::number(altitude,'g', 3)));
                return true;
            }

        }

    }

    if (minMoonAngle == -1)
        appendLogText(i18n("No night time found for %1 to rise above minimum altitude of %2 degrees.", job->getName(), QString::number(minAltitude,'g', 3)));
    else
        appendLogText(i18n("No night time found for %1 to rise above minimum altitude of %2 degrees with minimum moon separation of %3 degrees.", job->getName(), QString::number(minAltitude,'g', 3), QString::number(minMoonAngle,'g', 3)));
    return false;
}

bool Scheduler::calculateCulmination(SchedulerJob *job)
{
    SkyPoint target = job->getTargetCoords();

    SkyObject o;

    o.setRA0(target.ra0());
    o.setDec0(target.dec0());

    o.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

    QDateTime midnight (KStarsData::Instance()->lt().date(), QTime());
    KStarsDateTime dt = geo->LTtoUT(midnight);

    QTime transitTime = o.transitTime(dt, geo);

    appendLogText(i18n("%1 Transit time is %2", job->getName(), transitTime.toString()));

    int dayOffset =0;
    if (KStarsData::Instance()->lt().time() > transitTime)
        dayOffset=1;

    QDateTime observationDateTime(QDate::currentDate().addDays(dayOffset), transitTime.addSecs(job->getCulminationOffset()* 60));

    appendLogText(i18np("%1 Observation time is %2 adjusted for %3 minute.", "%1 Observation time is %2 adjusted for %3 minutes.",
                        job->getName(), observationDateTime.toString(), job->getCulminationOffset()));

    if (getDarkSkyScore(observationDateTime) < 0)
    {
        appendLogText(i18n("%1 culminates during the day and cannot be scheduled for observation.", job->getName()));
        return false;
    }

    if (observationDateTime < (static_cast<QDateTime> (KStarsData::Instance()->lt())))
    {
        appendLogText(i18n("Observation time for %1 already passed.", job->getName()));
        return false;
    }


    job->setStartupTime(observationDateTime);
    job->setStartupCondition(SchedulerJob::START_AT);
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
        newStatus = (IPState) weatherReply.value();

        switch (newStatus)
        {
            case IPS_OK:
                statusString = i18n("Weather conditions are OK.");
                break;

            case IPS_BUSY:
                statusString = i18n("Warning! Weather conditions are in the WARNING zone.");
                break;

            case IPS_ALERT:
                statusString = i18n("Caution! Weather conditions are in the DANGER zone!");
                break;

            default:
                if (noWeatherCounter++ >= MAX_FAILURE_ATTEMPTS)
                {
                    noWeatherCounter=0;
                    appendLogText(i18n("Warning: Ekos did not receive any weather updates for the last %1 minutes.", weatherTimer.interval()/(60000.0)));
                }
            break;
        }

        if (newStatus != weatherStatus)
        {
            weatherStatus = newStatus;

            if (Options::verboseLogging())
                qDebug() << "Scheduler: " << statusString;

            if (weatherStatus == IPS_OK)
                weatherLabel->setPixmap(QIcon::fromTheme("security-high", QIcon(":/icons/breeze/default/security-high.svg")).pixmap(QSize(32,32)));
            else if (weatherStatus == IPS_BUSY)
            {
                weatherLabel->setPixmap(QIcon::fromTheme("security-medium", QIcon(":/icons/breeze/default/security-medium.svg")).pixmap(QSize(32,32)));
                KNotification::event( QLatin1String( "WeatherWarning" ) , i18n("Weather conditions in warning zone"));
            }
            else if (weatherStatus == IPS_ALERT)
            {
                weatherLabel->setPixmap(QIcon::fromTheme("security-low", QIcon(":/icons/breeze/default/security-low.svg")).pixmap(QSize(32,32)));
                KNotification::event( QLatin1String( "WeatherAlert" ) , i18n("Weather conditions are critical. Observatory shutdown is imminent"));
            }
            else
                weatherLabel->setPixmap(QIcon::fromTheme("chronometer", QIcon(":/icons/breeze/default/chronometer.svg")).pixmap(QSize(32,32)));

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
                stopCurrentJobAction();
                stopGuiding();
                jobTimer.stop();
                currentJob->setState(SchedulerJob::JOB_ABORTED);
                currentJob->setStage(SchedulerJob::STAGE_IDLE);
            }
            checkShutdownState();
            //connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkStatus()), Qt::UniqueConnection);
        }
    }
}

int16_t Scheduler::getWeatherScore()
{
    if (weatherCheck->isEnabled() == false || weatherCheck->isChecked() == false)
        return 0;

      if (weatherStatus == IPS_BUSY)
            return BAD_SCORE/2;
      else if (weatherStatus == IPS_ALERT)
            return BAD_SCORE;

    return 0;
}

int16_t Scheduler::getDarkSkyScore(const QDateTime &observationDateTime)
{
  //  if (job->getStartingCondition() == SchedulerJob::START_CULMINATION)
    //    return -1000;

    int16_t score=0;
    double dayFraction = 0;

    // Anything half an hour before dawn shouldn't be a good candidate
    double earlyDawn = Dawn - Options::preDawnTime()/(60.0 * 24.0);

    dayFraction = observationDateTime.time().msecsSinceStartOfDay() / (24.0 * 60.0 * 60.0 * 1000.0);

    // The farther the target from dawn, the better.
    if (dayFraction > earlyDawn && dayFraction < Dawn)
        score = BAD_SCORE/50;
    else if (dayFraction < Dawn)
        score = (Dawn - dayFraction) * 100;
    else if (dayFraction > Dusk)
    {
      score = (dayFraction - Dusk) * 100;
    }
    else
      score = BAD_SCORE;

    appendLogText(i18n("Dark sky score is %1 for time %2", score, observationDateTime.toString()));

    return score;
}

int16_t Scheduler::calculateJobScore(SchedulerJob *job, QDateTime when)
{
    int16_t total=0;

    if (job->getEnforceTwilight())
        total += getDarkSkyScore(when);
    if (job->getStepPipeline() != SchedulerJob::USE_NONE)
        total += getAltitudeScore(job, when);
    total += getMoonSeparationScore(job, when);

    return total;
}

int16_t Scheduler::getAltitudeScore(SchedulerJob *job, QDateTime when)
{
    int16_t score=0;
    double currentAlt  = findAltitude(job->getTargetCoords(), when);

    if (currentAlt < 0)
        score = BAD_SCORE;
    // If minimum altitude is specified
    else if (job->getMinAltitude() > 0)
    {
        // if current altitude is lower that's not good
        if (currentAlt < job->getMinAltitude())
            score = BAD_SCORE;
        else
        {
            double HA=0;

            if (indiState == INDI_READY)
            {
                QDBusReply<double> haReply = mountInterface->call(QDBus::AutoDetect,"getHourAngle");
                if (haReply.error().type() == QDBusError::NoError)
                    HA = haReply.value();
            }

            // If already passed the merdian and setting we check if it is within setting alttidue cut off value (3 degrees default)
            // If it is within that value then it is useless to start the job which will end very soon so we better look for a better job.
            if (HA > 0 && (currentAlt - SETTING_ALTITUDE_CUTOFF) < job->getMinAltitude())
                score = BAD_SCORE/2.0;
            else
            // Otherwise, adjust score and add current altitude to score weight
            score = (1.5 * pow(1.06, currentAlt) ) - (minAltitude->minimum() / 10.0);
        }
    }
    // If it's below minimum hard altitude (15 degrees now), set score to 10% of altitude value
    else if (currentAlt < minAltitude->minimum())
        score = currentAlt / 10.0;
    // If no minimum altitude, then adjust altitude score to account for current target altitude
    else
        score = (1.5 * pow(1.06, currentAlt) ) - (minAltitude->minimum() / 10.0);

    appendLogText(i18n("%1 altitude at %2 is %3 degrees. %1 altitude score is %4.", job->getName(), when.toString(), QString::number(currentAlt,'g', 3), score));

    return score;
}

double Scheduler::getCurrentMoonSeparation(SchedulerJob *job)
{
    // Get target altitude given the time
    SkyPoint p = job->getTargetCoords();
    QDateTime midnight( KStarsData::Instance()->lt().date(), QTime() );
    KStarsDateTime ut = geo->LTtoUT( midnight );
    KStarsDateTime myUT = ut.addSecs(KStarsData::Instance()->lt().time().msecsSinceStartOfDay()/1000);
    CachingDms LST = geo->GSTtoLST( myUT.gst() );
    p.EquatorialToHorizontal( &LST, geo->lat() );

    // Update moon
    ut = geo->LTtoUT(KStarsData::Instance()->lt());
    KSNumbers ksnum(ut.djd());
    LST = geo->GSTtoLST( ut.gst() );
    moon->updateCoords(&ksnum, true, geo->lat(), &LST, true);

    // Moon/Sky separation p
    return moon->angularDistanceTo(&p).Degrees();
}

int16_t Scheduler::getMoonSeparationScore(SchedulerJob *job, QDateTime when)
{
    int16_t score=0;

    // Get target altitude given the time
    SkyPoint p = job->getTargetCoords();
    QDateTime midnight( when.date(), QTime() );
    KStarsDateTime ut = geo->LTtoUT( midnight );
    KStarsDateTime myUT = ut.addSecs(when.time().msecsSinceStartOfDay()/1000);
    CachingDms LST = geo->GSTtoLST( myUT.gst() );
    p.EquatorialToHorizontal( &LST, geo->lat() );
    double currentAlt = p.alt().Degrees();

    // Update moon
    ut = geo->LTtoUT(when);
    KSNumbers ksnum(ut.djd());
    LST = geo->GSTtoLST( ut.gst() );
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
        score =  100;
    else
    {
        // JM: Some magic voodoo formula I came up with!
        double moonEffect = ( pow(separation, 1.7) * pow(zMoon, 0.5) ) / ( pow(zTarget, 1.1) * pow(illum, 0.5) );

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

    appendLogText(i18n("%1 Moon score %2 (separation %3).", job->getName(), score, separation));

    return score;

}

void Scheduler::calculateDawnDusk()
{
    KSAlmanac ksal;
    Dawn = ksal.getDawnAstronomicalTwilight();
    Dusk = ksal.getDuskAstronomicalTwilight();

    QTime now  = KStarsData::Instance()->lt().time();
    QTime dawn = QTime(0,0,0).addSecs(Dawn*24*3600);
    QTime dusk = QTime(0,0,0).addSecs(Dusk*24*3600);

    duskDateTime.setDate(KStars::Instance()->data()->lt().date());
    duskDateTime.setTime(dusk);

    appendLogText(i18n("Astronomical twilight rise is at %1, set is at %2, and current time is %3", dawn.toString(), dusk.toString(), now.toString()));

}

void Scheduler::executeJob(SchedulerJob *job)
{
    if (job->getCompletionCondition() == SchedulerJob::FINISH_SEQUENCE && Options::rememberJobProgress())
    {
        QString targetName = job->getName().replace(" ", "");
        QList<QVariant> targetArgs;
        targetArgs.append(targetName);
        captureInterface->callWithArgumentList(QDBus::AutoDetect, "setTargetName", targetArgs);

    }

    currentJob = job;

    currentJob->setState(SchedulerJob::JOB_BUSY);

    updatePreDawn();

    // No need to continue evaluating jobs as we already have one.

    schedulerTimer.stop();
    jobTimer.start();
}

bool    Scheduler::checkEkosState()
{
    if (state == SCHEDULER_PAUSED)
        return false;

    switch (ekosState)
    {
        case EKOS_IDLE:
        {
        // Even if state is IDLE, check if Ekos is already started. If not, start it.
        QDBusReply<int> isEkosStarted;
        isEkosStarted = ekosInterface->call(QDBus::AutoDetect,"getEkosStartingStatus");
        if (isEkosStarted.value() == EkosManager::EKOS_STATUS_SUCCESS)
        {
            ekosState = EKOS_READY;
            return true;
        }
        else
        {
            ekosInterface->call(QDBus::AutoDetect,"start");
            ekosState = EKOS_STARTING;

            currentOperationTime.start();

            return false;
        }
        }
        break;


        case EKOS_STARTING:
        {
            QDBusReply<int> isEkosStarted;
            isEkosStarted = ekosInterface->call(QDBus::AutoDetect,"getEkosStartingStatus");
            if(isEkosStarted.value()== EkosManager::EKOS_STATUS_SUCCESS)
            {
                appendLogText(i18n("Ekos started."));
                ekosState = EKOS_READY;
                return true;
            }
            else if(isEkosStarted.value()== EkosManager::EKOS_STATUS_ERROR)
            {
                appendLogText(i18n("Ekos failed to start."));
                stop();
                return false;
            }
            // If a minute passed, give up
            else if (currentOperationTime.elapsed() > (60*1000))
            {
                appendLogText(i18n("Ekos timed out."));
                stop();
                return false;
            }
        }
        break;

        case EKOS_READY:
            return true;
        break;
    }

    return false;

}

bool    Scheduler::checkINDIState()
{
    if (state == SCHEDULER_PAUSED)
        return false;

    if (Options::verboseLogging())
        qDebug() << "Scheduler: Checking INDI State...";

    switch (indiState)
    {
    case INDI_IDLE:
    {
        // Even in idle state, we make sure that INDI is not already connected.
        QDBusReply<int> isINDIConnected = ekosInterface->call(QDBus::AutoDetect,"getINDIConnectionStatus");
        if (isINDIConnected.value()== EkosManager::EKOS_STATUS_SUCCESS)
        {
            indiState = INDI_PROPERTY_CHECK;

            if (Options::verboseLogging())
                qDebug() << "Scheduler: Checking INDI Properties...";

            return false;
        }
        else
        {
            ekosInterface->call(QDBus::AutoDetect,"connectDevices");
            indiState = INDI_CONNECTING;

            currentOperationTime.start();

            if (Options::verboseLogging())
                qDebug() << "Scheduler: Connecting INDI Devices";

            return false;
        }
    }
        break;

    case INDI_CONNECTING:
    {
        QDBusReply<int> isINDIConnected = ekosInterface->call(QDBus::AutoDetect,"getINDIConnectionStatus");
        if(isINDIConnected.value()== EkosManager::EKOS_STATUS_SUCCESS)
        {
            appendLogText(i18n("INDI devices connected."));
            indiState = INDI_PROPERTY_CHECK;
            return false;
        }
        else if(isINDIConnected.value()== EkosManager::EKOS_STATUS_ERROR)
        {
            if (indiConnectFailureCount++ < MAX_FAILURE_ATTEMPTS)
            {
                appendLogText(i18n("One or more INDI devices failed to connect. Retrying..."));
                ekosInterface->call(QDBus::AutoDetect,"connectDevices");
                return false;
            }

            appendLogText(i18n("INDI devices failed to connect. Check INDI control panel for details."));
            stop();
            return false;
        }
        // If 30 seconds passed, we retry
        else if (currentOperationTime.elapsed() > (30*1000))
        {
            if (indiConnectFailureCount++ < MAX_FAILURE_ATTEMPTS)
            {
                appendLogText(i18n("One or more INDI devices failed to connect. Retrying..."));
                ekosInterface->call(QDBus::AutoDetect,"connectDevices");
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

    case INDI_PROPERTY_CHECK:
    {
        // Check if mount and dome support parking or not.
        QDBusReply<bool> boolReply = mountInterface->call(QDBus::AutoDetect,"canPark");
        unparkMountCheck->setEnabled(boolReply.value());
        parkMountCheck->setEnabled(boolReply.value());

        //qDebug() << "Mount can park " << boolReply.value();

        boolReply = domeInterface->call(QDBus::AutoDetect,"canPark");
        unparkDomeCheck->setEnabled(boolReply.value());
        parkDomeCheck->setEnabled(boolReply.value());

        boolReply = captureInterface->call(QDBus::AutoDetect,"hasCoolerControl");
        warmCCDCheck->setEnabled(boolReply.value());


        QDBusReply<int> updateReply = weatherInterface->call(QDBus::AutoDetect, "getUpdatePeriod");
        if (updateReply.error().type() == QDBusError::NoError)
        {
            weatherCheck->setEnabled(true);
            if (updateReply.value() > 0)
            {
                weatherTimer.setInterval(updateReply.value() * 1000);
                connect(&weatherTimer, SIGNAL(timeout()), this, SLOT(checkWeather()));
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

    if (Options::verboseLogging())
        qDebug() << "Scheduler: Checking Startup State...";

    switch (startupState)
    {
    case STARTUP_IDLE:
    {
        KNotification::event( QLatin1String( "ObservatoryStartup" ) , i18n("Observatory is in the startup process"));

        if (Options::verboseLogging())
            qDebug() << "Scheduler: Startup Idle. Starting startup process...";

        // If Ekos is already started, we skip the script and move on to dome unpark step
        // unless we do not have light frames, then we skip all
        QDBusReply<int> isEkosStarted;
        isEkosStarted = ekosInterface->call(QDBus::AutoDetect,"getEkosStartingStatus");
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

        if (profileCombo->currentText() != i18n("Default"))
        {
            QList<QVariant> profile;
            profile.append(profileCombo->currentText());
            ekosInterface->callWithArgumentList(QDBus::AutoDetect,"setProfile", profile);
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
        if (currentJob->getLightFramesRequired())
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

    if (Options::verboseLogging())
        qDebug() << "Scheduler: Checking shutown state...";

    switch (shutdownState)
    {
    case SHUTDOWN_IDLE:
        KNotification::event( QLatin1String( "ObservatoryShutdown" ) , i18n("Observatory is in the shutdown process"));

        if (Options::verboseLogging())
            qDebug() << "Scheduler: Starting shutdown process...";

        weatherTimer.stop();
        weatherTimer.disconnect();
        weatherLabel->hide();

        jobTimer.stop();

        currentJob = NULL;

        if(state == SCHEDULER_RUNNIG)
            schedulerTimer.start();

        if (preemptiveShutdown == false)
        {
            sleepTimer.stop();
            sleepTimer.disconnect();
        }

        if (warmCCDCheck->isEnabled() && warmCCDCheck->isChecked())
        {
            appendLogText(i18n("Warming up CCD..."));

            // Turn it off
            QVariant arg(false);
            captureInterface->call(QDBus::AutoDetect, "setCoolerControl", arg);
        }

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
        if (shutdownScriptURL.isEmpty() == false)
        {
            shutdownState = SHUTDOWN_SCRIPT;
            return false;
        }

        shutdownState = SHUTDOWN_COMPLETE;
        return true;
        break;

    case SHUTDOWN_PARK_CAP:
        if (capCheck->isEnabled() && capCheck->isChecked())
            parkCap();
        else
            shutdownState = SHUTDOWN_PARK_MOUNT;
        break;

    case SHUTDOWN_PARKING_CAP:
        checkCapParkingStatus();
        break;

    case SHUTDOWN_PARK_MOUNT:
        if (parkMountCheck->isEnabled() && parkMountCheck->isChecked())
            parkMount();
        else
            shutdownState = SHUTDOWN_PARK_DOME;
        break;

    case SHUTDOWN_PARKING_MOUNT:
        checkMountParkingStatus();
        break;

    case SHUTDOWN_PARK_DOME:
        if (parkDomeCheck->isEnabled() && parkDomeCheck->isChecked())
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
            // Disconnect devices
            stopEkos();

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

    if (Options::verboseLogging())
        qDebug() << "Scheduler: Checking Park Wait State...";

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
    appendLogText(i18n("Executing script %1 ...", filename));

    connect(&scriptProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(readProcessOutput()));

    connect(&scriptProcess, SIGNAL(finished(int)), this, SLOT(checkProcessExit(int)));

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

void Scheduler::checkStatus()
{
    if (state == SCHEDULER_PAUSED)
        return;

    // #1 If no current job selected, let's check if we need to shutdown or evaluate jobs
    if (currentJob == NULL)
    {
        // #2.1 If shutdown is already complete or in error, we need to stop
        if (shutdownState == SHUTDOWN_COMPLETE || shutdownState == SHUTDOWN_ERROR)
        {
            if (shutdownState == SHUTDOWN_COMPLETE)
                appendLogText(i18n("Shutdown complete."));
            else
                appendLogText(i18n("Shutdown procedure failed, aborting..."));

            // Stop Ekos if there is no shutdown script since stopEkos is called right before executing the shutdown script
            if (shutdownScriptURL.isEmpty())
                stopEkos();

            // Stop Scheduler
            stop();

            return;
        }

        // #2.2  Check if shutdown is in progress
        if (shutdownState > SHUTDOWN_IDLE)
        {
            checkShutdownState();
            return;
        }

        // #2.3 Check if park wait procedure is in progress
        if (checkParkWaitState() == false)
            return;

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
            return;
        }

        // #4 Check if startup procedure Phase #1 is complete (Startup script)
        if ( (startupState == STARTUP_IDLE && checkStartupState() == false) || startupState == STARTUP_SCRIPT)
            return;

        // #5 Check if Ekos is started
        if (checkEkosState() == false)
            return;

        // #6 Check if INDI devices are connected.
        if (checkINDIState() == false)
            return;

        // #7 Check if startup procedure Phase #2 is complete (Unparking phase)
        if (startupState > STARTUP_SCRIPT && startupState < STARTUP_ERROR && checkStartupState() == false)
            return;

        // #8 Execute the job
        executeJob(currentJob);
    }
}

void Scheduler::checkJobStage()
{
    if (state == SCHEDULER_PAUSED)
        return;

    Q_ASSERT(currentJob != NULL);

    // #1 Check if we need to stop at some point
    if (currentJob->getCompletionCondition() == SchedulerJob::FINISH_AT && currentJob->getState() == SchedulerJob::JOB_BUSY)
    {
        // If the job reached it COMPLETION time, we stop it.
        if (KStarsData::Instance()->lt().secsTo(currentJob->getCompletionTime()) <= 0)
        {
            findNextJob();
            return;
        }
    }

    // #2 Check if altitude restriction still holds true
    if (currentJob->getMinAltitude() > 0)
    {
        SkyPoint p = currentJob->getTargetCoords();

        p.EquatorialToHorizontal(KStarsData::Instance()->lst(), geo->lat());

        if (p.alt().Degrees() < currentJob->getMinAltitude())
        {
            // Only terminate job due to altitude limitation if mount is NOT parked.
            if (isMountParked() == false)
            {
                appendLogText(i18n("%1 current altitude (%2 degrees) crossed minimum constraint altitude (%3 degrees), aborting job...", currentJob->getName(),
                                   p.alt().Degrees(), currentJob->getMinAltitude()));

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
                appendLogText(i18n("Current moon separation (%1 degrees) is lower than %2 minimum constraint (%3 degrees), aborting job...", moonSeparation, currentJob->getName(),
                                   currentJob->getMinMoonSeparation()));

                currentJob->setState(SchedulerJob::JOB_ABORTED);
                stopCurrentJobAction();
                stopGuiding();
                findNextJob();
                return;
            }
        }
    }

    // #4 Check if we're not at dawn
    if (currentJob->getEnforceTwilight() && KStarsData::Instance()->lt() > preDawnDateTime)
    {
        // If either mount or dome are not parked, we shutdown if we approach dawn
        if (isMountParked() == false || (parkDomeCheck->isEnabled() && isDomeParked() == false))
        {
            // Minute is a DOUBLE value, do not use i18np
            appendLogText(i18n("Approaching astronomical twilight rise limit at %1 (%2 minutes safety margin), aborting all jobs...", preDawnDateTime.toString(), Options::preDawnTime()));

            currentJob->setState(SchedulerJob::JOB_ABORTED);
            stopCurrentJobAction();
            stopGuiding();
            checkShutdownState();

            //disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkJobStage()), Qt::UniqueConnection);
            //connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkStatus()), Qt::UniqueConnection);
            return;
        }
    }

    switch(currentJob->getStage())
    {
    case SchedulerJob::STAGE_IDLE:
        getNextAction();
        break;

    case SchedulerJob::STAGE_SLEWING:
    {
        QDBusReply<int> slewStatus = mountInterface->call(QDBus::AutoDetect,"getSlewStatus");
        bool isDomeMoving=false;

        if (parkDomeCheck->isEnabled())
        {
            QDBusReply<bool> domeReply = domeInterface->call(QDBus::AutoDetect, "isMoving");
            if (domeReply.error().type() == QDBusError::NoError && domeReply.value() == true)
                isDomeMoving=true;
        }

        if (slewStatus.error().type() == QDBusError::UnknownObject)
        {
            appendLogText(i18n("Connection to INDI is lost. Aborting..."));
            currentJob->setState(SchedulerJob::JOB_ABORTED);
            checkShutdownState();
            return;
        }

        if (Options::verboseLogging())
            qDebug() << "Scheduler: Slewing Stage... Slew Status is " << pstateStr(static_cast<IPState>(slewStatus.value()));

        if(slewStatus.value() == IPS_OK && isDomeMoving == false)
        {
            appendLogText(i18n("%1 slew is complete.", currentJob->getName()));
            currentJob->setStage(SchedulerJob::STAGE_SLEW_COMPLETE);
            getNextAction();
            return;
        }
        else if(slewStatus.value() == IPS_ALERT)
        {
            appendLogText(i18n("%1 slew failed!", currentJob->getName()));
            currentJob->setState(SchedulerJob::JOB_ERROR);

            findNextJob();
            return;
        }
    }
        break;

    case SchedulerJob::STAGE_FOCUSING:
    case SchedulerJob::STAGE_POSTALIGN_FOCUSING:
    {
        QDBusReply<int> focusReply = focusInterface->call(QDBus::AutoDetect,"getStatus");

        if (focusReply.error().type() == QDBusError::UnknownObject)
        {
            appendLogText(i18n("Connection to INDI is lost. Aborting..."));
            currentJob->setState(SchedulerJob::JOB_ABORTED);
            checkShutdownState();
            return;
        }

        if (Options::verboseLogging())
            qDebug() << "Scheduler: Focus stage...";

        Ekos::FocusState focusStatus = static_cast<Ekos::FocusState>(focusReply.value());

        // Is focus complete?
        if(focusStatus == Ekos::FOCUS_COMPLETE)
        {
                appendLogText(i18n("%1 focusing is complete.", currentJob->getName()));

                autofocusCompleted = true;

                if (currentJob->getStage() == SchedulerJob::STAGE_FOCUSING)
                {
                    // Reset frame to original size.
                    //focusInterface->call(QDBus::AutoDetect,"resetFrame");
                    currentJob->setStage(SchedulerJob::STAGE_FOCUS_COMPLETE);
                }
                else
                    currentJob->setStage(SchedulerJob::STAGE_POSTALIGN_FOCUSING_COMPLETE);

                getNextAction();
                return;
         }
            else if (focusStatus == Ekos::FOCUS_FAILED || focusStatus == Ekos::FOCUS_ABORTED)
            {
                appendLogText(i18n("%1 focusing failed!", currentJob->getName()));

                if (focusFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Restarting %1 focusing procedure...", currentJob->getName()));
                    // Reset frame to original size.
                    focusInterface->call(QDBus::AutoDetect,"resetFrame");
                    // Restart focusing
                    startFocusing();
                    return;
                }

                currentJob->setState(SchedulerJob::JOB_ERROR);

                findNextJob();
                return;
            }
        }

        // Is focus complete?
        #if 0
        if(focusReply.value())
        {
            focusReply = focusInterface->call(QDBus::AutoDetect,"isAutoFocusSuccessful");
            // Is focus successful ?
            if(focusReply.value())
            {
                appendLogText(i18n("%1 focusing is complete.", currentJob->getName()));

                autofocusCompleted = true;

                if (currentJob->getStage() == SchedulerJob::STAGE_FOCUSING)
                {
                    // Reset frame to original size.
                    //focusInterface->call(QDBus::AutoDetect,"resetFrame");
                    currentJob->setStage(SchedulerJob::STAGE_FOCUS_COMPLETE);
                }
                else
                    currentJob->setStage(SchedulerJob::STAGE_POSTALIGN_FOCUSING_COMPLETE);

                getNextAction();
                return;
            }
            else
            {
                appendLogText(i18n("%1 focusing failed!", currentJob->getName()));

                if (focusFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Restarting %1 focusing procedure...", currentJob->getName()));
                    // Reset frame to original size.
                    focusInterface->call(QDBus::AutoDetect,"resetFrame");
                    // Restart focusing
                    startFocusing();
                    return;
                }

                currentJob->setState(SchedulerJob::JOB_ERROR);

                findNextJob();
                return;
            }
        }
    #endif
        break;

    case SchedulerJob::STAGE_ALIGNING:
    {
       QDBusReply<int> alignReply;

       if (Options::verboseLogging())
           qDebug() << "Scheduler: Alignment stage...";

    #if 0
       if (currentJob->getFITSFile().isEmpty() == false && loadAndSlewProgress)
       {
           QDBusReply<int> loadSlewReply = alignInterface->call(QDBus::AutoDetect,"getLoadAndSlewStatus");

           if (Options::verboseLogging())
               qDebug() << "Scheduler: Checking Load And Slew Status";

           if (loadSlewReply.value() == IPS_OK)
           {
               appendLogText(i18n("%1 is solved and aligned successfully.", currentJob->getFITSFile().toString()));
               loadAndSlewProgress = false;
               currentJob->setStage(SchedulerJob::STAGE_ALIGN_COMPLETE);
               getNextAction();

           }
           else if (loadSlewReply.value() == IPS_ALERT)
           {
               appendLogText(i18n("%1 Load And Slew failed!", currentJob->getName()));

              if (alignFailureCount++ < MAX_FAILURE_ATTEMPTS)
              {
                  appendLogText(i18n("Restarting %1 alignment procedure...", currentJob->getName()));
                  startAstrometry();
                  return;
              }

              currentJob->setState(SchedulerJob::JOB_ERROR);
              findNextJob();
           }

           return;
       }
       else
       #endif

       alignReply = alignInterface->call(QDBus::AutoDetect,"getStatus");

       if (alignReply.error().type() == QDBusError::UnknownObject)
       {
           appendLogText(i18n("Connection to INDI is lost. Aborting..."));
           currentJob->setState(SchedulerJob::JOB_ABORTED);
           checkShutdownState();
           return;
       }

       Ekos::AlignState alignStatus = static_cast<Ekos::AlignState>(alignReply.value());

       // Is solver complete?
        if(alignStatus == Ekos::ALIGN_COMPLETE)
        {

                appendLogText(i18n("%1 alignment is complete.", currentJob->getName()));

                currentJob->setStage(SchedulerJob::STAGE_ALIGN_COMPLETE);
                getNextAction();
                return;
            }
            else if (alignStatus == Ekos::ALIGN_FAILED || alignStatus == Ekos::ALIGN_ABORTED)
            {
                appendLogText(i18n("%1 alignment failed!", currentJob->getName()));

                if (alignFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Restarting %1 alignment procedure...", currentJob->getName()));
                    startAstrometry();
                    return;
                }

                currentJob->setState(SchedulerJob::JOB_ERROR);

                findNextJob();
            }
    }
    break;


    case SchedulerJob::STAGE_RESLEWING:
    {
        QDBusReply<int> slewStatus = mountInterface->call(QDBus::AutoDetect,"getSlewStatus");
        bool isDomeMoving=false;

        if (Options::verboseLogging())
            qDebug() << "Scheduler: Re-slewing stage...";

        if (parkDomeCheck->isEnabled())
        {
            QDBusReply<bool> domeReply = domeInterface->call(QDBus::AutoDetect, "isMoving");
            if (domeReply.error().type() == QDBusError::NoError && domeReply.value() == true)
                isDomeMoving=true;
        }

        if (slewStatus.error().type() == QDBusError::UnknownObject)
        {
            appendLogText(i18n("Connection to INDI is lost. Aborting..."));
            currentJob->setState(SchedulerJob::JOB_ABORTED);
            checkShutdownState();
            return;
        }

        if(slewStatus.value() == IPS_OK && isDomeMoving == false)
        {
            appendLogText(i18n("%1 repositioning is complete.", currentJob->getName()));
            currentJob->setStage(SchedulerJob::STAGE_RESLEWING_COMPLETE);
            getNextAction();
            return;
        }
        else if(slewStatus.value() == IPS_ALERT)
        {
            appendLogText(i18n("%1 slew failed!", currentJob->getName()));
            currentJob->setState(SchedulerJob::JOB_ERROR);

            findNextJob();
            return;
        }
    }
        break;

    case SchedulerJob::STAGE_GUIDING:
    {
        QDBusReply<int> guideReply = guideInterface->call(QDBus::AutoDetect,"getStatus");

        if (Options::verboseLogging())
            qDebug() << "Scheduler: Calibration & Guide stage...";

        if (guideReply.error().type() == QDBusError::UnknownObject)
        {
            appendLogText(i18n("Connection to INDI is lost. Aborting..."));
            currentJob->setState(SchedulerJob::JOB_ABORTED);
            checkShutdownState();
            return;
        }

        Ekos::GuideState guideStatus = static_cast<Ekos::GuideState>(guideReply.value());

        // If calibration stage complete?
        if(guideStatus == Ekos::GUIDE_GUIDING)
        {
            appendLogText(i18n("%1 guiding is in progress...", currentJob->getName()));

            currentJob->setStage(SchedulerJob::STAGE_GUIDING_COMPLETE);
            getNextAction();
            return;
        }
        else if (guideStatus == Ekos::GUIDE_CALIBRATION_ERROR || guideStatus == Ekos::GUIDE_ABORTED)
        {
            if (guideStatus == Ekos::GUIDE_ABORTED || guideStatus == Ekos::GUIDE_CALIBRATION_ERROR)
                appendLogText(i18n("%1 guiding failed!", currentJob->getName()));
            else
                appendLogText(i18n("%1 calibration failed!", currentJob->getName()));

            if (guideFailureCount++ < MAX_FAILURE_ATTEMPTS)
            {
                appendLogText(i18n("Restarting %1 guiding procedure...", currentJob->getName()));
                startGuiding();
                return;
            }

            currentJob->setState(SchedulerJob::JOB_ERROR);

            findNextJob();
            return;
        }
    }
    break;

    #if 0
    case SchedulerJob::STAGE_CALIBRATING:
    {
        QDBusReply<bool> guideReply = guideInterface->call(QDBus::AutoDetect,"isCalibrationComplete");

        if (Options::verboseLogging())
            qDebug() << "Scheduler: Calibration & Guide stage...";

        if (guideReply.error().type() == QDBusError::UnknownObject)
        {
            appendLogText(i18n("Connection to INDI is lost. Aborting..."));
            currentJob->setState(SchedulerJob::JOB_ABORTED);
            checkShutdownState();
            return;
        }

        // If calibration stage complete?
        if(guideReply.value())
        {
            guideReply = guideInterface->call(QDBus::AutoDetect,"isCalibrationSuccessful");
            // If calibration successful?
            if(guideReply.value())
            {
                appendLogText(i18n("%1 calibration is complete.", currentJob->getName()));

                guideReply = guideInterface->call(QDBus::AutoDetect,"guide");
                if(guideReply.value() == false)
                {
                    appendLogText(i18n("%1 guiding failed!", currentJob->getName()));

                    currentJob->setState(SchedulerJob::JOB_ERROR);

                    findNextJob();
                    return;
                }

                appendLogText(i18n("%1 guiding is in progress...", currentJob->getName()));

                currentJob->setStage(SchedulerJob::STAGE_GUIDING);
                getNextAction();
                return;
            }
            else
            {
                appendLogText(i18n("%1 calibration failed!", currentJob->getName()));

                if (guideFailureCount++ < MAX_FAILURE_ATTEMPTS)
                {
                    appendLogText(i18n("Restarting %1 calibration procedure...", currentJob->getName()));
                    startCalibrating();
                    return;
                }

                currentJob->setState(SchedulerJob::JOB_ERROR);

                findNextJob();
                return;
            }
        }
    }
    break;
    #endif

    case SchedulerJob::STAGE_CAPTURING:
    {
         QDBusReply<QString> captureReply = captureInterface->call(QDBus::AutoDetect,"getSequenceQueueStatus");

         if (captureReply.error().type() == QDBusError::UnknownObject)
         {
             appendLogText(i18n("Connection to INDI is lost. Aborting..."));
             currentJob->setState(SchedulerJob::JOB_ABORTED);
             checkShutdownState();
             return;
         }

         if(captureReply.value().toStdString()=="Aborted" || captureReply.value().toStdString()=="Error")
         {
             appendLogText(i18n("%1 capture failed!", currentJob->getName()));

             // If capture failed due to guiding error, let's try to restart that
             if ( (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE) && captureFailureCount++ < MAX_FAILURE_ATTEMPTS)
             {
                 // Check if it is guiding related.
                 QDBusReply<int> guideReply = guideInterface->call(QDBus::AutoDetect,"getStatus");
                 if (guideReply.value() == Ekos::GUIDE_ABORTED || guideReply.value() == Ekos::GUIDE_CALIBRATION_ERROR || guideReply.value() == GUIDE_DITHERING_ERROR)
                 // If guiding failed, let's restart it
                 //if(guideReply.value() == false)
                 {
                    appendLogText(i18n("Restarting %1 guiding procedure...", currentJob->getName()));
                    //currentJob->setStage(SchedulerJob::STAGE_GUIDING);
                    startGuiding();
                    return;
                 }
             }

             currentJob->setState(SchedulerJob::JOB_ERROR);

             findNextJob();
             return;
         }

         if(captureReply.value().toStdString()=="Complete")
         {
             currentJob->setState(SchedulerJob::JOB_COMPLETE);
             //currentJob->setStage(SchedulerJob::STAGE_COMPLETE);
             captureInterface->call(QDBus::AutoDetect,"clearSequenceQueue");

             findNextJob();
             return;
         }
    }
        break;

    default:
        break;
    }
}

void Scheduler::getNextAction()
{
    if (Options::verboseLogging())
        qDebug() << "Scheduler: Get next action...";

    switch(currentJob->getStage())
    {

    case SchedulerJob::STAGE_IDLE:
        if (currentJob->getLightFramesRequired())
        {
            if  (currentJob->getStepPipeline() & SchedulerJob::USE_TRACK)
                startSlew();
            else if  (currentJob->getStepPipeline() & SchedulerJob::USE_FOCUS && autofocusCompleted == false)
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
                appendLogText(i18n("Proceeding directly to capture stage because only calibration frames are pending."));
            startCapture();
        }

        break;

    case SchedulerJob::STAGE_SLEW_COMPLETE:
        if  (currentJob->getStepPipeline() & SchedulerJob::USE_FOCUS && autofocusCompleted == false)
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
        if  ( (currentJob->getStepPipeline() & SchedulerJob::USE_FOCUS) && currentJob->getInSequenceFocus())
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
    if (Options::verboseLogging())
        qDebug() << "Scheduler: Stop current action...";

    switch(currentJob->getStage())
    {
    case SchedulerJob::STAGE_IDLE:
        break;

    case SchedulerJob::STAGE_SLEWING:
        mountInterface->call(QDBus::AutoDetect,"abort");
        break;

    case SchedulerJob::STAGE_FOCUSING:
        focusInterface->call(QDBus::AutoDetect,"abort");
        break;

    case SchedulerJob::STAGE_ALIGNING:
       alignInterface->call(QDBus::AutoDetect,"abort");
       break;

    //case SchedulerJob::STAGE_CALIBRATING:
//        guideInterface->call(QDBus::AutoDetect,"stopCalibration");
//    break;

    case SchedulerJob::STAGE_GUIDING:
        stopGuiding();
    break;

    case SchedulerJob::STAGE_CAPTURING:
        captureInterface->call(QDBus::AutoDetect,"abort");
        //stopGuiding();
        break;

    default:
        break;
    }
}

void Scheduler::load()
{
    QUrl fileURL = QFileDialog::getOpenFileUrl(this, i18n("Open Ekos Scheduler List"), dirPath, "Ekos Scheduler List (*.esl)");
    if (fileURL.isEmpty())
        return;

    if (fileURL.isValid() == false)
    {
       QString message = i18n( "Invalid URL: %1", fileURL.toLocalFile() );
       KMessageBox::sorry( 0, message, i18n( "Invalid URL" ) );
       return;
    }

    dirPath = QUrl(fileURL.url(QUrl::RemoveFilename));

    loadScheduler(fileURL.toLocalFile());
}

bool Scheduler::loadScheduler(const QString &fileURL)
{
    QFile sFile;
    sFile.setFileName(fileURL);

    if ( !sFile.open( QIODevice::ReadOnly))
    {
        QString message = i18n( "Unable to open file %1",  fileURL);
        KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
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
    XMLEle *root = NULL;
    XMLEle *ep;
    char c;

    while ( sFile.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
        {
             for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
             {
                 const char *tag = tagXMLEle(ep);
                 if (!strcmp(tag, "Job"))
                    processJobInfo(ep);
                 else if (!strcmp(tag, "StartupProcedure"))
                 {
                     XMLEle *procedure;
                     startupScript->clear();
                     unparkDomeCheck->setChecked(false);
                     unparkMountCheck->setChecked(false);
                     uncapCheck->setChecked(false);

                     for (procedure = nextXMLEle(ep, 1) ; procedure != NULL ; procedure = nextXMLEle(ep, 0))
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

                     for (procedure = nextXMLEle(ep, 1) ; procedure != NULL ; procedure = nextXMLEle(ep, 0))
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

    for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep), "Name"))
            nameEdit->setText(pcdataXMLEle(ep));
        else if (!strcmp(tagXMLEle(ep), "Priority"))
            prioritySpin->setValue(atoi(pcdataXMLEle(ep)));
        else if (!strcmp(tagXMLEle(ep), "Coordinates"))
        {
            subEP = findXMLEle(ep, "J2000RA");
            if (subEP)
                raBox->setDMS(pcdataXMLEle(subEP));
            subEP = findXMLEle(ep, "J2000DE");
            if (subEP)
                decBox->setDMS(pcdataXMLEle(subEP));
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
            for (subEP = nextXMLEle(ep, 1) ; subEP != NULL ; subEP = nextXMLEle(ep, 0))
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
            for (subEP = nextXMLEle(ep, 1) ; subEP != NULL ; subEP = nextXMLEle(ep, 0))
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
            for (subEP = nextXMLEle(ep, 1) ; subEP != NULL ; subEP = nextXMLEle(ep, 0))
            {
                if (!strcmp("Sequence", pcdataXMLEle(subEP)))
                    sequenceCompletionR->setChecked(true);
                else if (!strcmp("Loop", pcdataXMLEle(subEP)))
                    loopCompletionR->setChecked(true);
                else if (!strcmp("At", pcdataXMLEle(subEP)))
                {
                    timeCompletionR->setChecked(true);
                    completionTimeEdit->setDateTime(QDateTime::fromString(findXMLAttValu(subEP, "value"), Qt::ISODate));
                }
            }
        }
        else if (!strcmp(tagXMLEle(ep), "Profile"))
        {
            profileCombo->setCurrentText(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "Steps"))
        {
            XMLEle *module;
            trackStepCheck->setChecked(false);
            focusStepCheck->setChecked(false);
            alignStepCheck->setChecked(false);
            guideStepCheck->setChecked(false);

            for (module = nextXMLEle(ep, 1) ; module != NULL ; module = nextXMLEle(ep, 0))
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

    if (schedulerURL.toLocalFile().startsWith("/tmp/") || schedulerURL.toLocalFile().contains("/Temp"))
        schedulerURL.clear();

    // If no changes made, return.
    if( mDirty == false && !schedulerURL.isEmpty())
        return;

    if (schedulerURL.isEmpty())
    {
        schedulerURL = QFileDialog::getSaveFileUrl(this, i18n("Save Ekos Scheduler List"), dirPath, "Ekos Scheduler List (*.esl)");
        // if user presses cancel
        if (schedulerURL.isEmpty())
        {
            schedulerURL = backupCurrent;
            return;
        }

        dirPath = QUrl(schedulerURL.url(QUrl::RemoveFilename));

        if (schedulerURL.toLocalFile().contains('.') == 0)
            schedulerURL.setPath(schedulerURL.toLocalFile() + ".esl");

        if (QFile::exists(schedulerURL.toLocalFile()))
        {
            int r = KMessageBox::warningContinueCancel(0,
                        i18n( "A file named \"%1\" already exists. "
                              "Overwrite it?", schedulerURL.fileName() ),
                        i18n( "Overwrite File?" ),
                        KGuiItem(i18n( "&Overwrite" )) );
            if(r==KMessageBox::Cancel) return;
        }
    }

    if ( schedulerURL.isValid() )
    {
        if ( (saveScheduler(schedulerURL)) == false)
        {
            KMessageBox::error(KStars::Instance(), i18n("Failed to save scheduler list"), i18n("Save"));
            return;
        }

        mDirty = false;

    } else
    {
        QString message = i18n( "Invalid URL: %1", schedulerURL.url() );
        KMessageBox::sorry(KStars::Instance(), message, i18n( "Invalid URL" ) );
    }
}

bool Scheduler::saveScheduler(const QUrl &fileURL)
{
    QFile file;
    file.setFileName(fileURL.toLocalFile());

    if ( !file.open( QIODevice::WriteOnly))
    {
        QString message = i18n( "Unable to write to file %1",  fileURL.toLocalFile());
        KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
        return false;
    }

    QTextStream outstream(&file);

    outstream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
    outstream << "<SchedulerList version='1.2'>" << endl;

    foreach(SchedulerJob *job, jobs)
    {
         outstream << "<Job>" << endl;

         outstream << "<Name>" << job->getName() << "</Name>" << endl;
         outstream << "<Priority>" << job->getPriority() << "</Priority>" << endl;
         outstream << "<Coordinates>" << endl;
            outstream << "<J2000RA>"<< job->getTargetCoords().ra0().Hours() << "</J2000RA>" << endl;
            outstream << "<J2000DE>"<< job->getTargetCoords().dec0().Degrees() << "</J2000DE>" << endl;
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
            outstream << "<Condition value='" << job->getStartupTime().toString(Qt::ISODate) << "'>At</Condition>" << endl;
        outstream << "</StartupCondition>" << endl;

        outstream << "<Constraints>" << endl;
        if (job->getMinAltitude() > 0)
            outstream << "<Constraint value='" << job->getMinAltitude() << "'>MinimumAltitude</Constraint>" << endl;
        if (job->getMinMoonSeparation() > 0)
            outstream << "<Constraint value='" << job->getMinMoonSeparation() << "'>MoonSeparation</Constraint>" << endl;
        if (job->getEnforceWeather())
            outstream << "<Constraint>EnforceWeather</Constraint>" << endl;
        if (job->getEnforceTwilight())
            outstream << "<Constraint>EnforceTwilight</Constraint>" << endl;
        outstream << "</Constraints>" << endl;

        outstream << "<CompletionCondition>" << endl;
       if (job->getCompletionCondition() == SchedulerJob::FINISH_SEQUENCE)
           outstream << "<Condition>Sequence</Condition>" << endl;
       else if (job->getCompletionCondition() == SchedulerJob::FINISH_LOOP)
           outstream << "<Condition>Loop</Condition>" << endl;
       else if (job->getCompletionCondition() == SchedulerJob::FINISH_AT)
           outstream << "<Condition value='" << job->getCompletionTime().toString(Qt::ISODate) << "'>At</Condition>" << endl;
       outstream << "</CompletionCondition>" << endl;

       outstream << "<Profile>" << job->getProfile() << "</Profile>" << endl;
       outstream << "<Steps>" << endl;
       if  (job->getStepPipeline() & SchedulerJob::USE_TRACK)
           outstream << "<Step>Track</Step>" << endl;
       if  (job->getStepPipeline() & SchedulerJob::USE_FOCUS)
           outstream << "<Step>Focus</Step>" << endl;
       if  (job->getStepPipeline() & SchedulerJob::USE_ALIGN)
           outstream << "<Step>Align</Step>" << endl;
       if  (job->getStepPipeline() & SchedulerJob::USE_GUIDE)
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
    Q_ASSERT(currentJob != NULL);

    SkyPoint target = currentJob->getTargetCoords();
    //target.EquatorialToHorizontal(KStarsData::Instance()->lst(), geo->lat());

    QList<QVariant> telescopeSlew;
    telescopeSlew.append(target.ra().Hours());
    telescopeSlew.append(target.dec().Degrees());

    appendLogText(i18n("Slewing to %1 ...", currentJob->getName()));

    mountInterface->callWithArgumentList(QDBus::AutoDetect,"slew",telescopeSlew);

    currentJob->setStage(SchedulerJob::STAGE_SLEWING);
}

void Scheduler::startFocusing()
{
    // Check if autofocus is supported
    QDBusReply<bool> focusModeReply;
    focusModeReply = focusInterface->call(QDBus::AutoDetect,"canAutoFocus");

    if ( focusModeReply.error().type() != QDBusError::NoError)
    {
        appendLogText(i18n("canAutoFocus DBUS error: %1", QDBusError::errorString(focusModeReply.error().type())));
        return;
    }

    if (focusModeReply.value() == false)
    {
        appendLogText(i18n("Autofocus is not supported."));
        currentJob->setStepPipeline(static_cast<SchedulerJob::StepPipeline> (currentJob->getStepPipeline() & ~SchedulerJob::USE_FOCUS));
        currentJob->setStage(SchedulerJob::STAGE_FOCUS_COMPLETE);
        getNextAction();
        return;
    }

    // Clear the HFR limit value set in the capture module
    captureInterface->call(QDBus::AutoDetect,"clearAutoFocusHFR");

    QDBusMessage reply;

    // We always need to reset frame first
    if ( (reply = focusInterface->call(QDBus::AutoDetect,"resetFrame")).type() == QDBusMessage::ErrorMessage)
    {
        appendLogText(i18n("resetFrame DBUS error: %1", reply.errorMessage()));
        return;
    }


    // Set autostar & use subframe
    QList<QVariant> autoStar;
    autoStar.append(true);
    if ( (reply = focusInterface->callWithArgumentList(QDBus::AutoDetect,"setAutoStarEnabled",autoStar)).type() == QDBusMessage::ErrorMessage)
    {
        appendLogText(i18n("setAutoFocusStar DBUS error: %1", reply.errorMessage()));
        return;
    }

    // Start auto-focus
    if ( (reply = focusInterface->call(QDBus::AutoDetect,"start")).type() == QDBusMessage::ErrorMessage)
    {
        appendLogText(i18n("startFocus DBUS error: %1", reply.errorMessage()));
        return;
    }

    if (currentJob->getStage() == SchedulerJob::STAGE_RESLEWING_COMPLETE || currentJob->getStage() == SchedulerJob::STAGE_POSTALIGN_FOCUSING)
    {
        currentJob->setStage(SchedulerJob::STAGE_POSTALIGN_FOCUSING);
        appendLogText(i18n("Post-alignment focusing for %1 ...", currentJob->getName()));
    }
    else
    {
        currentJob->setStage(SchedulerJob::STAGE_FOCUSING);
        appendLogText(i18n("Focusing %1 ...", currentJob->getName()));
    }
}

void Scheduler::findNextJob()
{
    jobTimer.stop();

    if (Options::verboseLogging())
        qDebug() << "Scheduler: Find next job...";

    if (currentJob->getState() == SchedulerJob::JOB_ERROR)
    {
        appendLogText(i18n("%1 observation job terminated due to errors.", currentJob->getName()));
        captureBatch=0;

        // Stop Guiding if it was used
        stopGuiding();

        currentJob = NULL;
        schedulerTimer.start();
        return;
    }

    if (currentJob->getState() == SchedulerJob::JOB_ABORTED)
    {
        currentJob = NULL;
        schedulerTimer.start();
        return;
    }

    // Check completion criteria

    // We're done whether the job completed successfully or not.
    if (currentJob->getCompletionCondition() == SchedulerJob::FINISH_SEQUENCE)
    {
        currentJob->setState(SchedulerJob::JOB_COMPLETE);
        captureBatch=0;

        // Stop Guiding if it was used
        stopGuiding();

        currentJob = NULL;
        schedulerTimer.start();
        return;
    }

    if (currentJob->getCompletionCondition() == SchedulerJob::FINISH_LOOP)
    {
        currentJob->setState(SchedulerJob::JOB_BUSY);
        currentJob->setStage(SchedulerJob::STAGE_CAPTURING);
        captureBatch++;

        startCapture();
        jobTimer.start();
        return;
    }

    if (currentJob->getCompletionCondition() == SchedulerJob::FINISH_AT)
    {
        if (KStarsData::Instance()->lt().secsTo(currentJob->getCompletionTime()) <= 0)
        {
            appendLogText(i18np("%1 observation job reached completion time with #%2 batch done. Stopping...",
                                "%1 observation job reached completion time with #%2 batches done. Stopping...", currentJob->getName(), captureBatch+1));
            currentJob->setState(SchedulerJob::JOB_COMPLETE);

            stopCurrentJobAction();
            stopGuiding();

            captureBatch=0;
            currentJob = NULL;
            schedulerTimer.start();
            return;
        }
        else
        {
            appendLogText(i18n("%1 observation job completed and will restart now...", currentJob->getName()));
            currentJob->setState(SchedulerJob::JOB_BUSY);
            currentJob->setStage(SchedulerJob::STAGE_CAPTURING);

            captureBatch++;

            startCapture();
            jobTimer.start();
            return;
        }
    }
}

void Scheduler::startAstrometry()
{
    QDBusMessage reply;
    setGOTOMode(Align::GOTO_SLEW);

    // Always turn update coords on
    QVariant arg(true);
    alignInterface->call(QDBus::AutoDetect,"setUpdateCoords", arg);

    // If FITS file is specified, then we use load and slew
    if (currentJob->getFITSFile().isEmpty() == false)
    {
        QList<QVariant> solveArgs;
        solveArgs.append(currentJob->getFITSFile().toString(QUrl::PreferLocalFile));

        if ( (reply = alignInterface->callWithArgumentList(QDBus::AutoDetect,"loadAndSlew",solveArgs)).type() == QDBusMessage::ErrorMessage)
        {
            appendLogText(i18n("loadAndSlew DBUS error: %1", reply.errorMessage()));
            return;
        }

        loadAndSlewProgress=true;
        appendLogText(i18n("Solving %1 ...", currentJob->getFITSFile().fileName()));
    }
    else
    {
        if ( (reply = alignInterface->call(QDBus::AutoDetect,"captureAndSolve")).type() == QDBusMessage::ErrorMessage)
        {
            appendLogText(i18n("captureAndSolve DBUS error: %1", reply.errorMessage()));
            return;
        }

        appendLogText(i18n("Capturing and solving %1 ...", currentJob->getName()));
    }

    currentJob->setStage(SchedulerJob::STAGE_ALIGNING);
}

void Scheduler::startGuiding()
{
    // Make sure calibration is auto
    //QVariant arg(true);
    //guideInterface->call(QDBus::AutoDetect,"setCalibrationAutoStar", arg);

    //QDBusReply<bool> guideReply = guideInterface->call(QDBus::AutoDetect,"startAutoCalibrateGuide");
    guideInterface->call(QDBus::AutoDetect,"startAutoCalibrateGuide");
    /*if (guideReply.value() == false)
    {
        appendLogText(i18n("Starting guide calibration failed. If using external guide application, ensure it is up and running."));
        currentJob->setState(SchedulerJob::JOB_ERROR);
    }
    else
    {*/
        currentJob->setStage(SchedulerJob::STAGE_GUIDING);

        appendLogText(i18n("Starting guiding procedure for %1 ...", currentJob->getName()));
    //}
}

void Scheduler::startCapture()
{
    captureInterface->call(QDBus::AutoDetect,"clearSequenceQueue");

    QString targetName = currentJob->getName().replace(" ", "");
    QList<QVariant> targetArgs;
    targetArgs.append(targetName);
    captureInterface->callWithArgumentList(QDBus::AutoDetect, "setTargetName", targetArgs);

    QString url = currentJob->getSequenceFile().toLocalFile();

    QList<QVariant> dbusargs;
    dbusargs.append(url);
    captureInterface->callWithArgumentList(QDBus::AutoDetect,"loadSequenceQueue",dbusargs);

    // If sequence is a loop, ignore sequence history
    if (currentJob->getCompletionCondition() != SchedulerJob::FINISH_SEQUENCE)
            captureInterface->call(QDBus::AutoDetect,"ignoreSequenceHistory");

    // Start capture process
    captureInterface->call(QDBus::AutoDetect,"start");

    currentJob->setStage(SchedulerJob::STAGE_CAPTURING);

    if (captureBatch > 0)
        appendLogText(i18n("%1 capture is in progress (Batch #%2)...", currentJob->getName(), captureBatch+1));
    else
        appendLogText(i18n("%1 capture is in progress...", currentJob->getName()));
}

void Scheduler::stopGuiding()
{
    if ( (currentJob->getStepPipeline() & SchedulerJob::USE_GUIDE) && (currentJob->getStage() == SchedulerJob::STAGE_GUIDING_COMPLETE ||  currentJob->getStage() == SchedulerJob::STAGE_CAPTURING) )
    {
        guideInterface->call(QDBus::AutoDetect,"abort");
        guideFailureCount=0;
    }
}

void Scheduler::setGOTOMode(Align::GotoMode mode)
{
    QVariant gotoMode(static_cast<int>(mode));
    alignInterface->call(QDBus::AutoDetect,"setGOTOMode",gotoMode);
}

void Scheduler::stopEkos()
{
    if (Options::verboseLogging())
        qDebug() << "Scheduler: Stopping Ekos...";

    indiConnectFailureCount=0;
    ekosInterface->call(QDBus::AutoDetect,"disconnectDevices");
    ekosInterface->call(QDBus::AutoDetect,"stop");

    startupState = STARTUP_IDLE;
    shutdownState= SHUTDOWN_IDLE;
    weatherStatus= IPS_IDLE;
}

void Scheduler::setDirty()
{
    mDirty = true;

    if (sender() == startupProcedureButtonGroup || sender() == shutdownProcedureGroup)
        return;

    if (jobUnderEdit >= 0 && state != SCHEDULER_RUNNIG && queueTable->selectedItems().isEmpty() == false)
        saveJob();
}

bool Scheduler::estimateJobTime(SchedulerJob *schedJob)
{
    // We can't estimate times that do not finish when sequence is done
    if (schedJob->getCompletionCondition() != SchedulerJob::FINISH_SEQUENCE)
        return false;

    QList<SequenceJob*> jobs;
    bool hasAutoFocus = false;

    if (loadSequenceQueue(schedJob->getSequenceFile().toLocalFile(),schedJob, jobs, hasAutoFocus) == false)
        return false;

    bool lightFramesRequired = false;

    int totalSequenceCount=0, totalCompletedCount=0;
    double totalImagingTime=0;
    bool rememberJobProgress = Options::rememberJobProgress();
    foreach (SequenceJob *job, jobs)
    {
        if (job->getUploadMode() == ISD::CCD::UPLOAD_LOCAL)
        {
            appendLogText(i18n("Cannot estimate time since the sequence saves the files remotely."));
            qDeleteAll(jobs);
            return false;
        }

        int completed = 0;
        if (rememberJobProgress)
            completed = getCompletedFiles(job->getFITSDir(), job->getFullPrefix());

        // If we have a LIGHT frame that still has remaining images then return false
        if (job->getFrameType() == FRAME_LIGHT && completed < job->getCount())
            lightFramesRequired = true;

        totalSequenceCount  += job->getCount();
        totalCompletedCount += rememberJobProgress ? completed : 0;
        totalImagingTime    += fabs((job->getExposure() + job->getDelay()) * (job->getCount() - completed));

        if (completed < job->getCount() && job->getFrameType() == FRAME_LIGHT)
        {
            // If inSequenceFocus is true
            if (hasAutoFocus)
                // Wild guess that each in sequence auto focus takes an average of 30 seconds. It can take any where from 2 seconds to 2+ minutes.
                totalImagingTime += (job->getCount() - completed) * 30;
            // If we're dithering after each exposure, that's another 10-20 seconds
            if (schedJob->getStepPipeline() & SchedulerJob::USE_GUIDE && Options::ditherEnabled())
                totalImagingTime += ((job->getCount() - completed) * 15) / Options::ditherFrames();
        }
    }

    schedJob->setLightFramesRequired(lightFramesRequired);

    qDeleteAll(jobs);

    if (totalCompletedCount > 0 && totalCompletedCount >= totalSequenceCount)
    {
        appendLogText(i18n("%1 observation job is already complete.", schedJob->getName()));
        schedJob->setEstimatedTime(0);
        return true;
    }

    if (lightFramesRequired)
    {
        // Are we doing tracking? It takes about 30 seconds
        if (schedJob->getStepPipeline() & SchedulerJob::USE_TRACK)
            totalImagingTime += 30;
        // Are we doing initial focusing? That can take about 2 minutes
        if (schedJob->getStepPipeline() & SchedulerJob::USE_FOCUS)
            totalImagingTime += 120;
        // Are we doing astrometry? That can take about 30 seconds
        if (schedJob->getStepPipeline() & SchedulerJob::USE_ALIGN)
            totalImagingTime += 30;
        // Are we doing guiding? Calibration process can take about 2 mins
        if (schedJob->getStepPipeline() & SchedulerJob::USE_GUIDE)
            totalImagingTime += 120;
    }

    dms estimatedTime;
    estimatedTime.setH(totalImagingTime/3600.0);
    appendLogText(i18n("%1 observation job is estimated to take %2 to complete.", schedJob->getName(), estimatedTime.toHMSString()));

    schedJob->setEstimatedTime(totalImagingTime);

    return true;
}

void    Scheduler::parkMount()
{
    QDBusReply<int> MountReply = mountInterface->call(QDBus::AutoDetect, "getParkingStatus");
    Mount::ParkingStatus status = (Mount::ParkingStatus) MountReply.value();

    if (status != Mount::PARKING_OK)
    {
        if (status == Mount::PARKING_BUSY)
            appendLogText(i18n("Parking mount in progress..."));
        else
        {
            mountInterface->call(QDBus::AutoDetect,"park");
            appendLogText(i18n("Parking mount..."));

            currentOperationTime.start();
        }

        if (shutdownState == SHUTDOWN_PARK_MOUNT)
                shutdownState = SHUTDOWN_PARKING_MOUNT;
        else if (parkWaitState == PARKWAIT_PARK)
                parkWaitState = PARKWAIT_PARKING;

    }
    else if (status == Mount::PARKING_OK)
    {
        appendLogText(i18n("Mount already parked."));

        if (shutdownState == SHUTDOWN_PARK_MOUNT)
            shutdownState = SHUTDOWN_PARK_DOME;
        else if (parkWaitState == PARKWAIT_PARK)
                parkWaitState = PARKWAIT_PARKED;
    }

}

void    Scheduler::unParkMount()
{
    QDBusReply<int> MountReply = mountInterface->call(QDBus::AutoDetect, "getParkingStatus");
    Mount::ParkingStatus status = (Mount::ParkingStatus) MountReply.value();

    if (status != Mount::UNPARKING_OK)
    {
        if (status == Mount::UNPARKING_BUSY)
            appendLogText(i18n("Unparking mount in progress..."));
        else
        {
            mountInterface->call(QDBus::AutoDetect,"unpark");
            appendLogText(i18n("Unparking mount..."));

            currentOperationTime.start();
        }

        if (startupState == STARTUP_UNPARK_MOUNT)
                startupState = STARTUP_UNPARKING_MOUNT;
        else if (parkWaitState == PARKWAIT_UNPARK)
                parkWaitState = PARKWAIT_UNPARKING;
    }
    else if (status == Mount::UNPARKING_OK)
    {
        appendLogText(i18n("Mount already unparked."));

        if (startupState == STARTUP_UNPARK_MOUNT)
                startupState = STARTUP_UNPARK_CAP;
        else if (parkWaitState == PARKWAIT_UNPARK)
                parkWaitState = PARKWAIT_UNPARKED;

    }

}

void Scheduler::checkMountParkingStatus()
{
    static int parkingFailureCount=0;
    QDBusReply<int> mountReply = mountInterface->call(QDBus::AutoDetect, "getParkingStatus");
    Mount::ParkingStatus status = (Mount::ParkingStatus) mountReply.value();

    if (mountReply.error().type() == QDBusError::UnknownObject)
        status = Mount::PARKING_ERROR;

    switch (status)
    {
        case Mount::PARKING_OK:
            appendLogText(i18n("Mount parked."));
            if (shutdownState == SHUTDOWN_PARKING_MOUNT)
               shutdownState = SHUTDOWN_PARK_DOME;
            else if (parkWaitState == PARKWAIT_PARKING)
                parkWaitState = PARKWAIT_PARKED;
            parkingFailureCount=0;
        break;

        case Mount::UNPARKING_OK:
        appendLogText(i18n("Mount unparked."));
        if (startupState == STARTUP_UNPARKING_MOUNT)
                startupState = STARTUP_UNPARK_CAP;
        else if (parkWaitState == PARKWAIT_UNPARKING)
            parkWaitState = PARKWAIT_UNPARKED;
        parkingFailureCount=0;
         break;

       case Mount::PARKING_BUSY:
       case Mount::UNPARKING_BUSY:
        // TODO make the timeouts configurable by the user
        if (currentOperationTime.elapsed() > (60*1000))
        {
             if (parkingFailureCount++ < MAX_FAILURE_ATTEMPTS)
             {
                 appendLogText(i18n("Operation timeout. Restarting operation..."));
                 if (status == Mount::PARKING_BUSY)
                     parkMount();
                 else
                     unParkMount();
                 break;
             }
        }
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
        parkingFailureCount=0;
        break;

       default:
        break;
    }

}

bool Scheduler::isMountParked()
{
    QDBusReply<int> mountReply = mountInterface->call(QDBus::AutoDetect, "getParkingStatus");
    Mount::ParkingStatus status = (Mount::ParkingStatus) mountReply.value();

    if (mountReply.error().type() == QDBusError::UnknownObject)
        return false;

    if (status == Mount::PARKING_OK || status == Mount::PARKING_IDLE)
        return true;
    else
        return false;
}

void    Scheduler::parkDome()
{
    QDBusReply<int> domeReply = domeInterface->call(QDBus::AutoDetect, "getParkingStatus");
    Dome::ParkingStatus status = (Dome::ParkingStatus) domeReply.value();

    if (status != Dome::PARKING_OK)
    {
       shutdownState = SHUTDOWN_PARKING_DOME;
        domeInterface->call(QDBus::AutoDetect,"park");
        appendLogText(i18n("Parking dome..."));

        currentOperationTime.start();
    }
    else if (status == Dome::PARKING_OK)
    {
        appendLogText(i18n("Dome already parked."));
        shutdownState= SHUTDOWN_SCRIPT;
    }
}

void    Scheduler::unParkDome()
{
    QDBusReply<int> domeReply = domeInterface->call(QDBus::AutoDetect, "getParkingStatus");
    Dome::ParkingStatus status = (Dome::ParkingStatus) domeReply.value();

    if (status != Dome::UNPARKING_OK)
    {
        startupState = STARTUP_UNPARKING_DOME;
        domeInterface->call(QDBus::AutoDetect,"unpark");
        appendLogText(i18n("Unparking dome..."));

        currentOperationTime.start();
    }
    else if (status == Dome::UNPARKING_OK)
    {
        appendLogText(i18n("Dome already unparked."));
        startupState = STARTUP_UNPARK_MOUNT;
    }

}

void Scheduler::checkDomeParkingStatus()
{
    static int parkingFailureCount=0;
    QDBusReply<int> domeReply = domeInterface->call(QDBus::AutoDetect, "getParkingStatus");
    Dome::ParkingStatus status = (Dome::ParkingStatus) domeReply.value();

    if (domeReply.error().type() == QDBusError::UnknownObject)
        status = Dome::PARKING_ERROR;

    switch (status)
    {
        case Dome::PARKING_OK:
            if (shutdownState == SHUTDOWN_PARKING_DOME)
            {
                appendLogText(i18n("Dome parked."));

                shutdownState = SHUTDOWN_SCRIPT;
            }
            parkingFailureCount=0;
        break;

        case Dome::UNPARKING_OK:
        if (startupState == STARTUP_UNPARKING_DOME)
        {
           startupState = STARTUP_UNPARK_MOUNT;
           appendLogText(i18n("Dome unparked."));
        }
        parkingFailureCount=0;
         break;

    case Dome::PARKING_BUSY:
    case Dome::UNPARKING_BUSY:
     // TODO make the timeouts configurable by the user
     if (currentOperationTime.elapsed() > (120*1000))
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
        parkingFailureCount=0;
        break;

       default:
        break;
    }

}

bool Scheduler::isDomeParked()
{
    QDBusReply<int> domeReply = domeInterface->call(QDBus::AutoDetect, "getParkingStatus");
    Dome::ParkingStatus status = (Dome::ParkingStatus) domeReply.value();

    if (domeReply.error().type() == QDBusError::UnknownObject)
        return false;

    if (status == Dome::PARKING_OK || status == Dome::PARKING_IDLE)
        return true;
    else
        return false;
}

void    Scheduler::parkCap()
{
    QDBusReply<int> capReply = capInterface->call(QDBus::AutoDetect, "getParkingStatus");
    DustCap::ParkingStatus status = (DustCap::ParkingStatus) capReply.value();

    if (status != DustCap::PARKING_OK)
    {
        shutdownState = SHUTDOWN_PARKING_CAP;
        capInterface->call(QDBus::AutoDetect,"park");
        appendLogText(i18n("Parking Cap..."));

        currentOperationTime.start();
    }
    else if (status == DustCap::PARKING_OK)
    {
        appendLogText(i18n("Cap already parked."));
        shutdownState= SHUTDOWN_PARK_MOUNT;
    }
}

void    Scheduler::unParkCap()
{
    QDBusReply<int> capReply = capInterface->call(QDBus::AutoDetect, "getParkingStatus");
    DustCap::ParkingStatus status = (DustCap::ParkingStatus) capReply.value();

    if (status != DustCap::UNPARKING_OK)
    {
        startupState = STARTUP_UNPARKING_CAP;
        capInterface->call(QDBus::AutoDetect,"unpark");
        appendLogText(i18n("Unparking cap..."));

        currentOperationTime.start();
    }
    else if (status == DustCap::UNPARKING_OK)
    {
        appendLogText(i18n("Cap already unparked."));
        startupState = STARTUP_COMPLETE;
    }
}

void Scheduler::checkCapParkingStatus()
{
    static int parkingFailureCount=0;
    QDBusReply<int> capReply = capInterface->call(QDBus::AutoDetect, "getParkingStatus");
    DustCap::ParkingStatus status = (DustCap::ParkingStatus) capReply.value();

    if (capReply.error().type() == QDBusError::UnknownObject)
        status = DustCap::PARKING_ERROR;

    switch (status)
    {
        case DustCap::PARKING_OK:
            if (shutdownState == SHUTDOWN_PARKING_CAP)
            {
                appendLogText(i18n("Cap parked."));
                shutdownState = SHUTDOWN_PARK_MOUNT;
            }
            parkingFailureCount=0;
        break;

        case DustCap::UNPARKING_OK:
        if (startupState == STARTUP_UNPARKING_CAP)
        {
           startupState = STARTUP_COMPLETE;
           appendLogText(i18n("Cap unparked."));
        }
        parkingFailureCount=0;
         break;

    case DustCap::PARKING_BUSY:
    case DustCap::UNPARKING_BUSY:
    // TODO make the timeouts configurable by the user
     if (currentOperationTime.elapsed() > (60*1000))
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
        parkingFailureCount=0;
        break;

       default:
        break;
    }

}

void Scheduler::startJobEvaluation()
{
    jobEvaluationOnly = true;
    if (Dawn < 0)
        calculateDawnDusk();
    evaluateJobs();
}

void Scheduler::updatePreDawn()
{
    double earlyDawn = Dawn - Options::preDawnTime()/(60.0 * 24.0);
    int dayOffset=0;
    QTime dawn = QTime(0,0,0).addSecs(Dawn*24*3600);
    if (KStarsData::Instance()->lt().time() >= dawn)
        dayOffset=1;
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
        appendLogText(i18n("%1 observation job aborted due to bad weather.", job->getName()));
    }
    /*else if (weatherStatus == IPS_BUSY)
    {
        appendLogText(i18n("%1 observation job delayed due to bad weather.", job->getName()));
        schedulerTimer.stop();
        connect(this, SIGNAL(weatherChanged(IPState)), this, SLOT(resumeCheckStatus()));
    }*/

    return false;
}

void Scheduler::resumeCheckStatus()
{
    disconnect(this, SIGNAL(weatherChanged(IPState)), this, SLOT(resumeCheckStatus()));
    schedulerTimer.start();
}

void Scheduler::startMosaicTool()
{
    bool raOk=false, decOk=false;
    dms ra( raBox->createDms( false, &raOk ) ); //false means expressed in hours
    dms dec( decBox->createDms( true, &decOk ) );

    if (raOk == false)
    {
        appendLogText(i18n("RA value %1 is invalid.", raBox->text()));
        return;
    }

    if (decOk == false)
    {
        appendLogText(i18n("DEC value %1 is invalid.", decBox->text()));
        return;
    }

    Mosaic mosaicTool(this);

    SkyPoint center;
    center.setRA0(ra);
    center.setDec0(dec);

    mosaicTool.setCenter(center);
    mosaicTool.calculateFOV();
    mosaicTool.adjustSize();

    int batchCount=1;

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
        qDebug() << "Job accepted with # " << mosaicTool.getJobs().size() << " jobs and fits dir " << mosaicTool.getJobsDir();

        QString outputDir = mosaicTool.getJobsDir();
        QString targetName = nameEdit->text().simplified().remove(" ");

        XMLEle *root = getSequenceJobRoot();
        if (root == NULL)
            return;

        int currentJobsCount = jobs.count();

        foreach (OneTile *oneJob, mosaicTool.getJobs())
        {
            QString prefix = QString("%1-Part%2").arg(targetName).arg(batchCount++);
            prefix = prefix.replace(" ", "-");

            nameEdit->setText(prefix);

            if (createJobSequence(root, prefix, outputDir) == false)
                return;

            QString filename  = QString("%1/%2.esq").arg(outputDir).arg(prefix);
            sequenceEdit->setText(filename);
            sequenceURL = QUrl::fromLocalFile(filename);

            raBox->setText(oneJob->skyCenter.ra0().toHMSString());
            decBox->setText(oneJob->skyCenter.dec0().toDMSString());

            saveJob();
        }

        delXMLEle(root);

        // Delete any prior jobs before saving
        for (int i=0; i < currentJobsCount; i++)
        {
            delete (jobs.takeFirst());
            queueTable->removeRow(0);
        }

        QUrl mosaicURL = QUrl::fromLocalFile((QString("%1/%2_mosaic.esl").arg(outputDir).arg(targetName)));

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

XMLEle * Scheduler::getSequenceJobRoot()
{
    QFile sFile;
    sFile.setFileName(sequenceURL.toLocalFile());

    if ( !sFile.open( QIODevice::ReadOnly))
    {
        KMessageBox::sorry(KStars::Instance(), i18n( "Unable to open file %1",  sFile.fileName()), i18n( "Could Not Open File" ) );
        return NULL;
    }

    LilXML *xmlParser = newLilXML();
    char errmsg[MAXRBUF];
    XMLEle *root = NULL;
    char c;

    while ( sFile.getChar(&c))
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

    if ( !sFile.open( QIODevice::ReadOnly))
    {
        KMessageBox::sorry(KStars::Instance(), i18n( "Unable to open file %1",  sFile.fileName()), i18n( "Could Not Open File" ) );
        return false;
    }


    XMLEle *ep=NULL, *subEP=NULL;

    for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep), "Job"))
        {

            for (subEP = nextXMLEle(ep, 1) ; subEP != NULL ; subEP = nextXMLEle(ep, 0))
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
                    editXMLEle(subEP, QString("%1/%2").arg(outputDir).arg(prefix).toLatin1().constData());
                }
            }

        }
    }


    QDir().mkpath(outputDir);

    QString filename = QString("%1/%2.esq").arg(outputDir).arg(prefix);

    FILE *outputFile = fopen(filename.toLatin1().constData(), "w");

    if ( outputFile == NULL)
    {
        QString message = i18n( "Unable to write to file %1",  filename);
        KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
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

    foreach(SchedulerJob *job, jobs)
    {
        job->setState(SchedulerJob::JOB_IDLE);
        job->setStartupCondition(job->getFileStartupCondition());
    }
}

void Scheduler::checkTwilightWarning(bool enabled)
{
    if (enabled == false)
    {
        if (KMessageBox::warningContinueCancel(NULL, i18n("Warning! Turning off astronomial twilight check may cause the observatory to run during daylight. This can cause irreversible damage to your equipment!"),
                         i18n("Astronomial Twilight Warning"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(), "astronomical_twilight_warning") == KMessageBox::Cancel)
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

            startupB->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/icons/breeze/default/media-playback-start.svg")));
        }
}

void Scheduler::runStartupProcedure()
{
    if (startupState == STARTUP_IDLE || startupState == STARTUP_ERROR || startupState == STARTUP_COMPLETE)
    {
        if (KMessageBox::questionYesNo(NULL, i18n("Are you sure you want to execute the startup procedure manually?")) == KMessageBox::Yes)
        {
            appendLogText(i18n("Warning! Executing startup procedure manually..."));
            startupB->setIcon(QIcon::fromTheme("media-playback-stop", QIcon(":/icons/breeze/default/media-playback-stop.svg")));
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
                appendLogText(i18n("Manual shutdown procedure completed successfully."));
            else if (shutdownState == SHUTDOWN_ERROR)
                appendLogText(i18n("Manual shutdown procedure terminated due to errors."));

            shutdownState = SHUTDOWN_IDLE;
            shutdownB->setIcon(QIcon::fromTheme("media-playback-start", QIcon(":/icons/breeze/default/media-playback-start.svg")));
        }

}

void Scheduler::runShutdownProcedure()
{
    if (shutdownState == SHUTDOWN_IDLE || shutdownState == SHUTDOWN_ERROR || shutdownState == SHUTDOWN_COMPLETE)
    {
        if (KMessageBox::questionYesNo(NULL, i18n("Are you sure you want to execute the shutdown procedure manually?")) == KMessageBox::Yes)
        {
            appendLogText(i18n("Warning! Executing shutdown procedure manually..."));
            shutdownB->setIcon(QIcon::fromTheme("media-playback-stop", QIcon(":/icons/breeze/default/media-playback-stop.svg")));
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
    QString currentProfile = profileCombo->currentText();

    QDBusReply<QStringList> profiles = ekosInterface->call(QDBus::AutoDetect,"getProfiles");

    if (profiles.error().type() == QDBusError::NoError)
    {
        profileCombo->blockSignals(true);
        profileCombo->clear();
        profileCombo->addItem(i18n("Default"));
        profileCombo->addItems(profiles);
        profileCombo->setCurrentText(currentProfile);
        profileCombo->blockSignals(false);
    }
}

bool Scheduler::loadSequenceQueue(const QString &fileURL, SchedulerJob *schedJob, QList<SequenceJob*> &jobs, bool &hasAutoFocus)
{
    QFile sFile;
    sFile.setFileName(fileURL);

    if ( !sFile.open( QIODevice::ReadOnly))
    {
        appendLogText(i18n( "Unable to open file %1",  fileURL));
        return false;
    }

    LilXML *xmlParser = newLilXML();
    char errmsg[MAXRBUF];
    XMLEle *root = NULL;
    XMLEle *ep;
    char c;

    while ( sFile.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
        {
             for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
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

SequenceJob * Scheduler::processJobInfo(XMLEle *root, SchedulerJob *schedJob)
{
    XMLEle *ep;
    XMLEle *subEP;

    const QMap<QString,int> frameTypes = { {"Light" , FRAME_LIGHT}, {"Dark", FRAME_DARK}, {"Bias", FRAME_BIAS}, {"Flat", FRAME_FLAT}};

    SequenceJob *job = new SequenceJob();
    QString rawPrefix, frameType, filterType;
    double exposure=0;
    bool filterEnabled=false, expEnabled=false, tsEnabled=false;

    for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
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
            job->setFrameType(frameTypes[frameType], frameType);
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
                expEnabled = ( !strcmp("1", pcdataXMLEle(subEP)));

            subEP = findXMLEle(ep, "TimeStampEnabled");
            if (subEP)
                tsEnabled = ( !strcmp("1", pcdataXMLEle(subEP)));

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
            job->setFITSDir(pcdataXMLEle(ep));
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

    if (filterEnabled && filterType.isEmpty() == false && (job->getFrameType() == FRAME_LIGHT || job->getFrameType() == FRAME_FLAT))
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

    // FITS Dir
    QString finalFITSDir = job->getFITSDir();

    QString targetName = schedJob->getName().remove(" ");
    finalFITSDir += QLatin1Literal("/") + targetName + QLatin1Literal("/") + frameType;
    if ( (job->getFrameType() == FRAME_LIGHT || job->getFrameType() == FRAME_FLAT) && filterType.isEmpty() == false)
        finalFITSDir += QLatin1Literal("/") + filterType;

    job->setFITSDir(finalFITSDir);

    return job;
}

int Scheduler::getCompletedFiles(const QString &path, const QString &seqPrefix)
{
    QString tempName;
    int seqFileCount=0;

    QDirIterator it(path, QDir::Files);

    while (it.hasNext())
    {
        tempName = it.next();
        QFileInfo info(tempName);
        tempName = info.baseName();

        // find the prefix first
        if (tempName.startsWith(seqPrefix) == false)
            continue;

        seqFileCount++;
    }

    return seqFileCount;
}

}


