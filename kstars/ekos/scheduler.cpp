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
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Scheduler",  this);
       // connect(SelectButton,SIGNAL(clicked()),this,SLOT(newslot()));
        connect(SelectButton,SIGNAL(clicked()),this,SLOT(selectSlot()));
        connect(addToQueueB,SIGNAL(clicked()),this,SLOT(addToTableSlot()));
        connect(removeFromQueueB,SIGNAL(clicked()),this,SLOT(removeTableSlot()));
        connect(LoadButton,SIGNAL(clicked()),this,SLOT(setSequenceSlot()));
        connect(startButton,SIGNAL(clicked()),this,SLOT(startSlot()));
        connect(queueSaveAsB,SIGNAL(clicked()),this,SLOT(saveSlot()));
}

void Scheduler::stopindi(){
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface *ekosinterface = new QDBusInterface("org.kde.kstars",
                                                   "/KStars/Ekos",
                                                   "org.kde.kstars.Ekos",
                                                   bus,
                                                   this);
    ekosinterface->call(QDBus::AutoDetect,"stop");
}

void Scheduler::processSession(int i, QUrl filename){
    //Dbus
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface *interface = new QDBusInterface("org.kde.kstars",
                                                   "/KStars/INDI",
                                                   "org.kde.kstars.INDI",
                                                   bus,
                                                   this);
    QDBusConnection bus2 = QDBusConnection::sessionBus();
    QDBusInterface *ekosinterface = new QDBusInterface("org.kde.kstars",
                                                   "/KStars/Ekos",
                                                   "org.kde.kstars.Ekos",
                                                   bus2,
                                                   this);

    QDBusConnection bus3 = QDBusConnection::sessionBus();
    QDBusInterface *captureinterface = new QDBusInterface("org.kde.kstars",
                                                   "/KStars/Ekos/Capture",
                                                   "org.kde.kstars.Ekos.Capture",
                                                   bus3,
                                                   this);
    //Ekos Start
    QDBusReply<bool> ekosisstarted = ekosinterface->call(QDBus::AutoDetect,"isStarted");
    if(!ekosisstarted.value())
        ekosinterface->call(QDBus::AutoDetect,"start");

    QEventLoop loop;
    QTimer::singleShot(3000, &loop, SLOT(quit()));
    loop.exec();

    //Telescope and CCD

    ekosinterface->call(QDBus::AutoDetect,"connectDevices");
    QEventLoop loop2;
    QTimer::singleShot(5000, &loop2, SLOT(quit()));
    loop2.exec();

    //Telescope - here we slew (using INDI for the moment)
    QList<QVariant> args6;
    args6.append("Telescope Simulator");
    args6.append("EQUATORIAL_EOD_COORD");
    args6.append("RA");
    args6.append(objects.at(i).getOb()->ra().Hours());

    QDBusReply<QString> reply6 = interface->callWithArgumentList(QDBus::AutoDetect,"setNumber",args6);
    //DEC
    QList<QVariant> args7;
    args7.append("Telescope Simulator");
    args7.append("EQUATORIAL_EOD_COORD");
    args7.append("DEC");
    args7.append(objects.at(i).getOb()->dec().Degrees());
    QDBusReply<QString> reply7 = interface->callWithArgumentList(QDBus::AutoDetect,"setNumber",args7);

    QList<QVariant> argsaux;
    argsaux.append("Telescope Simulator");
    argsaux.append("EQUATORIAL_EOD_COORD");
    QDBusReply<QString> replyaux = interface->callWithArgumentList(QDBus::AutoDetect,"sendProperty",argsaux);

    //Waiting for slew
    while(true){
        QDBusReply<QString> replys = interface->callWithArgumentList(QDBus::AutoDetect,"getPropertyState",argsaux);
        if(replys.value().toStdString()!="Ok")
        {
            QEventLoop loop;
            QTimer::singleShot(1000, &loop, SLOT(quit()));
            loop.exec();
        }
        else break;
    }

    //CCD Exposure

      //Loading Sequence file
      QList<QVariant> loadsequence;
      loadsequence.append(filename);
      captureinterface->callWithArgumentList(QDBus::AutoDetect,"loadSequenceQueue",loadsequence);

      //Starting the jobs
      captureinterface->call(QDBus::AutoDetect,"startSequence");
}

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
    int i;
    for(i=0;i<objects.length();i++){
        if(objects.at(i).getNowCheck()==true)
        {
            //here i use default limits but i will need a DBUS call to access the values
            //Also here i will need getSequenceStatus(), clearSequence(). After an ibject is done
            //we clear sequence
            if(objects.at(i).getOb()->alt().degree() > 0 && objects.at(i).getOb()->alt().degree() < 90){
                   QUrl fileURL = objects.at(i).getFileName();
                   processSession(i,fileURL);
                   tableWidget->setItem(i,tableCountCol+1,new QTableWidgetItem("Done"));
                   QEventLoop loop;
                   QTimer::singleShot(4000, &loop, SLOT(quit()));
                   loop.exec();
            }
            else tableWidget->setItem(i,tableCountCol+1,new QTableWidgetItem("Aborted"));
        }

        //Currently not working
//        if(objects.at(i).specTime==true)
//        {
//            while(true){
//                QEventLoop loop;
//                QTimer::singleShot(10000, &loop, SLOT(quit()));
//                loop.exec();
//                if(objects.at(i).hours==QTime::currentTime().hour() && objects.at(i).mins == QTime::currentTime().minute())
//                    break;
//            }
//            processSession(i);
//        }
        //
    }
    //Jobs are finished, stoppig indi
    QEventLoop loop;
    QTimer::singleShot(4000, &loop, SLOT(quit()));
    loop.exec();
    stopindi();
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
            newOb.setOnTimeCheck(true);
            newOb.setFinTime(dateTimeEdit_2->time().toString());
        }
        else if(seqCompButton->isChecked())
            newOb.setWhenSeqCompCheck(true);
        else if(loopButton->isChecked())
            newOb.setLoopCheck(true);

        objects.append(newOb);

        //Adding to table
        tableCountCol=0;
        const int currentRow = tableWidget->rowCount();
        tableWidget->setRowCount(currentRow + 1);
        tableWidget->setItem(tableCountRow,tableCountCol,new QTableWidgetItem(NameEdit->text()));
        tableWidget->setItem(tableCountRow,tableCountCol+1,new QTableWidgetItem("Idle"));
        tableWidget->setItem(tableCountRow,tableCountCol+2,new QTableWidgetItem(dateTimeEdit->time().toString()));
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
