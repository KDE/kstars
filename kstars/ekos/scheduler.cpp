/*  Ekos Scheduler Module
    Copyright (C) Jasem Mutlaq <mutlaqja@ikarustech.com>

    Based on GSoC 2015 Ekos Scheduler project by Daniel Leu <daniel_mihai.leu@cti.pub.ro>

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

namespace Ekos
{

Scheduler::Scheduler()
{
    setupUi(this);

    state = Scheduler::IDLE;

    // Set initial time for startup and completion times
    startupTime->setDateTime(QDateTime::currentDateTime());
    completionTime->setDateTime(QDateTime::currentDateTime());

    // Set up DBus interfaces
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Scheduler",  this);
    ekosInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos", "org.kde.kstars.Ekos", QDBusConnection::sessionBus(), this);

    focusInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Focus", "org.kde.kstars.Ekos.Focus", QDBusConnection::sessionBus(),  this);
    captureInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Capture", "org.kde.kstars.Ekos.Capture", QDBusConnection::sessionBus(), this);
    mountInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Mount", "org.kde.kstars.Ekos.Mount", QDBusConnection::sessionBus(), this);
    alignInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Align", "org.kde.kstars.Ekos.Align", QDBusConnection::sessionBus(), this);
    guideInterface = new QDBusInterface("org.kde.kstars", "/KStars/Ekos/Guide", "org.kde.kstars.Ekos.Guide", QDBusConnection::sessionBus(), this);

    pi = new QProgressIndicator(this);
    bottomLayout->addWidget(pi,0,0);

    raBox->setDegType(false); //RA box should be HMS-style

    addToQueueB->setIcon(QIcon::fromTheme("list-add"));
    removeFromQueueB->setIcon(QIcon::fromTheme("list-remove"));
    queueSaveAsB->setIcon(QIcon::fromTheme("document-save"));
    queueLoadB->setIcon(QIcon::fromTheme("document-open"));

    solveFITSB->setIcon(QIcon::fromTheme("games-solve"));
    loadSequenceB->setIcon(QIcon::fromTheme("document-open"));
    selectStartupScriptB->setIcon(QIcon::fromTheme("document-open"));
    selectShutdownScriptB->setIcon(QIcon::fromTheme("document-open"));

    connect(selectObjectB,SIGNAL(clicked()),this,SLOT(selectObject()));
    connect(selectFITSB,SIGNAL(clicked()),this,SLOT(selectFITS()));
    connect(loadSequenceB,SIGNAL(clicked()),this,SLOT(selectSequence()));

    connect(addToQueueB,SIGNAL(clicked()),this,SLOT(addToQueue()));
    connect(removeFromQueueB,SIGNAL(clicked()),this,SLOT(removeFromQueue()));

#if 0


    iterations = 0;
    tableCountRow=0;

    moon = static_cast<SkyPoint*> (KStarsData::Instance()->skyComposite()->findByName("Moon"));
    isStarted=false;




    connect(startButton,SIGNAL(clicked()),this,SLOT(start()));
    connect(queueSaveAsB,SIGNAL(clicked()),this,SLOT(save()));

    connect(solveFITSB,SIGNAL(clicked()),this,SLOT(solveFITS()));
    connect(queueLoadB,SIGNAL(clicked()),this,SLOT(loadScheduler()));



 #endif
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

            appendLogText(xi18n("Object selected. Please select the sequence file."));
        }
    }

    delete fd;

    if (sequenceFile->text().isEmpty() == false)
        addToQueueB->setEnabled(true);
}

void Scheduler::selectFITS()
{
    fitsURL = QFileDialog::getOpenFileUrl(this, xi18n("Open FITS Image"), QString(), "FITS (*.fits *.fit)");
    if (fitsURL.isEmpty())
        return;

    fitsEdit->setText(fitsURL.path());

    raBox->clear();
    decBox->clear();

    solveFITSB->setEnabled(true);

    if (nameEdit->text().isEmpty())
        nameEdit->setText(fitsURL.fileName());

    appendLogText(xi18n("FITS file selected. Please solve FITS to obtain target coordinates."));
}

void Scheduler::selectSequence()
{
    sequenceURL = QFileDialog::getOpenFileUrl(this, xi18n("Open Sequence Queue"), QString(), xi18n("Ekos Sequence Queue (*.esq)"));
    if (sequenceURL.isEmpty())
        return;

    sequenceFile->setText(sequenceURL.path());

    if (raBox->isEmpty() == false && decBox->isEmpty() == false && nameEdit->text().isEmpty() == false)
        addToQueueB->setEnabled(true);
}

void Scheduler::addToQueue()
{
    if(nameEdit->text().isEmpty())
    {
        appendLogText(xi18n("Target name is required."));
        return;
    }

    if (sequenceFile->text().isEmpty())
    {
        appendLogText(xi18n("Sequence file is required."));
        return;
    }

    if (raBox->isEmpty() || decBox->isEmpty())
    {
        // Check first if FITS needs to be solved.
        if (fitsEdit->text().isEmpty() == false)
        {
            appendLogText(xi18n("Please solve the FITS image to obtain target coordinates or add the coordinates manually."));
            return;
        }

        appendLogText(xi18n("Target coordinates are required."));
        return;
    }

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


    // Create a scheduler job

    SchedulerJob *job = new SchedulerJob();
    job->setName(nameEdit->text());
    job->setTargetCoords(ra, dec);

    if (nowConditionR->isChecked())
        job->setStartupCondition(SchedulerJob::START_NOW);
    else if (culminationConditionR->isChecked())
        job->setStartupCondition(SchedulerJob::START_CULMINATION);
    else
    {
        job->setStartupCondition(SchedulerJob::START_AT);
        job->setStartupTime(startupTime->dateTime());
    }

    job->setSequenceFile(sequenceURL);
    if (fitsURL.isEmpty() == false)
        job->setFitsFile(fitsURL);


#if 0

    if(nameEdit->text().isEmpty() == false && sequenceFile->text().isEmpty() == false)
    {
        //Adding to vector
        //Start up
        SchedulerJob newObservation;
        newObservation.setName(nameEdit->text());

        if(fitsEdit->text().isEmpty() == false)
        {
            o = new SkyObject();
            newObservation.setOb(o);
            newObservation.setSolverState(SchedulerJob::TO_BE_SOLVED);
            newObservation.setFITSPath(fitsEdit->text());
            newObservation.setIsFITSSelected(true);
        }
        else
        {
            newObservation.setOb(o);
            newObservation.setRA(raEdit->text());
            newObservation.setDEC(decEdit->text());
            newObservation.setNormalRA(o->ra().Hours());
            newObservation.setNormalDEC(o->dec().Degrees());
        }

        newObservation.setFileName(sequenceFile->text());

        if(startupTimeConditionR->isChecked())
        {
            newObservation.setSpecificTime(true);
            newObservation.setStartTime(startupTime->dateTime());
        }
        else if(nowConditionR->isChecked())
        {
            newObservation.setNowCheck(true);
            //newObservation.setStartTime(QTime::currentTime().toString());
            //newObservation.setHours(QTime::currentTime().hour());
            //newObservation.setMinutes(QTime::currentTime().minute());
        }
        else if(altConditionR->isChecked())
        {
            newObservation.setSpecificAlt(true);
            newObservation.setAlt(startupAlt->value());
        }

        //Constraints
        if(moonSepBox->isChecked())
        {
            newObservation.setMoonSeparationCheck(true);
            newObservation.setMoonSeparation(moonSeparation->value());
        }

        newObservation.setMeridianFlip(!noMeridianFlipCheck->isChecked());

        //Completion
        if(sequenceCompletionR->isChecked())
        {

            newObservation.setCompletionTime(completionTime);

        }
        else if(sequenceCompletionR->isChecked())
            newObservation.setWhenSeqCompCheck(true);
        else if(loopCompletionR->isChecked())
            newObservation.setLoopCheck(true);

        //Shutdown
        /*if(parkCheck->isChecked())
            newObservation.setParkTelescopeCheck(true);
        else newObservation.setParkTelescopeCheck(false);
        if(warmCCDCheck->isChecked())
            newObservation.setWarmCCDCheck(true);
        else newObservation.setWarmCCDCheck(false);
        if(closeDomeCheck->isChecked())
            newObservation.setCloseDomeCheck(true);
        else newObservation.setCloseDomeCheck(false);*/

        //Proceedure
        newObservation.setFocusCheck(focusModuleCheck->isChecked());
        newObservation.setAlignCheck(alignModuleCheck->isChecked());
        newObservation.setGuideCheck(guideModuleCheck->isChecked());

        newObservation.setRowNumber(tableCountRow);

        objects.append(newObservation);

        // Adding to table
        tableCountCol=0;
        const int currentRow = schedulerQueueTable->rowCount();
        schedulerQueueTable->setRowCount(currentRow + 1);

        schedulerQueueTable->setItem(tableCountRow,tableCountCol,new QTableWidgetItem(nameEdit->text()));

        schedulerQueueTable->setItem(tableCountRow,tableCountCol+1,new QTableWidgetItem("Idle"));

        if(OnButton->isChecked())
            schedulerQueueTable->setItem(tableCountRow,tableCountCol+2,new QTableWidgetItem(startupTime->time().toString()));
        else if(NowButton->isChecked())
            schedulerQueueTable->setItem(tableCountRow,tableCountCol+2,new QTableWidgetItem("On Startup"));
        else if(altButton->isChecked())
            schedulerQueueTable->setItem(tableCountRow,tableCountCol+2,new QTableWidgetItem("--"));

        if(onFinButton->isChecked())
            schedulerQueueTable->setItem(tableCountRow,tableCountCol+3,new QTableWidgetItem(completionTime->time().toString()));
        else
            schedulerQueueTable->setItem(tableCountRow,tableCountCol+3,new QTableWidgetItem("--"));

        tableCountRow++;

        raEdit->clear();
        sequenceFile->clear();
        decEdit->clear();
        nameEdit->clear();
        fitsEdit->clear();
        startButton->setEnabled(true);
        solveFITSB->setEnabled(true);
        //if(isFITSSelected)
        //    appendLogText("FITS Selected. Don't forget to solve after the queue selection is complete");
    }
    else
        QMessageBox::information(NULL, "Error", "All fields must be completed.");
