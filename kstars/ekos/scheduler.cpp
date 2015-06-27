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

struct Sequence {
    float exposure;
    int binx;
    int biny;
    int framex;
    int framey;
    int W;
    int H;
    int count;
    int delay;

    bool type;
    bool filter;
    bool duration;
    bool TS;

    QString Type;
    QString Filter;
    QString FITSDir;
};

struct Observable {
QString name;
QString RA;
QString DEC;
QString startTime;
QString finTime;
QString fileName;

SkyObject *ob;
mutable Sequence seq;

float alt;
float MoonSep;
int hours;
int mins;

bool Now;
bool specTime;
bool specAlt;
bool moonSep;
bool merFlip;

bool whenSeqComp;
bool loop;
bool onTime;
};

int tableCountRow=0;
int tableCountCol;

QVector<Observable> objects;
SkyObject *o;

Scheduler::Scheduler()
{
    setupUi(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Scheduler",  this);
       // connect(SelectButton,SIGNAL(clicked()),this,SLOT(newslot()));
        connect(SelectButton,SIGNAL(clicked()),this,SLOT(selectSlot()));
        connect(addToQueueB,SIGNAL(clicked()),this,SLOT(addToTableSlot()));
        connect(removeFromQueueB,SIGNAL(clicked()),this,SLOT(removeTableSlot()));
        connect(AddButton_3,SIGNAL(clicked()),this,SLOT(setSequenceSlot()));
        connect(startButton,SIGNAL(clicked()),this,SLOT(startSlot()));
        connect(queueSaveAsB,SIGNAL(clicked()),this,SLOT(saveSlot()));
}

void Scheduler::processSession(int i){
    //Dbus
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface *interface = new QDBusInterface("org.kde.kstars",
                                                   "/KStars/INDI",
                                                   "org.kde.kstars.INDI",
                                                   bus,
                                                   this);

    //INDI
    //Alternative method
    Options::setTelescopeDriver("Telescope Simulator");
    Options::setCCDDriver("CCD Simulator");
    EkosManager *ob;
            ob = KStars::Instance()->ekosManager();
            ob->processINDI();
    //

    //DBUS method-currently not working
//    QList<QVariant> args;
//    args.append("7624");
//    args.append("indi_simulator_telescope");
//    args.append("indi_simulator_ccd");
//    QDBusReply<int> reply = interface->callWithArgumentList(QDBus::AutoDetect,"start",args);
//    qDebug()<<reply;
//    if (reply.isValid())
//    {
//         QMessageBox::information(NULL, "Good", "Good");
//    }
    //

    QEventLoop loop;
    QTimer::singleShot(3000, &loop, SLOT(quit()));
    loop.exec();

    //Telescope
    QList<QVariant> args2;
    args2.append("Telescope Simulator");
    args2.append("CONNECTION");
    args2.append("CONNECT");
    args2.append("On");
     QDBusReply<QString> reply2 =  interface->callWithArgumentList(QDBus::AutoDetect,"setSwitch",args2);
    QList<QVariant> args3;
    args3.append("Telescope Simulator");
    args3.append("CONNECTION");
    interface->callWithArgumentList(QDBus::AutoDetect,"sendProperty",args3);

    QDBusReply<QString> reply3 = interface->callWithArgumentList(QDBus::AutoDetect,"getPropertyState",args3);

    //CCD
    QList<QVariant> args4;
    args4.append("CCD Simulator");
    args4.append("CONNECTION");
    args4.append("CONNECT");
    args4.append("On");
     QDBusReply<QString> reply4 =  interface->callWithArgumentList(QDBus::AutoDetect,"setSwitch",args4);
    QList<QVariant> args5;
    args5.append("CCD Simulator");
    args5.append("CONNECTION");
    interface->callWithArgumentList(QDBus::AutoDetect,"sendProperty",args5);

    QDBusReply<QString> reply5 = interface->callWithArgumentList(QDBus::AutoDetect,"getPropertyState",args5);
    QEventLoop loop2;
    QTimer::singleShot(5000, &loop2, SLOT(quit()));
    loop2.exec();

    //Telescope
    QList<QVariant> args6;
    args6.append("Telescope Simulator");
    args6.append("EQUATORIAL_EOD_COORD");
    args6.append("RA");
    args6.append(objects.at(i).ob->ra().Hours());

    QDBusReply<QString> reply6 = interface->callWithArgumentList(QDBus::AutoDetect,"setNumber",args6);
    //DEC
    QList<QVariant> args7;
    args7.append("Telescope Simulator");
    args7.append("EQUATORIAL_EOD_COORD");
    args7.append("DEC");
    args7.append(objects.at(i).ob->dec().Degrees());
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
    //Binning
    QList<QVariant> args11;
    args11.append("CCD Simulator");
    args11.append("CCD_BINNING");
    args11.append("HOR_BIN");
    args11.append(objects.at(i).seq.binx);
    QDBusReply<QString> reply11 = interface->callWithArgumentList(QDBus::AutoDetect,"setNumber",args11);

    QList<QVariant> args10;
    args10.append("CCD Simulator");
    args10.append("CCD_BINNING");
    args10.append("VER_BIN");
    args10.append(objects.at(i).seq.biny);
    QDBusReply<QString> reply10 = interface->callWithArgumentList(QDBus::AutoDetect,"setNumber",args10);

    QList<QVariant> argsbin;
    argsbin.append("CCD Simulator");
    argsbin.append("CCD_BINNING");
    QDBusReply<QString> replybin = interface->callWithArgumentList(QDBus::AutoDetect,"sendProperty",argsbin);

    //Type
    QList<QVariant> argst;
    argst.append("CCD Simulator");
    argst.append("CCD_FRAME_TYPE");
    if(!qstrcmp(objects.at(i).seq.Type.toStdString().c_str(),"Light"))
         argst.append("FRAME_LIGHT");
    if(!qstrcmp(objects.at(i).seq.Type.toStdString().c_str(),"Bias"))
         argst.append("FRAME_BIAS");
    if(!qstrcmp(objects.at(i).seq.Type.toStdString().c_str(),"Dark"))
         argst.append("FRAME_DARK");
    if(!qstrcmp(objects.at(i).seq.Type.toStdString().c_str(),"Flat"))
         argst.append("FRAME_FLAT");
    argst.append("On");
    interface->callWithArgumentList(QDBus::AutoDetect,"setSwitch",argst);

    QList<QVariant> argstt;
    argstt.append("CCD Simulator");
    argstt.append("CCD_FRAME_TYPE");
    interface->callWithArgumentList(QDBus::AutoDetect,"sendProperty",argstt);

    //Frame
    QList<QVariant> argsf;
    argsf.append("CCD Simulator");
    argsf.append("CCD_FRAME");
    argsf.append("X");
    argsf.append(objects.at(i).seq.framex);
    interface->callWithArgumentList(QDBus::AutoDetect,"setNumber",argsf);

    argsf.clear();
    argsf.append("CCD Simulator");
    argsf.append("CCD_FRAME");
    argsf.append("Y");
    argsf.append(objects.at(i).seq.framey);
    interface->callWithArgumentList(QDBus::AutoDetect,"setNumber",argsf);

    argsf.clear();
    argsf.append("CCD Simulator");
    argsf.append("CCD_FRAME");
    argsf.append("WIDTH");
    argsf.append(objects.at(i).seq.W);
    interface->callWithArgumentList(QDBus::AutoDetect,"setNumber",argsf);

    argsf.clear();
    argsf.append("CCD Simulator");
    argsf.append("CCD_FRAME");
    argsf.append("HEIGHT");
    argsf.append(objects.at(i).seq.H);
    interface->callWithArgumentList(QDBus::AutoDetect,"setNumber",argsf);

    //Exposure
    QList<QVariant> args8;
    args8.append("CCD Simulator");
    args8.append("CCD_EXPOSURE");
    args8.append("CCD_EXPOSURE_VALUE");
    args8.append(objects.at(i).seq.exposure);
    QDBusReply<QString> reply8 = interface->callWithArgumentList(QDBus::AutoDetect,"setNumber",args8);

    QList<QVariant> args9;
    args9.append("CCD Simulator");
    args9.append("CCD_EXPOSURE");
    QDBusReply<QString> reply9 = interface->callWithArgumentList(QDBus::AutoDetect,"sendProperty",args9);

    //Waiting for exposure
    while(true){
        QDBusReply<QString> replye = interface->callWithArgumentList(QDBus::AutoDetect,"getPropertyState",args9);
        if(replye.value().toStdString()!="Ok")
        {
            QEventLoop loop;
            QTimer::singleShot(1000, &loop, SLOT(quit()));
            loop.exec();
        }
        else break;
    }
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
        obj.setAttribute("Name",objects.at(i).name);
        obj.setAttribute("RA",objects.at(i).RA);
        obj.setAttribute("DEC",objects.at(i).DEC);
        obj.setAttribute("Sequence",objects.at(i).fileName);
        obj.setAttribute("Now",objects.at(i).Now);
        obj.setAttribute("On Time",objects.at(i).onTime);
        obj.setAttribute("On Alt",objects.at(i).specAlt);
        obj.setAttribute("Start Time",objects.at(i).specTime);
        obj.setAttribute("MoonSep",objects.at(i).moonSep);
        obj.setAttribute("MoonSepVal",objects.at(i).MoonSep);
        obj.setAttribute("MerFlip",objects.at(i).merFlip);
        obj.setAttribute("Loop",objects.at(i).loop);
        obj.setAttribute("When Seq completes",objects.at(i).whenSeqComp);
        obj.setAttribute("FinTime",objects.at(i).onTime);
        obj.setAttribute("FinTimeVal",objects.at(i).finTime);
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

    for(i=0;i<objects.length();i++)
        {
            QUrl fileURL = objects.at(i).fileName;
            if (fileURL.isEmpty())
                return;

            if (fileURL.isValid() == false)
            {
               QString message = xi18n( "Invalid URL: %1", fileURL.path() );
               KMessageBox::sorry( 0, message, xi18n( "Invalid URL" ) );
               return;
            }

            QFile sFile;
            sFile.setFileName(fileURL.path());

            if ( !sFile.open( QIODevice::ReadOnly))
            {
                QString message = xi18n( "Unable to open file %1",  fileURL.path());
                KMessageBox::sorry( 0, message, xi18n( "Could Not Open File" ) );
                return;
            }

            LilXML *xmlParser = newLilXML();
            char errmsg[MAXRBUF];
            XMLEle *root = NULL;
            XMLEle *ep1;
            char c;

            while ( sFile.getChar(&c))
            {
                root = readXMLEle(xmlParser, c, errmsg);

                if (root)
                {
                     for (ep1 = nextXMLEle(root, 1) ; ep1 != NULL ; ep1 = nextXMLEle(root, 0))
                     {

                         XMLEle *ep;
                         XMLEle *subEP;

                         for (ep = nextXMLEle(ep1, 1) ; ep != NULL ; ep = nextXMLEle(ep1, 0))
                         {
                             if (!strcmp(tagXMLEle(ep), "Exposure"))
                                 objects.at(i).seq.exposure = atof(pcdataXMLEle(ep));
                             else if (!strcmp(tagXMLEle(ep), "Binning"))
                             {
                                 subEP = findXMLEle(ep, "X");
                                 if (subEP)
                                    objects.at(i).seq.binx = atoi(pcdataXMLEle(subEP));
                                 subEP = findXMLEle(ep, "Y");
                                 if (subEP)
                                     objects.at(i).seq.biny = atoi(pcdataXMLEle(subEP));
                             }
                             else if (!strcmp(tagXMLEle(ep), "Frame"))
                             {
                                 subEP = findXMLEle(ep, "X");
                                 if (subEP)
                                     objects.at(i).seq.framex = atoi(pcdataXMLEle(subEP));
                                 subEP = findXMLEle(ep, "Y");
                                 if (subEP)
                                      objects.at(i).seq.framey = atoi(pcdataXMLEle(subEP));
                                 subEP = findXMLEle(ep, "W");
                                 if (subEP)
                                      objects.at(i).seq.W = atoi(pcdataXMLEle(subEP));
                                 subEP = findXMLEle(ep, "H");
                                 if (subEP)
                                      objects.at(i).seq.H = atoi(pcdataXMLEle(subEP));
                             }
                             else if (!strcmp(tagXMLEle(ep), "Filter"))
                             {
                                 if(atoi(pcdataXMLEle(ep))==1)
                                     objects.at(i).seq.Filter = "Red";
                                 if(atoi(pcdataXMLEle(ep))==2)
                                     objects.at(i).seq.Filter = "Green";
                                 if(atoi(pcdataXMLEle(ep))==3)
                                     objects.at(i).seq.Filter = "Blue";
                                 if(atoi(pcdataXMLEle(ep))==4)
                                     objects.at(i).seq.Filter = "H_Alpha";
                                 if(atoi(pcdataXMLEle(ep))==5)
                                     objects.at(i).seq.Filter = "Luminosity";
                             }
                             else if (!strcmp(tagXMLEle(ep), "Type"))
                             {
                                 if(atoi(pcdataXMLEle(ep))==0)
                                     objects.at(i).seq.Type = "Light";
                                 if(atoi(pcdataXMLEle(ep))==1)
                                     objects.at(i).seq.Type = "Bias";
                                 if(atoi(pcdataXMLEle(ep))==2)
                                     objects.at(i).seq.Type = "Dark";
                                 if(atoi(pcdataXMLEle(ep))==3)
                                     objects.at(i).seq.Type = "Flat";
                             }
                             else if (!strcmp(tagXMLEle(ep), "Count"))
                             {
                                 objects.at(i).seq.count = atoi(pcdataXMLEle(ep));
                             }
                             else if (!strcmp(tagXMLEle(ep), "Delay"))
                             {
                                 objects.at(i).seq.delay = atoi(pcdataXMLEle(ep));
                             }
                             else if (!strcmp(tagXMLEle(ep), "FITSDirectory"))
                             {
                                 objects.at(i).seq.FITSDir = pcdataXMLEle(ep);
                             }
                         }

                     }
                     delXMLEle(root);
                }
            }
        }

    for(i=0;i<objects.length();i++){
        if(objects.at(i).Now==true)
        {
           processSession(i);
           tableWidget->setItem(i,tableCountCol+1,new QTableWidgetItem("Done"));
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
}

void Scheduler::setSequenceSlot()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
    "",
    tr("Seq Files (*.esq)"));
    lineEdit_3->setText(fileName);
}

void Scheduler::addToTableSlot()
{
    if(lineEdit_6->text().length()!=0 && lineEdit_2->text().length()!=0 && lineEdit->text().length()!=0 && lineEdit_3->text().length()!=0)
    {
        //Adding to vector
        //Start up
        Observable newOb;
        newOb.ob=o;
        newOb.name = lineEdit_6->text();
        newOb.RA = lineEdit->text();
        newOb.DEC = lineEdit_2->text();
        newOb.fileName = lineEdit_3->text();
        if(OnButton->isChecked()){
            newOb.specTime = true;
            newOb.startTime = dateTimeEdit->time().toString();
            newOb.hours = dateTimeEdit->time().hour();
            newOb.mins = dateTimeEdit->time().minute();
        }
        else if(NowButton->isChecked()){
            newOb.Now = true;
            newOb.startTime = QTime::currentTime().toString();
            newOb.hours = QTime::currentTime().hour();
            newOb.mins = QTime::currentTime().minute();
        }
        else if(altButton->isChecked()){
            newOb.specAlt = true;
            newOb.alt = doubleSpinBox->value();
        }

        //Constraints
        if(moonSepBox->isChecked()){
            newOb.moonSep = true;
            newOb.MoonSep = doubleSpinBox_3->value();
        }
        else if(merFlipBox->isChecked())
            newOb.merFlip = true;

        //Completion
        if(onFinButton->isChecked()){
            newOb.onTime = true;
            newOb.finTime = dateTimeEdit_2->time().toString();
        }
        else if(seqCompButton->isChecked())
            newOb.whenSeqComp = true;
        else if(loopButton->isChecked())
            newOb.loop = true;

        objects.append(newOb);

        //Adding to table
        tableCountCol=0;
        const int currentRow = tableWidget->rowCount();
        tableWidget->setRowCount(currentRow + 1);
        tableWidget->setItem(tableCountRow,tableCountCol,new QTableWidgetItem(lineEdit_6->text()));
        tableWidget->setItem(tableCountRow,tableCountCol+1,new QTableWidgetItem("Idle"));
        tableWidget->setItem(tableCountRow,tableCountCol+2,new QTableWidgetItem(dateTimeEdit->time().toString()));
        if(onFinButton->isChecked())
            tableWidget->setItem(tableCountRow,tableCountCol+3,new QTableWidgetItem(dateTimeEdit_2->time().toString()));
        else
            tableWidget->setItem(tableCountRow,tableCountCol+3,new QTableWidgetItem("N/S"));
        tableCountRow++;
        lineEdit->clear();
        lineEdit_2->clear();
        lineEdit_6->clear();
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
            lineEdit_6->setText(o->name());
            lineEdit->setText(o->ra().toHMSString());
            lineEdit_2->setText(o->dec().toDMSString());
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
