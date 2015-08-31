/*  Ekos Scheduler Module
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    DBus calls from GSoC 2015 Ekos Scheduler project by Daniel Leu <daniel_mihai.leu@cti.pub.ro>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "Options.h"

#include <KMessageBox>
#include <KLocalizedString>
#include <QtDBus>
#include <QFileDialog>

#include "dialogs/finddialog.h"
#include "ekosmanager.h"
#include "kstars.h"
#include "scheduler.h"
#include "skymapcomposite.h"
#include "kstarsdata.h"
#include "ksmoon.h"
#include "ksalmanac.h"

namespace Ekos
{

Scheduler::Scheduler()
{
    setupUi(this);

    state       = SCHEDULER_IDLE;
    ekosState   = EKOS_IDLE;
    indiState   = INDI_IDLE;

    startupState = STARTUP_IDLE;
    shutdownState= SHUTDOWN_IDLE;

    currentJob = NULL;
    geo        = 0;
    jobUnderEdit = false;
    mDirty       = false;

    // Set initial time for startup and completion times
    startupTimeEdit->setDateTime(KStarsData::Instance()->lt());
    completionTimeEdit->setDateTime(KStarsData::Instance()->lt());    

    // Set up DBus interfaces
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Scheduler",  this);
    ekosInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos", "org.kde.kstars.Ekos", QDBusConnection::sessionBus(), this);

    focusInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Focus", "org.kde.kstars.Ekos.Focus", QDBusConnection::sessionBus(),  this);
    captureInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Capture", "org.kde.kstars.Ekos.Capture", QDBusConnection::sessionBus(), this);
    mountInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Mount", "org.kde.kstars.Ekos.Mount", QDBusConnection::sessionBus(), this);
    alignInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Align", "org.kde.kstars.Ekos.Align", QDBusConnection::sessionBus(), this);
    guideInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Guide", "org.kde.kstars.Ekos.Guide", QDBusConnection::sessionBus(), this);

    moon = dynamic_cast<KSMoon*> (KStarsData::Instance()->skyComposite()->findByName("Moon"));

    pi = new QProgressIndicator(this);
    bottomLayout->addWidget(pi,0,0);

    raBox->setDegType(false); //RA box should be HMS-style

    addToQueueB->setIcon(QIcon::fromTheme("list-add"));
    removeFromQueueB->setIcon(QIcon::fromTheme("list-remove"));
    queueSaveAsB->setIcon(QIcon::fromTheme("document-save"));
    queueLoadB->setIcon(QIcon::fromTheme("document-open"));

    loadSequenceB->setIcon(QIcon::fromTheme("document-open"));
    selectStartupScriptB->setIcon(QIcon::fromTheme("document-open"));
    selectShutdownScriptB->setIcon(QIcon::fromTheme("document-open"));
    selectFITSB->setIcon(QIcon::fromTheme("document-open"));

    connect(selectObjectB,SIGNAL(clicked()),this,SLOT(selectObject()));
    connect(selectFITSB,SIGNAL(clicked()),this,SLOT(selectFITS()));
    connect(loadSequenceB,SIGNAL(clicked()),this,SLOT(selectSequence()));

    connect(addToQueueB,SIGNAL(clicked()),this,SLOT(addJob()));
    connect(removeFromQueueB, SIGNAL(clicked()), this, SLOT(removeJob()));
    connect(queueTable, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(editJob(QModelIndex)));
    connect(queueTable, SIGNAL(itemSelectionChanged()), this, SLOT(resetJobEdit()));

    connect(startB,SIGNAL(clicked()),this,SLOT(toggleScheduler()));
    connect(queueSaveAsB,SIGNAL(clicked()),this,SLOT(save()));
    connect(queueLoadB,SIGNAL(clicked()),this,SLOT(load()));

    // Load scheduler settings
    startupScript->setText(Options::startupScript());
    shutdownScript->setText(Options::shutdownScript());
    warmCCDCheck->setChecked(Options::warmUpCCD());
    parkTelescopeCheck->setChecked(Options::parkScope());
    parkDomeCheck->setChecked(Options::parkDome());
    unparkTelescopeCheck->setChecked(Options::unParkScope());
    unparkDomeCheck->setChecked(Options::unParkDome());
}

Scheduler::~Scheduler()
{

}

void Scheduler::appendLogText(const QString &text)
{

    logText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

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
        SkyObject *o = fd->selectedObject();
        if( o != NULL )
        {
            nameEdit->setText(o->name());
            raBox->setText(o->ra0().toHMSString());
            decBox->setText(o->dec0().toDMSString());

            if (sequenceEdit->text().isEmpty())
                appendLogText(i18n("Object selected. Please select the sequence file."));
            else
                addToQueueB->setEnabled(true);

        }
    }

    delete fd;

}

void Scheduler::selectFITS()
{
    fitsURL = QFileDialog::getOpenFileUrl(this, i18n("Open FITS Image"), QDir::homePath(), "FITS (*.fits *.fit)");
    if (fitsURL.isEmpty())
        return;

    fitsEdit->setText(fitsURL.path());

    raBox->clear();
    decBox->clear();

    if (nameEdit->text().isEmpty())
        nameEdit->setText(fitsURL.fileName());

    if (sequenceEdit->text().isEmpty())
        appendLogText(i18n("FITS selected. Please select the sequence file."));
    else
        addToQueueB->setEnabled(true);
}

void Scheduler::selectSequence()
{
    sequenceURL = QFileDialog::getOpenFileUrl(this, i18n("Open Sequence Queue"), QDir::homePath(), i18n("Ekos Sequence Queue (*.esq)"));
    if (sequenceURL.isEmpty())
        return;

    sequenceEdit->setText(sequenceURL.path());

    // For object selection, all fields must be filled
    if ( (raBox->isEmpty() == false && decBox->isEmpty() == false && nameEdit->text().isEmpty() == false)
    // For FITS selection, only the name and fits URL should be filled.
        || (nameEdit->text().isEmpty() == false && fitsURL.isEmpty() == false) )
                addToQueueB->setEnabled(true);
}

void Scheduler::addJob()
{
    if (state == SCHEDULER_RUNNIG)
    {
        appendLogText(i18n("Cannot add or modify a job while the scheduler is running."));
        return;
    }

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

    if (jobUnderEdit)
        job = jobs.at(queueTable->currentRow());
    else
        job = new SchedulerJob();

    job->setName(nameEdit->text());

    // Only get target coords if FITS file is not selected.
    if (fitsURL.isEmpty())
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

        job->setTargetCoords(ra, dec);
    }

    job->setDateTimeDisplayFormat(startupTimeEdit->displayFormat());
    job->setSequenceFile(sequenceURL);
    if (fitsURL.isEmpty() == false)
        job->setFITSFile(fitsURL);

    // #1 Startup conditions

    if (nowConditionR->isChecked())
        job->setStartupCondition(SchedulerJob::START_NOW);
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

    // #2 Constraints

    // Do we have minimum altitude constraint?
    if (altConstraintCheck->isChecked())
        job->setMinAltitude(minAltitude->value());
    // Do we have minimum moon separation constraint?
    if (moonSeparationCheck->isChecked())
        job->setMinMoonSeparation(minMoonSeparation->value());

    // Check weather enforcement and no meridian flip constraints
    job->setEnforceWeather(weatherB->isChecked());
    job->setNoMeridianFlip(noMeridianFlipCheck->isChecked());

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

    // Ekos Modules usage
    job->setModuleUsage(SchedulerJob::USE_NONE);
    if (focusModuleCheck->isChecked())
        job->setModuleUsage(static_cast<SchedulerJob::ModuleUsage> (job->getModuleUsage() | SchedulerJob::USE_FOCUS));
    if (alignModuleCheck->isChecked())
        job->setModuleUsage(static_cast<SchedulerJob::ModuleUsage> (job->getModuleUsage() | SchedulerJob::USE_ALIGN));
    if (guideModuleCheck->isChecked())
        job->setModuleUsage(static_cast<SchedulerJob::ModuleUsage> (job->getModuleUsage() | SchedulerJob::USE_GUIDE));


    // Add job to queue if it is new
    if (jobUnderEdit == false)
        jobs.append(job);

    int currentRow = 0;
    if (jobUnderEdit == false)
    {
        currentRow = queueTable->rowCount();
        queueTable->insertRow(currentRow);
    }
    else
        currentRow = queueTable->currentRow();

    QTableWidgetItem *nameCell = jobUnderEdit ? queueTable->item(currentRow, 0) : new QTableWidgetItem();
    nameCell->setText(job->getName());
    nameCell->setTextAlignment(Qt::AlignHCenter);
    nameCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    QTableWidgetItem *statusCell = jobUnderEdit ? queueTable->item(currentRow, 1) : new QTableWidgetItem();
    statusCell->setTextAlignment(Qt::AlignHCenter);
    statusCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    job->setStatusCell(statusCell);
    // Refresh state
    job->setState(job->getState());

    QTableWidgetItem *startupCell = jobUnderEdit ? queueTable->item(currentRow, 2) : new QTableWidgetItem();
    if (startupTimeConditionR->isChecked())
        startupCell->setText(startupTimeEdit->text());
    startupCell->setTextAlignment(Qt::AlignHCenter);
    startupCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    job->setStartupCell(startupCell);

    QTableWidgetItem *completionCell = jobUnderEdit ? queueTable->item(currentRow, 3) : new QTableWidgetItem();
    if (timeCompletionR->isChecked())
        completionCell->setText(completionTimeEdit->text());
    completionCell->setTextAlignment(Qt::AlignHCenter);
    completionCell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    if (jobUnderEdit == false)
    {
        queueTable->setItem(currentRow, 0, nameCell);
        queueTable->setItem(currentRow, 1, statusCell);
        queueTable->setItem(currentRow, 2, startupCell);
        queueTable->setItem(currentRow, 3, completionCell);
    }

    removeFromQueueB->setEnabled(true);

    if (queueTable->rowCount() > 0)
    {
        queueSaveAsB->setEnabled(true);
        mDirty = true;
    }

    if (jobUnderEdit)
    {
        jobUnderEdit = false;
        resetJobEdit();
        appendLogText(i18n("Job #%1 changes applied.", currentRow+1));
    }

    startB->setEnabled(true);
}

void Scheduler::editJob(QModelIndex i)
{
    if (state == SCHEDULER_RUNNIG)
    {
        appendLogText(i18n("Cannot add or modify a job while the scheduler is running."));
        return;
    }

    SchedulerJob *job = jobs.at(i.row());
    if (job == NULL)
        return;

    nameEdit->setText(job->getName());

    raBox->setText(job->getTargetCoords().ra0().toHMSString());
    decBox->setText(job->getTargetCoords().dec0().toDMSString());

    if (job->getFITSFile().isEmpty() == false)
        fitsEdit->setText(job->getFITSFile().path());

    sequenceEdit->setText(job->getSequenceFile().path());

    switch (job->getStartingCondition())
    {
        case SchedulerJob::START_NOW:
            nowConditionR->setChecked(true);
            break;

        case SchedulerJob::START_CULMINATION:
            culminationConditionR->setChecked(true);
            culminationOffset->setValue(job->getCulminationOffset());
            break;

        case SchedulerJob::START_AT:
            startupTimeConditionR->setChecked(true);
            startupTimeEdit->setDateTime(job->getStartupTime());
            break;
    }

    if (job->getMinAltitude() >= 0)
    {
        altConstraintCheck->setChecked(true);
        minAltitude->setValue(job->getMinAltitude());
    }

    if (job->getMinMoonSeparation() >= 0)
    {
        moonSeparationCheck->setChecked(true);
        minMoonSeparation->setValue(job->getMinMoonSeparation());
    }

    weatherB->setChecked(job->getEnforceWeather());
    noMeridianFlipCheck->setChecked(job->getNoMeridianFlip());

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

   addToQueueB->setIcon(QIcon::fromTheme("svn-update"));

   jobUnderEdit = true;
}

void Scheduler::resetJobEdit()
{
   if (jobUnderEdit)
       appendLogText(i18n("Editing job canceled."));

   jobUnderEdit = false;
   addToQueueB->setIcon(QIcon::fromTheme("list-add"));
}

void Scheduler::removeJob()
{
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
        removeFromQueueB->setEnabled(false);

    for (int i=0; i < jobs.count(); i++)
    {
        jobs.at(i)->setStatusCell(queueTable->item(i, 1));
        jobs.at(i)->setStartupCell(queueTable->item(i, 2));
    }

    queueTable->selectRow(queueTable->currentRow());

    if (queueTable->rowCount() == 0)
    {
        queueSaveAsB->setEnabled(false);
    }

    mDirty = true;

}

void Scheduler::toggleScheduler()
{
    if (state == SCHEDULER_RUNNIG)
        stop();
    else
        start();
}

void Scheduler::stop()
{
    if(state != SCHEDULER_RUNNIG)
        return;

    // TODO stop any running jobs!

    disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkStatus()));
    disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkJobStage()));

    state = SCHEDULER_IDLE;
    ekosState = EKOS_IDLE;
    indiState = INDI_IDLE;

    currentJob = NULL;

     pi->stopAnimation();
     startB->setText("Start Scheduler");
}

void Scheduler::start()
{
    if(state == SCHEDULER_RUNNIG)
        return;

    // Save settings
    Options::setStartupScript(startupScript->text());
    Options::setShutdownScript(shutdownScript->text());
    Options::setWarmUpCCD(warmCCDCheck->isChecked());
    Options::setParkScope(parkTelescopeCheck->isChecked());
    Options::setParkDome(parkDomeCheck->isChecked());
    Options::setUnParkScope(unparkTelescopeCheck->isChecked());
    Options::setUnParkDome(unparkDomeCheck->isChecked());

    pi->startAnimation();

    startB->setText("Stop Scheduler");

    geo = KStarsData::Instance()->geo();

    calculateDawnDusk();

    state = SCHEDULER_RUNNIG;

    currentJob = NULL;

    connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkStatus()));

}

void Scheduler::evaluateJobs()
{
    foreach(SchedulerJob *job, jobs)
    {
        if (job->getState() > SchedulerJob::JOB_SCHEDULED)
            continue;

        if (job->getState() == SchedulerJob::JOB_IDLE)
            job->setState(SchedulerJob::JOB_EVALUATION);

        int16_t score = 0, altScore=0, moonScore=0, darkScore=0, weatherScore=0;

        // #1 Update target horizontal coords.
        SkyPoint target = job->getTargetCoords();
        target.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

        // #2 Check startup conditions
        switch (job->getStartingCondition())
        {
                // #2.1 Now?
                case SchedulerJob::START_NOW:
                    altScore     = getAltitudeScore(job, target);
                    moonScore    = getMoonSeparationScore(job, target);
                    darkScore    = getDarkSkyScore(KStarsData::Instance()->lt().time());
                    weatherScore = getWeatherScore(job);
                    score = altScore + moonScore + darkScore + weatherScore;
                    job->setScore(score);

                    // If we can't start now, let's schedule it
                    if (score < 0)
                    {
                        // If Altitude or Dark score are negative, we try to schedule a better time for altitude and dark sky period.
                        if ( (altScore < 0 || darkScore < 0) && calculateAltitudeTime(job, job->getMinAltitude() > 0 ? job->getMinAltitude() : minAltitude->minimum()))
                        {
                            job->setState(SchedulerJob::JOB_SCHEDULED);
                            return;
                        }
                        else
                        {
                            job->setState(SchedulerJob::JOB_INVALID);
                            appendLogText(i18n("Cannot schedule %1", job->getName()));
                        }
                    }
                    break;

                  // #2.2 Culmination?
                  case SchedulerJob::START_CULMINATION:
                        if (calculateCulmination(job))
                        {
                            job->setState(SchedulerJob::JOB_SCHEDULED);
                            return;
                        }
                        else
                            job->setState(SchedulerJob::JOB_INVALID);
                        break;

                 // #2.3 Start at?
                 case SchedulerJob::START_AT:
                    if (KStarsData::Instance()->lt().secsTo(job->getStartupTime()) <= 300)
                    {
                        score += getAltitudeScore(job, target);
                        score += getMoonSeparationScore(job, target);
                        score += getDarkSkyScore(job->getStartupTime().time());
                        score += getWeatherScore(job);
                    }
                    else
                        score += -1000;

                    job->setScore(score);
                    break;
        }

       // appendLogText(i18n("Job total score is %1", score));

        if (score > 0 && job->getState() == SchedulerJob::JOB_EVALUATION)
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

    if (upcomingJobs == 0)
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

        appendLogText(i18n("Scheduler complete. Starting shutdown procedure..."));

        stop();
        return;
    }

    int maxScore=0;
    SchedulerJob *bestCandidate = NULL;

    foreach(SchedulerJob *job, jobs)
    {
        int jobScore = job->getScore();
        if (jobScore > 0 && jobScore > maxScore)
        {
                maxScore = jobScore;
                bestCandidate = job;
        }
    }

    if (bestCandidate != NULL)
    {
        appendLogText(i18n("Found candidate job %1", bestCandidate->getName()));
        //executeJob(bestCandidate);
        currentJob = bestCandidate;
    }
}

bool Scheduler::calculateAltitudeTime(SchedulerJob *job, double minAltitude)
{
    //int DayOffset = 0;
    double altitude=0;
    QDateTime lt( KStarsData::Instance()->lt().date(), QTime() );
    KStarsDateTime ut = geo->LTtoUT( lt );
   // if (ut.time().hour() > 12 )
       // DayOffset = 1;

    SkyPoint target = job->getTargetCoords();

    QTime now = QTime::currentTime();
    double fraction = now.hour() + now.minute()/60.0 + now.second()/3600;
    double rawFrac  = 0;

    for (double hour=fraction; hour < (fraction+24); hour+= 1.0/60.0)
    {
        KStarsDateTime myUT = ut.addSecs(hour * 3600.0);

        rawFrac = (hour > 24 ? (hour - 24) : hour) / 24.0;

        if (rawFrac < Dawn || rawFrac > Dusk)
        {

            dms LST = geo->GSTtoLST( myUT.gst() );
            target.EquatorialToHorizontal( &LST, geo->lat() );
            altitude =  target.alt().Degrees();

            if (altitude > minAltitude)
            {
                QDateTime startTime = geo->UTtoLT(myUT);
                job->setStartupTime(startTime);
                job->setStartupCondition(SchedulerJob::START_AT);                
                return true;
            }

        }

    }

    return false;
}

bool Scheduler::calculateCulmination(SchedulerJob *job)
{
    SkyPoint target = job->getTargetCoords();

    SkyObject o;

    o.setRA0(target.ra0());
    o.setDec0(target.dec0());

    o.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

    QDateTime lt (KStarsData::Instance()->lt().date(), QTime());
    KStarsDateTime dt = geo->LTtoUT(lt);

    QTime transitTime = o.transitTime(dt, geo);

    appendLogText(i18n("Transit time is %1", transitTime.toString()));

    QDateTime observationDateTime(QDate::currentDate(), transitTime.addSecs(-1 * job->getCulminationOffset()* 60));

    appendLogText(i18n("Observation time is %1", observationDateTime.toString()));

    if (getDarkSkyScore(observationDateTime.time()) < 0)
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

int16_t Scheduler::getWeatherScore(SchedulerJob * job)
{
    INDI_UNUSED(job);
    // TODO
    return 0;
}

int16_t Scheduler::getDarkSkyScore(const QTime &observationTime)
{
  //  if (job->getStartingCondition() == SchedulerJob::START_CULMINATION)
    //    return -1000;

    int16_t score=0;
    double dayFraction = 0;

    //if (job->getStartingCondition() == SchedulerJob::START_AT)
        //observationTime = job->getStartupTime().time();

    dayFraction = (observationTime.hour() + observationTime.minute()/60.0 + observationTime.second()/3600.0)/24.0;

    // The farther the target from dawn, the better.
    if (dayFraction < Dawn)
        score += (Dawn - dayFraction) * 100;
    else if (dayFraction > Dusk)
    {
      score += (dayFraction - Dusk) * 100;
    }
    else
      score -= 500;

    appendLogText(i18n("Dark sky score is %1", score));

    return score;
}

int16_t Scheduler::getAltitudeScore(SchedulerJob *job, const SkyPoint &target)
{
    int16_t score=0;
    double currentAlt  = target.alt().Degrees();

    if (currentAlt < 0)
        score -= 1000;
    // If minimum altitude is specified
    else if (job->getMinAltitude() > 0)
    {
        // if current altitude is lower that's not good
        if (currentAlt < job->getMinAltitude())
            score -= 100;
        // Otherwise, adjust score and add current altitude to score weight
        else
            score += 100 + currentAlt;
    }
    // If no minimum altitude, then adjust altitude score to account for current target altitude
    else
        score += (currentAlt - minAltitude->minimum()) * 10.0;

    appendLogText(i18n("Altitude score is %1", score));

    return score;
}

int16_t Scheduler::getMoonSeparationScore(SchedulerJob *job, const SkyPoint &target)
{
    // TODO Add moon phase to the score
    int16_t score=0;

    double mSeparation = moon->angularDistanceTo(&target).Degrees();

    if (job->getMinMoonSeparation() > 0)
    {
        if (mSeparation < job->getMinMoonSeparation())
            score -= 500;
        else
            score += mSeparation / 10.0;
    }
    else
        score += mSeparation / 10.0;

    appendLogText(i18n("Moon separation %1, moon score is %2", mSeparation, score));

    return score;

}

void Scheduler::calculateDawnDusk()
{
    KSAlmanac ksal;
    Dawn = ksal.getDawnAstronomicalTwilight();
    Dusk = ksal.getDuskAstronomicalTwilight();

    QTime now = KStarsData::Instance()->lt().time();

    double fraction = (now.hour() + now.minute()/60.0 + now.second()/3600.0)/24.0;

    appendLogText(i18n("Dawn is %1 Dusk is %2 and current fraction is %3", Dawn, Dusk, fraction));
}

void Scheduler::executeJob(SchedulerJob *job)
{
    currentJob = job;

    currentJob->setState(SchedulerJob::JOB_BUSY);

    // No need to continue evaluating jobs as we already have one.

    disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkStatus()));
    connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkJobStage()));
}

bool    Scheduler::checkEkosState()
{
    switch (ekosState)
    {
        case EKOS_IDLE:
        {
        // Even if state is IDLE, check if Ekos is already started. If not, start it.
        QDBusReply<int> isEkosStarted;
        isEkosStarted = ekosInterface->call(QDBus::AutoDetect,"getEkosStartingStatus");
        if (isEkosStarted.value() == EkosManager::STATUS_SUCCESS)
        {
            ekosState = EKOS_READY;
            return true;
        }
        else
        {
            ekosInterface->call(QDBus::AutoDetect,"start");
            ekosState = EKOS_STARTING;
            return false;
        }
        }
        break;


        case EKOS_STARTING:
        {
            QDBusReply<int> isEkosStarted;
            isEkosStarted = ekosInterface->call(QDBus::AutoDetect,"getEkosStartingStatus");
            if(isEkosStarted.value()== EkosManager::STATUS_SUCCESS)
            {
                appendLogText(i18n("Ekos started."));
                ekosState = EKOS_READY;
                return true;
            }
            else if(isEkosStarted.value()== EkosManager::STATUS_ERROR)
            {
                appendLogText(i18n("Ekos failed to start."));
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
    switch (indiState)
    {
        case INDI_IDLE:
        {
            // Even in idle state, we make sure that INDI is not already connected.
            QDBusReply<int> isINDIConnected = ekosInterface->call(QDBus::AutoDetect,"getINDIConnectionStatus");
            if (isINDIConnected.value()== EkosManager::STATUS_SUCCESS)
            {
                indiState = INDI_READY;
                return true;
            }
            else
            {
                ekosInterface->call(QDBus::AutoDetect,"connectDevices");
                indiState = INDI_CONNECTING;
                return false;
            }
        }
        break;

        case INDI_CONNECTING:
        {
         QDBusReply<int> isINDIConnected = ekosInterface->call(QDBus::AutoDetect,"getINDIConnectionStatus");
        if(isINDIConnected.value()== EkosManager::STATUS_SUCCESS)
        {
            appendLogText(i18n("INDI devices connected."));
            indiState = INDI_READY;
            return true;
        }
        else if(isINDIConnected.value()== EkosManager::STATUS_ERROR)
        {
            appendLogText(i18n("INDI devices failed to connect. Check INDI control panel for details."));

            stop();

            // TODO deal with INDI connection error? Wait until user resolves it? stop scheduler?
           return false;
        }
        else
            return false;
        }
        break;

    case INDI_READY:
        return true;
        break;
    }

    return false;
}

/*bool    Scheduler::checkFITSJobState()
{
    foreach(SchedulerJob *job, jobs)
    {
        if (job->getFITSFile().isEmpty() == false)
        {
            switch (job->getFITSState())
            {
                case SchedulerJob::FITS_IDLE:
                    currentJob = job;
                    startFITSSolving();
                    return false;
                    break;

                case SchedulerJob::FITS_SOLVING:
                {
                    QDBusReply<bool> isSolverComplete, isSolverSuccessful;
                    isSolverComplete = alignInterface->call(QDBus::AutoDetect,"isSolverComplete");
                    if(isSolverComplete.value())
                    {
                        isSolverSuccessful = alignInterface->call(QDBus::AutoDetect,"isSolverSuccessful");
                        if (isSolverSuccessful.value())
                        {
                            getFITSAstrometryResults();
                            currentJob = NULL;
                            return true;
                        }
                        else
                        {

                            currentJob->setFITSState(SchedulerJob::FITS_ERROR);
                            stop();
                            return false;
                        }
                    }
                    else
                        // Still solver in progress, return
                        return false;
                  }
                  break;

                case SchedulerJob::FITS_ERROR:
                    break;

                case SchedulerJob::FITS_COMPLETE:
                    break;
            }
        }
    }

    return true;

}
*/