#endif
}


#if 0


void Scheduler::connectDevices()
{
    QDBusReply<int> replyconnect = ekosInterface->call(QDBus::AutoDetect,"getINDIConnectionStatus");
    if(replyconnect.value()!=2)
        ekosInterface->call(QDBus::AutoDetect,"connectDevices");
}

void Scheduler::startSlew()
{
    QList<QVariant> telescopeSLew;
    telescopeSLew.append(currentjob->getOb()->ra().Hours());
    telescopeSLew.append(currentjob->getOb()->dec().Degrees());
    mountInterface->callWithArgumentList(QDBus::AutoDetect,"slew",telescopeSLew);
    currentjob->setState(SchedulerJob::SLEWING);
}

void Scheduler::startFocusing()
{
    QList<QVariant> focusMode;
    focusMode.append(1);
    focusInterface->call(QDBus::AutoDetect,"resetFrame");
    focusInterface->callWithArgumentList(QDBus::AutoDetect,"setFocusMode",focusMode);
    QList<QVariant> focusList;
    focusList.append(true);
    focusList.append(true);
    focusInterface->callWithArgumentList(QDBus::AutoDetect,"setAutoFocusOptions",focusList);
    focusInterface->call(QDBus::AutoDetect,"startFocus");
    currentjob->setState(SchedulerJob::FOCUSING);
}

