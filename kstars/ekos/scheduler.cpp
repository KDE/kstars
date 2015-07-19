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
    iterations = 0;
    tableCountRow=0;
    moon = &Moon;
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
    pi = new QProgressIndicator(this);
    horizontalLayout_10->addWidget(pi,0,0);
       // connect(SelectButton,SIGNAL(clicked()),this,SLOT(newslot()));
        connect(SelectButton,SIGNAL(clicked()),this,SLOT(selectSlot()));
        connect(addToQueueB,SIGNAL(clicked()),this,SLOT(addToTableSlot()));
        connect(removeFromQueueB,SIGNAL(clicked()),this,SLOT(removeTableSlot()));
        connect(LoadButton,SIGNAL(clicked()),this,SLOT(setSequenceSlot()));
        connect(startButton,SIGNAL(clicked()),this,SLOT(startSlot()));
        connect(queueSaveAsB,SIGNAL(clicked()),this,SLOT(saveSlot()));
        connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(stopindi()));
}

void Scheduler::connectDevices(){
    QDBusReply<bool> replyconnect = ekosinterface->call(QDBus::AutoDetect,"isConnected");
    if(!replyconnect.value())
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
    o->setIsOk(1);

    currentjob = NULL;
    iterations++;
    connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(evaluateJobs()));
}

void Scheduler::startAstrometry(){

}

void Scheduler::startGuiding(){

}

void Scheduler::startCapture(){
    QUrl filename = currentjob->getFileName();
    QList<QVariant> loadsequence;
    loadsequence.append(filename);
    captureinterface->callWithArgumentList(QDBus::AutoDetect,"loadSequenceQueue",loadsequence);
    captureinterface->call(QDBus::AutoDetect,"startSequence");
    currentjob->setState(Schedulerjob::CAPTURING);
}

void Scheduler::executeJob(Schedulerjob *o){
    currentjob = o;
    connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkJobStatus()));
    disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(evaluateJobs()));
    //    if(iterations==objects.length())
//        stopindi();
}

void Scheduler::checkJobStatus(){
    if(currentjob==NULL)
        return;
//    if(QTime::currentTime().hour() >= currentjob->getFinishingHour() && QTime::currentTime().minute() >= currentjob->getFinishingMinute())
//    {
//        terminateJob(currentjob);
//        return;
//    }
    QDBusReply<QString> replyslew ;
    QDBusReply<bool> replyconnect;
    QDBusReply<bool> replyfocus;
    QDBusReply<bool> replyfocus2;
    QDBusReply<QString> replysequence;
    QDBusReply<bool> ekosIsStarted;

    switch(state)
    {
    case Scheduler::IDLE:
        startEkos();
        state = STARTING_EKOS;
        return;
    case Scheduler::STARTING_EKOS:
        ekosIsStarted = ekosinterface->call(QDBus::AutoDetect,"isStarted");
        if(ekosIsStarted.value())
        {
            state = EKOS_STARTED;
        }
        return;

    case Scheduler::EKOS_STARTED:
        connectDevices();
        state = CONNECTING;
        return;

    case Scheduler::CONNECTING:
        replyconnect = ekosinterface->call(QDBus::AutoDetect,"isConnected");
        if(replyconnect.value())
        {
            state=READY;
            break;
        }
        else return;
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
            currentjob->setState(Schedulerjob::SLEW_COMPLETE);
            getNextAction();
            return;
        }
        else if(replyslew.value().toStdString() == "Error")
        {
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
                currentjob->setState(Schedulerjob::FOCUSING_COMPLETE);
                getNextAction();
                return;
            }
            else {
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
              QMessageBox::information(NULL, "Success", "CAPTURING");
             currentjob->setState(Schedulerjob::CAPTURING_COMPLETE);
             captureinterface->call(QDBus::AutoDetect,"clearSequenceQueue");
             terminateJob(currentjob);
             disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(checkJobStatus()));
             return;
         }
        break;
    }
}