void Scheduler::checkStatus()
{
    // #1 Check if startup procedure is done
    // TODO

    // #2 Now evaluate jobs and select the best candidate
    if (currentJob == NULL)
    {
        evaluateJobs();
    }
    else
    {
        // #3 Check if Ekos is started
        if (checkEkosState() == false)
            return;

        // #4 Check if INDI devices are connected.
        if (checkINDIState() == false)
            return;

        // #5 Execute the job
        executeJob(currentJob);
    }
}

void Scheduler::checkJobStage()
{
    Q_ASSERT(currentJob != NULL);

    if (currentJob->getCompletionCondition() == SchedulerJob::FINISH_AT)
    {
        // If the job reached it COMPLETION time, we stop it.
        if (currentJob->getCompletionTime().secsTo(KStarsData::Instance()->lt()) <= 0)
        {
            appendLogText(i18n("%1 observation job reached completion time. Stopping...", currentJob->getName()));
            currentJob->setState(SchedulerJob::JOB_COMPLETE);

            stopEkosAction();

            findNextJob();

            return;

        }
    }

    switch(currentJob->getStage())
    {
    case SchedulerJob::STAGE_IDLE:
    {
        QList<QVariant> meridianFlip;
        meridianFlip.append(!currentJob->getNoMeridianFlip());
        ekosInterface->callWithArgumentList(QDBus::AutoDetect,"setMeridianFlip",meridianFlip);
        getNextAction();
    }
        break;

    case SchedulerJob::STAGE_SLEWING:
    {
        QDBusReply<QString> slewStatus = mountInterface->call(QDBus::AutoDetect,"getSlewStatus");
        if(slewStatus.value().toStdString() == "Complete")
        {
            appendLogText(i18n("%1 slew is complete.", currentJob->getName()));
            currentJob->setStage(SchedulerJob::STAGE_SLEW_COMPLETE);
            getNextAction();
            return;
        }
        else if(slewStatus.value().toStdString() == "Error")
        {
            appendLogText(i18n("%1 slew failed!", currentJob->getName()));
            currentJob->setState(SchedulerJob::JOB_ERROR);

            findNextJob();
            return;
        }
    }
        break;

    case SchedulerJob::STAGE_FOCUSING:
    {
        QDBusReply<bool> focusReply = focusInterface->call(QDBus::AutoDetect,"isAutoFocusComplete");
        // Is focus complete?
        if(focusReply.value())
        {
            focusReply = focusInterface->call(QDBus::AutoDetect,"isAutoFocusSuccessful");
            // Is focus successful ?
            if(focusReply.value())
            {
                appendLogText(i18n("%1 focusing is complete.", currentJob->getName()));
                currentJob->setStage(SchedulerJob::STAGE_FOCUS_COMPLETE);
                getNextAction();
                return;
            }
            else
            {
                appendLogText(i18n("%1 focusing failed!", currentJob->getName()));
                currentJob->setState(SchedulerJob::JOB_ERROR);

                findNextJob();
                return;
            }
        }
    }
        break;

    case SchedulerJob::STAGE_ALIGNING:
    {
       QDBusReply<bool> alignReply = alignInterface->call(QDBus::AutoDetect,"isSolverComplete");
       // Is solver complete?
        if(alignReply.value())
        {
            alignReply = alignInterface->call(QDBus::AutoDetect,"isSolverSuccessful");
            // Is solver successful?
            if(alignReply.value())
            {
                appendLogText(i18n("%1 alignment is complete.", currentJob->getName()));

                currentJob->setStage(SchedulerJob::STAGE_ALIGN_COMPLETE);
                getNextAction();
                return;
            }
            else
            {
                appendLogText(i18n("%1 alignment failed!", currentJob->getName()));
                currentJob->setState(SchedulerJob::JOB_ERROR);

                findNextJob();
            }
        }
    }
    break;

    case SchedulerJob::STAGE_CALIBRATING:
    {
        QDBusReply<bool> guideReply = guideInterface->call(QDBus::AutoDetect,"isCalibrationComplete");
        // If calibration stage complete?
        if(guideReply.value())
        {
            guideReply = guideInterface->call(QDBus::AutoDetect,"isCalibrationSuccessful");
            // If calibration successful?
            if(guideReply.value())
            {
                appendLogText(i18n("%1 calibration is complete.", currentJob->getName()));

                guideReply = guideInterface->call(QDBus::AutoDetect,"startGuiding");
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
                currentJob->setState(SchedulerJob::JOB_ERROR);

                findNextJob();
                return;
            }
        }
    }
    break;

    case SchedulerJob::STAGE_CAPTURING:
    {
         QDBusReply<QString> captureReply = captureInterface->call(QDBus::AutoDetect,"getSequenceQueueStatus");
         if(captureReply.value().toStdString()=="Aborted" || captureReply.value().toStdString()=="Error")
         {
             appendLogText(i18n("%1 capture failed!", currentJob->getName()));
             currentJob->setState(SchedulerJob::JOB_ERROR);

             findNextJob();
             return;
         }

         if(captureReply.value().toStdString()=="Complete")
         {
             currentJob->setState(SchedulerJob::JOB_COMPLETE);
             currentJob->setStage(SchedulerJob::STAGE_COMPLETE);
             captureInterface->call(QDBus::AutoDetect,"clearSequenceQueue");
             if (currentJob->getModuleUsage() & SchedulerJob::USE_GUIDE)
                 guideInterface->call(QDBus::AutoDetect,"stopGuiding");

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
    switch(currentJob->getStage())
    {

    case SchedulerJob::STAGE_IDLE:
        startSlew();
        break;

    case SchedulerJob::STAGE_SLEW_COMPLETE:
        if  (currentJob->getModuleUsage() & SchedulerJob::USE_FOCUS)
            startFocusing();
        else if (currentJob->getModuleUsage() & SchedulerJob::USE_ALIGN)
            startAstrometry();
        else if (currentJob->getModuleUsage() & SchedulerJob::USE_GUIDE)
            startCalibrating();
        else
            startCapture();
        break;

    case SchedulerJob::STAGE_FOCUS_COMPLETE:
        if (currentJob->getModuleUsage() & SchedulerJob::USE_ALIGN)
             startAstrometry();
         else if (currentJob->getModuleUsage() & SchedulerJob::USE_GUIDE)
             startCalibrating();
         else
             startCapture();
        break;

    case SchedulerJob::STAGE_ALIGN_COMPLETE:
        if (currentJob->getModuleUsage() & SchedulerJob::USE_GUIDE)
           startCalibrating();
        else
           startCapture();
        break;

    case SchedulerJob::STAGE_GUIDING:
        startCapture();
        break;

     default:
        break;
    }
}

void Scheduler::stopEkosAction()
{
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

    case SchedulerJob::STAGE_CALIBRATING:
        guideInterface->call(QDBus::AutoDetect,"stopCalibration");
    break;

    case SchedulerJob::STAGE_CAPTURING:
        captureInterface->call(QDBus::AutoDetect,"abort");
        break;

    default:
        break;
    }
}

void Scheduler::load()
{
    // TODO
}

void Scheduler::save()
{
    // TODO
}

void Scheduler::startSlew()
{
    Q_ASSERT(currentJob != NULL);

    SkyPoint target = currentJob->getTargetCoords();
    //target.EquatorialToHorizontal(KStarsData::Instance()->lst(), geo->lat());

    QList<QVariant> telescopeSlew;
    telescopeSlew.append(target.ra().Hours());
    telescopeSlew.append(target.dec().Degrees());

    mountInterface->callWithArgumentList(QDBus::AutoDetect,"slew",telescopeSlew);

    currentJob->setStage(SchedulerJob::STAGE_SLEWING);
}

void Scheduler::startFocusing()
{

    QDBusMessage reply;

    // We always need to reset frame first
    if ( (reply = focusInterface->call(QDBus::AutoDetect,"resetFrame")).type() == QDBusMessage::ErrorMessage)
    {
        appendLogText(i18n("resetFrame DBUS error: %1", reply.errorMessage()));
        return;
    }

    // Set focus mode to auto (1)
    QList<QVariant> focusMode;
    focusMode.append(1);

    if ( (reply = focusInterface->callWithArgumentList(QDBus::AutoDetect,"setFocusMode",focusMode)).type() == QDBusMessage::ErrorMessage)
    {
        appendLogText(i18n("setFocusMode DBUS error: %1", reply.errorMessage()));
        return;
    }

    // Set autostar & use subframe
    QList<QVariant> autoStar;
    autoStar.append(true);
    if ( (reply = focusInterface->callWithArgumentList(QDBus::AutoDetect,"setAutoFocusStar",autoStar)).type() == QDBusMessage::ErrorMessage)
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

    currentJob->setStage(SchedulerJob::STAGE_FOCUSING);

}

void Scheduler::findNextJob()
{
    disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkJobStage()));

    // Check completion criteria

    // We're done whether the job completed successfully or not.
    if (currentJob->getCompletionCondition() == SchedulerJob::FINISH_SEQUENCE)
    {
        currentJob->setState(SchedulerJob::JOB_COMPLETE);
        currentJob = NULL;
        connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkStatus()));
        return;
    }

    if (currentJob->getCompletionCondition() == SchedulerJob::FINISH_LOOP)
    {
        currentJob->setState(SchedulerJob::JOB_SCHEDULED);
        currentJob->setStage(SchedulerJob::STAGE_IDLE);
        connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkStatus()));
        return;
    }

    if (currentJob->getCompletionCondition() == SchedulerJob::FINISH_AT)
    {
        if (currentJob->getCompletionTime().secsTo(KStarsData::Instance()->lt()) <= 0)
        {
            appendLogText(i18n("%1 observation job reached completion time. Stopping...", currentJob->getName()));
            currentJob->setState(SchedulerJob::JOB_COMPLETE);
            stopEkosAction();
            currentJob = NULL;
            connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkStatus()));
            return;
        }
        else
        {
            appendLogText(i18n("%1 observation job completed and will restart now...", currentJob->getName()));
            currentJob->setState(SchedulerJob::JOB_SCHEDULED);
            currentJob->setStage(SchedulerJob::STAGE_IDLE);
            connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkStatus()));
            return;
        }
    }
}