void Scheduler::terminateJob(SchedulerJob *o)
{
    updateJobInfo(currentjob);
    o->setIsOk(1);
    stopGuiding();
    iterations++;
    if(iterations==objects.length())
        state = SHUTDOWN;
    else {
        currentjob = NULL;
        disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkJobStatus()));
        connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(evaluateJobs()));
    }
}

void Scheduler::startAstrometry()
{
    QList<QVariant> astrometryArgs;
    astrometryArgs.append(false);
    alignInterface->callWithArgumentList(QDBus::AutoDetect,"setSolverType",astrometryArgs);
    alignInterface->call(QDBus::AutoDetect,"captureAndSolve");
    currentjob->setState(SchedulerJob::ALIGNING);
    updateJobInfo(currentjob);
}

void Scheduler::startGuiding()
{
    QDBusReply<bool> replyguide;
    QDBusReply<bool> replyguide2;
    QList<QVariant> guideArgs;
    guideArgs.append(true);
    guideArgs.append(true);
    guideArgs.append(true);
    guideInterface->callWithArgumentList(QDBus::AutoDetect,"setCalibrationOptions",guideArgs);
        replyguide = guideInterface->call(QDBus::AutoDetect,"startCalibration");
        if(!replyguide.value())
            currentjob->setState(SchedulerJob::ABORTED);
        else currentjob->setState(SchedulerJob::GUIDING);
        updateJobInfo(currentjob);
}

void Scheduler::startCapture()
{
    QString url = currentjob->getFileName();
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
    currentjob->setState(SchedulerJob::CAPTURING);
}

void Scheduler::executeJob(SchedulerJob *o)
{
    currentjob = o;
    connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkJobStatus()));
    disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(evaluateJobs()));
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