void Scheduler::getNextAction(){
    switch(currentjob->getState())
    {

    case Schedulerjob::IDLE:
        startSlew();
        break;

    case Schedulerjob::SLEW_COMPLETE:
        if(currentjob->getFocusCheck())
            startFocusing();
        else if(currentjob->getAlignCheck())
            startAstrometry();
        else if(currentjob->getGuideCheck())
            startGuiding();
        else startCapture();
        break;

    case Schedulerjob::FOCUSING_COMPLETE:
        if(currentjob->getAlignCheck())
            startAstrometry();
        else if(currentjob->getGuideCheck())
            startGuiding();
        else startCapture();
        break;

    case Schedulerjob::ALIGNING_COMPLETE:
        if(currentjob->getGuideCheck())
            startGuiding();
        else startCapture();
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
        appendLogText("Telescope slewing");
        tableWidget->setItem(o->getRowNumber(),tableCountCol+1,new QTableWidgetItem("Slewing"));
    }
    if(o->getState()==Schedulerjob::FOCUSING){
        appendLogText("Focuser started");
        tableWidget->setItem(o->getRowNumber(),tableCountCol+1,new QTableWidgetItem("Focusing"));
    }
    if(o->getState()==Schedulerjob::CAPTURING){
        appendLogText("Sequence in progress");
        tableWidget->setItem(o->getRowNumber(),tableCountCol+1,new QTableWidgetItem("Capturing"));
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
        if(objects.at(i).getState()==Schedulerjob::IDLE){
            if(objects[i].getNowCheck())
                objects[i].setScore(10);
            if(objects[i].getSpecificTime()){
                if(objects[i].getHours()==QTime::currentTime().hour() && objects.at(i).getMinutes() == QTime::currentTime().minute())
                    objects[i].setScore(10);
                else objects[i].setScore(-1000);
            }
            if(objects.at(i).getSpecificAlt())
            {
                if(objects[i].getOb()->alt().Degrees()>objects.at(i).getAlt())
                    objects[i].setScore(10);
                else objects[i].setScore(-1000);
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
           // objects[index].setIsOk(1);
            executeJob(&objects[index]);
        }
}

void Scheduler::startEkos(){
    //Ekos Start
    QDBusReply<bool> ekosisstarted = ekosinterface->call(QDBus::AutoDetect,"isStarted");
    if(!ekosisstarted.value())
        ekosinterface->call(QDBus::AutoDetect,"start");

}

void Scheduler::stopindi(){
    if(iterations==objects.length()){
        QDBusConnection bus = QDBusConnection::sessionBus();
        QDBusInterface *ekosinterface = new QDBusInterface("org.kde.kstars",
                                                       "/KStars/Ekos",
                                                       "org.kde.kstars.Ekos",
                                                       bus,
                                                       this);
        ekosinterface->call(QDBus::AutoDetect,"stop");
        pi->stopAnimation();
        disconnect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(stopindi()));
    }
}


//void Scheduler::processSession(Schedulerjob o){
//    QUrl filename = o.getFileName();
//    //Dbus


//    //Telescope and CCD

//    ekosinterface->call(QDBus::AutoDetect,"connectDevices");

//    //waiting for connection
//    int timer=0;
//    bool check = true;
//    while(true){
//        QDBusReply<bool> replyconnect = ekosinterface->call(QDBus::AutoDetect,"isConnected");
//        if(!replyconnect.value())
//        {
//            if(o.getOnTimeCheck())
//            {
//                if(QTime::currentTime().hour() >= o.getFinishingHour() && QTime::currentTime().minute() >= o.getFinishingMinute())
//                    return;
//            }
//            QEventLoop loop2;
//            QTimer::singleShot(1000, &loop2, SLOT(quit()));
//            loop2.exec();
//            timer = timer+1;
//            if(timer == 20)
//            {
//                check = false;
//                timer = 0;
//                break;
//            }
//        }
//        else break;
//    }
//    if(!check){
//        o.setState(Schedulerjob::ABORTED);
//        updateJobInfo(&o);
//        return;
//    }
//    QList<QVariant> telescopeSLew;
//    telescopeSLew.append(o.getOb()->ra().Hours());
//    telescopeSLew.append(o.getOb()->dec().Degrees());
//    mountinterface->callWithArgumentList(QDBus::AutoDetect,"slew",telescopeSLew);
//    o.setState(Schedulerjob::SLEWING);
//    updateJobInfo(&o);
//    //Waiting for slew
//    while(true){
//        QDBusReply<QString> replyslew = mountinterface->call(QDBus::AutoDetect,"getSlewStatus");
//        if(replyslew.value().toStdString()=="Error")
//        {
//            o.setState(Schedulerjob::ABORTED);
//            updateJobInfo(&o);
//            return;
//        }
//        if(replyslew.value().toStdString()!="Complete")
//        {
//            if(o.getOnTimeCheck())
//            {
//                if(QTime::currentTime().hour() >= o.getFinishingHour() && QTime::currentTime().minute() >= o.getFinishingMinute())
//                    return;
//            }
//            QEventLoop loop2;
//            QTimer::singleShot(1000, &loop2, SLOT(quit()));
//            loop2.exec();
//        }
//        else break;
//    }
//    //Focusing - optional
//    if(o.getFocusCheck())
//    {
//        QList<QVariant> focusMode;
//        focusMode.append(1);
//        focusinterface->callWithArgumentList(QDBus::AutoDetect,"setFocusMode",focusMode);
//        QList<QVariant> focusList;
//        focusList.append(true);
//        focusList.append(true);
//        focusinterface->callWithArgumentList(QDBus::AutoDetect,"setAutoFocusOptions",focusList);
//        focusinterface->call(QDBus::AutoDetect,"startFocus");
//        o.setState(Schedulerjob::FOCUSING);
//        updateJobInfo(&o);
//        while(true){
//            QDBusReply<bool> replyfocus = focusinterface->call(QDBus::AutoDetect,"isAutoFocusComplete");
//            if(!replyfocus.value())
//            {
//                if(o.getOnTimeCheck())
//                {
//                    if(QTime::currentTime().hour() >= o.getFinishingHour() && QTime::currentTime().minute() >= o.getFinishingMinute())
//                        return;
//                }
//                QEventLoop loop;
//                QTimer::singleShot(1000, &loop, SLOT(quit()));
//                loop.exec();
//            }
//            else break;
//        }
//        QDBusReply<bool> replyfocus2 = focusinterface->call(QDBus::AutoDetect,"isAutoFocusSuccessful");
//        if(!replyfocus2){
//            o.setState(Schedulerjob::ABORTED);
//            updateJobInfo(&o);
//            return;
//        }
//        QEventLoop loopFocus;
//        QTimer::singleShot(1000, &loopFocus, SLOT(quit()));
//        loopFocus.exec();
//    }

//    //Allignment - not working
//    //i think it exits the while loop but then it stucks.
////    if(o.getAlignCheck())
////    {
////        aligninterface->call(QDBus::AutoDetect,"captureAndSolve");
////        while(true){
////            QDBusReply<bool> replyalign = aligninterface->call(QDBus::AutoDetect,"isSolverComplete");
////            QMessageBox::information(NULL, "Success", QString::number(replyalign.value()));
////            if(!replyalign.value())
////            {
////                QMessageBox::information(NULL, "Success", QString::number(replyalign.value()));
////                if(o.getOnTimeCheck())
////                {
////                    if(QTime::currentTime().hour() >= o.getFinishingHour() && QTime::currentTime().minute() >= o.getFinishingMinute())
////                        return;
////                }

////                //to be added state
////            }
////            else break;
////        }

////    }

//      //CCD Exposure

//      //Loading Sequence file
//      QList<QVariant> loadsequence;
//      loadsequence.append(filename);
//      captureinterface->callWithArgumentList(QDBus::AutoDetect,"loadSequenceQueue",loadsequence);

//      //Starting the jobs
//      captureinterface->call(QDBus::AutoDetect,"startSequence");
//      //Wait for sequence
////      QEventLoop loop4;
////      QTimer::singleShot(4000, &loop4, SLOT(quit()));
////      loop4.exec();
//      o.setState(Schedulerjob::CAPTURING);
//      updateJobInfo(&o);
//      while(true){
//          QDBusReply<QString> replysequence = captureinterface->call(QDBus::AutoDetect,"getSequenceQueueStatus");
//          if(replysequence.value().toStdString()=="Aborted" || replysequence.value().toStdString()=="Error")
//          {
//              o.setState(Schedulerjob::ABORTED);
//              break;
//          }
//          if(replysequence.value().toStdString()!="Complete")
//          {
//              if(o.getOnTimeCheck())
//              {
//                  if(QTime::currentTime().hour() >= o.getFinishingHour() && QTime::currentTime().minute() >= o.getFinishingMinute())
//                      return;
//              }
//              QEventLoop loop;
//              QTimer::singleShot(1000, &loop, SLOT(quit()));
//              loop.exec();
//          }
//          else break;
//      }
//    updateJobInfo(&o);
//      //Clear sequence
//      captureinterface->call(QDBus::AutoDetect,"clearSequenceQueue");

//      int checking = 0 ;
//      for(int j=0;j<objects.length();j++)
//          if(objects.at(j).getScore()==0)
//              checking = 1;
//      if(checking==1)
//           connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(evaluateJobs()));
//      if(iterations==objects.length())
//          stopindi();
//}

void Scheduler::saveSlot()
{
    //Write xml
    QDomDocument document;

    //Root element
    QDomElement root = document.createElement("Observable");
    document.appendChild(root);

    int i;
    for(i=0;i<objects.length();i++)
    {
        QDomElement obj = document.createElement("Object");
        obj.setAttribute("Name",objects.at(i).getName());
        obj.setAttribute("RA",objects.at(i).getRA());
        obj.setAttribute("DEC",objects.at(i).getDEC());
        obj.setAttribute("Sequence",objects.at(i).getFileName());
        obj.setAttribute("Now",objects.at(i).getNowCheck());
        obj.setAttribute("On Time",objects.at(i).getOnTimeCheck());
        obj.setAttribute("On Alt",objects.at(i).getSpecificAlt());
        obj.setAttribute("Start Time",objects.at(i).getSpecificTime());
        obj.setAttribute("MoonSep",objects.at(i).getMoonSeparationCheck());
        obj.setAttribute("MoonSepVal",objects.at(i).getMoonSeparation());
        obj.setAttribute("MerFlip",objects.at(i).getMeridianFlip());
        obj.setAttribute("Loop",objects.at(i).getLoopCheck());
        obj.setAttribute("When Seq completes",objects.at(i).getWhenSeqCompCheck());
        obj.setAttribute("FinTime",objects.at(i).getOnTimeCheck());
        obj.setAttribute("FinTimeVal",objects.at(i).getFinTime());
        root.appendChild(obj);
    }

    QFile file("SchedulerQueue.sch");
    file.open(QIODevice::ReadWrite | QIODevice::Text);
    QTextStream stream(&file);
    stream<<document.toString();
    file.close();
}

void Scheduler::startSlot()
{
    pi->startAnimation();
    //startEkos();
    //QMessageBox::information(NULL, "Success", QString::number(objects.at(0).getOb()->angularDistanceTo(moon).Degrees()));
    connect(KStars::Instance()->data()->clock(), SIGNAL(timeAdvanced()), this, SLOT(evaluateJobs()));
}

void Scheduler::setSequenceSlot()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
    "",
    tr("Seq Files (*.esq)"));
    FileNameEdit->setText(fileName);
}