void Scheduler::startAstrometry()
{   
    setGOTOMode(Align::ALIGN_SLEW);

    // If FITS file is specified, then we use load and slew
    if (currentJob->getFITSFile().isEmpty() == false)
    {
        QList<QVariant> solveArgs;
        solveArgs.append(currentJob->getFITSFile().toString(QUrl::PreferLocalFile));
        solveArgs.append(false);

        alignInterface->callWithArgumentList(QDBus::AutoDetect,"start",solveArgs);
    }
    else
        alignInterface->call(QDBus::AutoDetect,"captureAndSolve");

    appendLogText(i18n("Solving %1 ...", currentJob->getFITSFile().fileName()));

    currentJob->setStage(SchedulerJob::STAGE_ALIGNING);

}

void Scheduler::startCalibrating()
{        
    // Make sure calibration is auto
    QList<QVariant> guideArgs;
    guideArgs.append(true);
    guideInterface->callWithArgumentList(QDBus::AutoDetect,"setCalibrationAuto",guideArgs);

    QDBusReply<bool> guideReply = guideInterface->call(QDBus::AutoDetect,"startCalibration");
    if (guideReply.value() == false)
        currentJob->setState(SchedulerJob::JOB_ERROR);
    else
        currentJob->setStage(SchedulerJob::STAGE_CALIBRATING);
}

