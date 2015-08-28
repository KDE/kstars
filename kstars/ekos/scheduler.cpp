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
}

Scheduler::~Scheduler()
{

}

void Scheduler::appendLogText(const QString &text)
{

    logText.insert(0, xi18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

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
                appendLogText(xi18n("Object selected. Please select the sequence file."));
            else
                addToQueueB->setEnabled(true);

        }
    }

    delete fd;

}

void Scheduler::selectFITS()
{
    fitsURL = QFileDialog::getOpenFileUrl(this, xi18n("Open FITS Image"), QDir::homePath(), "FITS (*.fits *.fit)");
    if (fitsURL.isEmpty())
        return;

    fitsEdit->setText(fitsURL.path());

    raBox->clear();
    decBox->clear();

    if (nameEdit->text().isEmpty())
        nameEdit->setText(fitsURL.fileName());

    if (sequenceEdit->text().isEmpty())
        appendLogText(xi18n("FITS selected. Please select the sequence file."));
    else
        addToQueueB->setEnabled(true);
}

void Scheduler::selectSequence()
{
    sequenceURL = QFileDialog::getOpenFileUrl(this, xi18n("Open Sequence Queue"), QDir::homePath(), xi18n("Ekos Sequence Queue (*.esq)"));
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
        appendLogText(xi18n("Cannot add or modify a job while the scheduler is running."));
        return;
    }

    if(nameEdit->text().isEmpty())
    {
        appendLogText(xi18n("Target name is required."));
        return;
    }

    if (sequenceEdit->text().isEmpty())
    {
        appendLogText(xi18n("Sequence file is required."));
        return;
    }

    // Coordinates are required unless it is a FITS file
    if ( (raBox->isEmpty() || decBox->isEmpty()) && fitsURL.isEmpty())
    {
        appendLogText(xi18n("Target coordinates are required."));
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
            appendLogText(xi18n("RA value %1 is invalid.", raBox->text()));
            return;
        }

        if (decOk == false)
        {
            appendLogText(xi18n("DEC value %1 is invalid.", decBox->text()));
            return;
        }

        job->setTargetCoords(ra, dec);
    }

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
        job->setcompletionTimeEdit(completionTimeEdit->dateTime());
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
        appendLogText(xi18n("Job #%1 changes applied.", currentRow+1));
    }

    startB->setEnabled(true);
}

void Scheduler::editJob(QModelIndex i)
{
    if (state == SCHEDULER_RUNNIG)
    {
        appendLogText(xi18n("Cannot add or modify a job while the scheduler is running."));
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
            completionTimeEdit->setDateTime(job->getcompletionTimeEdit());
            break;
    }

   appendLogText(xi18n("Editing job #%1...", i.row()+1));

   addToQueueB->setIcon(QIcon::fromTheme("svn-update"));

   jobUnderEdit = true;
}

