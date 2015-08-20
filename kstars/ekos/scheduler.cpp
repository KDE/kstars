/*  Ekos Scheduler Module
    Copyright (C) 2015 Daniel Leu <daniel_mihai.leu@cti.pub.ro>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "Options.h"

#include <KMessageBox>
#include <KLocalizedString>
#include <KPlotting/KPlotWidget>
#include <KPlotting/KPlotObject>
#include <KPlotting/KPlotAxis>
#include <QtDBus>
#include <QFileDialog>
#include <QtXml>

#include "indi/driverinfo.h"
#include "indi/indicommon.h"
#include "indi/clientmanager.h"
#include "indi/indifilter.h"

#include "fitsviewer/fitsviewer.h"
#include "fitsviewer/fitstab.h"
#include "fitsviewer/fitsview.h"
#include "ekosmanager.h"
#include "Options.h"

#include "ksnotify.h"
#include "kstars.h"
#include "kstars.cpp"
#include "focusadaptor.h"

#include "focus.h"

#include <basedevice.h>
#include "scheduler.h"

namespace Ekos
{

Scheduler::Scheduler()
{
    setupUi(this);
    state = Scheduler::IDLE;
    dateTimeEdit->setDateTime(QDateTime::currentDateTime());
    dateTimeEdit_2->setDateTime(QDateTime::currentDateTime());
    iterations = 0;
    tableCountRow=0;
    isFITSSelected = 0;
    moon = &Moon;
    isStarted=false;
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Scheduler",  this);
    focusinterface = new QDBusInterface("org.kde.kstars",
                                                       "/KStars/Ekos/Focus",
                                                       "org.kde.kstars.Ekos.Focus",
                                                       bus,
                                                       this);
    ekosinterface = new QDBusInterface("org.kde.kstars",
                                                       "/KStars/Ekos",
                                                       "org.kde.kstars.Ekos",
                                                       bus,
                                                       this);
    captureinterface = new QDBusInterface("org.kde.kstars",
                                                       "/KStars/Ekos/Capture",
                                                       "org.kde.kstars.Ekos.Capture",
                                                       bus,
                                                       this);
    mountinterface = new QDBusInterface("org.kde.kstars",
                                                       "/KStars/Ekos/Mount",
                                                       "org.kde.kstars.Ekos.Mount",
                                                       bus,
                                                       this);
    aligninterface = new QDBusInterface("org.kde.kstars",
                                                       "/KStars/Ekos/Align",
                                                       "org.kde.kstars.Ekos.Align",
                                                       bus,
                                                       this);
    guideinterface = new QDBusInterface("org.kde.kstars",
                                                       "/KStars/Ekos/Guide",
                                                       "org.kde.kstars.Ekos.Guide",
                                                       bus,
                                                       this);
    pi = new QProgressIndicator(this);
    horizontalLayout_10->addWidget(pi,0,0);
        connect(SelectButton,SIGNAL(clicked()),this,SLOT(selectSlot()));
        connect(addToQueueB,SIGNAL(clicked()),this,SLOT(addToTableSlot()));
        connect(removeFromQueueB,SIGNAL(clicked()),this,SLOT(removeTableSlot()));
        connect(LoadButton,SIGNAL(clicked()),this,SLOT(setSequenceSlot()));
        connect(startButton,SIGNAL(clicked()),this,SLOT(startSlot()));
        connect(queueSaveAsB,SIGNAL(clicked()),this,SLOT(saveSlot()));
        connect(selectFITSButton,SIGNAL(clicked()),this,SLOT(selectFITSSlot()));
        connect(SolveFITSB,SIGNAL(clicked()),this,SLOT(solveFITSSlot()));
        connect(queueLoadB,SIGNAL(clicked()),this,SLOT(loadSlot()));
        addToQueueB->setIcon(QIcon::fromTheme("list-add"));
        removeFromQueueB->setIcon(QIcon::fromTheme("list-remove"));
        queueSaveAsB->setIcon(QIcon::fromTheme("document-save"));
        queueLoadB->setIcon(QIcon::fromTheme("document-open"));
}

void Scheduler::selectFITSSlot(){
    QString path = QFileDialog::getOpenFileName(this, tr("Open File"),
    "",
    tr("Seq Files (*.fits)"));
    QFileInfo finfo(path);
    QString fileName = finfo.fileName();
    NameEdit->setText(fileName);
    FITSEdit->setText(path);
    isFITSSelected = 1;
}

void Scheduler::clearLog(){
    logText.clear();
    emit newLog();
}

void Scheduler::processObjectInfo(XMLEle *root , Schedulerjob *ob){
    XMLEle *ep;
    XMLEle *subEP;

    for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
    {
        qDebug()<<tagXMLEle(ep);
        if(!strcmp(tagXMLEle(ep), "Object")){
            SkyObject *o = new SkyObject();
            subEP = findXMLEle(ep, "isFits");
            if(subEP)
            {
                if(atoi(pcdataXMLEle(subEP))==1)
                    ob->setIsFITSSelected(true);
                else ob->setIsFITSSelected(false);
            }
            subEP = findXMLEle(ep, "RAValue");
            if(subEP)
                ob->setNormalRA(atof(pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "DECValue");
            if(subEP)
                ob->setNormalDEC(atof(pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "Name");
            if (subEP)
                ob->setName(pcdataXMLEle(subEP));
            subEP = findXMLEle(ep, "RA");
            if (subEP)
            {
                ob->setRA(pcdataXMLEle(subEP));
            }
            subEP = findXMLEle(ep, "DEC");
            if (subEP)
            {
                ob->setDEC(pcdataXMLEle(subEP));
            }
            subEP = findXMLEle(ep, "Sequence");
            if (subEP)
            {
               ob->setFileName(pcdataXMLEle(subEP));
            }
            subEP = findXMLEle(ep, "Astrometry");
            if (subEP)
            {
                if(atoi(pcdataXMLEle(subEP))==1)
                    ob->setAlignCheck(true);
                else ob->setAlignCheck(false);
            }
            subEP = findXMLEle(ep, "Guide");
            if (subEP)
            {
                if(atoi(pcdataXMLEle(subEP))==1)
                    ob->setGuideCheck(true);
                else ob->setGuideCheck(false);
            }
            subEP = findXMLEle(ep, "Focus");
            if (subEP)
            {
                if(atoi(pcdataXMLEle(subEP))==1)
                    ob->setFocusCheck(true);
                else ob->setFocusCheck(false);
            }
            subEP = findXMLEle(ep, "NowCheck");
            if (subEP)
            {
                if(atoi(pcdataXMLEle(subEP))==1)
                    ob->setNowCheck(true);
                else ob->setNowCheck(false);
            }
            subEP = findXMLEle(ep, "SpecificTimeCheck");
            if(subEP)
            {
                if(atoi(pcdataXMLEle(subEP))==1)
                    ob->setSpecificTime(true);
                else ob->setSpecificTime(false);
            }
            subEP = findXMLEle(ep, "SpecificTimeValue");
            if(subEP)
            {
                ob->setStartTime(pcdataXMLEle(subEP));
            }
            subEP = findXMLEle(ep, "Hours");
            if(subEP)
            {
                ob->setHours(atoi(pcdataXMLEle(subEP)));
            }
            subEP = findXMLEle(ep, "Minutes");
            if(subEP)
            {
                ob->setMinutes(atoi(pcdataXMLEle(subEP)));
            }
            subEP = findXMLEle(ep, "Day");
            if(subEP)
            {
                ob->setDay(atoi(pcdataXMLEle(subEP)));
            }
            subEP = findXMLEle(ep, "Month");
            if(subEP)
            {
                ob->setMonth(atoi(pcdataXMLEle(subEP)));
            }
            subEP = findXMLEle(ep, "AltCheck");
            if(subEP)
            {
                if(atoi(pcdataXMLEle(subEP))==1)
                    ob->setSpecificAlt(true);
                else ob->setSpecificAlt(false);
            }
            subEP = findXMLEle(ep, "AltValue");
            if(subEP)
            {
                ob->setAlt(atof(pcdataXMLEle(subEP)));
            }
            subEP = findXMLEle(ep, "MoonSepCheck");
            if(subEP)
            {
                if(atoi(pcdataXMLEle(subEP))==1)
                    ob->setMoonSeparationCheck(true);
                else ob->setMoonSeparationCheck(false);
            }
            subEP = findXMLEle(ep, "MoonSepValue");
            if(subEP)
            {
                ob->setMoonSeparation(atof(pcdataXMLEle(subEP)));
            }
            subEP = findXMLEle(ep, "WhenSeqCompletesCheck");
            if(subEP)
            {
                if(atoi(pcdataXMLEle(subEP))==1)
                    ob->setWhenSeqCompCheck(true);
                else ob->setWhenSeqCompCheck(false);
            }
            subEP = findXMLEle(ep, "FinishingTime");
            if(subEP)
            {
                ob->setFinTime(pcdataXMLEle(subEP));
            }
            subEP = findXMLEle(ep, "EndingHour");
            if(subEP)
            {
                ob->setFinishingHour(atoi(pcdataXMLEle(subEP)));
            }
            subEP = findXMLEle(ep, "EndingMinute");
            if(subEP)
            {
                ob->setFinishingMinute(atoi(pcdataXMLEle(subEP)));
            }
            subEP = findXMLEle(ep, "EndingDay");
            if(subEP)
            {
                ob->setFinishingDay(atoi(pcdataXMLEle(subEP)));
            }
            subEP = findXMLEle(ep, "EndingMonth");
            if(subEP)
            {
                ob->setFinishingMonth(atoi(pcdataXMLEle(subEP)));
            }
            o->setRA(ob->getNormalRA());
            o->setDec(ob->getNormalDEC());
            ob->setOb(o);
            ob->setRowNumber(tableCountRow);
            if(ob->getIsFITSSelected())
                ob->setSolverState(Schedulerjob::TO_BE_SOLVED);
            objects.append(*ob);
            tableCountCol=0;
            const int currentRow = tableWidget->rowCount();
            tableWidget->setRowCount(currentRow + 1);
            tableWidget->setItem(tableCountRow,tableCountCol,new QTableWidgetItem(ob->getName()));
            tableWidget->setItem(tableCountRow,tableCountCol+1,new QTableWidgetItem("Idle"));
            if(ob->getSpecificTime())
                tableWidget->setItem(tableCountRow,tableCountCol+2,new QTableWidgetItem(ob->getStartTime()));
            else if(ob->getNowCheck())
                tableWidget->setItem(tableCountRow,tableCountCol+2,new QTableWidgetItem("On Startup"));
            else if(ob->getSpecificAlt())
                tableWidget->setItem(tableCountRow,tableCountCol+2,new QTableWidgetItem("N/S"));

            if(ob->getOnTimeCheck())
                tableWidget->setItem(tableCountRow,tableCountCol+3,new QTableWidgetItem(ob->getFinTime()));
            else
                tableWidget->setItem(tableCountRow,tableCountCol+3,new QTableWidgetItem("N/S"));
            tableCountRow++;
            if(ob->getIsFITSSelected())
                appendLogText("FITS Selected. Don't forget to solve after the queue is selection is complete");
        }
    }
}

void Scheduler::loadSlot(){
    QString path = QFileDialog::getOpenFileName(this, tr("Open File"),
    "",
    tr("Scheduler Files (*.sch)"));
    QFileInfo finfo(path);
    QFile sFile(path);
    if (!sFile.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox::information(NULL, "Error", "Error loading file!");
    }

   // Rxml.readNextStartElement();

    Schedulerjob *obj ;

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
                 if (!strcmp(tagXMLEle(ep), "Object"))
                 {
                      obj = new Schedulerjob();
                      processObjectInfo(root, obj);
                 }


             }
             delXMLEle(root);
        }

    }
    startButton->setEnabled(true);
    SolveFITSB->setEnabled(true);

}

void Scheduler::connectDevices(){
    QDBusReply<int> replyconnect = ekosinterface->call(QDBus::AutoDetect,"getINDIConnectionStatus");
    if(replyconnect.value()!=2)
        ekosinterface->call(QDBus::AutoDetect,"connectDevices");
}

void Scheduler::startSlew(){
    QList<QVariant> telescopeSLew;
    telescopeSLew.append(currentjob->getOb()->ra().Hours());
    telescopeSLew.append(currentjob->getOb()->dec().Degrees());
    mountinterface->callWithArgumentList(QDBus::AutoDetect,"slew",telescopeSLew);
    currentjob->setState(Schedulerjob::SLEWING);
}

void Scheduler::startFocusing(){
    QList<QVariant> focusMode;
    focusMode.append(1);
    focusinterface->call(QDBus::AutoDetect,"resetFrame");
    focusinterface->callWithArgumentList(QDBus::AutoDetect,"setFocusMode",focusMode);
    QList<QVariant> focusList;
    focusList.append(true);
    focusList.append(true);
    focusinterface->callWithArgumentList(QDBus::AutoDetect,"setAutoFocusOptions",focusList);
    focusinterface->call(QDBus::AutoDetect,"startFocus");
    currentjob->setState(Schedulerjob::FOCUSING);
}

void Scheduler::terminateJob(Schedulerjob *o){
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

void Scheduler::startAstrometry(){
    QList<QVariant> astrometryArgs;
    astrometryArgs.append(false);
    aligninterface->callWithArgumentList(QDBus::AutoDetect,"setSolverType",astrometryArgs);
    aligninterface->call(QDBus::AutoDetect,"captureAndSolve");
    currentjob->setState(Schedulerjob::ALIGNING);
    updateJobInfo(currentjob);
}

void Scheduler::startGuiding(){
    QDBusReply<bool> replyguide;
    QDBusReply<bool> replyguide2;
    QList<QVariant> guideArgs;
    guideArgs.append(true);
    guideArgs.append(true);
    guideArgs.append(true);
    guideinterface->callWithArgumentList(QDBus::AutoDetect,"setCalibrationOptions",guideArgs);
        replyguide = guideinterface->call(QDBus::AutoDetect,"startCalibration");
        if(!replyguide.value())
            currentjob->setState(Schedulerjob::ABORTED);
        else currentjob->setState(Schedulerjob::GUIDING);
        updateJobInfo(currentjob);
}

void Scheduler::startCapture(){
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

    captureinterface->callWithArgumentList(QDBus::AutoDetect,"loadSequenceQueue",dbusargs);
    captureinterface->call(QDBus::AutoDetect,"startSequence");
    currentjob->setState(Schedulerjob::CAPTURING);
}

void Scheduler::executeJob(Schedulerjob *o){
    currentjob = o;
    connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkJobStatus()));
    disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(evaluateJobs()));
}

void Scheduler::parkTelescope(){
    state = PARK_TELESCOPE;
}

void Scheduler::warmCCD(){
    state = WARM_CCD;
}

void Scheduler::closeDome(){
    state = CLOSE_DOME;
}

void Scheduler::stopGuiding(){
    guideinterface->call(QDBus::AutoDetect,"stopGuiding");
}

void Scheduler::setGOTOMode(int mode){
    QList<QVariant> solveArgs;
    solveArgs.append(mode);
    aligninterface->callWithArgumentList(QDBus::AutoDetect,"setGOTOMode",solveArgs);
}

void Scheduler::startSolving(){
    QList<QVariant> astrometryArgs;
    astrometryArgs.append(false);
    aligninterface->callWithArgumentList(QDBus::AutoDetect,"setSolverType",astrometryArgs);
    QList<QVariant> solveArgs;
    solveArgs.append(currentFITSjob->getFITSPath());
    solveArgs.append(false);
    setGOTOMode(2);
    aligninterface->callWithArgumentList(QDBus::AutoDetect,"startSovling",solveArgs);
    currentFITSjob->setSolverState(Schedulerjob::SOLVING);

}

void Scheduler::checkJobStatus(){
    if(currentjob==NULL)
        return;
    if(QDate::currentDate().day() >= currentjob->getFinishingDay() &&
            QTime::currentTime().hour() >= currentjob->getFinishingHour() &&
            QTime::currentTime().minute() >= currentjob->getFinishingMinute())
    {
        currentjob->setState(Schedulerjob::ABORTED);
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
        ekosIsStarted = ekosinterface->call(QDBus::AutoDetect,"getEkosStartingStatus");
        if(ekosIsStarted.value()==2)
        {
            appendLogText("Ekos started");
            state = EKOS_STARTED;
        }
        else if(ekosIsStarted.value()==3){
            appendLogText("Ekos start error!");
            currentjob->setState(Schedulerjob::ABORTED);
            terminateJob(currentjob);
            return;
        }

        return;

    case Scheduler::EKOS_STARTED:
        connectDevices();
        state = CONNECTING;
        return;

    case Scheduler::CONNECTING:
        replyconnect = ekosinterface->call(QDBus::AutoDetect,"getINDIConnectionStatus");
        if(replyconnect.value()==2)
        {
            appendLogText("Devices connected");
            state=READY;
            break;
        }
        else if(replyconnect.value()==3)
        {
            appendLogText("Ekos start error!");
            currentjob->setState(Schedulerjob::ABORTED);
            terminateJob(currentjob);
            return;
        }
        else return;

    case Scheduler::SHUTDOWN:
        if(currentjob->getParkTelescopeCheck())
            parkTelescope();
            else if(currentjob->getWarmCCDCheck())
                warmCCD();
            else if(currentjob->getCloseDomeCheck())
                closeDome();
            else stopindi();


    case Scheduler::PARK_TELESCOPE:
        if(currentjob->getWarmCCDCheck())
            warmCCD();
        else if(currentjob->getCloseDomeCheck())
            closeDome();
        else stopindi();

    case Scheduler::WARM_CCD:
        if(currentjob->getCloseDomeCheck())
            closeDome();
        else stopindi();

    case Scheduler::CLOSE_DOME:
        stopindi();
    default:break;
    }


    switch(currentjob->getState())
    {
    case Schedulerjob::IDLE:
        getNextAction();
        break;

    case Schedulerjob::SLEWING:
        replyslew = mountinterface->call(QDBus::AutoDetect,"getSlewStatus");
        if(replyslew.value().toStdString() == "Complete")
        {
            appendLogText("Slewing completed without errors");
            currentjob->setState(Schedulerjob::SLEW_COMPLETE);
            getNextAction();
            return;
        }
        else if(replyslew.value().toStdString() == "Error")
        {
            appendLogText("Error during slewing!");
            currentjob->setState(Schedulerjob::ABORTED);
            terminateJob(currentjob);
            return;
        }
        break;

    case Schedulerjob::FOCUSING:
        replyfocus = focusinterface->call(QDBus::AutoDetect,"isAutoFocusComplete");
        if(replyfocus.value())
        {
            replyfocus2 = focusinterface->call(QDBus::AutoDetect,"isAutoFocusSuccessful");
            if(replyfocus2.value())
            {
                appendLogText("Focusing completed without errors");
                currentjob->setState(Schedulerjob::FOCUSING_COMPLETE);
                getNextAction();
                return;
            }
            else {
                appendLogText("Error during focusing!");
                currentjob->setState(Schedulerjob::ABORTED);
                terminateJob(currentjob);
                return;
            }
        }
        break;

    case Schedulerjob::ALIGNING:
        replyalign = aligninterface->call(QDBus::AutoDetect,"isSolverComplete");
        if(replyalign.value())
        {
            replyalign2 = aligninterface->call(QDBus::AutoDetect,"isSolverSuccessful");
            if(replyalign2.value()){
                appendLogText("Astrometry completed without error");
                currentjob->setState(Schedulerjob::ALIGNING_COMPLETE);
                getNextAction();
                return;
            }
            else {
                appendLogText("Error during aligning!");
                currentjob->setState(Schedulerjob::ABORTED);
                terminateJob(currentjob);
            }
        }

    case Schedulerjob::GUIDING:
        replyguide = guideinterface->call(QDBus::AutoDetect,"isCalibrationComplete");
        if(replyguide.value())
        {
            replyguide2 = guideinterface->call(QDBus::AutoDetect,"isCalibrationSuccessful");
            if(replyguide2.value())
            {
                appendLogText("Calibration completed without error");
                replyguide3 = guideinterface->call(QDBus::AutoDetect,"startGuiding");
                if(!replyguide3.value())
                {
                    appendLogText("Guiding start error!");
                    currentjob->setState(Schedulerjob::ABORTED);
                    terminateJob(currentjob);
                    return;
                }
                appendLogText("Guiding completed witout error");
                currentjob->setState(Schedulerjob::GUIDING_COMPLETE);
                getNextAction();
                return;
            }
            else {
                appendLogText("Error during calibration!");
                currentjob->setState(Schedulerjob::ABORTED);
                terminateJob(currentjob);
                return;
            }
        }
        break;

    case Schedulerjob::CAPTURING:
         replysequence = captureinterface->call(QDBus::AutoDetect,"getSequenceQueueStatus");
         if(replysequence.value().toStdString()=="Aborted" || replysequence.value().toStdString()=="Error")
         {
             currentjob->setState(Schedulerjob::ABORTED);
             terminateJob(currentjob);
             return;
         }
         if(replysequence.value().toStdString()=="Complete")
         {
             currentjob->setState(Schedulerjob::CAPTURING_COMPLETE);
             updateJobInfo(currentjob);
             captureinterface->call(QDBus::AutoDetect,"clearSequenceQueue");
             terminateJob(currentjob);
             return;
         }
        break;

    default:
        break;
    }
}

void Scheduler::getResults(){
    QDBusReply<QList<double>> results = aligninterface->call(QDBus::AutoDetect,"getSolutionResult");
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

    case Schedulerjob::IDLE:
        startSlew();
        updateJobInfo(currentjob);
        break;

    case Schedulerjob::SLEW_COMPLETE:
        if(currentjob->getFocusCheck())
            startFocusing();
        else if(currentjob->getAlignCheck())
            startAstrometry();
        else if(currentjob->getGuideCheck())
            startGuiding();
        else startCapture();
        updateJobInfo(currentjob);
        break;

    case Schedulerjob::FOCUSING_COMPLETE:
        if(currentjob->getAlignCheck())
            startAstrometry();
        else if(currentjob->getGuideCheck())
            startGuiding();
        else startCapture();
        updateJobInfo(currentjob);
        break;

    case Schedulerjob::ALIGNING_COMPLETE:
        if(currentjob->getGuideCheck())
            startGuiding();
        else startCapture();
        updateJobInfo(currentjob);
        break;

    case Schedulerjob::GUIDING_COMPLETE:
        startCapture();
        updateJobInfo(currentjob);
        break;

    }
}

Schedulerjob *Scheduler::getCurrentjob() const
{
    return currentjob;
}

void Scheduler::setCurrentjob(Schedulerjob *value)
{
    currentjob = value;
}


void Scheduler::appendLogText(const QString &text)
{

    logText.insert(0, xi18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    emit newLog();
}

void Scheduler::updateJobInfo(Schedulerjob *o){
    if(o->getState()==Schedulerjob::SLEWING){
        tableWidget->setItem(o->getRowNumber(),tableCountCol+1,new QTableWidgetItem("Slewing"));
    }
    if(o->getState()==Schedulerjob::FOCUSING){
        tableWidget->setItem(o->getRowNumber(),tableCountCol+1,new QTableWidgetItem("Focusing"));
    }
    if(o->getState()==Schedulerjob::ALIGNING){
        tableWidget->setItem(o->getRowNumber(),tableCountCol+1,new QTableWidgetItem("Aligning"));
    }
    if(o->getState()==Schedulerjob::GUIDING){
        appendLogText("Guiding");
        tableWidget->setItem(o->getRowNumber(),tableCountCol+1,new QTableWidgetItem("Guiding"));
    }
    if(o->getState()==Schedulerjob::CAPTURING){
        tableWidget->setItem(o->getRowNumber(),tableCountCol+1,new QTableWidgetItem("Capturing"));
    }
    if(o->getState()==Schedulerjob::CAPTURING_COMPLETE){
        appendLogText("Completed");
        tableWidget->setItem(o->getRowNumber(),tableCountCol+1,new QTableWidgetItem("Completed"));
    }
    if(o->getState()==Schedulerjob::ABORTED){
        appendLogText("Job Aborted");
        tableWidget->setItem(o->getRowNumber(),tableCountCol+1,new QTableWidgetItem("Aborted"));
    }
}


void Scheduler::evaluateJobs()
{
    int i;
    int maxScore=0, index=-1;

    for(i=0;i<objects.length();i++)
    {
            objects[i].getOb()->EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
        if(objects.at(i).getState()==Schedulerjob::IDLE){
            if(objects[i].getNowCheck())
                objects[i].setScore(10);
            if(objects[i].getSpecificTime()){
                if(objects[i].getDay()==QDate::currentDate().day() &&
                        objects[i].getHours()==QTime::currentTime().hour() &&
                        objects.at(i).getMinutes() == QTime::currentTime().minute())
                    objects[i].setScore(10);
                else objects[i].setScore(-1000);
            }
            if(objects.at(i).getSpecificAlt())
            {
                if(objects[i].getIsFITSSelected()){
                    objects[i].getOb()->EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
                    if(objects[i].getOb()->alt().Degrees()>objects.at(i).getAlt())
                        objects[i].setScore(10);
                    else objects[i].setScore(-1000);
                }
                else {
                    if(objects[i].getOb()->alt().Degrees()>objects.at(i).getAlt())
                        objects[i].setScore(10);
                    else objects[i].setScore(-1000);
                }
            }
            objects[i].setScore(objects.at(i).getScore()+objects.at(i).getOb()->alt().Degrees());
            if(objects.at(i).getMoonSeparation()){
                if(objects.at(i).getOb()->angularDistanceTo(moon).Degrees()>objects.at(i).getMoonSeparation() &&
                        objects.at(i).getOb()->alt().Degrees() > objects.at(i).getAlt())
                    objects[i].setScore(objects.at(i).getScore()+10);
                else objects[i].setScore(-1000);
            }
        }
    }
    for(i=0;i<objects.length();i++){
        if(objects.at(i).getScore()>maxScore && objects.at(i).getIsOk()==0)
        {
            maxScore = objects.at(i).getScore();
            index = i;
        }
    }
    if(maxScore>0)
        if(index > -1){
            tableWidget->setItem(index,tableCountCol+1,new QTableWidgetItem("In Progress"));
            executeJob(&objects[index]);
        }
}

void Scheduler::startEkos(){
    //Ekos Start
    QDBusReply<int> ekosisstarted = ekosinterface->call(QDBus::AutoDetect,"getEkosStartingStatus");
    if(ekosisstarted.value()!=2)
        ekosinterface->call(QDBus::AutoDetect,"start");

}

void Scheduler::stopindi(){
    if(iterations==objects.length()){
        state = FINISHED;
        ekosinterface->call(QDBus::AutoDetect,"disconnectDevices");
        ekosinterface->call(QDBus::AutoDetect,"stop");
        pi->stopAnimation();
        disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkJobStatus()));
    }
}


void Scheduler::saveSlot()
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
    foreach(Schedulerjob job, objects)
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

void Scheduler::startSlot()
{
    if(isStarted){
       disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkJobStatus()));
       disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(evaluateJobs()));
       currentjob->setState(Schedulerjob::IDLE);
       currentjob = NULL;
        state = IDLE;
        ekosinterface->call(QDBus::AutoDetect,"disconnectDevices");
        ekosinterface->call(QDBus::AutoDetect,"stop");
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
        if(objects[i].getSolverState()==Schedulerjob::TO_BE_SOLVED)
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

void Scheduler::setSequenceSlot()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
    "",
    tr("Seq Files (*.esq)"));
    FileNameEdit->setText(fileName);
}

void Scheduler::getNextFITSAction(){
    switch(currentFITSjob->getSolverState()){
    case Schedulerjob::TO_BE_SOLVED:
        startSolving();
        break;

    case Schedulerjob::SOLVING_COMPLETE:
        getResults();
        setGOTOMode(1);
        break;
    }
}

void Scheduler::checkFITSStatus(){
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
        ekosIsStarted = ekosinterface->call(QDBus::AutoDetect,"getEkosStartingStatus");
        if(ekosIsStarted.value()==2)
        {
            appendLogText("Ekos started");
            state = EKOS_STARTED;
        }
        else if(ekosIsStarted.value()==3){
            appendLogText("Ekos start error!");
            currentjob->setState(Schedulerjob::ABORTED);
            terminateJob(currentjob);
            return;
        }
        return;

    case Scheduler::EKOS_STARTED:
        connectDevices();
        state = CONNECTING;
        return;

    case Scheduler::CONNECTING:
        replyconnect = ekosinterface->call(QDBus::AutoDetect,"getINDIConnectionStatus");
        if(replyconnect.value()==2)
        {
            appendLogText("Devices connected");
            state=READY;
            break;
        }
        else if(replyconnect.value()==3)
        {
            appendLogText("Ekos start error!");
            currentjob->setState(Schedulerjob::ABORTED);
            terminateJob(currentjob);
            return;
        }
        else return;

    default:
        break;
    }

    QDBusReply<bool> replySolve;

    switch(currentFITSjob->getSolverState()){
    case Schedulerjob::TO_BE_SOLVED:
        getNextFITSAction();
        break;

    case Schedulerjob::SOLVING:
        replySolve = aligninterface->call(QDBus::AutoDetect,"isSolverComplete");
        qDebug()<<replySolve;
        if(replySolve.value()){
            replySolveSuccesful = aligninterface->call(QDBus::AutoDetect,"isSolverSuccessful");
            if(replySolveSuccesful.value()){
                appendLogText("Solver is Successful");
                currentFITSjob->setSolverState(Schedulerjob::SOLVING_COMPLETE);
                getNextFITSAction();
                terminateFITSJob(currentFITSjob);
            }
            else{
                currentFITSjob->setSolverState(Schedulerjob::ERROR);
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

void Scheduler::terminateFITSJob(Schedulerjob *value){
     disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkFITSStatus()));
     int i,ok=0;
     for(i=0;i<objects.length();i++){
         if(objects[i].getSolverState()==Schedulerjob::TO_BE_SOLVED)
             ok=1;
     }
     if(ok==1)
        connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(solveFITSAction()));
     else pi->stopAnimation();
}

void Scheduler::processFITS(Schedulerjob *value){
    currentFITSjob = value;
    disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(solveFITSAction()));
    connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkFITSStatus()));
}

void Scheduler::solveFITSAction(){
    int i;
    for(i=0;i<objects.length();i++)
    {
        if(objects.at(i).getSolverState()==Schedulerjob::TO_BE_SOLVED)
        {
            processFITS(&objects[i]);
        }
    }
}

void Scheduler::solveFITSSlot(){
    int i;
    bool doSolve = false;
    if(objects.length()==0)
    {
        QMessageBox::information(NULL, "Error", "No objects detected!");
        return;
    }
    for(i=0;i<objects.length();i++)
        if(objects.at(i).getSolverState()==Schedulerjob::TO_BE_SOLVED)
            doSolve = true;
    if(!doSolve){
        QMessageBox::information(NULL, "Error", "No FITS object detected!");
        return;
    }
    pi->startAnimation();
    connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(solveFITSAction()));
}

void Scheduler::addToTableSlot()
{
    if(NameEdit->text().length()!=0 && FileNameEdit->text().length()!=0)
    {
        //Adding to vector
        //Start up
        Schedulerjob newOb;
        newOb.setName(NameEdit->text());
        if(isFITSSelected)
        {
            o = new SkyObject();
            newOb.setOb(o);
            newOb.setSolverState(Schedulerjob::TO_BE_SOLVED);
            newOb.setFITSPath(FITSEdit->text());
            newOb.setIsFITSSelected(true);
        }
        else {
            newOb.setOb(o);
            newOb.setRA(RaEdit->text());
            newOb.setDEC(DecEdit->text());
            newOb.setNormalRA(o->ra().Hours());
            newOb.setNormalDEC(o->dec().Degrees());
        }
        newOb.setFileName(FileNameEdit->text());
        if(OnButton->isChecked()){
            newOb.setSpecificTime(true);
            newOb.setStartTime(dateTimeEdit->time().toString());
            newOb.setHours(dateTimeEdit->time().hour());
            newOb.setMinutes(dateTimeEdit->time().minute());
            newOb.setMonth(dateTimeEdit->date().month());
            newOb.setDay(dateTimeEdit->date().day());
        }
        else if(NowButton->isChecked()){
            newOb.setNowCheck(true);
            newOb.setStartTime(QTime::currentTime().toString());
            newOb.setHours(QTime::currentTime().hour());
            newOb.setMinutes(QTime::currentTime().minute());
        }
        else if(altButton->isChecked()){
            newOb.setSpecificAlt(true);
            newOb.setAlt(doubleSpinBox->value());
        }

        //Constraints
        if(moonSepBox->isChecked()){
            newOb.setMoonSeparationCheck(true);
            newOb.setMoonSeparation(doubleSpinBox_3->value());
        }
        else if(merFlipBox->isChecked())
            newOb.setMeridianFlip(true);

        //Completion
        if(onFinButton->isChecked()){
            newOb.setFinishingHour(dateTimeEdit_2->time().hour());
            newOb.setFinishingMinute(dateTimeEdit_2->time().minute());
            newOb.setOnTimeCheck(true);
            newOb.setFinTime(dateTimeEdit_2->time().toString());
            newOb.setFinishingMonth(dateTimeEdit_2->date().month());
            newOb.setFinishingDay(dateTimeEdit_2->date().day());
        }
        else if(seqCompButton->isChecked())
            newOb.setWhenSeqCompCheck(true);
        else if(loopButton->isChecked())
            newOb.setLoopCheck(true);

        //Shutdown
        if(parkCheck->isChecked())
            newOb.setParkTelescopeCheck(true);
        else newOb.setParkTelescopeCheck(false);
        if(warmCCDCheck->isChecked())
            newOb.setWarmCCDCheck(true);
        else newOb.setWarmCCDCheck(false);
        if(closeDomeCheck->isChecked())
            newOb.setCloseDomeCheck(true);
        else newOb.setCloseDomeCheck(false);

        //Proceedure
        if(focusButton->isChecked())
            newOb.setFocusCheck(true);
        else newOb.setFocusCheck(false);

        if(alignCheck->isChecked())
            newOb.setAlignCheck(true);
        else newOb.setAlignCheck(false);

        if(guideCheck->isChecked())
            newOb.setGuideCheck(true);
        else newOb.setGuideCheck(false);

        newOb.setRowNumber(tableCountRow);

        objects.append(newOb);

        //Adding to table
        tableCountCol=0;
        const int currentRow = tableWidget->rowCount();
        tableWidget->setRowCount(currentRow + 1);
        tableWidget->setItem(tableCountRow,tableCountCol,new QTableWidgetItem(NameEdit->text()));
        tableWidget->setItem(tableCountRow,tableCountCol+1,new QTableWidgetItem("Idle"));
        if(OnButton->isChecked())
            tableWidget->setItem(tableCountRow,tableCountCol+2,new QTableWidgetItem(dateTimeEdit->time().toString()));
        else if(NowButton->isChecked())
            tableWidget->setItem(tableCountRow,tableCountCol+2,new QTableWidgetItem("On Startup"));
        else if(altButton->isChecked())
            tableWidget->setItem(tableCountRow,tableCountCol+2,new QTableWidgetItem("N/S"));

        if(onFinButton->isChecked())
            tableWidget->setItem(tableCountRow,tableCountCol+3,new QTableWidgetItem(dateTimeEdit_2->time().toString()));
        else
            tableWidget->setItem(tableCountRow,tableCountCol+3,new QTableWidgetItem("N/S"));
        tableCountRow++;
        RaEdit->clear();
        FileNameEdit->clear();
        DecEdit->clear();
        NameEdit->clear();
        FITSEdit->clear();
        startButton->setEnabled(true);
        SolveFITSB->setEnabled(true);
        if(isFITSSelected)
            appendLogText("FITS Selected. Don't forget to solve after the queue selection is complete");
    }
    else
        QMessageBox::information(NULL, "Error", "All fields must be completed");
}

void Scheduler::removeTableSlot()
{
    if(objects.length()==0)
    {
        QMessageBox::information(NULL, "Error", "No objects detected!");
        return;
    }
    int row = tableWidget->currentRow();
    if (row < 0)
    {
        row = tableWidget->rowCount()-1;
        if (row < 0)
            return;
    }
    tableWidget->removeRow(row);
    objects.remove(objects.at(row).getRowNumber());
    for(int i = row; i<objects.length();i++)
        objects[i].setRowNumber(objects.at(i).getRowNumber()-1);
    tableCountRow--;
    if(tableCountRow==0){
        startButton->setEnabled(false);
        SolveFITSB->setEnabled(false);
    }
}

void Scheduler::selectSlot()
{
    isFITSSelected = 0;
    QPointer<FindDialog> fd = new FindDialog( KStars::Instance() );
    if ( fd->exec() == QDialog::Accepted ) {
        o = fd->selectedObject();
        if( o != 0 ) {
            NameEdit->setText(o->name());
            RaEdit->setText(o->ra().toHMSString());
            DecEdit->setText(o->dec().toDMSString());
            appendLogText("Select the sequence file.");
        }
    }
    delete fd;
}



//Check if weather is good. This works. I won t
//add this to the functionallity for the purpose of testing
int Scheduler::checkWeather()
{
    QEventLoop eventLoop;

            // "quit()" the event-loop, when the network request "finished()"
            QNetworkAccessManager mgr;
            QObject::connect(&mgr, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));

            // the HTTP request
            QString oras="Bucharest";
            QNetworkRequest req( QUrl( QString(("http://api.openweathermap.org/data/2.5/weather?q=%1")).arg(oras) ));
            QNetworkReply *reply = mgr.get(req);
            QTimer::singleShot(5000, &eventLoop, SLOT(quit()));
            eventLoop.exec();
            QString strReply = (QString)reply->readAll();
         //  ui->textEdit->setText(strReply);

            QStringList cloudStatus;
            QJsonDocument jsonResponse = QJsonDocument::fromJson(strReply.toUtf8());
            QJsonObject jsonObject = jsonResponse.object();
            QJsonArray jsonArray = jsonObject["weather"].toArray();

            foreach (const QJsonValue & value, jsonArray) {
                QJsonObject obj = value.toObject();
                cloudStatus.append(obj["description"].toString());
            }
          //  textEdit->setText(cloudStatus[0]);
            int res = QString::compare(cloudStatus[0],"Sky is Clear",Qt::CaseInsensitive);
            if(res == 0)
                return 1;
            else return 0;
}
//
Scheduler::~Scheduler()
{

}

}