void Scheduler::startCapture()
{
    QString url = currentJob->getSequenceFile().toString(QUrl::PreferLocalFile);

   /*
    // convert to an url
    QRegExp withProtocol(QStringLiteral("^[a-zA-Z]+:"));
    if (withProtocol.indexIn(url) == 0)
    {
        dbusargs.append(QUrl::fromUserInput(url).toString());
    }
    else
    {
        const QString path = QDir::current().absoluteFilePath(url);
        dbusargs.append(QUrl::fromLocalFile(path).toString());
    }*/

    QList<QVariant> dbusargs;
    dbusargs.append(url);
    captureInterface->callWithArgumentList(QDBus::AutoDetect,"loadSequenceQueue",dbusargs);

    captureInterface->call(QDBus::AutoDetect,"start");

    currentJob->setStage(SchedulerJob::STAGE_CAPTURING);

    appendLogText(i18n("%1 capture is in progress...", currentJob->getName()));
}

/*void Scheduler::stopGuiding()
{
    guideInterface->call(QDBus::AutoDetect,"stopGuiding");
}*/

void Scheduler::setGOTOMode(Align::GotoMode mode)
{
    QList<QVariant> solveArgs;
    solveArgs.append(static_cast<int>(mode));
    alignInterface->callWithArgumentList(QDBus::AutoDetect,"setGOTOMode",solveArgs);
}