void Scheduler::startSolving()
{
    QList<QVariant> astrometryArgs;
    astrometryArgs.append(false);
    alignInterface->callWithArgumentList(QDBus::AutoDetect,"setSolverType",astrometryArgs);
    QList<QVariant> solveArgs;
    solveArgs.append(currentFITSjob->getFITSPath());
    solveArgs.append(false);
    setGOTOMode(2);
    alignInterface->callWithArgumentList(QDBus::AutoDetect,"startSovling",solveArgs);
    currentFITSjob->setSolverState(SchedulerJob::SOLVING);

}

void Scheduler::checkJobStatus()
{
    if(currentjob==NULL)
        return;

    if(QDate::currentDate().day() >= currentjob->getFinishingDay() &&
            QTime::currentTime().hour() >= currentjob->getFinishingHour() &&
            QTime::currentTime().minute() >= currentjob->getFinishingMinute())
    {
        currentjob->setState(SchedulerJob::ABORTED);
        terminateJob(currentjob);
    }
    QDBusReply<QString> replyslew ;
    QDBusReply<int> replyconnect;
    QDBusReply<bool> replyfocus;
    QDBusReply<bool> replyfocus2;
    QDBusReply<bool> replyalign;
    QDBusReply<bool> replyalign2;
    QDBusReply<QString> replysequence;
    QDBusReply<int> ekosIsStarted;
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
            currentjob->setState(SchedulerJob::ABORTED);
            terminateJob(currentjob);
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
            currentjob->setState(SchedulerJob::ABORTED);
            terminateJob(currentjob);
            return;
        }
        else return;

    default:
        break;
    }


    switch(currentjob->getState())
    {
    case SchedulerJob::IDLE:
        getNextAction();
        break;

    case SchedulerJob::SLEWING:
        replyslew = mountInterface->call(QDBus::AutoDetect,"getSlewStatus");
        if(replyslew.value().toStdString() == "Complete")
        {
            appendLogText("Slewing completed without errors");
            currentjob->setState(SchedulerJob::SLEW_COMPLETE);
            getNextAction();
            return;
        }
        else if(replyslew.value().toStdString() == "Error")
        {
            appendLogText("Error during slewing!");
            currentjob->setState(SchedulerJob::ABORTED);
            terminateJob(currentjob);
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
                currentjob->setState(SchedulerJob::FOCUSING_COMPLETE);
                getNextAction();
                return;
            }
            else {
                appendLogText("Error during focusing!");
                currentjob->setState(SchedulerJob::ABORTED);
                terminateJob(currentjob);
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
                currentjob->setState(SchedulerJob::ALIGNING_COMPLETE);
                getNextAction();
                return;
            }
            else {
                appendLogText("Error during aligning!");
                currentjob->setState(SchedulerJob::ABORTED);
                terminateJob(currentjob);
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
                    currentjob->setState(SchedulerJob::ABORTED);
                    terminateJob(currentjob);
                    return;
                }
                appendLogText("Guiding completed witout error");
                currentjob->setState(SchedulerJob::GUIDING_COMPLETE);
                getNextAction();
                return;
            }
            else {
                appendLogText("Error during calibration!");
                currentjob->setState(SchedulerJob::ABORTED);
                terminateJob(currentjob);
                return;
            }
        }
        break;

    case SchedulerJob::CAPTURING:
         replysequence = captureInterface->call(QDBus::AutoDetect,"getSequenceQueueStatus");
         if(replysequence.value().toStdString()=="Aborted" || replysequence.value().toStdString()=="Error")
         {
             currentjob->setState(SchedulerJob::ABORTED);
             terminateJob(currentjob);
             return;
         }
         if(replysequence.value().toStdString()=="Complete")
         {
             currentjob->setState(SchedulerJob::CAPTURING_COMPLETE);
             updateJobInfo(currentjob);
             captureInterface->call(QDBus::AutoDetect,"clearSequenceQueue");
             terminateJob(currentjob);
             return;
         }
        break;

    default:
        break;
    }
}

void Scheduler::getResults()
{
    QDBusReply<QList<double>> results = alignInterface->call(QDBus::AutoDetect,"getSolutionResult");
    currentFITSjob->setFitsRA(results.value().at(1)*0.067);
    currentFITSjob->setNormalRA(results.value().at(1)*0.067);
    currentFITSjob->setFitsDEC(results.value().at(2));
    currentFITSjob->setNormalDEC(results.value().at(2));
    currentFITSjob->getOb()->setRA(results.value().at(1)*0.067);
    currentFITSjob->getOb()->setDec(results.value().at(2));
}