void Scheduler::resetJobEdit()
{
   if (jobUnderEdit)
       appendLogText(xi18n("Editing job canceled."));

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
        jobs.at(i)->setStatusCell(queueTable->item(i, 0));

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
    //disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(evaluateJobs()));

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

    pi->startAnimation();

    //connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(evaluateJobs()));

    startB->setText("Stop Scheduler");

    geo = KStarsData::Instance()->geo();

    calculateDawnDusk();

    state = SCHEDULER_RUNNIG;

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

        // Update target horizontal coords.
        SkyPoint target = job->getTargetCoords();
        target.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

        // #1 Get current altitude, Moon separation, and weather scores.


        // #2 Check startup conditions

        // #2.1 Now?
        switch (job->getStartingCondition())
        {
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
                        if ( (altScore < 0 || darkScore < 0) && calculateAltitudeTime(job, job->getMinAltitude() > 0 ? job->getMinAltitude() : minAltitude->minimum()))
                            return;
                        else
                        {
                            job->setState(SchedulerJob::JOB_INVALID);
                            appendLogText(xi18n("Cannot schedule %1", job->getName()));
                        }
                    }
                    break;

                  case SchedulerJob::START_CULMINATION:
                        if (calculateCulmination(job))
                        {
                            job->setState(SchedulerJob::JOB_SCHEDULED);
                            return;
                        }
                        else
                            job->setState(SchedulerJob::JOB_INVALID);
                        break;

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

       // appendLogText(xi18n("Job total score is %1", score));

        if (score > 0 && job->getState() == SchedulerJob::JOB_EVALUATION)
            job->setState(SchedulerJob::JOB_SCHEDULED);
    }



    int invalidJobs=0;

    // Find invalid jobs
    foreach(SchedulerJob *job, jobs)
    {
        if (job->getState() == SchedulerJob::JOB_INVALID)
            invalidJobs++;
    }

    if (invalidJobs == jobs.count())
    {
        appendLogText(xi18n("No valid jobs found, aborting..."));
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
        appendLogText(xi18n("Found candidate job %1", bestCandidate->getName()));
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
                job->setState(SchedulerJob::JOB_SCHEDULED);
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

    appendLogText(xi18n("Transit time is %1", transitTime.toString()));

    QDateTime observationDateTime(QDate::currentDate(), transitTime.addSecs(-1 * job->getCulminationOffset()* 60));

    appendLogText(xi18n("Observation time is %1", observationDateTime.toString()));

    if (getDarkSkyScore(observationDateTime.time()) < 0)
    {
        appendLogText(xi18n("%1 culminates during the day and cannot be scheduled for observation.", job->getName()));
        return false;
    }

    if (observationDateTime < (static_cast<QDateTime> (KStarsData::Instance()->lt())))
    {
        appendLogText(xi18n("Observation time for %1 already passed.", job->getName()));
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

    appendLogText(xi18n("Dark sky score is %1", score));

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

    appendLogText(xi18n("Altitude score is %1", score));

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

    appendLogText(xi18n("Moon separation %1, moon score is %2", mSeparation, score));

    return score;

}

void Scheduler::calculateDawnDusk()
{
    KSAlmanac ksal;
    Dawn = ksal.getDawnAstronomicalTwilight();
    Dusk = ksal.getDuskAstronomicalTwilight();

    QTime now = KStarsData::Instance()->lt().time();

    double fraction = (now.hour() + now.minute()/60.0 + now.second()/3600.0)/24.0;

    appendLogText(xi18n("Dawn is %1 Dusk is %2 and current fraction is %3", Dawn, Dusk, fraction));
}

void Scheduler::executeJob(SchedulerJob *job)
{
    currentJob = job;

    connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkStatus()));
    //disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(evaluateJobs()));
}

