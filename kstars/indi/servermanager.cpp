/*  INDI Server Manager
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include <errno.h>

#include <indicom.h>

#include <config-kstars.h>

#include <QTcpSocket>
#include <QTextEdit>

#include <KProcess>
#include <KLocale>
#include <KDebug>
#include <KMessageBox>
#include <KStatusBar>
#include <KStandardDirs>


#include "servermanager.h"
#include "drivermanager.h"
#include "driverinfo.h"

#include "Options.h"
#include "kstars.h"
#include "kstarsdatetime.h"


const int INDI_MAX_TRIES=3;
const int MAX_FILENAME_LEN = 1024;

ServerManager::ServerManager(QString inHost, uint inPort)
{
    serverProcess	= NULL;
    XMLParser		= NULL;
    host		= inHost;
    port		= QString::number(inPort);

    //qDebug() << "We got port unit with value of " << inPort << " and now as tring it is equal to #" << port << "#" << endl;
}

ServerManager::~ServerManager()
{
    serverSocket.close();
    indiFIFO.close();

    if (serverProcess)
        serverProcess->close();

    delete (serverProcess);
}

bool ServerManager::start()
  {
    bool connected=false;
    int fd=0;
    serverProcess = new KProcess(this);

    *serverProcess << Options::indiServer();
    *serverProcess << "-v" << "-p" << port;
    *serverProcess << "-m" << "100";

    QString fifoFile = QString("/tmp/indififo%1").arg(QUuid::createUuid().toString().mid(1, 8));

    if ( (fd = mkfifo (fifoFile.toLatin1(), S_IRUSR| S_IWUSR) < 0))
    {
         KMessageBox::error(NULL, i18n("Error making FIFO file %1: %2.", fifoFile, strerror(errno)));
         return false;
   }

    indiFIFO.setFileName(fifoFile);

   if (!indiFIFO.open(QIODevice::ReadWrite | QIODevice::Text))
   {
         qDebug() << "Unable to create INDI FIFO file: " << fifoFile << endl;
         return false;
   }


    *serverProcess << "-f" << fifoFile;

    serverProcess->setOutputChannelMode(KProcess::SeparateChannels);
    serverProcess->setReadChannel(QProcess::StandardError);


    serverProcess->start();

    connected = serverProcess->waitForStarted();

    if (connected)
    {
        connect(serverProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processServerError(QProcess::ProcessError)));
        connect(serverProcess, SIGNAL(readyReadStandardError()),  this, SLOT(processStandardError()));

        emit started();
    }

    return connected;



}

bool ServerManager::startDriver(DriverInfo *dv)
{
    QTextStream out(&indiFIFO);

    managedDrivers.append(dv);
    dv->setServerManager(this);

    dv->setUniqueLabel(DriverManager::Instance()->getUniqueDeviceLabel(dv->getTreeLabel()));

     if (KStandardDirs::findExe(dv->getDriver()).isEmpty())
     {
         KMessageBox::error(NULL, i18n("Driver %1 was not found on the system. Please make sure the package that provides the '%1' binary is installed.", dv->getDriver()));
         return false;
     }

        //qDebug() << "Will run driver: " << dv->getName() << " with driver " << dv->getDriver() << endl;
        out << "start " << dv->getDriver() << " '" << dv->getUniqueLabel() << "'" << endl;
        //qDebug() << "Writing to " << file_template << endl << out.string() << endl;
        out.flush();

        dv->setServerState(true);

        dv->setPort(port);

        return true;
}

void ServerManager::stopDriver(DriverInfo *dv)
{
    QTextStream out(&indiFIFO);

    managedDrivers.removeOne(dv);


        //qDebug() << "Will run driver: " << dv->getName() << " with driver " << dv->getDriver() << endl;
        out << "stop " << dv->getDriver() << " '" << dv->getUniqueLabel() << "'" << endl;
        //qDebug() << "Writing to " << file_template << endl << out.string() << endl;
        out.flush();

        dv->setServerState(false);

        dv->setPort(dv->getUserPort());

}

bool ServerManager::isDriverManaged(DriverInfo *di)
{
    foreach(DriverInfo *dv, managedDrivers)
    {
        if (dv == di)
            return true;
    }

    return false;
}

void ServerManager::stop()
{

    if (serverProcess == NULL)
        return;

    foreach(DriverInfo *device, managedDrivers)
    {
        device->setServerState(false);
        device->clear();
    }

    serverProcess->disconnect(SIGNAL(error(QProcess::ProcessError)));

    serverProcess->terminate();

    serverProcess->waitForFinished();

    delete serverProcess;

    serverProcess = NULL;

}

void ServerManager::connectionSuccess()
{

    foreach(DriverInfo *device, managedDrivers)
         device->setServerState(true);

    connect(serverProcess, SIGNAL(readyReadStandardError()),  this, SLOT(processStandardError()));

    emit started();
}

void ServerManager::processServerError(QProcess::ProcessError err)
{
  emit serverFailure(this);
}

void ServerManager::processStandardError()
{

    serverBuffer.append(serverProcess->readAllStandardError());

    //qDebug() << "Recevined new stderr from server " << serverBuffer << endl;

    emit newServerLog();
}

QString ServerManager::errorString()
{
    if (serverProcess)
        return serverProcess->errorString();

    return NULL;
}

#include "servermanager.moc"