void Scheduler::addToTableSlot()
{
    if(NameEdit->text().length()!=0 && DecEdit->text().length()!=0 && RaEdit->text().length()!=0 && FileNameEdit->text().length()!=0)
    {
        //Adding to vector
        //Start up
        Schedulerjob newOb;
        newOb.setOb(o);
        newOb.setName(NameEdit->text());
        newOb.setRA(RaEdit->text());
        newOb.setDEC(DecEdit->text());
        newOb.setFileName(FileNameEdit->text());
        if(OnButton->isChecked()){
            newOb.setSpecificTime(true);
            newOb.setStartTime(dateTimeEdit->time().toString());
            newOb.setHours(dateTimeEdit->time().hour());
            newOb.setMinutes(dateTimeEdit->time().minute());
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
        }
        else if(seqCompButton->isChecked())
            newOb.setWhenSeqCompCheck(true);
        else if(loopButton->isChecked())
            newOb.setLoopCheck(true);

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
    }
    else
        QMessageBox::information(NULL, "Error", "All fields must be completed");
}

void Scheduler::removeTableSlot()
{
    tableWidget->removeRow(tableWidget->currentRow());
    tableCountRow--;
}

void Scheduler::selectSlot()
{
    QPointer<FindDialog> fd = new FindDialog( KStars::Instance() );
    if ( fd->exec() == QDialog::Accepted ) {
        o = fd->selectedObject();
        if( o != 0 ) {
            NameEdit->setText(o->name());
            RaEdit->setText(o->ra().toHMSString());
            DecEdit->setText(o->dec().toDMSString());
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