void Scheduler::checkStatus()
{

    // #1 Check if Ekos is started
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
            break;
        }
        else
        {
            ekosInterface->call(QDBus::AutoDetect,"start");
            ekosState = EKOS_STARTING;
            return;
        }
        }
        break;


        case EKOS_STARTING:
        {
            QDBusReply<int> isEkosStarted;
            isEkosStarted = ekosInterface->call(QDBus::AutoDetect,"getEkosStartingStatus");
            if(isEkosStarted.value()== EkosManager::STATUS_SUCCESS)
            {
                appendLogText(xi18n("Ekos started."));
                ekosState = EKOS_READY;
                break;
            }
            else if(isEkosStarted.value()== EkosManager::STATUS_ERROR)
            {
                appendLogText(xi18n("Ekos failed to start."));

                stop();
                // Stop scheduler

                //currentJob->setState(SchedulerJob::ABORTED);
                //terminateJob(currentJob);
            }
            return;
        }

        case EKOS_READY:
        break;
    }

    // #2 Check if INDI devices are connected.
    switch (indiState)
    {
        case INDI_IDLE:
        {
            // Even in idle state, we make sure that INDI is not already connected.
            QDBusReply<int> isINDIConnected = ekosInterface->call(QDBus::AutoDetect,"getINDIConnectionStatus");
            if (isINDIConnected.value()== EkosManager::STATUS_SUCCESS)
            {
                indiState = INDI_READY;
                break;
            }
            else
            {
                ekosInterface->call(QDBus::AutoDetect,"connectDevices");
                indiState = INDI_CONNECTING;
                return;
            }
        }
        break;

        case INDI_CONNECTING:
        {
         QDBusReply<int> isINDIConnected = ekosInterface->call(QDBus::AutoDetect,"getINDIConnectionStatus");
        if(isINDIConnected.value()== EkosManager::STATUS_SUCCESS)
        {
            appendLogText(xi18n("INDI devices connected."));
            indiState = INDI_READY;
            break;
        }
        else if(isINDIConnected.value()== EkosManager::STATUS_ERROR)
        {
            appendLogText(xi18n("INDI devices failed to connect. Check INDI control panel for details."));

            stop();

            // TODO deal with INDI connection error? Wait until user resolves it? stop scheduler?
            //currentJob->setState(SchedulerJob::ABORTED);
            //terminateJob(currentJob);
            return;
        }
        else return;
        }

    case INDI_READY:
        break;
    }


    // #3 Check for any FITS jobs
    foreach(SchedulerJob *job, jobs)
    {
        if (job->getFITSFile().isEmpty() == false)
        {
            switch (job->getFITSState())
            {
                case SchedulerJob::FITS_IDLE:
                    currentJob = job;
                    startFITSSolving();
                    return;

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
                            break;
                        }
                        else
                        {

                            currentJob->setFITSState(SchedulerJob::FITS_ERROR);
                            stop();
                            return;
                        }
                    }
                    else
                        // Still solver in progress, return
                        return;
                  }
                  break;

                case SchedulerJob::FITS_ERROR:
                    // Stop scheduler?
                    break;

                case SchedulerJob::FITS_COMPLETE:
                    break;
            }
        }
    }


    // #4 Now evaluate jobs and select the best candidate
    if (currentJob == NULL)
    {
        evaluateJobs();
        return;
    }

    appendLogText(xi18n("Testing Complete, stopping"));

    stop();