/*
void Scheduler::startFITSSolving()
{
    currentJob->setFITSState(SchedulerJob::FITS_SOLVING);

    QList<QVariant> astrometryArgs;
    astrometryArgs.append(false);
    alignInterface->callWithArgumentList(QDBus::AutoDetect,"setSolverType",astrometryArgs);
    QList<QVariant> solveArgs;
    solveArgs.append(currentJob->getFITSFile().path());
    solveArgs.append(false);
    setGOTOMode(2);
    alignInterface->callWithArgumentList(QDBus::AutoDetect,"start",solveArgs);

    appendLogText(i18n("Solving %1 ...", currentJob->getFITSFile().fileName()));
}


void Scheduler::getFITSAstrometryResults()
{

    QDBusReply<QList<double>> results = alignInterface->call(QDBus::AutoDetect,"getSolutionResult");

    dms ra(results.value().at(1));
    dms de(results.value().at(2));

    currentJob->setTargetCoords(ra, de);

    currentJob->setFITSState(SchedulerJob::FITS_COMPLETE);

    appendLogText(i18n("%1 FITS solution results are RA: %2 DEC: %3", currentJob->getName(), ra.toHMSString(), de.toDMSString()));
}
*/

void Scheduler::stopINDI()
{
   /* if(iterations==objects.length()){
        state = FINISHED;
        ekosInterface->call(QDBus::AutoDetect,"disconnectDevices");
        ekosInterface->call(QDBus::AutoDetect,"stop");
        pi->stopAnimation();
        disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkJobStatus()));
    }*/
}

}