void Scheduler::getNextAction(){
    switch(currentjob->getState())
    {

    case SchedulerJob::IDLE:
        startSlew();
        updateJobInfo(currentjob);
        break;

    case SchedulerJob::SLEW_COMPLETE:
        if(currentjob->getFocusCheck())
            startFocusing();
        else if(currentjob->getAlignCheck())
            startAstrometry();
        else if(currentjob->getGuideCheck())
            startGuiding();
        else startCapture();
        updateJobInfo(currentjob);
        break;

    case SchedulerJob::FOCUSING_COMPLETE:
        if(currentjob->getAlignCheck())
            startAstrometry();
        else if(currentjob->getGuideCheck())
            startGuiding();
        else startCapture();
        updateJobInfo(currentjob);
        break;

    case SchedulerJob::ALIGNING_COMPLETE:
        if(currentjob->getGuideCheck())
            startGuiding();
        else startCapture();
        updateJobInfo(currentjob);
        break;

    case SchedulerJob::GUIDING_COMPLETE:
        startCapture();
        updateJobInfo(currentjob);
        break;

    }
}

void Scheduler::updateJobInfo(SchedulerJob *o)
{
    if(o->getState()==SchedulerJob::SLEWING)
    {
        schedulerQueueTable->setItem(o->getRowNumber(),tableCountCol+1,new QTableWidgetItem("Slewing"));
    }
    if(o->getState()==SchedulerJob::FOCUSING){
        schedulerQueueTable->setItem(o->getRowNumber(),tableCountCol+1,new QTableWidgetItem("Focusing"));
    }
    if(o->getState()==SchedulerJob::ALIGNING){
        schedulerQueueTable->setItem(o->getRowNumber(),tableCountCol+1,new QTableWidgetItem("Aligning"));
    }
    if(o->getState()==SchedulerJob::GUIDING){
        appendLogText("Guiding");
        schedulerQueueTable->setItem(o->getRowNumber(),tableCountCol+1,new QTableWidgetItem("Guiding"));
    }
    if(o->getState()==SchedulerJob::CAPTURING){
        schedulerQueueTable->setItem(o->getRowNumber(),tableCountCol+1,new QTableWidgetItem("Capturing"));
    }
    if(o->getState()==SchedulerJob::CAPTURING_COMPLETE){
        appendLogText("Completed");
        schedulerQueueTable->setItem(o->getRowNumber(),tableCountCol+1,new QTableWidgetItem("Completed"));
    }
    if(o->getState()==SchedulerJob::ABORTED){
        appendLogText("Job Aborted");
        schedulerQueueTable->setItem(o->getRowNumber(),tableCountCol+1,new QTableWidgetItem("Aborted"));
    }
}

void Scheduler::startEkos()
{
    //Ekos Start
    QDBusReply<int> ekosisstarted = ekosInterface->call(QDBus::AutoDetect,"getEkosStartingStatus");
    if(ekosisstarted.value()!=2)
        ekosInterface->call(QDBus::AutoDetect,"start");

}

void Scheduler::stopindi()
{
    if(iterations==objects.length()){
        state = FINISHED;
        ekosInterface->call(QDBus::AutoDetect,"disconnectDevices");
        ekosInterface->call(QDBus::AutoDetect,"stop");
        pi->stopAnimation();
        disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkJobStatus()));
    }
}


void Scheduler::save()
{
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
}

void Scheduler::start()
{
    if(isStarted){
       disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkJobStatus()));
       disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(evaluateJobs()));
       currentjob->setState(SchedulerJob::IDLE);
       currentjob = NULL;
        state = IDLE;
        ekosInterface->call(QDBus::AutoDetect,"disconnectDevices");
        ekosInterface->call(QDBus::AutoDetect,"stop");
        isStarted=false;
        pi->stopAnimation();
        startButton->setText("Start Scheduler");
        return;
    }
    if(objects.length()==0)
    {
        QMessageBox::information(NULL, "Error", "No objects detected!");
        return;
    }
    int i,ok=0;
    for(i=0;i<objects.length();i++){
        if(objects[i].getSolverState()==SchedulerJob::TO_BE_SOLVED)
            ok=1;
    }
    if(ok==1)
    {
        QMessageBox::information(NULL, "Error", "FITS objects need to be solved first!");
        return;
    }
    pi->startAnimation();
    //startEkos();
    connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(evaluateJobs()));
    startButton->setText("Stop Scheduler");
    isStarted = true;
    iterations=0;
}

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
            currentjob->setState(SchedulerJob::ABORTED);
            terminateJob(currentjob);
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
            currentjob->setState(SchedulerJob::ABORTED);
            terminateJob(currentjob);
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




#endif

}