#if 0
    if(currentJob==NULL)
        return;

    // Check if job timeout expired.
    /*if(QDate::currentDate().day() >= currentJob->getFinishingDay() &&
            QTime::currentTime().hour() >= currentJob->getFinishingHour() &&
            QTime::currentTime().minute() >= currentJob->getFinishingMinute())
    {
        currentJob->setState(SchedulerJob::ABORTED);
        terminateJob(currentJob);
    }*/

    QDBusReply<QString> replyslew ;
    QDBusReply<int> replyconnect;
    QDBusReply<bool> replyfocus;
    QDBusReply<bool> replyfocus2;
    QDBusReply<bool> replyalign;
    QDBusReply<bool> replyalign2;
    QDBusReply<QString> replysequence;

    QDBusReply<bool> replyguide;
    QDBusReply<bool> replyguide2;
    QDBusReply<bool> replyguide3;

    switch(state)
    {
    case Scheduler::IDLE:
        startEkos();
        state = STARTING_EKOS;
        return;
    case Scheduler::STARTING_EKOS:
        ekosIsStarted = ekosInterface->call(QDBus::AutoDetect,"getEkosStartingStatus");
        if(ekosIsStarted.value()==2)
        {
            appendLogText("Ekos started");
            state = EKOS_STARTED;
        }
        else if(ekosIsStarted.value()==3){
            appendLogText("Ekos start error!");
            currentJob->setState(SchedulerJob::ABORTED);
            terminateJob(currentJob);
            return;
        }

        return;

    case Scheduler::EKOS_STARTED:
        connectDevices();
        state = CONNECTING;
        return;

    case Scheduler::CONNECTING:
        replyconnect = ekosInterface->call(QDBus::AutoDetect,"getINDIConnectionStatus");
        if(replyconnect.value()==2)
        {
            appendLogText("Devices connected");
            state=READY;
            break;
        }
        else if(replyconnect.value()==3)
        {
            appendLogText("Ekos start error!");
            currentJob->setState(SchedulerJob::ABORTED);
            terminateJob(currentJob);
            return;
        }
        else return;

    default:
        break;
    }


    switch(currentJob->getState())
    {
    case SchedulerJob::IDLE:
        getNextAction();
        break;

    case SchedulerJob::SLEWING:
        replyslew = mountInterface->call(QDBus::AutoDetect,"getSlewStatus");
        if(replyslew.value().toStdString() == "Complete")
        {
            appendLogText("Slewing completed without errors");
            currentJob->setState(SchedulerJob::SLEW_COMPLETE);
            getNextAction();
            return;
        }
        else if(replyslew.value().toStdString() == "Error")
        {
            appendLogText("Error during slewing!");
            currentJob->setState(SchedulerJob::ABORTED);
            terminateJob(currentJob);
            return;
        }
        break;

    case SchedulerJob::FOCUSING:
        replyfocus = focusInterface->call(QDBus::AutoDetect,"isAutoFocusComplete");
        if(replyfocus.value())
        {
            replyfocus2 = focusInterface->call(QDBus::AutoDetect,"isAutoFocusSuccessful");
            if(replyfocus2.value())
            {
                appendLogText("Focusing completed without errors");
                currentJob->setState(SchedulerJob::FOCUSING_COMPLETE);
                getNextAction();
                return;
            }
            else {
                appendLogText("Error during focusing!");
                currentJob->setState(SchedulerJob::ABORTED);
                terminateJob(currentJob);
                return;
            }
        }
        break;

    case SchedulerJob::ALIGNING:
        replyalign = alignInterface->call(QDBus::AutoDetect,"isSolverComplete");
        if(replyalign.value())
        {
            replyalign2 = alignInterface->call(QDBus::AutoDetect,"isSolverSuccessful");
            if(replyalign2.value()){
                appendLogText("Astrometry completed without error");
                currentJob->setState(SchedulerJob::ALIGNING_COMPLETE);
                getNextAction();
                return;
            }
            else {
                appendLogText("Error during aligning!");
                currentJob->setState(SchedulerJob::ABORTED);
                terminateJob(currentJob);
            }
        }

    case SchedulerJob::GUIDING:
        replyguide = guideInterface->call(QDBus::AutoDetect,"isCalibrationComplete");
        if(replyguide.value())
        {
            replyguide2 = guideInterface->call(QDBus::AutoDetect,"isCalibrationSuccessful");
            if(replyguide2.value())
            {
                appendLogText("Calibration completed without error");
                replyguide3 = guideInterface->call(QDBus::AutoDetect,"startGuiding");
                if(!replyguide3.value())
                {
                    appendLogText("Guiding start error!");
                    currentJob->setState(SchedulerJob::ABORTED);
                    terminateJob(currentJob);
                    return;
                }
                appendLogText("Guiding completed witout error");
                currentJob->setState(SchedulerJob::GUIDING_COMPLETE);
                getNextAction();
                return;
            }
            else {
                appendLogText("Error during calibration!");
                currentJob->setState(SchedulerJob::ABORTED);
                terminateJob(currentJob);
                return;
            }
        }
        break;

    case SchedulerJob::CAPTURING:
         replysequence = captureInterface->call(QDBus::AutoDetect,"getSequenceQueueStatus");
         if(replysequence.value().toStdString()=="Aborted" || replysequence.value().toStdString()=="Error")
         {
             currentJob->setState(SchedulerJob::ABORTED);
             terminateJob(currentJob);
             return;
         }
         if(replysequence.value().toStdString()=="Complete")
         {
             currentJob->setState(SchedulerJob::CAPTURING_COMPLETE);
             updateJobInfo(currentJob);
             captureInterface->call(QDBus::AutoDetect,"clearSequenceQueue");
             terminateJob(currentJob);
             return;
         }
        break;

    default:
        break;
    }

#endif
}

void Scheduler::getNextAction()
{
#if 0
    switch(currentJob->getState())
    {

    case SchedulerJob::IDLE:
        startSlew();
        updateJobInfo(currentJob);
        break;

    case SchedulerJob::SLEW_COMPLETE:
        if(currentJob->getFocusCheck())
            startFocusing();
        else if(currentJob->getAlignCheck())
            startAstrometry();
        else if(currentJob->getGuideCheck())
            startGuiding();
        else startCapture();
        updateJobInfo(currentJob);
        break;

    case SchedulerJob::FOCUSING_COMPLETE:
        if(currentJob->getAlignCheck())
            startAstrometry();
        else if(currentJob->getGuideCheck())
            startGuiding();
        else startCapture();
        updateJobInfo(currentJob);
        break;

    case SchedulerJob::ALIGNING_COMPLETE:
        if(currentJob->getGuideCheck())
            startGuiding();
        else startCapture();
        updateJobInfo(currentJob);
        break;

    case SchedulerJob::GUIDING_COMPLETE:
        startCapture();
        updateJobInfo(currentJob);
        break;

    }
#endif
}


void Scheduler::load()
{
    // TODO
}

void Scheduler::save()
{
#if 0
    if(objects.length()==0){
        QMessageBox::information(NULL, "Error", "No objects detected!");
        return;
    }

    QFile file("SchedulerQueue.sch");
    file.open(QIODevice::ReadWrite | QIODevice::Text);


    QTextStream outstream(&file);

    outstream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;

    outstream << "<Observable>" << endl;
    foreach(SchedulerJob job, objects)
    {

         outstream << "<Object>" << endl;
         outstream << "<Astrometry>" << job.getAlignCheck() << "</Astrometry>";
         outstream << "<Guide>" << job.getGuideCheck() << "</Guide>";
         outstream << "<Focus>" << job.getFocusCheck() << "</Focus>";
         outstream << "<isFits>" << job.getIsFITSSelected() << "</isFits>";
         outstream << "<Name>" << job.getName() << "</Name>";
         outstream << "<RAValue>" << job.getNormalRA() << "</RAValue>";
         outstream << "<DECValue>" << job.getNormalDEC() << "</DECValue>";
         outstream << "<RA>" << job.getRA() << "</RA>";
         outstream << "<DEC>" << job.getDEC() << "</DEC>";
         outstream << "<Sequence>" << job.getFileName() << "</Sequence>";
         outstream << "<NowCheck>" << job.getNowCheck() << "</NowCheck>";
         outstream << "<SpecificTimeCheck>" << job.getSpecificTime() << "</SpecificTimeCheck>";
         outstream << "<SpecificTimeValue>" << job.getStartTime() << "</SpecificTimeValue>";
         outstream << "<Hours>" << job.getHours() << "</Hours>";
         outstream << "<Minutes>" << job.getMinutes() << "</Minutes>";
         outstream << "<Day>" << job.getDay() << "</Day>";
         outstream << "<Month>" << job.getMonth() << "</Month>";
         outstream << "<AltCheck>" << job.getSpecificAlt() << "</AltCheck>";
         outstream << "<AltValue>" << job.getAlt() << "</AltValue>";
         outstream << "<MoonSepCheck>" << job.getMoonSeparationCheck() << "</MoonSepCheck>";
         outstream << "<MoonSepValue>" << job.getMoonSeparation() << "</MoonSepValue>";
         outstream << "<WhenSeqCompletesCheck>" << job.getWhenSeqCompCheck() << "</WhenSeqCompletesCheck>";
         outstream << "<FinishingTime>" <<job.getFinTime() << "</FinishingTime>";
         outstream << "<EndOnSpecTime>" << job.getOnTimeCheck() << "</EndOnSpecTime>";
         outstream << "<EndingHour>" << job.getFinishingHour() << "</EndingHour>";
         outstream << "<EndingMinute>" << job.getFinishingMinute() << "</EndingMinute>";
         outstream << "<EndingDay>" << job.getFinishingDay() << "</EndingDay>";
         outstream << "<EndingMonth>" << job.getFinishingMonth() << "</EndingMonth>";
        outstream << "</Object>" << endl;
    }

    outstream << "</Observable>" << endl;
    file.close();
#endif
}





void Scheduler::startSlew()
{
   /* QList<QVariant> telescopeSLew;
    telescopeSLew.append(currentJob->getOb()->ra().Hours());
    telescopeSLew.append(currentJob->getOb()->dec().Degrees());
    mountInterface->callWithArgumentList(QDBus::AutoDetect,"slew",telescopeSLew);
    currentJob->setState(SchedulerJob::SLEWING);*/
}

void Scheduler::startFocusing()
{
    /*
    QList<QVariant> focusMode;
    focusMode.append(1);
    focusInterface->call(QDBus::AutoDetect,"resetFrame");
    focusInterface->callWithArgumentList(QDBus::AutoDetect,"setFocusMode",focusMode);
    QList<QVariant> focusList;
    focusList.append(true);
    focusList.append(true);
    focusInterface->callWithArgumentList(QDBus::AutoDetect,"setAutoFocusOptions",focusList);
    focusInterface->call(QDBus::AutoDetect,"startFocus");
    currentJob->setState(SchedulerJob::FOCUSING);
    */
}

/*void Scheduler::terminateJob(SchedulerJob *o)
{
    updateJobInfo(currentJob);
    o->setIsOk(1);
    stopGuiding();
    iterations++;
    if(iterations==objects.length())
        state = SHUTDOWN;
    else {
        currentJob = NULL;
        disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkJobStatus()));
        connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(evaluateJobs()));
    }
}*/

void Scheduler::startAstrometry()
{
    /*
    QList<QVariant> astrometryArgs;
    astrometryArgs.append(false);
    alignInterface->callWithArgumentList(QDBus::AutoDetect,"setSolverType",astrometryArgs);
    alignInterface->call(QDBus::AutoDetect,"captureAndSolve");
    currentJob->setState(SchedulerJob::ALIGNING);
    updateJobInfo(currentJob);
    */
}

void Scheduler::startGuiding()
{
    /*
    QDBusReply<bool> replyguide;
    QDBusReply<bool> replyguide2;
    QList<QVariant> guideArgs;
    guideArgs.append(true);
    guideArgs.append(true);
    guideArgs.append(true);
    guideInterface->callWithArgumentList(QDBus::AutoDetect,"setCalibrationOptions",guideArgs);
        replyguide = guideInterface->call(QDBus::AutoDetect,"startCalibration");
        if(!replyguide.value())
            currentJob->setState(SchedulerJob::ABORTED);
        else currentJob->setState(SchedulerJob::GUIDING);
        updateJobInfo(currentJob);

        */
}

void Scheduler::startCapture()
{
    /*
    QString url = currentJob->getFileName();
    QList<QVariant> dbusargs;
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
    }

    captureInterface->callWithArgumentList(QDBus::AutoDetect,"loadSequenceQueue",dbusargs);
    captureInterface->call(QDBus::AutoDetect,"startSequence");
    currentJob->setState(SchedulerJob::CAPTURING);
    */
}

void Scheduler::stopGuiding()
{
    guideInterface->call(QDBus::AutoDetect,"stopGuiding");
}

void Scheduler::setGOTOMode(int mode)
{
    QList<QVariant> solveArgs;
    solveArgs.append(mode);
    alignInterface->callWithArgumentList(QDBus::AutoDetect,"setGOTOMode",solveArgs);
}

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
    alignInterface->callWithArgumentList(QDBus::AutoDetect,"startSovling",solveArgs);

    appendLogText(xi18n("Solving %1 ...", currentJob->getFITSFile().fileName()));
}


void Scheduler::getFITSAstrometryResults()
{

    QDBusReply<QList<double>> results = alignInterface->call(QDBus::AutoDetect,"getSolutionResult");

    dms ra(results.value().at(1));
    dms de(results.value().at(2));

    currentJob->setTargetCoords(ra, de);

    currentJob->setFITSState(SchedulerJob::FITS_COMPLETE);

    appendLogText(xi18n("%1 FITS solution results are RA: %2 DEC: %3", currentJob->getName(), ra.toHMSString(), de.toDMSString()));
}




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



/*
void Scheduler::getNextFITSAction()
{
    switch(currentFITSjob->getSolverState())
    {
    case SchedulerJob::TO_BE_SOLVED:
        startSolving();
        break;

    case SchedulerJob::SOLVING_COMPLETE:
        getResults();
        setGOTOMode(1);
        break;
    }
}

void Scheduler::checkFITSStatus()
{
    QDBusReply<bool> ekosIsStarted;
    QDBusReply<bool> replyconnect;
    QDBusReply<bool> replySolveSuccesful;
    if(currentFITSjob == NULL)
        return;
    switch(state)
    {
    case Scheduler::IDLE:
        startEkos();
        state = STARTING_EKOS;
        return;
    case Scheduler::STARTING_EKOS:
        ekosIsStarted = ekosInterface->call(QDBus::AutoDetect,"getEkosStartingStatus");
        if(ekosIsStarted.value()==2)
        {
            appendLogText("Ekos started");
            state = EKOS_STARTED;
        }
        else if(ekosIsStarted.value()==3){
            appendLogText("Ekos start error!");
            currentJob->setState(SchedulerJob::ABORTED);
            terminateJob(currentJob);
            return;
        }
        return;

    case Scheduler::EKOS_STARTED:
        connectDevices();
        state = CONNECTING;
        return;

    case Scheduler::CONNECTING:
        replyconnect = ekosInterface->call(QDBus::AutoDetect,"getINDIConnectionStatus");
        if(replyconnect.value()==2)
        {
            appendLogText("Devices connected");
            state=READY;
            break;
        }
        else if(replyconnect.value()==3)
        {
            appendLogText("Ekos start error!");
            currentJob->setState(SchedulerJob::ABORTED);
            terminateJob(currentJob);
            return;
        }
        else return;

    default:
        break;
    }

    QDBusReply<bool> replySolve;

    switch(currentFITSjob->getSolverState()){
    case SchedulerJob::TO_BE_SOLVED:
        getNextFITSAction();
        break;

    case SchedulerJob::SOLVING:
        replySolve = alignInterface->call(QDBus::AutoDetect,"isSolverComplete");
        qDebug()<<replySolve;
        if(replySolve.value()){
            replySolveSuccesful = alignInterface->call(QDBus::AutoDetect,"isSolverSuccessful");
            if(replySolveSuccesful.value()){
                appendLogText("Solver is Successful");
                currentFITSjob->setSolverState(SchedulerJob::SOLVING_COMPLETE);
                getNextFITSAction();
                terminateFITSJob(currentFITSjob);
            }
            else{
                currentFITSjob->setSolverState(SchedulerJob::ERROR);
                appendLogText("Solver encountered an error");
                terminateFITSJob(currentFITSjob);
            }
            return;
        }
        break;
    default:
        break;
    }

}

void Scheduler::terminateFITSJob(SchedulerJob *value){
     disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkFITSStatus()));
     int i,ok=0;
     for(i=0;i<objects.length();i++){
         if(objects[i].getSolverState()==SchedulerJob::TO_BE_SOLVED)
             ok=1;
     }
     if(ok==1)
        connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(solveFITSAction()));
     else pi->stopAnimation();
}

void Scheduler::processFITS(SchedulerJob *value){
    currentFITSjob = value;
    disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(solveFITSAction()));
    connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkFITSStatus()));
}

void Scheduler::solveFITSAction(){
    int i;
    for(i=0;i<objects.length();i++)
    {
        if(objects.at(i).getSolverState()==SchedulerJob::TO_BE_SOLVED)
        {
            processFITS(&objects[i]);
        }
    }
}

void Scheduler::solveFITS(){
    int i;
    bool doSolve = false;
    if(objects.length()==0)
    {
        QMessageBox::information(NULL, "Error", "No objects detected!");
        return;
    }
    for(i=0;i<objects.length();i++)
        if(objects.at(i).getSolverState()==SchedulerJob::TO_BE_SOLVED)
            doSolve = true;
    if(!doSolve){
        QMessageBox::information(NULL, "Error", "No FITS object detected!");
        return;
    }
    pi->startAnimation();
    connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(solveFITSAction()));
}


void Scheduler::removeFromQueue()
{
    if(objects.length()==0)
    {
        QMessageBox::information(NULL, "Error", "No objects detected!");
        return;
    }
    int row = schedulerQueueTable->currentRow();
    if (row < 0)
    {
        row = schedulerQueueTable->rowCount()-1;
        if (row < 0)
            return;
    }
    schedulerQueueTable->removeRow(row);
    objects.remove(objects.at(row).getRowNumber());
    for(int i = row; i<objects.length();i++)
        objects[i].setRowNumber(objects.at(i).getRowNumber()-1);
    tableCountRow--;
    if(tableCountRow==0){
        startButton->setEnabled(false);
        solveFITSB->setEnabled(false);
    }
}

*/


}


